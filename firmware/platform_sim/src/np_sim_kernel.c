/*
 * The hub's task layer on a virtual clock (NP-SIM-FW-001 §31, OI-SIMFW-45, OI-SIMFW-50).  HOST ONLY; see np_sim_kernel.h for what a task,
 * a tick and a timeout mean here and what this is not.
 *
 * It defines the FreeRTOS calls the hub's own sources make (task creation, delays, event groups, binary semaphores and mutexes), so
 * np_hub_control_main.c's five tasks, np_session_runner.c and np_transport.c run as the device's code.  Nothing in firmware/ is changed.
 *
 * Mechanism: one host thread per task, and a baton (g_cur) that names the one thread allowed to run.  A thread gives the baton away
 * only in a kernel call.  The thread of the harness (the "host") holds the baton between steps and takes it back whenever every task
 * is blocked, which is where virtual time moves.
 */
#include "np_sim_kernel.h"
#include "np_sim_bus.h"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define K_MAX_TASKS      8U
#define K_STACK_BYTES    (1024U * 1024U)
#define K_SWITCH_LIMIT   2000000U   /* context switches at one virtual instant before the run is declared wedged */
#define K_SEM_MAGIC      0x5E4A0001U
#define K_EVG_MAGIC      0xE76A0002U

typedef enum { W_NONE = 0, W_DELAY, W_EVENT, W_SEM } wait_t;
typedef enum { T_READY = 0, T_RUNNING, T_BLOCKED, T_DONE } tstate_t;

typedef struct ksem ksem_t;
typedef struct kevg kevg_t;

typedef struct ktask {
    TaskFunction_t fn;
    void          *arg;
    char           name[configMAX_TASK_NAME_LEN];
    UBaseType_t    prio;
    pthread_t      th;
    pthread_cond_t cv;
    tstate_t       st;
    wait_t         wait;
    uint64_t       deadline;
    bool           has_deadline;
    bool           timed_out;
    kevg_t        *evg;
    EventBits_t    want, got;
    bool           clear, all;
    ksem_t        *sem;
    bool           sem_got;
    uint64_t       seq;
} ktask_t;

struct kevg { uint32_t magic; EventBits_t bits; };
struct ksem { uint32_t magic; uint8_t kind; /* 0 binary, 1 mutex, 2 recursive mutex */ uint8_t count; void *owner; unsigned depth; };

#define HOST_OWNER ((void *)&g_tasks)

static ktask_t         g_tasks[K_MAX_TASKS];
static unsigned        g_ntasks;
static bool            g_sched;
static bool            g_boot_returned;
static uint64_t        g_seq;
static uint32_t        g_switches;
static uint32_t        g_switches_at_instant;
static uint64_t        g_instant;
static const ktask_t  *g_cur;           /* the thread that holds the baton; NULL is the host */
static pthread_mutex_t g_m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_host_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  g_boot_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  g_park_cv = PTHREAD_COND_INITIALIZER;
static __thread ktask_t *t_self;        /* NULL on the harness thread and on the boot thread */

static uint64_t now64(void) { return np_sim_now_ms(); }

/* ── the scheduler ────────────────────────────────────────────────────────────────────────────────────────────────────────────── */

static ktask_t *pick(void)
{
    ktask_t *best = NULL;
    for (unsigned i = 0U; i < g_ntasks; i++) {
        ktask_t *t = &g_tasks[i];
        if (t->st != T_READY) { continue; }
        if (best == NULL || t->prio > best->prio || (t->prio == best->prio && t->seq < best->seq)) { best = t; }
    }
    return best;
}

static void make_ready(ktask_t *t)
{
    t->st = T_READY;
    t->wait = W_NONE;
    t->has_deadline = false;
    t->seq = ++g_seq;
}

/* Give the baton to the best ready task (or to the host if none) and wait until it comes back to `self`.  `self` is NULL for the host.
 * The caller has already set self's state: READY if it only yields, BLOCKED if it waits, DONE if it ended. */
