/**
 * The server's stdout lines about clients (NP-SIM-FW-001 REQ-SIMFW-27 (b); OI-PRIV-45).
 *
 * No line names a client's network address: the count of clients is all a developer needs to see a
 * connection, and once the simulator is a test bench (OI-SIMFW-05) a log of who connected when is a
 * record nobody asked to keep. A client's version and a session's protocol ID are printed only when
 * NP_SIM_DEBUG=1, the server's counterpart of the page's ?debug=1: a protocol ID with its time says
 * which session ran when, which is the person's once a person wears the helmet.
 */

'use strict';

/** True only when NP_SIM_DEBUG is exactly '1'. */
function logDetailEnabled(env) {
  return (env ?? {}).NP_SIM_DEBUG === '1';
}

function connectedLine(total) {
  return `[NP-SIM] Client connected  (${total} total)`;
}

function helloLine(role, version, detail) {
  return detail
    ? `[NP-SIM] CLIENT_HELLO  role=${role}  version=${version}`
    : `[NP-SIM] CLIENT_HELLO  role=${role}`;
}

function sessionStartLine(protocolId, detail) {
  return detail
    ? `[NP-SIM] SESSION_START  protocol=${protocolId ?? 'raw'}`
    : '[NP-SIM] SESSION_START';
}

module.exports = { logDetailEnabled, connectedLine, helloLine, sessionStartLine };
