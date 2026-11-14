/*
 * NeurOne Firmware-in-the-Loop Simulator — the hub's own tasks running a session (NP-SIM-FW-001 §31, REQ-SIMFW-23)
 *
 * HOST TEST CODE ONLY.  hub_boot runs np_hub_control_app_main() (the device's own boot ordering and its five tasks) on the twin's
 * virtual-time task layer (np_sim_kernel.c); hub_run moves virtual time.  A protocol the app signs (the TEST key, a simulator test aid,
 * REQ-SIMFW-08) is written to the hub's own transport, task_protocol_rx takes it, np_runner_load admits it, task_hub_control runs
 * np_runner_run, and the hub's own module drivers command the sim modules through the real safety MCU model (§22).  A stop, a pause
 * and a resume reach the runner through the hub's own GATT handlers, as the app's writes do.
 *
 * What the scenarios play, because the device's own code does not: the contact impedances and the modules (hardware), and the session
 * signature step (`safety_sig`: np_safety_spi_send_session_sig has no caller in hub_control/, OI-SIMFW-74).
 *
 * What these tests are, and are not.  The kernel is the twin's model of the calls the hub makes, a task is never preempted between kernel
 * calls, and time is a virtual clock (UC-124); a pass is NOT verification evidence (REQ-SIMFW-06).  Each scenario runs in a forked child
 * (the safety MCU and the hub keep file-scope state a session latches).
 */
#define NP_SIM_CAP_LONG   /* a session reports a state every 250 virtual ms and a hal event per driver call */
#include "np_sim_bus.h"
#include "np_sim_kernel.h"
#include "np_sim_pbm_harness.h"
#include "np_sim_safety.h"
#include "np_sim_test_util.h"
#include "np_sim_safety_test_consts.h"

#include "np_sim_storage.h"

#include "np_hub_types.h"
#include "np_cfg_store.h"
#include "np_lfs_instance.h"
#include "np_lfs_log_instance.h"
#include "np_hexmap_inventory.h"
#include "np_map3_record.h"
#include "np_module_map.h"
#include "np_safety_spi.h"
#include "np_session_count.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define GOLDEN_JSON_PATH NP_SIM_GOLDEN_JSON   /* common/testdata/hub-descriptor-golden.json: the app compiler's own output */

#define GATT_STOP   15
#define GATT_PAUSE  26
#define GATT_CHECK  24

static int g_failures;

static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

static void in_child(void (*scenario)(void))
{
    fflush(stdout);
    const pid_t pid = fork();
    if (pid == 0) {
        g_failures = 0;
        scenario();
        fflush(stdout);
        exit(g_failures == 0 ? 0 : 1);
    }
    int st = 0;
    (void)waitpid(pid, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) { g_failures++; printf("FAIL: scenario child did not finish cleanly\n"); }
}

/* ── the golden protocol, and the app's side of an upload ────────────────────────────────────────────────────────────────────── */

static char g_hex[4096];

/* One golden descriptor's hex, by its name in the app compiler's own JSON.  An unsigned blob (the signature slot is zero). */
static const char *golden(const char *name)
{
    static char buf[1 << 16];
    FILE *f = fopen(GOLDEN_JSON_PATH, "r");
    if (f == NULL) { return NULL; }
    const size_t n = fread(buf, 1U, sizeof buf - 1U, f);
    fclose(f);
    buf[n] = '\0';
    char key[64];
    (void)snprintf(key, sizeof key, "\"%s\": \"", name);
    const char *at = strstr(buf, key);
    if (at == NULL) { return NULL; }
    at += strlen(key);
    const char *end = strchr(at, '"');
    if (end == NULL || (size_t)(end - at) >= sizeof g_hex) { return NULL; }
    memcpy(g_hex, at, (size_t)(end - at));
    g_hex[end - at] = '\0';
    return g_hex;
}

