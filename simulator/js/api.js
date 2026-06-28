/**
 * DeviceAPI — browser-side WebSocket client for the NeuroPulse virtual device API.
 *
 * Connects to ws://localhost:9000 (simulator/server/index.js) on load.
 * Falls back silently to standalone mode when the server is not running —
 * all existing UI controls continue to work unchanged.
 *
 * Role: 'display' — receives SESSION_START / SESSION_PAUSE / SESSION_STOP /
 * ZONE_CONFIG / ACCESSORY_CONFIG from the server, runs the session engine, and
 * sends TELEMETRY, FAULT, and SESSION_COMPLETE back to connected controller
 * clients (apps).
 */

import { SIM_VERSION } from './version.js';

const WS_URL              = 'ws://localhost:9000';
const RECONNECT_DELAY_MS  = 5_000;
const TELEMETRY_PERIOD_MS = 100;   // 10 Hz minimum per spec

export class DeviceAPI {
  /**
   * @param {{
   *   onSessionStart:    (descriptor: object) => void,
   *   onSessionPause:    () => void,
   *   onSessionStop:     () => void,
   *   onZoneConfig:      (zoneId: string, config: object) => void,
   *   onAccessoryChange: (name: string, visible: boolean) => void,
   * }} callbacks
   */
  constructor(callbacks) {
    this._cb           = callbacks;
    this._ws           = null;
    this._badge        = document.getElementById('api-badge');
    this._lastTelemetry = 0;   // ms timestamp of last TELEMETRY send
    this._reconnectTimeout = null;

    this._connect();
  }

  // ─── public ───────────────────────────────────────────────────────────────

  /**
   * Called from the app's RAF loop. Sends a TELEMETRY snapshot if the server
   * is connected and at least TELEMETRY_PERIOD_MS has elapsed since the last send.
   *
   * @param {object} snap  - SessionEngine.snapshot() result
   * @param {number} wallTime  - seconds since page load (from RAF)
   */
  maybeSendTelemetry(snap, wallTime) {
    if (!this._open()) return;
    const nowMs = wallTime * 1000;
    if (nowMs - this._lastTelemetry < TELEMETRY_PERIOD_MS) return;
    this._lastTelemetry = nowMs;
    this._send({ type: 'TELEMETRY', timestamp: Date.now(), snap });
  }

  /**
   * Send a FAULT event to connected controller clients.
   *
   * @param {string} code    - Fault code, e.g. 'IMPEDANCE_OUT_OF_RANGE'
   * @param {object} details - Additional fields merged into the message
   */
  sendFault(code, details = {}) {
    if (!this._open()) return;
    this._send({ type: 'FAULT', code, ...details });
  }

  /**
   * Send SESSION_COMPLETE with UHDR and SHDR summary objects.
   */
  sendSessionComplete(uhdr_summary, shdr_summary) {
    if (!this._open()) return;
    this._send({ type: 'SESSION_COMPLETE', uhdr_summary, shdr_summary });
  }

  get connected() { return this._open(); }

  // ─── private ──────────────────────────────────────────────────────────────

  _connect() {
    // WebSocket constructor throws synchronously for invalid URL but not for
    // connection refused — that arrives as an error event.
    try {
      this._ws = new WebSocket(WS_URL);
    } catch {
      this._setBadge(false);
      return;
    }

    this._ws.addEventListener('open', () => {
      this._setBadge(true);
      this._send({ type: 'CLIENT_HELLO', role: 'display', version: SIM_VERSION });
    });

    this._ws.addEventListener('message', (e) => {
      try { this._dispatch(JSON.parse(e.data)); } catch { /* ignore malformed */ }
    });

    this._ws.addEventListener('close', () => {
      this._setBadge(false);
      this._ws = null;
      // Reconnect after a delay — server may restart
      clearTimeout(this._reconnectTimeout);
      this._reconnectTimeout = setTimeout(() => this._connect(), RECONNECT_DELAY_MS);
    });

    this._ws.addEventListener('error', () => {
      // 'close' fires immediately after 'error', so reconnect is handled there
      this._setBadge(false);
    });
  }

  _dispatch(msg) {
    switch (msg.type) {
      case 'CONNECTED':
        // Server handshake acknowledgement — nothing extra needed
        break;

      case 'SESSION_START':
        this._cb.onSessionStart?.(msg.descriptor ?? {});
        break;

      case 'SESSION_PAUSE':
        this._cb.onSessionPause?.();
        break;

      case 'SESSION_STOP':
        this._cb.onSessionStop?.();
        break;

      case 'ZONE_CONFIG':
        if (msg.zoneId && msg.config) {
          this._cb.onZoneConfig?.(msg.zoneId, msg.config);
        }
        break;

      case 'ACCESSORY_CONFIG':
        if (typeof msg.name === 'string' && typeof msg.visible === 'boolean') {
          this._cb.onAccessoryChange?.(msg.name, msg.visible);
        }
        break;

      case 'ERROR':
        console.warn(`[NP-API] Server error ${msg.code}: ${msg.message}`);
        break;

      default:
        // Unknown message types are silently ignored
    }
  }

  _open() {
    return this._ws !== null && this._ws.readyState === WebSocket.OPEN;
  }

  _send(obj) {
    if (!this._open()) return;
    this._ws.send(JSON.stringify(obj));
  }

  _setBadge(online) {
    if (!this._badge) return;
    if (online) {
      this._badge.textContent = 'API: Connected';
      this._badge.classList.add('online');
    } else {
      this._badge.textContent = 'API: Offline';
      this._badge.classList.remove('online');
    }
  }
}