static void switch_away(ktask_t *self)
{
    ktask_t *next = pick();
    if (self != NULL && next == self) { self->st = T_RUNNING; return; }
    if (self == NULL && next == NULL) { return; }

    const uint64_t t0 = now64();
    if (t0 != g_instant) { g_instant = t0; g_switches_at_instant = 0U; }
    if (++g_switches_at_instant > K_SWITCH_LIMIT) {
        np_sim_emit("{\"ev\":\"error\",\"what\":\"the hub's tasks switched %u times at one virtual instant: wedged (np_sim_kernel.c)\"}", (unsigned)g_switches_at_instant);
        fflush(stdout);
        abort();
    }
    g_switches++;
    if (next != NULL) { next->st = T_RUNNING; }

    pthread_mutex_lock(&g_m);
    g_cur = next;
    if (next != NULL) { pthread_cond_signal(&next->cv); } else { pthread_cond_signal(&g_host_cv); }
    if (self != NULL) { while (g_cur != self) { pthread_cond_wait(&self->cv, &g_m); } }
    else              { while (g_cur != NULL) { pthread_cond_wait(&g_host_cv, &g_m); } }
    pthread_mutex_unlock(&g_m);
}

/* The running task blocks (its fields say on what) and runs again only when woken. */
static void block_current(void)
{
    ktask_t *self = t_self;
    self->st = T_BLOCKED;
    switch_away(self);
}

/* A task was made ready by the running one: if it outranks the runner, the device would preempt at once. */
static void maybe_preempt(const ktask_t *woken)
{
    ktask_t *self = t_self;
    if (self == NULL || woken->prio <= self->prio) { return; }
    self->st = T_READY;
    self->seq = ++g_seq;
    switch_away(self);
}

static void expire_deadlines(void)
{
    const uint64_t now = now64();
    for (unsigned i = 0U; i < g_ntasks; i++) {
        ktask_t *t = &g_tasks[i];
        if (t->st == T_BLOCKED && t->has_deadline && t->deadline <= now) {
            t->timed_out = true;
            make_ready(t);
        }
    }
}

static uint64_t next_deadline(void)
{
    uint64_t nd = UINT64_MAX;
    for (unsigned i = 0U; i < g_ntasks; i++) {
        const ktask_t *t = &g_tasks[i];
        if (t->st == T_BLOCKED && t->has_deadline && t->deadline < nd) { nd = t->deadline; }
    }
    return nd;
}

uint32_t np_sim_kernel_run_ms(uint32_t ms)
{
    if (!g_sched) { np_sim_advance_ms_raw(ms); return ms; }
    const uint64_t target = now64() + ms;
    for (;;) {
        expire_deadlines();
        switch_away(NULL);                      /* every ready task runs until all are blocked */
        const uint64_t now = now64();
        if (now >= target) { break; }
        const uint64_t nd = next_deadline();
        uint64_t to = (nd < target) ? nd : target;
        if (to <= now) { continue; }
        np_sim_advance_ms_raw((uint32_t)(to - now));
    }
    return ms;
}

static uint32_t advance_override(uint32_t ms) { return np_sim_kernel_run_ms(ms); }

/* ── boot and tasks ───────────────────────────────────────────────────────────────────────────────────────────────────────────── */

static void *boot_main(void *p)
{
    ((void (*)(void))p)();
    pthread_mutex_lock(&g_m);
    g_boot_returned = true;
    pthread_cond_signal(&g_boot_cv);
    pthread_mutex_unlock(&g_m);
    return NULL;
}

bool np_sim_kernel_boot(void (*app_main)(void))
{
    if (g_sched || g_boot_returned || app_main == NULL) { return false; }
    np_sim_advance_ms_raw(0U);                  /* the clock is virtual from here */
    g_cur = NULL;
    pthread_t boot;
    if (pthread_create(&boot, NULL, boot_main, (void *)app_main) != 0) { return false; }
    pthread_detach(boot);
    pthread_mutex_lock(&g_m);
    while (!g_sched && !g_boot_returned) { pthread_cond_wait(&g_boot_cv, &g_m); }
    pthread_mutex_unlock(&g_m);
    if (g_sched) { np_sim_set_advance_override(advance_override); }
    return g_sched;
}