/* The session length lives at byte 60 of the header, little-endian: the one field a scenario changes, before the signing service signs. */
static void patch_duration(char *hex, unsigned ms)
{
    char le[9];
    (void)snprintf(le, sizeof le, "%02x%02x%02x%02x", ms & 0xFFU, (ms >> 8) & 0xFFU, (ms >> 16) & 0xFFU, (ms >> 24) & 0xFFU);
    memcpy(hex + 120, le, 8U);
}

static void runl(const char *text)
{
    char *buf = strdup(text);
    np_simh_line(buf);
    free(buf);
}

/* The signing service signs the blob (a TEST key), then the app writes the framed stream to the hub's transport in BLE-sized writes. */
static void upload(const char *unsigned_hex, int flip_a_bit)
{
    char *cmd = malloc(strlen(unsigned_hex) + 16U);
    (void)snprintf(cmd, strlen(unsigned_hex) + 16U, "proto_sign %s", unsigned_hex);
    g_cap_n = 0;
    runl(cmd);
    free(cmd);
    const char *at = strstr(last_with("\"ev\":\"proto_signed\""), "\"hex\":\"");
    if (at == NULL) { printf("FAIL: the signing service gave nothing\n"); g_failures++; return; }
    at += 7;
    const char *end = strchr(at, '"');
    const size_t hexlen = (size_t)(end - at);
    char *blob = malloc(hexlen + 1U);
    memcpy(blob, at, hexlen);
    blob[hexlen] = '\0';
    if (flip_a_bit) { blob[hexlen - 4U] = (blob[hexlen - 4U] == '0') ? '1' : '0'; }   /* a byte inside the signature slot */
    const size_t nbytes = hexlen / 2U;
    char frame[16];
    (void)snprintf(frame, sizeof frame, "%02x%02x%02x%02x", (unsigned)(nbytes & 0xFFU), (unsigned)((nbytes >> 8) & 0xFFU), 0U, 0U);
    char *stream = malloc(strlen(frame) + hexlen + 1U);
    (void)snprintf(stream, strlen(frame) + hexlen + 1U, "%s%s", frame, blob);
    free(blob);
    const size_t total = strlen(stream);
    for (size_t i = 0U; i < total; i += 40U) {            /* 20 bytes a write */
        char part[64];
        const size_t take = (total - i) < 40U ? (total - i) : 40U;
        char w[96];
        memcpy(part, stream + i, take);
        part[take] = '\0';
        (void)snprintf(w, sizeof w, "proto_feed %s", part);
        runl(w);
    }
    free(stream);
}

/* The upload-check frame the hub notified last: status, then the refusal's reason (a positive np_hub_status_t code). */
static void check_frame(int *status, int *reason)
{
    *status = -1;
    *reason = -1;
    for (int i = g_cap_n - 1; i >= 0; i--) {
        if (strstr(g_cap[i], "\"ev\":\"gatt_notify\"") != NULL && strstr(g_cap[i], "\"id\":24,") != NULL) {
            const char *h = strstr(g_cap[i], "\"hex\":\"");
            if (h != NULL) { unsigned s = 0U, r = 0U; (void)sscanf(h + 7, "%2x%2x", &s, &r); *status = (int)s; *reason = (int)r; }
            return;
        }
    }
}

/* ── the hub ─────────────────────────────────────────────────────────────────────────────────────────────────────────────────── */

static void hardware(void)
{
    np_sim_set_sink(capture);
    np_sim_use_real_clock();
    np_simh_init();
    run("safety_root 7");
    run("safety_uid 9");
    run("safety_tier 1 1");
    run("safety_attach");
    for (int ch = 0; ch < 4; ch++) { char c[32]; (void)snprintf(c, sizeof c, "safety_imp %d 5000", ch); run(c); }   /* every contact in contact */
    run("stim_add 0 5");
    run("stim_add 1 5");
}

static void hub_run_ms(unsigned ms)
{
    char c[32];
    (void)snprintf(c, sizeof c, "hub_run %u", ms);
    run(c);
}

static bool runner_is(const char *name)
{
    char want[48];
    (void)snprintf(want, sizeof want, "\"runner\":\"%s\"", name);
    return strstr(last_with("\"ev\":\"hub_state\""), want) != NULL;
}

