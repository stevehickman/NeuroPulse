/*
 * neurone_npps.h — C ABI of the shared NPPS core (common/npps-core, OI-NPPS-CORE-01).
 *
 * Six calls, an allocator for WebAssembly hosts, and a free. Text goes in as UTF-8 bytes (no
 * terminator needed), and what comes out is a buffer this library allocated, which the caller
 * releases with npps_free().
 *
 *   npps_parse_json    NPPS source            -> JSON object: everything the file declares
 *                                                ({entries, zones, conditions, wavelengthRules, limits})
 *   npps_content_hash_json hash request (JSON) -> 64 lowercase hex digits: the protocol's identity
 *                                                (SWR-HV-01; neurone_npps_core::identity)
 *   npps_namespace_json {files:[parse results]} -> JSON: the files folded into one namespace and its
 *                                                cross-references checked
 *   npps_compile_json  compile request (JSON) -> the NP-FW-HUB-001 §4 descriptor bytes,
 *                                                signature slot zeroed for the caller to sign
 *   npps_serialize_json {items:[…]} (JSON)    -> `.npps` text: the models of parse_json written back
 *   npps_validate_json {entry,limits,allProtocols} -> JSON: the issues, as locale keys and arguments
 *   npps_resolve_limits_json {global,helmet,individual} -> JSON: the effective limits and the tier of each value
 *   npps_library_json  {op, …} (JSON)         -> JSON: the protocol library's rules (save, clone, upload gate and
 *                                                outcome, import merge, layouts; neurone_npps_core::library)
 *   npps_migration_json {op, …} (JSON)        -> JSON: the device-replacement migration and UHDR backup rules (bundle
 *                                                kinds and header, withdrawal merge, claim, clear, probe; neurone_npps_core::migration)
 *
 * The request and result shapes are documented on neurone_npps_core::api (parse_json,
 * namespace_json, compile_json, serialize_json, validate_json, resolve_limits_json). Every call returns 0 on success. On failure they return non-zero and the
 * output buffer holds the refusal as UTF-8 text (the message the web parser or compiler gives);
 * it is released with npps_free() all the same. No call retains a pointer it is given.
 *
 * Return codes are the NPPS_STATUS_* macros of neurone_npps_status.h, which is generated from
 * common/npps/constants.json (group NppsStatus) along with every other runtime's copy: success, refused
 * (message in the buffer), internal error (the core panicked; message in the buffer), bad argument
 * (null pointer or non-UTF-8 input; no buffer).
 *
 * Hand-maintained: common/npps-ffi/tests/c/run.sh compiles and runs a C program against it.
 */
#ifndef NEURONE_NPPS_H
#define NEURONE_NPPS_H

#include <stddef.h>
#include <stdint.h>

#include "neurone_npps_status.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t npps_parse_json(const uint8_t *src, size_t src_len, uint8_t **out, size_t *out_len);

int32_t npps_compile_json(const uint8_t *req, size_t req_len, uint8_t **out, size_t *out_len);

int32_t npps_content_hash_json(const uint8_t *req, size_t req_len, uint8_t **out, size_t *out_len);
int32_t npps_library_json(const uint8_t *req, size_t req_len, uint8_t **out, size_t *out_len);
int32_t npps_migration_json(const uint8_t *req, size_t req_len, uint8_t **out, size_t *out_len);

int32_t npps_namespace_json(const uint8_t *req, size_t req_len, uint8_t **out, size_t *out_len);

int32_t npps_serialize_json(const uint8_t *req, size_t req_len, uint8_t **out, size_t *out_len);

int32_t npps_validate_json(const uint8_t *req, size_t req_len, uint8_t **out, size_t *out_len);

int32_t npps_resolve_limits_json(const uint8_t *req, size_t req_len, uint8_t **out, size_t *out_len);

/* npps_migration_json for an operation that takes or returns a key (SWR-MG-30). The request's secrets travel beside
 * it in `secrets`: each is a 4-byte big-endian length and then the bytes, and the n-th is named in the JSON as
 * {"$secret": n}. The answer's secrets come back beside the answer in `secret_out`, framed the same way, and are named
 * {"$secret": n} in the answer. A secret is never text in either direction. `secrets` is the caller's buffer: overwrite
 * it when the call returns. Release `out` and `secret_out` with npps_free(), which wipes them; `secret_out` is null with
 * length 0 when the answer has no secret, and on a refusal. `secrets` may be null when `secrets_len` is 0. */
int32_t npps_migration_secret_json(const uint8_t *req, size_t req_len, const uint8_t *secrets, size_t secrets_len,
                                   uint8_t **out, size_t *out_len, uint8_t **secret_out, size_t *secret_out_len);

/* Allocate `len` zeroed bytes for a host that cannot hand this library its own memory (WebAssembly:
 * the host writes the input here, passes it to a call, then releases it with npps_free(ptr, len)). */
uint8_t *npps_alloc(size_t len);

/* Release a buffer returned through `out`/`out_len`, or allocated with npps_alloc(). A null pointer is ignored.
 * The buffer is overwritten with zeros before it is released (SWR-MG-28 (a)): an answer of npps_migration_json() can
 * hold a key, a recovery secret or plaintext UHDR items. A request buffer the caller allocated itself (Swift, C#) is the
 * caller's to overwrite. */
void npps_free(uint8_t *ptr, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* NEURONE_NPPS_H */