bool np_sim_kernel_running(void) { return g_sched; }

void vTaskStartScheduler(void)
{
    pthread_mutex_lock(&g_m);
    g_sched = true;
    pthread_cond_signal(&g_boot_cv);
    for (;;) { pthread_cond_wait(&g_park_cv, &g_m); }   /* does not return, as on the device */
}

static void *task_main(void *p)
{
    ktask_t *t = p;
    t_self = t;
    pthread_mutex_lock(&g_m);
    while (g_cur != t) { pthread_cond_wait(&t->cv, &g_m); }
    pthread_mutex_unlock(&g_m);
    t->fn(t->arg);
    t->st = T_DONE;                              /* a task that returns ends (vTaskDelete(NULL)) */
    switch_away(t);
    for (;;) { pause(); }
    return NULL;
}

BaseType_t xTaskCreate(TaskFunction_t fn, const char *const name, const configSTACK_DEPTH_TYPE stack, void *const arg,
                       UBaseType_t prio, TaskHandle_t *const created)
{
    (void)stack;
    if (g_ntasks >= K_MAX_TASKS || fn == NULL) { return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY; }
    ktask_t *t = &g_tasks[g_ntasks];
    memset(t, 0, sizeof *t);
    t->fn = fn;
    t->arg = arg;
    t->prio = prio;
    (void)snprintf(t->name, sizeof t->name, "%s", name != NULL ? name : "?");
    pthread_cond_init(&t->cv, NULL);
    make_ready(t);
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, K_STACK_BYTES);
    if (pthread_create(&t->th, &a, task_main, t) != 0) { pthread_attr_destroy(&a); return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY; }
    pthread_attr_destroy(&a);
    pthread_detach(t->th);
    g_ntasks++;
    if (created != NULL) { *created = (TaskHandle_t)t; }
    return pdPASS;
}

/* ── time ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────────── */

TickType_t xTaskGetTickCount(void) { return (TickType_t)(np_sim_now_ms() / portTICK_PERIOD_MS); }

static void block_for(TickType_t ticks, wait_t w)
{
    ktask_t *self = t_self;
    self->wait = w;
    self->timed_out = false;
    self->has_deadline = (ticks != portMAX_DELAY);
    self->deadline = now64() + (uint64_t)ticks * portTICK_PERIOD_MS;
    block_current();
}

void vTaskDelay(const TickType_t ticks)
{
    if (!g_sched || t_self == NULL) { np_sim_advance_ms((uint32_t)ticks * portTICK_PERIOD_MS); return; }
    if (ticks == 0U) { t_self->st = T_READY; t_self->seq = ++g_seq; switch_away(t_self); return; }
    block_for(ticks, W_DELAY);
}

BaseType_t xTaskDelayUntil(TickType_t *const prev, const TickType_t inc)
{
    const TickType_t now = xTaskGetTickCount();
    const TickType_t target = *prev + inc;
    const bool delay = (target > now);
    *prev = target;
    if (delay) { vTaskDelay(target - now); }
    return delay ? pdTRUE : pdFALSE;
}

/* ── event groups ─────────────────────────────────────────────────────────────────────────────────────────────────────────────── */

EventGroupHandle_t xEventGroupCreate(void)
{
    kevg_t *g = calloc(1U, sizeof *g);
    if (g != NULL) { g->magic = K_EVG_MAGIC; }
    return (EventGroupHandle_t)g;
}