static unsigned active_mask(void) { return (unsigned)field("\"ev\":\"hub_state\"", "\"active_mask\":"); }
static double   elapsed(void)     { return field("\"ev\":\"hub_state\"", "\"elapsed_ms\":"); }

/* The visual goggles' session as the hub starts it: the module present, the protocol signed and written, the hub run for the start gate. */
static void start_visual(unsigned duration_ms)
{
    hardware();
    run("vis_add 1 1 1");
    run("hub_boot");
    char *hex = strdup(golden("visual"));
    if (duration_ms != 0U) { patch_duration(hex, duration_ms); }
    upload(hex, 0);
    free(hex);
    hub_run_ms(1500);
}

/* @verifies REQ-SIMFW-23#a */
static void test_the_boot_path_brings_up_the_five_tasks_and_the_heartbeat_reaches_the_safety_mcu(void)
{
    hardware();
    run("hub_boot");
    check(count2("\"cmd\":\"hub_boot\"", "\"booted\":true") == 1 && count("\"tasks\":5") >= 1, "the hub's own boot ran and started its scheduler with five tasks");
    check(np_sim_kernel_task_count() == 5U, "np_hub_control_app_main created five tasks");
    check(strcmp(np_sim_kernel_task_name(0), "SafetyBeat") == 0 && np_sim_kernel_task_prio(0) == 4U &&
          strcmp(np_sim_kernel_task_name(1), "HubCtrl") == 0 && np_sim_kernel_task_prio(1) == 3U &&
          strcmp(np_sim_kernel_task_name(2), "ProtoRx") == 0 && np_sim_kernel_task_prio(2) == 2U &&
          strcmp(np_sim_kernel_task_name(4), "ModDetect") == 0 && np_sim_kernel_task_prio(4) == 1U,
          "they are the device's, at the device's priorities");
    check(count2("\"ev\":\"result\"", "\"admits\":true") >= 1, "the boot check let sessions in (the factory-reset check, the log backend and the registry scan ran in the device's order)");
    check(count("\"ev\":\"hub_led\"") >= 1 && strstr(last_with("\"ev\":\"hub_led\""), "\"led\":\"idle\"") != NULL, "the status LED was set idle at the end of the boot path");
    hub_run_ms(3000);
    check(np_safety_spi_get_tier() == NP_TIER_T1, "the hub's own heartbeat task exchanged frames with the safety MCU: the tier it reports is T1");
    check(runner_is("idle"), "and with nothing loaded the runner is idle");
    run("hub_status");
    check(strstr(last_with("\"name\":\"ProtoRx\""), "\"state\":\"semaphore\"") != NULL, "task_protocol_rx is blocked on the transport, as on the device");
    run("safety_run 200 0");
    check(strstr(last_with("\"ev\":\"error\""), "heartbeat task is running") != NULL, "a second heartbeat source (safety_run) is refused while the hub's task beats");
}

/* @verifies REQ-SIMFW-23#b */
static void test_a_loaded_protocol_starts_a_session_that_the_runner_steps(void)
{
    start_visual(0U);
    int st, why;
    check_frame(&st, &why);
    check(st == 1, "task_protocol_rx accepted the TEST-key-signed golden protocol (the upload check frame says ACCEPTED)");
    check(runner_is("running"), "np_runner_run started: the runner is running");
    check(strstr(last_with("\"ev\":\"hub_led\""), "\"led\":\"session\"") != NULL, "and the status LED says session");
    check((active_mask() & (1U << NP_MOD_VISUAL)) != 0U, "the runner's own progress frame names the visual command active");
    check(elapsed() > 1000.0 && elapsed() <= 1500.0, "elapsed session time is the runner's own, on the virtual clock");
    printf("  (elapsed %.0f ms after 1500 virtual ms)\n", elapsed());
    check(count2("\"id\":\"visual:0\"", "\"op\":\"vis_led_set\"") >= 1, "the hub's visual driver commanded the goggles through the seam");
    const double before = elapsed();
    hub_run_ms(1000);
    check(elapsed() >= before + 990.0 && elapsed() <= before + 1010.0, "a second of virtual time is a second of session time");
}

