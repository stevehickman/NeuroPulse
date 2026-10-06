/*
 * NeurOne Hub Protocol Compiler — the web app's entry to the shared NPPS core.
 * Document: NP-FW-HUB-001 Rev 1 §4 (docs/np_fw_hub_001.md), the wire format of record. Where the
 *           compiler and §4 disagree, §4 and np_protocol.c are the same artifact seen twice and the
 *           compiler is wrong (REQ-FWHUB-08). scripts/check-hub-wire-format.ts (OI-FWHUB-03, §4.6)
 *           runs `compileProtocol` below and decodes its output at the firmware's struct offsets, so a
 *           constant, length or offset changed in the core without §4 and np_hub_types.h fails CI.
 *
 * There is ONE writer of the descriptor: the shared NPPS core (common/npps-core, OI-NPPS-CORE-01),
 * loaded here as WebAssembly (nppsCore.ts). This file used to be a 1,000-line TypeScript compiler, one
 * of four hand-written ports that drifted; it now hands the core the protocol, the zone namespace, the
 * clock and the session UUID, and takes the descriptor back. What a protocol compiles to, and what
 * refuses it, is decided in the core and is the same on every runtime.
 *
 * Binary layout (NP-FW-HUB-001 §4; little-endian, packed):
 *   [64]  header   magic u32 | version u16 | flags u8 (bit0 T2 tier, bit1 autonomous) | cmd_count u8 |
 *                  session_uuid[16] | compiled_at_unix u32 | device_serial[32] | session_duration_ms u32
 *   [N]   commands cmd_hdr(14) + target[target_len] + params[params_len]
 *   [64]  Ed25519 signature slot, zeroed: signing is the caller's
 *
 * The wire constants live in the core now; the only ones this file reads back are the two header
 * bytes below.
 */

import type { NPProtocolDefinition, NPZoneDefinition } from '../../../../common/types/protocol';
import type { NPWavelengthRules } from '../../../../common/lib/wavelengthRules';
import { nppsCompile } from '../../../../common/lib/nppsCore';

const PROTO_UUID_LEN = 16;
const PROTO_SERIAL_LEN = 32;
const HEADER_FLAGS_OFFSET = 6;
const HEADER_CMD_COUNT_OFFSET = 7;
const HEADER_UUID_OFFSET = 8;
const FLAG_T2_TIER = 1 << 0;   // NP_PROTO_FLAG_T2_TIER (app-computed, carries no authority)

// ─── Compiled protocol output ─────────────────────────────────────────────────

export interface CompiledProtocol {
  /** Full protocol blob: header + commands + 64-byte zeroed signature placeholder. */
  blob: Uint8Array;
  /** Session UUID embedded in the blob (for UHDR key association). */
  sessionUuid: Uint8Array;
  /** True if any T2 modality is present (NP_PROTO_FLAG_T2_TIER was set). */
  isT2: boolean;
  /** Total command count including interval stop commands. */
  cmdCount: number;
}

// ─── Public API ───────────────────────────────────────────────────────────────

export interface CompileOptions {
  /** 32-byte device serial (replay guard); omit for dev/test (zeros). See hubSerial.ts (OI-AND-WIRE-02). */
  deviceSerial?: Uint8Array;
  /**
   * The zone namespace — `NPNamespace.zones`, loaded from every .npps file in
   * the protocol tree. Required whenever a modality targets named zones, which
   * is every shipped PBM transcranial protocol.
   */
  zones?: ReadonlyMap<string, NPZoneDefinition>;
  /**
   * Operator-chosen sockets for `zones: 'clinician_selected'` targets — the
   * patient-specific case (perilesional cortex and similar) that by definition
   * cannot be predefined. Absent, such a protocol does not compile.
   */
  clinicianSockets?: readonly number[];
  /**
   * The wavelength rules in force (NP-NPPS-REF-001 §7a) — the same resolved set
   * the eligibility check used, so what the operator was shown as runnable is
   * what compiles. Defaults to the shipped rules.
   */
  wavelengthRules?: NPWavelengthRules;
}

const hex = (bytes: Uint8Array): string => Array.from(bytes, b => b.toString(16).padStart(2, '0')).join('');

/**
 * Compile an NPProtocolDefinition into a binary hub protocol blob.
 *
 * In a browser, `await initNppsCore()` (nppsCore.ts) once before the first call; Node needs nothing.
 * A request the descriptor cannot express or the hardware cannot reach throws the core's refusal and
 * is never reshaped to fit.
 *
 * @param proto  Protocol definition from the app's protocol library.
 * @param opts   Device serial, zone namespace, operator socket selection.
 * @returns      CompiledProtocol with the complete blob ready for transmission.
 *               The Ed25519 signature (last 64 bytes) is zeroed — attach real
 *               signature from signing service before sending to hub.
 */
export function compileProtocol(
  proto: NPProtocolDefinition,
  opts: CompileOptions = {},
): CompiledProtocol {
  const sessionUuid = new Uint8Array(PROTO_UUID_LEN);
  crypto.getRandomValues(sessionUuid);

  // Device serial — 32 bytes (zeroed if absent, cut or padded to length otherwise).
  const serial = new Uint8Array(PROTO_SERIAL_LEN);
  if (opts.deviceSerial) serial.set(opts.deviceSerial.slice(0, PROTO_SERIAL_LEN));

  let zones: Record<string, number[]> | null = null;
  if (opts.zones) {
    zones = {};
    for (const [name, zone] of opts.zones) zones[name] = [...zone.sockets];
  }

  const blob = nppsCompile({
    def: {
      timingMode: proto.timingMode,
      modalities: proto.modalities.map(m => ({
        type: m.modalityParams.type,
        enabled: m.enabled,
        params: m.modalityParams.params,
        interval: m.interval,
      })),
    },
    zones,
    clinicianSockets: opts.clinicianSockets ? [...opts.clinicianSockets] : null,
    deviceSerialHex: hex(serial),
    nowUnix: Math.floor(Date.now() / 1000),
    sessionUuidHex: hex(sessionUuid),
    wavelengthRules: opts.wavelengthRules
      ? { name: opts.wavelengthRules.name, channels: opts.wavelengthRules.channels }
      : null,
  });

  return {
    blob,
    sessionUuid: blob.slice(HEADER_UUID_OFFSET, HEADER_UUID_OFFSET + PROTO_UUID_LEN),
    isT2: (blob[HEADER_FLAGS_OFFSET] & FLAG_T2_TIER) !== 0,
    cmdCount: blob[HEADER_CMD_COUNT_OFFSET],
  };
}
