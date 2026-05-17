# Test Key Pair — NOT for production use

These Ed25519 keys are **test-only credentials** for the NeuroPulse Helmet Simulator.
They are committed to the repository intentionally as development infrastructure.

**DO NOT use these keys in production.** The production manufacturing root key pair
is managed separately under HSM-backed secure key management and is never committed
to version control. See NP-FW-EMMC-001 Rev A §8.3.

## Signing a test descriptor

To sign a SESSION_START descriptor with the test private key (Node.js):

```js
const { sign } = require('crypto');
const fs = require('fs');

const privateKey = fs.readFileSync('test-private.pem', 'utf8');
const payload = { protocolId: 'gamma_40hz' };
const sig = sign(null, Buffer.from(JSON.stringify(payload)), privateKey);

const descriptor = { ...payload, signature: sig.toString('base64') };
// Send: { type: 'SESSION_START', descriptor }
```

## Key details

| Field | Value |
|-------|-------|
| Algorithm | Ed25519 (EdDSA) |
| Format | PKCS#8 (private) / SPKI (public) |
| Purpose | Simulator test only |
| Created | 2026-05-17 |