/* @verifies REQ-SIMFW-23#d */
static void test_a_pause_a_resume_and_a_stop_reach_the_runner_and_the_lease_is_released(void)
{
    start_visual(0U);
    run("gatt_write 26 01");
    check(strstr(last_with("\"ev\":\"gatt_write\""), "\"att\":0") != NULL, "a pause written to the hub's own characteristic was accepted");
    hub_run_ms(1000);
    check(runner_is("paused"), "the runner paused");
    const double at_pause = elapsed();
    hub_run_ms(2000);
    check(elapsed() == at_pause, "session time stood still while paused");
    run("gatt_write 26 02");
    hub_run_ms(1500);
    check(runner_is("running") && elapsed() > at_pause, "a resume was accepted and the session ran on");
    run("gatt_write 15 01");
    hub_run_ms(500);
    check(runner_is("stopping"), "a stop moved the runner to stopping (modules ramp down for up to 5 s)");
    hub_run_ms(5500);
    check(runner_is("fault") && strstr(last_with("\"ev\":\"hub_led\""), "\"led\":\"fault\"") != NULL,
          "then the runner ended in the state the device's runner gives any abort, a stop included (np_runner_run_tests.c pins it), and the LED followed");
    check(active_mask() == 0U, "nothing is active");
    /* The lease: a second protocol is taken in only because np_runner_run released it. */
    upload(golden("visual"), 0);
    hub_run_ms(1500);
    int st, why;
    check_frame(&st, &why);
    check(st == 1 && runner_is("running"), "the lease was released at the session's end: a second protocol loaded and started");
}

/* @verifies REQ-SIMFW-23#d */
static void test_a_session_that_reaches_its_length_completes_and_releases_the_lease(void)
{
    start_visual(4000U);
    check(runner_is("running"), "a 4 s protocol is running at 1.5 s");
    hub_run_ms(8000);   /* 2.5 s more to its length, then the runner's own 5 s shutdown wait */
    check(runner_is("complete"), "it ran to its length and the runner reports complete");
    check(strstr(last_with("\"ev\":\"hub_led\""), "\"led\":\"idle\"") != NULL, "and the LED went back to idle");
    check(elapsed() >= 3990.0 && elapsed() <= 4010.0, "the length it reports is the length it ran, on the virtual clock");
    upload(golden("visual"), 0);
    hub_run_ms(1500);
    check(runner_is("running"), "the lease was released: the next protocol started");
}

/* @verifies REQ-SIMFW-23#d */
static void test_a_second_protocol_is_refused_while_a_session_holds_the_lease(void)
{
    start_visual(0U);
    upload(golden("visual"), 0);
    hub_run_ms(500);
    int st, why;
    check_frame(&st, &why);
    check(st == 2 && why == -(int)NP_HUB_ERR_SESSION_ACTIVE, "the second protocol was refused by the hub: the lease is held (SESSION_ACTIVE)");
    check(runner_is("running"), "and the first session was not disturbed");
}

/* @verifies REQ-SIMFW-23#e */
static void test_a_protocol_with_a_bad_signature_starts_nothing(void)
{
    hardware();
    run("vis_add 1 1 1");
    run("hub_boot");
    upload(golden("visual"), 1);
    hub_run_ms(1500);
    int st, why;
    check_frame(&st, &why);
    check(st == 2 && why == -(int)NP_HUB_ERR_BAD_SIGNATURE, "the hub refused a flipped signature bit as BAD_SIGNATURE");
    check(runner_is("idle") && count2("\"ev\":\"hub_led\"", "\"led\":\"session\"") == 0, "no session started and the LED never said session");
    upload(golden("visual"), 0);
    hub_run_ms(1500);
    check_frame(&st, &why);
    check(st == 1 && runner_is("running"), "the refusal left the lease free: a good protocol then started");
}