static bool evg_satisfied(const kevg_t *g, EventBits_t want, bool all)
{
    return all ? ((g->bits & want) == want) : ((g->bits & want) != 0U);
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t h, const EventBits_t bits)
{
    kevg_t *g = (kevg_t *)h;
    g->bits |= bits;
    np_sim_emit("{\"ev\":\"runner_event\",\"t\":%u,\"set\":%u,\"bits\":%u}", (unsigned)np_sim_now_ms(), (unsigned)bits, (unsigned)g->bits);
    EventBits_t to_clear = 0U;
    ktask_t *woken[K_MAX_TASKS];
    unsigned nw = 0U;
    for (unsigned i = 0U; i < g_ntasks; i++) {
        ktask_t *t = &g_tasks[i];
        if (t->st == T_BLOCKED && t->wait == W_EVENT && t->evg == g && evg_satisfied(g, t->want, t->all)) {
            t->got = g->bits;
            if (t->clear) { to_clear |= t->want; }
            t->timed_out = false;
            make_ready(t);
            woken[nw++] = t;
        }
    }
    const EventBits_t ret = g->bits;
    g->bits &= ~to_clear;
    for (unsigned i = 0U; i < nw; i++) { maybe_preempt(woken[i]); }
    return ret;
}

/* xEventGroupGetBits is a macro for xEventGroupClearBits(g, 0). */
EventBits_t xEventGroupClearBits(EventGroupHandle_t h, const EventBits_t bits)
{
    kevg_t *g = (kevg_t *)h;
    const EventBits_t before = g->bits;
    g->bits &= ~bits;
    return before;
}

EventBits_t xEventGroupWaitBits(EventGroupHandle_t h, const EventBits_t bits, const BaseType_t clear, const BaseType_t all, const TickType_t ticks)
{
    kevg_t *g = (kevg_t *)h;
    if (evg_satisfied(g, bits, all != pdFALSE)) {
        const EventBits_t r = g->bits;
        if (clear != pdFALSE) { g->bits &= ~bits; }
        return r;
    }
    if (ticks == 0U) { return g->bits; }
    if (!g_sched || t_self == NULL) {            /* one thread, nothing can post while the caller waits: the wait is its timeout */
        if (ticks != portMAX_DELAY) { np_sim_advance_ms((uint32_t)ticks * portTICK_PERIOD_MS); }
        const EventBits_t r = g->bits;
        if (evg_satisfied(g, bits, all != pdFALSE) && clear != pdFALSE) { g->bits &= ~bits; }
        return r;
    }
    ktask_t *self = t_self;
    self->evg = g;
    self->want = bits;
    self->clear = (clear != pdFALSE);
    self->all = (all != pdFALSE);
    block_for(ticks, W_EVENT);
    return self->timed_out ? g->bits : self->got;
}

/* ── semaphores and mutexes ───────────────────────────────────────────────────────────────────────────────────────────────────── */

QueueHandle_t xQueueGenericCreate(const UBaseType_t len, const UBaseType_t item, const uint8_t type)
{
    configASSERT(type == queueQUEUE_TYPE_BINARY_SEMAPHORE && len == 1U && item == 0U);   /* the only queue the hub makes */
    ksem_t *s = calloc(1U, sizeof *s);
    if (s != NULL) { s->magic = K_SEM_MAGIC; s->kind = 0U; }
    return (QueueHandle_t)s;
}

QueueHandle_t xQueueCreateMutex(const uint8_t type)
{
    ksem_t *s = calloc(1U, sizeof *s);
    if (s != NULL) { s->magic = K_SEM_MAGIC; s->kind = (type == queueQUEUE_TYPE_RECURSIVE_MUTEX) ? 2U : 1U; }
    return (QueueHandle_t)s;
}

static void *me(void) { return (t_self != NULL) ? (void *)t_self : HOST_OWNER; }

static bool sem_try(ksem_t *s)
{
    if (s->kind == 0U) {
        if (s->count != 0U) { s->count = 0U; return true; }
        return false;
    }
    if (s->count == 0U) { s->count = 1U; s->owner = me(); s->depth = 1U; return true; }
    if (s->kind == 2U && s->owner == me()) { s->depth++; return true; }
    return false;
}

