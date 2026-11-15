/*
 * The UHDR key scheme, run in the simulator (NP-SIM-FW-001 §34, OI-SIMFW-52, REQ-SIMFW-26).
 *
 * np_uhdr_key.c is the device's own two-layer key lifecycle (NP-FW-EMMC-002 §C): provision, unlock, background, change credential.  It runs here
 * unchanged, built with -DNPTEST_HOST, which is the only way the device's own source offers its platform seams (TRNG, Argon2id, the hardware bind,
 * AES-GCM, the Config record, the UHDR mount).  Those seams are therefore the device's own HOST MODEL of them, not the twin's and not the device's
 * silicon: the KDF and the AEAD are SHA-256 constructions its comment says are "NOT cryptography", the TRNG is a counter, the device key is a
 * constant in RAM, the Config record is a RAM variable, and the mount only records that it was called.  What runs is the lifecycle's LOGIC and its
 * refusals (a wrong credential fails the tag, a changed credential re-wraps the same key, background zeroes the key); nothing here is an encryption
 * of anything, and UHDR in the files of np_sim_storage.c stays plaintext (OI-SIMFW-52).
 *
 * Synthetic only (CLAUDE.md §5.1, REQ-SIMFW-08): the twin holds no person's credential and no real key.  A command names a credential by a small
 * number, and the bytes handed to the module are "np-sim-synthetic-credential-NN", fixed text.  No key byte, wrapped or plain, is ever reported:
 * an event carries the status, the lock state and counts only.
 *
 * Commands (stdin, one line each; the bridge validates the arguments, REQ-SIMFW-12):
 *   uhdr_key provision <n>        np_uhdr_key_provision under synthetic credential n (0..15)
 *   uhdr_key unlock <n>           np_uhdr_key_unlock
 *   uhdr_key background           np_uhdr_key_background
 *   uhdr_key change <n>           np_uhdr_key_change_credential to synthetic credential n
 *   uhdr_key status               the lock state and counts
 */
#include "np_sim_bus.h"
#include "np_sim_pbm_harness.h"

#include "np_uhdr_key.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The device's own host-build accessors (np_uhdr_key.c, under NPTEST_HOST); the module declares them in no header. */
extern uint32_t              np_uhdr_host_mount_count(void);
extern const uint8_t        *np_uhdr_host_last_mounted_key(void);
extern const np_ukmd_record_t *np_uhdr_host_config(void);

#define UHDR_SYNTH_CREDENTIALS 16
static np_uhdr_key_ctx_t g_ctx;
static bool              g_provisioned;

static const char *status_name(np_uhdr_status_t s)
{
    switch (s) {
    case NP_UHDR_OK:          return "ok";
    case NP_UHDR_ERR_TRNG:    return "trng";
    case NP_UHDR_ERR_KDF:     return "kdf";
    case NP_UHDR_ERR_CRYPTO:  return "crypto";
    case NP_UHDR_ERR_AUTH:    return "auth";
    case NP_UHDR_ERR_CONFIG:  return "config";
    case NP_UHDR_ERR_MOUNT:   return "mount";
    case NP_UHDR_ERR_STATE:   return "state";
    case NP_UHDR_ERR_INVALID: return "invalid";
    case NP_UHDR_ERR_PARAM:   return "param";
    default:                  return "?";
    }
}

/* Never a key byte: the status, the lock state, how many times the partition was mounted, and whether the stored record's wrapped field differs from
 * the key that was mounted (the plaintext key is never on the Config record). */
static void emit(const char *cmd, np_uhdr_status_t rc)
{
    const bool wrapped = g_provisioned && memcmp(np_uhdr_host_config()->ukmd_ciphertext, np_uhdr_host_last_mounted_key(), NP_UHDR_UKMD_LEN) != 0;
    np_sim_emit("{\"ev\":\"uhdr_key\",\"cmd\":\"%s\",\"code\":%d,\"status\":\"%s\",\"unlocked\":%s,\"provisioned\":%s,\"mounts\":%u,\"record_is_wrapped\":%s,"
                "\"model\":\"host-model-not-cryptography\"}",
                cmd, (int)rc, status_name(rc), g_ctx.unlocked ? "true" : "false", g_provisioned ? "true" : "false", (unsigned)np_uhdr_host_mount_count(),
                wrapped ? "true" : "false");
}

static void bad(const char *cmd, const char *why)
{
    np_sim_emit("{\"ev\":\"error\",\"what\":\"%s: %s\"}", cmd, why);
}

/* The fixed synthetic credential n, as bytes: text the harness owns, never a person's. */
static size_t synth(long n, uint8_t out[40])
{
    return (size_t)snprintf((char *)out, 40U, "np-sim-synthetic-credential-%02ld", n);
}

static bool parse_n(const char *cmd, const char *a, long *n)
{
    char *end = NULL;
    if (a == NULL || *a == '\0') { bad(cmd, "needs a synthetic credential number, 0 to 15"); return false; }
    const long v = strtol(a, &end, 10);
    if (*end != '\0' || v < 0L || v >= (long)UHDR_SYNTH_CREDENTIALS) { bad(cmd, "needs a synthetic credential number, 0 to 15"); return false; }
    *n = v;
    return true;
}

bool np_simh_uhdr_line(const char *cmd, char *a, char *b)
{
    if (strcmp(cmd, "uhdr_key") != 0) { return false; }
    uint8_t cred[40];
    long n = 0L;
    if (a == NULL) { bad(cmd, "needs provision, unlock, background, change or status"); return true; }
    if (strcmp(a, "provision") == 0) {
        if (!parse_n("uhdr_key provision", b, &n)) { return true; }
        const np_uhdr_status_t rc = np_uhdr_key_provision(&g_ctx, cred, synth(n, cred));
        if (rc == NP_UHDR_OK) { g_provisioned = true; }
        emit("provision", rc);
    } else if (strcmp(a, "unlock") == 0) {
        if (!parse_n("uhdr_key unlock", b, &n)) { return true; }
        emit("unlock", np_uhdr_key_unlock(&g_ctx, cred, synth(n, cred)));
    } else if (strcmp(a, "background") == 0) {
        np_uhdr_key_background(&g_ctx);
        emit("background", NP_UHDR_OK);
    } else if (strcmp(a, "change") == 0) {
        if (!parse_n("uhdr_key change", b, &n)) { return true; }
        emit("change", np_uhdr_key_change_credential(&g_ctx, cred, synth(n, cred)));
    } else if (strcmp(a, "status") == 0) {
        emit("status", NP_UHDR_OK);
    } else {
        bad(cmd, "needs provision, unlock, background, change or status");
    }
    return true;
}
