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
    CHECK(rc == 0, "parse succeeds");
    CHECK(len > 0 && memmem(out, len, "\"bes_tacs\"", 10) != NULL, "parse output names the modality");
    npps_free(out, len);

    const char *req =
        "{\"def\":{\"timingMode\":{\"type\":\"duration\",\"seconds\":600},\"modalities\":[{\"type\":\"bes_tacs\","
        "\"enabled\":true,\"params\":{\"frequencyHz\":10,\"intensityMilliamps\":0.8,\"waveform\":\"sinusoidal\"},"
        "\"interval\":{\"intervalOnSeconds\":0,\"intervalOffSeconds\":0}}]},\"zones\":null,\"clinicianSockets\":null,"
        "\"deviceSerialHex\":null,\"nowUnix\":1700000000,\"sessionUuidHex\":\"abababababababababababababababab\","
        "\"wavelengthRules\":null}";
    rc = npps_compile_json((const uint8_t *)req, strlen(req), &out, &len);
    CHECK(rc == 0, "compile succeeds");
    /* header 64 + one command (14 header + 7 params, slot target has no block) + signature 64 */
    CHECK(len == 64 + 14 + 7 + 64, "descriptor length");
    CHECK(out[0] == 0x50 && out[1] == 0x48 && out[2] == 0x50 && out[3] == 0x4E, "NP_HUB_PROTO_MAGIC");
    int sig_zero = 1;
    for (size_t i = len - 64; i < len; i++) if (out[i] != 0) sig_zero = 0;
    CHECK(sig_zero, "the signature slot is zeroed for the caller");
    npps_free(out, len);

    const char *bad = "protocol \"x\" {\n pbm_transcranial {\n wavelength: \"808nm\"\n intensity: 80%\n }\n}\n";
    rc = npps_parse_json((const uint8_t *)bad, strlen(bad), &out, &len);
    CHECK(rc == 1, "a percentage is refused");
    CHECK(memmem(out, len, "percentage of a baseline", 24) != NULL, "the refusal is the web parser's message");
    npps_free(out, len);

    const char *ser = "{\"items\":[{\"kind\":\"condition\",\"condition\":{\"name\":\"C\",\"link\":\"http://x\"}}]}";
    rc = npps_serialize_json((const uint8_t *)ser, strlen(ser), &out, &len);
    CHECK(rc == 0 && memmem(out, len, "condition \"C\" {", 15) != NULL, "serialize writes the block");
    npps_free(out, len);

    const char *val = "{\"entry\":{\"kind\":\"single\",\"protocol\":{\"name\":\"P\",\"timingMode\":{\"type\":\"duration\","
        "\"seconds\":1200},\"modalities\":[]}},\"limits\":{}}";
    rc = npps_validate_json((const uint8_t *)val, strlen(val), &out, &len);
    CHECK(rc == 0 && memmem(out, len, "VALIDATE_MSG_GENERAL_MODALITIES", 31) != NULL, "validate returns locale keys");
    npps_free(out, len);

    const char *res = "{\"global\":{\"tdcs\":{\"maxIntensityMilliamps\":2}},\"helmet\":null,\"individual\":{\"tdcs\":{\"maxIntensityMilliamps\":1}}}";
    rc = npps_resolve_limits_json((const uint8_t *)res, strlen(res), &out, &len);
    CHECK(rc == 0 && memmem(out, len, "\"individual\"", 12) != NULL, "resolve names the winning tier");
    npps_free(out, len);

    CHECK(npps_parse_json(NULL, 0, &out, &len) == 3, "null input is a bad argument");
    puts("ok");
    return 0;
}
