/*
 * NeurOne Firmware-in-the-Loop Simulator — shared scenario-test helpers (NP-SIM-FW-001 REQ-SIMFW-13)
 * HOST TEST CODE ONLY.  Included by every np_sim_*_tests.c: the event capture and the readers over it, defined once.
 */
#ifndef NP_SIM_TEST_UTIL_H
#define NP_SIM_TEST_UTIL_H

#include "np_sim_pbm_harness.h"

#include <stdio.h>
#include <string.h>

#ifndef CAP_MAX
#define CAP_MAX   512
#endif
#define CAP_LINE  512
static char g_cap[CAP_MAX][CAP_LINE];
static int  g_cap_n;

static __attribute__((unused)) void capture(const char *line)
{
    if (g_cap_n < CAP_MAX) {
        strncpy(g_cap[g_cap_n], line, CAP_LINE - 1);
        g_cap[g_cap_n][CAP_LINE - 1] = '\0';
        g_cap_n++;
    }
}

static __attribute__((unused)) int count(const char *needle)
{
    int n = 0;
    for (int i = 0; i < g_cap_n; i++) { if (strstr(g_cap[i], needle) != NULL) { n++; } }
    return n;
}

static __attribute__((unused)) int count2(const char *a, const char *b)
{
    int n = 0;
    for (int i = 0; i < g_cap_n; i++) {
        if (strstr(g_cap[i], a) != NULL && strstr(g_cap[i], b) != NULL) { n++; }
    }
    return n;
}

static __attribute__((unused)) const char *last_with(const char *needle)
{
    for (int i = g_cap_n - 1; i >= 0; i--) { if (strstr(g_cap[i], needle) != NULL) { return g_cap[i]; } }
    return "";
}

static __attribute__((unused)) const char *last_result(void) { return last_with("\"ev\":\"result\""); }

static __attribute__((unused)) bool result_code_is(int code)
{
    char want[24];
    (void)snprintf(want, sizeof want, "\"code\":%d,", code);
    return strstr(last_result(), want) != NULL;
}

/* The numeric value after `key` in the last event that contains `needle`; -1e9 if none. */
static __attribute__((unused)) double field(const char *needle, const char *key)
{
    const char *line = last_with(needle);
    const char *at = strstr(line, key);
    if (*line == '\0' || at == NULL) { return -1e9; }
    double v = 0.0;
    (void)sscanf(at + strlen(key), "%lf", &v);
    return v;
}

static __attribute__((unused)) void run(const char *text)
{
    char buf[256];
    strncpy(buf, text, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';
    np_simh_line(buf);
}

#endif /* NP_SIM_TEST_UTIL_H */