/* @verifies REQ-SIMFW-23#e */
static void test_a_boot_that_refused_data_refuses_every_protocol(void)
{
    hardware();
    run("vis_add 1 1 1");
    run("hub_boot refuse");
    check(count2("\"cmd\":\"hub_boot\"", "\"admits\":false") == 1, "the device's own factory-reset boot check could not read Config, so the log backend refused sessions");
    upload(golden("visual"), 0);
    hub_run_ms(1500);
    int st, why;
    check_frame(&st, &why);
    check(st == 2 && why == -(int)NP_HUB_ERR_BOOT_REFUSED, "a correctly signed protocol is refused as BOOT_REFUSED");
    check(runner_is("idle"), "and no session started");
}

/* @verifies REQ-SIMFW-23#f */
static void test_the_safety_mcu_gates_the_module_the_hub_commands(void)
{
    start_visual(0U);
    check(count2("\"id\":\"visual:0\"", "\"op\":\"vis_led_set\"") >= 1, "the hub commanded the goggles");
    check((np_safety_spi_get_requested_mask() & 0x100U) != 0U, "and its driver requested the visual enable");
    check(!np_sim_safety_line_enabled(LINE_VIS) && (np_safety_spi_get_granted_mask() & 0x100U) == 0U,
          "the safety MCU grants nothing before the session signature: the line is not asserted while the hub commands");
    run("safety_sig good");
    hub_run_ms(2000);
    check(np_sim_safety_line_enabled(LINE_VIS) && (np_safety_spi_get_granted_mask() & 0x100U) != 0U, "a verified signature: the MCU grants the line and asserts it");
    run("safety_sig bad");
    hub_run_ms(1000);
    check(!np_sim_safety_line_enabled(LINE_VIS) && (np_safety_spi_get_status() & 0x01U) != 0U,
          "a bad signature: the MCU faults and cuts the line");
    check(runner_is("stopping") || runner_is("fault"), "and the hub's own heartbeat task posted the fault, so the runner aborted the session");
}

/* @verifies REQ-SIMFW-23#f */
static void test_an_electrical_line_without_declarations_is_withheld_while_the_hub_commands_it(void)
{
    hardware();
    run("hub_boot");
    upload(golden("tdcs"), 0);
    hub_run_ms(1500);
    run("safety_sig good");
    hub_run_ms(2500);
    run("safety_status");
    check(runner_is("running") && count2("\"id\":\"stim_pair:1\"", "\"op\":\"stim_dc_ramp\"") >= 1, "the hub's tDCS driver commanded the pair (a ramp)");
    check((np_safety_spi_get_requested_mask() & 0x40U) != 0U && (np_safety_spi_get_granted_mask() & 0x40U) == 0U && !np_sim_safety_line_enabled(2),
          "the hub requested the tDCS line and the MCU withheld it: no line, a status of OK, no fault");
    check(field("\"ev\":\"safety_state\"", "\"withhold_decl\":") == 64.0,
          "the withhold report names the declaration gate (why the declarations did not survive to a grant was not traced, OI-SIMFW-74)");
}

