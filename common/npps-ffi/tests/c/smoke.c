/* The C ABI from C: parse a protocol, compile it, check the descriptor, and refuse a bad one.
 * Built against include/neurone_npps.h and the static library by run.sh. */
#include <stdio.h>
#include <string.h>
#include "neurone_npps.h"

#define CHECK(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); return 1; } } while (0)

static const char *SRC =
    "protocol \"C smoke\" {\n  duration: 10m\n  bes_tacs {\n    frequency: 10Hz\n    intensity: 0.8mA\n  }\n}\n";

int main(void) {
    uint8_t *out = NULL; size_t len = 0;

    int rc = npps_parse_json((const uint8_t *)SRC, strlen(SRC), &out, &len);
    CHECK(rc == NPPS_STATUS_OK, "parse succeeds");
    CHECK(len > 0 && memmem(out, len, "\"bes_tacs\"", 10) != NULL, "parse output names the modality");
    npps_free(out, len);

    const char *req =
        "{\"def\":{\"timingMode\":{\"type\":\"duration\",\"seconds\":600},\"modalities\":[{\"type\":\"bes_tacs\","
        "\"enabled\":true,\"params\":{\"frequencyHz\":10,\"intensityMilliamps\":0.8,\"waveform\":\"sinusoidal\"},"
        "\"interval\":{\"intervalOnSeconds\":0,\"intervalOffSeconds\":0}}]},\"zones\":null,\"clinicianSockets\":null,"
        "\"deviceSerialHex\":null,\"nowUnix\":1700000000,\"sessionUuidHex\":\"abababababababababababababababab\","
        "\"wavelengthRules\":null}";
    rc = npps_compile_json((const uint8_t *)req, strlen(req), &out, &len);
    CHECK(rc == NPPS_STATUS_OK, "compile succeeds");
    /* header 107 (68 + the 39-byte rules stamp) + one command (14 header + 7 params, slot target has no block) + signature 64 */
    CHECK(len == 107 + 14 + 7 + 64, "descriptor length");
    CHECK(out[0] == 0x50 && out[1] == 0x48 && out[2] == 0x50 && out[3] == 0x4E, "NP_HUB_PROTO_MAGIC");
    int sig_zero = 1;
    for (size_t i = len - 64; i < len; i++) if (out[i] != 0) sig_zero = 0;
    CHECK(sig_zero, "the signature slot is zeroed for the caller");
    npps_free(out, len);

    const char *bad = "protocol \"x\" {\n pbm_transcranial {\n wavelength: \"808nm\"\n intensity: 80%\n }\n}\n";
    rc = npps_parse_json((const uint8_t *)bad, strlen(bad), &out, &len);
    CHECK(rc == NPPS_STATUS_REFUSED, "a percentage is refused");
    CHECK(memmem(out, len, "percentage of a baseline", 24) != NULL, "the refusal is the web parser's message");
    npps_free(out, len);

    const char *ser = "{\"items\":[{\"kind\":\"condition\",\"condition\":{\"name\":\"C\",\"link\":\"http://x\"}}]}";
    rc = npps_serialize_json((const uint8_t *)ser, strlen(ser), &out, &len);
    CHECK(rc == NPPS_STATUS_OK && memmem(out, len, "condition \"C\" {", 15) != NULL, "serialize writes the block");
    npps_free(out, len);

    const char *val = "{\"entry\":{\"kind\":\"single\",\"protocol\":{\"name\":\"P\",\"timingMode\":{\"type\":\"duration\","
        "\"seconds\":1200},\"modalities\":[]}},\"limits\":{}}";
    rc = npps_validate_json((const uint8_t *)val, strlen(val), &out, &len);
    CHECK(rc == NPPS_STATUS_OK && memmem(out, len, "VALIDATE_MSG_GENERAL_MODALITIES", 31) != NULL, "validate returns locale keys");
    npps_free(out, len);

    const char *res = "{\"global\":{\"tdcs\":{\"maxIntensityMilliamps\":2}},\"helmet\":null,\"individual\":{\"tdcs\":{\"maxIntensityMilliamps\":1}}}";
    rc = npps_resolve_limits_json((const uint8_t *)res, strlen(res), &out, &len);
    CHECK(rc == NPPS_STATUS_OK && memmem(out, len, "\"individual\"", 12) != NULL, "resolve names the winning tier");
    npps_free(out, len);

    CHECK(npps_parse_json(NULL, 0, &out, &len) == NPPS_STATUS_BAD_ARGUMENT, "null input is a bad argument");
    puts("ok");
    /* A key beside the request, never in it (SWR-MG-30): one secret, framed as a 4-byte big-endian length and the bytes. */
    uint8_t secrets[4 + 32] = {0, 0, 0, 32};
    memset(secrets + 4, 0x42, 32);
    const char *seal = "{\"op\":\"sealValue\",\"key\":{\"$secret\":0},\"nonce\":\"a0a1a2a3a4a5a6a7a8a9aaab\",\"name\":\"np.n\",\"value\":\"v\"}";
    uint8_t *beside = (uint8_t *)1; size_t beside_len = 7;
    rc = npps_migration_secret_json((const uint8_t *)seal, strlen(seal), secrets, sizeof secrets, &out, &len, &beside, &beside_len);
    CHECK(rc == NPPS_STATUS_OK, "sealValue with the key beside the request succeeds");
    CHECK(memmem(out, len, "\"sealed\"", 8) != NULL, "and answers with the sealed value");
    CHECK(beside == NULL && beside_len == 0, "an answer with no secret has nothing beside it");
    npps_free(out, len);
    memset(secrets, 0, sizeof secrets); /* the caller's buffer, the caller's to clear */

    /* A derived key comes back beside the answer as bytes, and the answer only names it. */
    uint8_t seed[4 + 32] = {0, 0, 0, 32};
    memset(seed + 4, 7, 32);
    const char *derive = "{\"op\":\"deriveUhdrKey\",\"seed\":{\"$secret\":0},\"salt\":\"0909090909090909090909090909090909090909090909090909090909090909\"}";
    rc = npps_migration_secret_json((const uint8_t *)derive, strlen(derive), seed, sizeof seed, &out, &len, &beside, &beside_len);
    CHECK(rc == NPPS_STATUS_OK, "deriveUhdrKey with the seed beside the request succeeds");
    CHECK(memmem(out, len, "{\"$secret\":0}", 13) != NULL, "the answer names the key");
    CHECK(beside != NULL && beside_len == 4 + 64 && beside[3] == 64, "and the 64-byte key is beside it, framed");
    npps_free(out, len);
    npps_free(beside, beside_len);

    /* A secret the call was not given is refused, with nothing beside the refusal. */
    rc = npps_migration_secret_json((const uint8_t *)seal, strlen(seal), NULL, 0, &out, &len, &beside, &beside_len);
    CHECK(rc == NPPS_STATUS_REFUSED && beside == NULL && beside_len == 0, "a secret that was not given is refused");
    npps_free(out, len);

    return 0;
}