static BaseType_t sem_take(ksem_t *s, TickType_t ticks)
{
    configASSERT(s != NULL && s->magic == K_SEM_MAGIC);
    if (sem_try(s)) { return pdTRUE; }
    if (ticks == 0U) { return pdFALSE; }
    if (!g_sched || t_self == NULL) {
        if (ticks != portMAX_DELAY) { np_sim_advance_ms((uint32_t)ticks * portTICK_PERIOD_MS); }   /* tasks run meanwhile and may give it */
        return sem_try(s) ? pdTRUE : pdFALSE;
    }
    ktask_t *self = t_self;
    self->sem = s;
    self->sem_got = false;
    block_for(ticks, W_SEM);
    return (!self->timed_out && self->sem_got) ? pdTRUE : pdFALSE;
}

BaseType_t xQueueSemaphoreTake(QueueHandle_t q, TickType_t ticks) { return sem_take((ksem_t *)q, ticks); }
BaseType_t xQueueTakeMutexRecursive(QueueHandle_t q, TickType_t ticks) { return sem_take((ksem_t *)q, ticks); }

/* The best task blocked on `s`, by priority and then by how long it has waited. */
static ktask_t *best_waiter(const ksem_t *s)
{
    ktask_t *best = NULL;
    for (unsigned i = 0U; i < g_ntasks; i++) {
        ktask_t *t = &g_tasks[i];
        if (t->st == T_BLOCKED && t->wait == W_SEM && t->sem == s) {
            if (best == NULL || t->prio > best->prio) { best = t; }
        }
    }
    return best;
}

static BaseType_t sem_give(ksem_t *s)
{
    configASSERT(s != NULL && s->magic == K_SEM_MAGIC);
    if (s->kind == 0U) {
        ktask_t *w = best_waiter(s);
        if (w != NULL) { w->sem_got = true; w->timed_out = false; make_ready(w); maybe_preempt(w); }
        else           { s->count = 1U; }
        return pdTRUE;
    }
    if (s->count == 0U || s->owner != me()) { return pdFALSE; }      /* a mutex is given by its holder */
    if (s->depth > 1U) { s->depth--; return pdTRUE; }
    ktask_t *w = best_waiter(s);
    if (w != NULL) {
        s->owner = w; s->depth = 1U;                                 /* ownership passes with the wake; the count stays locked */
        w->sem_got = true; w->timed_out = false; make_ready(w); maybe_preempt(w);
    } else {
        s->count = 0U; s->owner = NULL; s->depth = 0U;
    }
    return pdTRUE;
}

BaseType_t xQueueGenericSend(QueueHandle_t q, const void *item, TickType_t t, const BaseType_t pos)
{
    (void)item; (void)t; (void)pos;
    return sem_give((ksem_t *)q);
}
BaseType_t xQueueGiveMutexRecursive(QueueHandle_t q) { return sem_give((ksem_t *)q); }

/* ── introspection ────────────────────────────────────────────────────────────────────────────────────────────────────────────── */

unsigned    np_sim_kernel_task_count(void)        { return g_ntasks; }
const char *np_sim_kernel_task_name(unsigned i)   { return i < g_ntasks ? g_tasks[i].name : "?"; }
unsigned    np_sim_kernel_task_prio(unsigned i)   { return i < g_ntasks ? (unsigned)g_tasks[i].prio : 0U; }
uint32_t    np_sim_kernel_context_switches(void)  { return g_switches; }
const char *np_sim_kernel_task_state(unsigned i)
{
    if (i >= g_ntasks) { return "?"; }
    const ktask_t *t = &g_tasks[i];
    switch (t->st) {
    case T_READY:   return "ready";
    case T_RUNNING: return "running";
    case T_DONE:    return "done";
    default: break;
    }
    return t->wait == W_DELAY ? "delay" : t->wait == W_EVENT ? "event" : "semaphore";
}