/* @verifies REQ-SIMFW-23#c */
static void test_a_run_repeats_exactly(void)
{
    /* Two children run the same session; each reports its (time relative to boot, runner, elapsed) rows through a pipe; the parent compares. */
    int fd[2][2];
    char out[2][8192];
    for (int k = 0; k < 2; k++) {
        if (pipe(fd[k]) != 0) { check(0, "a pipe"); return; }
        fflush(stdout);
        const pid_t pid = fork();
        if (pid == 0) {
            close(fd[k][0]);
            start_visual(3000U);
            hub_run_ms(2500);
            run("gatt_write 26 01");
            hub_run_ms(1000);
            run("gatt_write 26 02");
            hub_run_ms(5000);
            char digest[8192];
            size_t n = 0U;
            double t0 = -1.0;
            for (int i = 0; i < g_cap_n && n + 160U < sizeof digest; i++) {
                if (strstr(g_cap[i], "\"ev\":\"hub_state\"") == NULL) { continue; }
                const char *tp = strstr(g_cap[i], "\"t\":");
                const char *rp = strstr(g_cap[i], "\"runner\":\"");
                const char *ep = strstr(g_cap[i], "\"elapsed_ms\":");
                if (tp == NULL || rp == NULL || ep == NULL) { continue; }
                double t = atof(tp + 4);
                if (t0 < 0.0) { t0 = t; }
                char runner[16];
                (void)sscanf(rp + 10, "%15[^\"]", runner);
                n += (size_t)snprintf(digest + n, sizeof digest - n, "%.0f %s %.0f;", t - t0, runner, atof(ep + 13));
            }
            (void)write(fd[k][1], digest, n);
            _exit(0);
        }
        close(fd[k][1]);
        ssize_t got = read(fd[k][0], out[k], sizeof out[k] - 1U);
        out[k][got > 0 ? got : 0] = '\0';
        int st = 0;
        (void)waitpid(pid, &st, 0);
    }
    check(strlen(out[0]) > 200U && strcmp(out[0], out[1]) == 0, "two runs of one session (start, pause, resume, completion) report the same states at the same virtual times");
}


/* A completed 3 s visual session, so the partitions hold what the hub's own logger wrote. */
static void run_a_short_session(void)
{
    start_visual(3000U);
    hub_run_ms(9000);   /* 1.5 s already run, then the length and the runner's own 5 s shutdown wait */
}

/* @verifies REQ-SIMFW-25#a */
static void test_each_partition_is_a_sparse_file_nobody_can_name_in_the_devices_geometry(void)
{
    run_a_short_session();
    check(runner_is("complete"), "a session ran to its length");
    const struct { np_sim_store_t which; const char *name; uint64_t bytes; } want[] = {
        { NP_SIM_STORE_CONFIG, "Config", (uint64_t)NP_LFS_CFG_BLOCK_SIZE * NP_LFS_CFG_BLOCK_COUNT },
        { NP_SIM_STORE_UHDR,   "UHDR",   (uint64_t)NP_LFS_LOG_UHDR_BLOCK_COUNT * NP_LFS_LOG_BLOCK_SIZE },
        { NP_SIM_STORE_SHDR,   "SHDR",   (uint64_t)NP_LFS_LOG_SHDR_BLOCK_COUNT * NP_LFS_LOG_BLOCK_SIZE },
    };
    for (unsigned i = 0U; i < 3U; i++) {
        np_sim_store_info_t in;
        char name[128];
        (void)snprintf(name, sizeof name, "%s: a mounted partition backed by a file of exactly the instance's size", want[i].name);
        check(np_sim_storage_info(want[i].which, &in) && in.up && in.apparent_bytes == want[i].bytes, name);
        (void)snprintf(name, sizeof name, "%s: the file has no name (unlinked) and only its owner can read or write it (0600)", want[i].name);
        check(in.nlink == 0U && in.mode == 0600U, name);
        (void)snprintf(name, sizeof name, "%s: it is sparse: under 64 MiB is allocated for a partition of up to 6.9 GiB", want[i].name);
        check(in.allocated_bytes < 64ULL * 1024ULL * 1024ULL, name);
    }
}

/* @verifies REQ-SIMFW-25#b */
static void test_a_sessions_records_are_in_the_partition_files_and_come_back_from_them(void)
{
    run_a_short_session();
    uint64_t counters[4];
    uint32_t sizes[4];
    check(np_sim_storage_uhdr_sessions(counters, sizes, 4U) == 1U && sizes[0] > 0U, "the hub's logger left one UHDR session file with bytes in it");
    uint32_t shdr = 0U;
    check(np_sim_storage_shdr_size(&shdr) && shdr > 0U, "and bytes in the one SHDR log file");
    uint8_t head[16];
    uint32_t got = 0U;
    check(np_sim_storage_uhdr_read(counters[0], 0U, head, sizeof head, &got) && got == sizeof head, "the UHDR file's first bytes read back");
    const uint32_t uhdr_before = sizes[0], shdr_before = shdr;
    (void)np_log_hal_part_close(NP_LOG_PART_SHDR);
    check(np_sim_storage_remount_log(NP_SIM_STORE_UHDR) && np_sim_storage_remount_log(NP_SIM_STORE_SHDR),
          "both log instances unmount and mount again: littlefs's RAM state is gone");
    uint32_t after_u[4], after_s = 0U;
    uint64_t after_c[4];
    check(np_sim_storage_uhdr_sessions(after_c, after_u, 4U) == 1U && after_u[0] == uhdr_before && after_c[0] == counters[0] &&
          np_sim_storage_shdr_size(&after_s) && after_s == shdr_before, "and the same files with the same sizes come back: what the hub logged is in the file");
}

