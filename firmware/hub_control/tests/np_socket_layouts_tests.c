/*
 * NeurOne Hub Control — Socket Layouts by Hardware Version, host tests
 * Document: NP-FW-HUB-001 Rev 33, REQ-FWHUB-64 (part a), OI-FWHUB-27
 *
 * The table is generated from hardware/np_socket_map*.json (scripts/sync-socket-map.ts), and that script's
 * --check is what ties the rows to the JSON. These tests assert what the firmware may rely on whatever the
 * rows hold: a version is found or it is not, a valid address is one the row marks present and nothing else,
 * the count is the number of such addresses, and the real module map accepts a row's geometry as it stands.
 * None of them restates a socket count, so adding a hardware version changes no test.
 *
 * Real np_module_map.c is linked; its persistence seams are doubles. No FreeRTOS, no hardware.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

#include "../include/np_hub_types.h"
#include "../include/np_module_map.h"
#include "../include/np_socket_layouts.h"

static int g_failures = 0;

static void check(int cond, const char *name)
{
    if (cond) { printf("PASS: %s\n", name); }
    else      { printf("FAIL: %s\n", name); g_failures++; }
}

np_hub_status_t np_hexmap_nvram_write(const uint8_t *buf, size_t len)
{ (void)buf; (void)len; return NP_HUB_OK; }

np_hub_status_t np_hexmap_nvram_read(uint8_t *buf, size_t len, size_t *read_len)
{ (void)buf; (void)len; if (read_len != NULL) { *read_len = 0; } return NP_HUB_ERR_NOT_PRESENT; }

static unsigned present_entries(const np_socket_layout_t *l)
{
    unsigned n = 0;
    for (unsigned i = 0; i < l->table_len; i++) { if (l->geom[i].present_in_helmet) { n++; } }
    return n;
}

/* @verifies REQ-FWHUB-64#a */
static void test_a_version_is_found_or_it_is_not(void)
{
    check(NP_SOCKET_LAYOUT_COUNT >= 1U, "the table has at least one hardware version");
    check(np_socket_layout_find(1U) != NULL, "hardware version 1, the default, has a layout");
    unsigned found = 0;
    for (unsigned v = 0; v < 256; v++) {
        const np_socket_layout_t *l = np_socket_layout_find((uint8_t)v);
        if (l != NULL) {
            found++;
            if (l->hardware_version != (uint8_t)v) { found += 1000; }
        }
    }
    check(found == NP_SOCKET_LAYOUT_COUNT, "find() returns exactly the rows in the table, each under its own version");
    check(np_socket_layout_find(0U) == NULL, "version 0 is not a hardware version and has no layout");
    check(np_socket_layout_find(16U) == NULL, "version 16 does not fit the version nibble and has no layout");
    check(np_socket_layout_find(255U) == NULL, "version 255 has no layout");
}

/* @verifies REQ-FWHUB-64#a */
static void test_rows_are_well_formed(void)
{
    bool ordered = true, in_range = true, tight = true, fits = true;
    for (unsigned i = 0; i < NP_SOCKET_LAYOUT_COUNT; i++) {
        const np_socket_layout_t *l = &NP_SOCKET_LAYOUTS[i];
        if (i > 0 && !(NP_SOCKET_LAYOUTS[i - 1].hardware_version < l->hardware_version)) { ordered = false; }
        if (l->hardware_version < 1U || l->hardware_version > 15U) { in_range = false; }
        if (l->table_len == 0U || !l->geom[l->table_len - 1U].present_in_helmet) { tight = false; }
        if (l->table_len > NP_HEXMAP_MAX_SOCKETS) { fits = false; }
    }
    check(ordered, "versions are strictly increasing, so none appears twice");
    check(in_range, "every version is 1 to 15");
    check(tight, "table_len is the highest valid index plus one: its last entry is a socket");
    check(fits, "no table is longer than the address domain");
}

/* @verifies REQ-FWHUB-64#a */
static void test_count_is_the_number_of_valid_addresses(void)
{
    bool counts = true, valid_matches = true, beyond_invalid = true;
    for (unsigned i = 0; i < NP_SOCKET_LAYOUT_COUNT; i++) {
        const np_socket_layout_t *l = &NP_SOCKET_LAYOUTS[i];
        if (l->socket_count != present_entries(l)) { counts = false; }
        for (unsigned a = 0; a < l->table_len; a++) {
            if (np_socket_layout_valid_address(l, (uint16_t)a) != l->geom[a].present_in_helmet) { valid_matches = false; }
        }
        if (np_socket_layout_valid_address(l, l->table_len)) { beyond_invalid = false; }
        if (np_socket_layout_valid_address(l, (uint16_t)NP_HEXMAP_MAX_SOCKETS)) { beyond_invalid = false; }
        if (np_socket_layout_valid_address(l, 65535U)) { beyond_invalid = false; }
    }
    check(counts, "socket_count equals the number of present entries, for every version");
    check(valid_matches, "an address is valid exactly when its entry is present");
    check(beyond_invalid, "no address at or past table_len is valid, however large");
    check(!np_socket_layout_valid_address(NULL, 0U), "a missing layout has no valid address");
}

/* @verifies REQ-FWHUB-64#a */
static void test_the_module_map_accepts_a_row_as_it_stands(void)
{
    bool binds = true, bound_count = true, past_end_refused = true;
    for (unsigned i = 0; i < NP_SOCKET_LAYOUT_COUNT; i++) {
        const np_socket_layout_t *l = &NP_SOCKET_LAYOUTS[i];
        if (np_module_map_init(l->geom, l->table_len) != NP_HUB_OK) { binds = false; continue; }
        if (np_module_map_socket_count() != l->table_len) { bound_count = false; }
        np_module_uid_t uid = { { 1, 2, 3, 4, 5, 6, 7, 8 } };
        bool changed = false;
        if (np_module_map_apply_poll(l->table_len, &uid, 0x00, NULL, NULL, &changed) == NP_HUB_OK) { past_end_refused = false; }
    }
    check(binds, "np_module_map_init() accepts every row's geometry and length");
    check(bound_count, "the bound socket count is the row's table length");
    check(past_end_refused, "a poll of the first index past a row's table is refused");
}

int main(void)
{
    test_a_version_is_found_or_it_is_not();
    test_rows_are_well_formed();
    test_count_is_the_number_of_valid_addresses();
    test_the_module_map_accepts_a_row_as_it_stands();
    printf("%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
