# Test Key Pair — NOT for production use

These Ed25519 keys are **test-only credentials** for the NeurOne Helmet Simulator.
They are committed to the repository intentionally as development infrastructure.

**DO NOT use these keys in production.** The production manufacturing root key pair
is managed separately under HSM-backed secure key management and is never committed
to version control. See NP-FW-EMMC-001 Rev 1 §8.3.

## Signing a test descriptor

Every `SESSION_START` must carry a signature: the server refuses one without, as the hub refuses an
unsigned protocol (CLAUDE.md §4.2; `NP-SIM-FW-001` `REQ-SIMFW-28`; `OI-PRIV-43`).

The signature is Ed25519 over **framed canonical bytes**, not over the JSON text: a 4-byte little-endian
length, then `NP-SIM-SESSION-START/1`, a zero byte and the descriptor without `signature` as RFC 8785
canonical JSON. Key order does not change them. Signing and verifying go through the shared NPPS core as
WebAssembly (CLAUDE.md §22), so build it first with `scripts/build-npps-wasm.sh`.

From the repository root (Node.js):

```js
const fs = require('fs');
const { signDescriptorForTest } = require('./simulator/server/session-auth');

const privateKey = fs.readFileSync('simulator/server/keys/test-private.pem', 'utf8');
const descriptor = signDescriptorForTest({ protocolId: '01-gamma-focus' }, privateKey);
// Send: { type: 'SESSION_START', descriptor }
```

A signature made this way is a **test** signature: the server's messages say the check was against this
test key, and it proves nothing about a production protocol.

## Key details

| Field | Value |
|-------|-------|
| Algorithm | Ed25519 (EdDSA) |
| Format | PKCS#8 (private) / SPKI (public) |
| Purpose | Simulator test only |
| Created | 2026-05-17 |