/* @verifies REQ-SIMFW-25#b */
static void test_a_second_session_gets_its_own_uhdr_file_and_the_shdr_log_grows(void)
{
    run_a_short_session();
    uint64_t c1[4];
    uint32_t s1[4], shdr1 = 0U;
    check(np_sim_storage_uhdr_sessions(c1, s1, 4U) == 1U && np_sim_storage_shdr_size(&shdr1), "one session so far");
    upload(golden("visual"), 0);
    hub_run_ms(1500);
    check(runner_is("running"), "a second protocol was taken in after the first completed");
    run("gatt_write 15 01");
    hub_run_ms(7000);
    uint64_t c2[4];
    uint32_t s2[4], shdr2 = 0U;
    check(np_sim_storage_uhdr_sessions(c2, s2, 4U) == 2U, "the second session is a second UHDR file");
    check(c2[0] == c1[0] && s2[0] == s1[0] && c2[1] != c2[0], "the first file is as it was: a closed session file is never written again, and the counters differ");
    check(np_sim_storage_shdr_size(&shdr2) && shdr2 > shdr1, "the SHDR log, one file for the device, grew");
}

/* @verifies REQ-SIMFW-25#c */
static void test_the_session_count_and_the_module_map_are_kept_in_the_config_file(void)
{
    run_a_short_session();
    uint32_t n = 99U;
    check(np_session_count_load(&n) == NP_HUB_OK && n == 1U, "the device's session count in the Config partition is 1 after one session");
    np_cfg_store_unmount();
    check(np_cfg_store_mount() == NP_HUB_OK, "the Config instance unmounts and mounts again");
    n = 99U;
    check(np_session_count_load(&n) == NP_HUB_OK && n == 1U, "and the count comes back from the file");
    check(np_module_map_persist() == NP_HUB_OK, "the module map persists through the device's own np_hexmap_nvram.c");
    static uint8_t blob[16384];
    size_t len = 0U;
    const np_hub_status_t rr = np_hexmap_nvram_read(blob, sizeof blob, &len);
    printf("  (module map blob read: code %d, %zu bytes)\n", (int)rr, len);
    check(rr == NP_HUB_OK && len > 0U && np_module_map_blob_verify(blob, len),
          "and reads back as a blob the module map's own check accepts");
}

/* @verifies REQ-SIMFW-25#d */
static void test_a_medium_that_cannot_be_made_refuses_the_boot(void)
{
    hardware();
    check(setenv("TMPDIR", "/nonexistent-np-sim-dir", 1) == 0, "the system temporary directory is one that does not exist");
    run("hub_boot");
    check(count2("\"cmd\":\"hub_boot\"", "\"booted\":true") == 0 && !result_code_is(0), "the hub is not booted: the twin does not fall back to a medium held only in memory");
    run("hub_run 100");
    check(strstr(last_with("\"ev\":\"error\""), "not running") != NULL, "and no hub runs");
}

static bool count_row(const np_map3_row_t *row, void *ctx) { (void)row; (*(unsigned *)ctx)++; return true; }

