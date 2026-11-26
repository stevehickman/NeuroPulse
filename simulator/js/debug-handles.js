/**
 * Debug handles on window, only when asked for (NP-SIM-FW-001 REQ-SIMFW-27 (a); OI-PRIV-45).
 *
 * window.__sim is the whole simulator, DeviceAPI.sendFirmware included, so a handle on it lets any
 * script on the page (a CDN script among them, OI-PRIV-44) read session state and send firmware
 * commands. The page therefore attaches its handles only when it was loaded with ?debug=1, which a
 * developer types and a test harness puts in the URL it opens. A script that runs inside the page
 * cannot set the flag after the fact: it is read once, from the URL the page was loaded with.
 *
 * Exact value only: ?debug, ?debug=0 and ?debug=true attach nothing, so the flag is never set by
 * accident.
 */

export const DEBUG_PARAM = 'debug';

/** True only when the query string carries debug=1. */
export function debugHandlesEnabled(search) {
  return new URLSearchParams(search ?? '').get(DEBUG_PARAM) === '1';
}

/**
 * Attach `value` to `win[name]` when `enabled`, and otherwise make sure nothing is there.
 * Returns whether it attached.
 */
export function setDebugHandle(win, enabled, name, value) {
  if (!enabled) {
    if (name in win) delete win[name];
    return false;
  }
  win[name] = value;
  return true;
}
