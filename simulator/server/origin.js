'use strict';
/**
 * Who may reach the simulator's device API (NP-SIM-FW-001 REQ-SIMFW-27, OI-SIMFW-07).
 *
 * The server runs commands that inject faults into a process running the hub's own stimulation logic, and it
 * broadcasts every TELEMETRY and SESSION_COMPLETE to its controllers. Two doors are closed here:
 *
 *   - The network. The listener binds the IPv4 loopback address in every mode, so no other host can connect.
 *   - The developer's own browser. Browsers apply no CORS to a WebSocket: any page open in any tab can open
 *     ws://127.0.0.1:9000. A browser always sends the page's Origin with the upgrade, so the upgrade is refused
 *     unless that origin is a loopback origin, which is where the simulator page and the Compose web host are
 *     served from (simulator/HOWTO.md §2: the developer's own file server, on a port they choose, so no port
 *     is fixed here). A native client that sends no Origin (Node's `ws` sends none unless told to) is let in:
 *     it is a program the developer ran, and it can only reach the loopback listener anyway.
 *
 * `Origin: null` (a file:// page, a sandboxed frame, an opaque redirect) is refused: any page can produce it.
 */

const BIND_HOST = '127.0.0.1';

// Hostnames as `new URL(origin).hostname` spells them. A name that merely starts or ends with one of these
// (localhost.example.com, 127.0.0.1.nip.io) is a different host and is refused.
const LOOPBACK_HOSTNAMES = new Set(['localhost', '127.0.0.1', '[::1]']);

/** True when a WebSocket upgrade carrying this Origin header value (undefined: no header) may proceed. */
function isAllowedOrigin(origin) {
  if (origin === undefined) return true;              // no Origin header: a native client
  if (typeof origin !== 'string') return false;
  let url;
  try { url = new URL(origin); } catch { return false; } // 'null', '', garbage
  if (url.protocol !== 'http:' && url.protocol !== 'https:') return false;
  if (url.origin !== origin) return false;             // a browser sends the serialised origin, nothing else
  return LOOPBACK_HOSTNAMES.has(url.hostname);
}

/** `ws` verifyClient, async form, so a refusal is a 403 rather than the default 401. */
function verifyClient(info, done) {
  const origin = info.req.headers.origin;
  if (isAllowedOrigin(origin)) { done(true); return; }
  console.log(`[NP-SIM] Refused a WebSocket upgrade from origin ${JSON.stringify(origin)} (OI-SIMFW-07)`);
  done(false, 403, 'Origin not allowed');
}

module.exports = { BIND_HOST, isAllowedOrigin, verifyClient };