/* @verifies REQ-SIMFW-25#e */
static void test_map3_rows_round_trip_and_a_damaged_row_ends_the_valid_prefix(void)
{
    uint8_t journal[3U * NP_MAP3_V1_BYTES];
    for (unsigned i = 0U; i < 3U; i++) {
        np_map3_rec_t r;
        memset(&r, 0, sizeof r);
        r.uid[0] = 0xA5U;
        r.seq = i + 1U;
        r.ordinal = i;
        r.session_count = 1U;
        r.dose_accum = 1000U * (i + 1U);
        check(np_map3_encode(&r, &journal[i * NP_MAP3_V1_BYTES], NP_MAP3_V1_BYTES) == NP_MAP3_V1_BYTES, "a row encodes to the version 1 length");
    }
    np_map3_scan_t sc;
    unsigned seen = 0U;
    np_map3_scan(journal, sizeof journal, count_row, &seen, &sc);
    check(seen == 3U && sc.rows == 3U && sc.end == NP_MAP3_END_CLEAN, "three rows scan back, and the scan ends clean");
    journal[NP_MAP3_V1_BYTES + 5U] ^= 0x01U;           /* a bit of the second row */
    seen = 0U;
    np_map3_scan(journal, sizeof journal, count_row, &seen, &sc);
    check(seen == 1U && sc.end == NP_MAP3_END_BAD_CRC, "a damaged second row ends the valid prefix at the first: nothing after it is trusted");
    seen = 0U;
    np_map3_scan(journal, NP_MAP3_V1_BYTES - 4U, count_row, &seen, &sc);
    check(seen == 0U && sc.end == NP_MAP3_END_TORN, "a journal cut short in its first row is a torn tail");
}

/* @verifies REQ-SIMFW-25#e */
static void test_a_socket_poll_stops_at_the_unrouted_register_tunnel_and_fails_closed(void)
{
    hardware();
    run("add 0 34.0");
    g_cap_n = 0;
    bool changed = false;
    const np_hub_status_t rc = np_hexmap_poll_socket(0U, &changed);
    check(rc != NP_HUB_OK, "the device's own poll of a socket returns an error: the smart-module register tunnel has no route in the twin");
    check(count2("\"ev\":\"unrouted\"", "np_pbm_hal_i2c_read") >= 1, "and the twin reports the unrouted read, as it does every seam with no route");
}

int main(void)
{
    if (golden("visual") == NULL) { printf("FAIL: the golden descriptors were not found\n"); return 1; }
    in_child(test_the_boot_path_brings_up_the_five_tasks_and_the_heartbeat_reaches_the_safety_mcu);
    in_child(test_a_loaded_protocol_starts_a_session_that_the_runner_steps);
    in_child(test_a_pause_a_resume_and_a_stop_reach_the_runner_and_the_lease_is_released);
    in_child(test_a_session_that_reaches_its_length_completes_and_releases_the_lease);
    in_child(test_a_second_protocol_is_refused_while_a_session_holds_the_lease);
    in_child(test_a_protocol_with_a_bad_signature_starts_nothing);
    in_child(test_a_boot_that_refused_data_refuses_every_protocol);
    in_child(test_the_safety_mcu_gates_the_module_the_hub_commands);
    in_child(test_an_electrical_line_without_declarations_is_withheld_while_the_hub_commands_it);
    in_child(test_a_run_repeats_exactly);
    in_child(test_each_partition_is_a_sparse_file_nobody_can_name_in_the_devices_geometry);
    in_child(test_a_sessions_records_are_in_the_partition_files_and_come_back_from_them);
    in_child(test_a_second_session_gets_its_own_uhdr_file_and_the_shdr_log_grows);
    in_child(test_the_session_count_and_the_module_map_are_kept_in_the_config_file);
    in_child(test_a_medium_that_cannot_be_made_refuses_the_boot);
    in_child(test_map3_rows_round_trip_and_a_damaged_row_ends_the_valid_prefix);
    in_child(test_a_socket_poll_stops_at_the_unrouted_register_tunnel_and_fails_closed);
    if (g_failures == 0) { printf("ALL PASS (0 failures)\n"); return 0; }
    printf("%d FAILURES\n", g_failures);
    return 1;
}
