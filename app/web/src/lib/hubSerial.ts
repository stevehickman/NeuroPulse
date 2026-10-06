/**
 * The hub's replay-guard serial (NP-FW-HUB-001 §4.2, OI-AND-WIRE-02).
 *
 * The hub publishes it as the DEVICE_SERIAL GATT characteristic (0x0017, READ 32 B, encrypted
 * link only). The web app has no hub transport yet, so nothing reads it here; this is the seam
 * the transport will fill and `compileProtocol({ deviceSerial })` will consume, matching iOS
 * (`NeurOneGATTManager.deviceSerial`) and Android (`NeurOneGattManager.deviceSerial`).
 *
 * Memory only: never persist, upload or display it.
 */

import { GattSizes, GattUuidStrings } from '../../../../common/lib/constants.generated';

/** The DEVICE_SERIAL characteristic and its length, generated from common/npps/constants.json for every runtime. */
export const DEVICE_SERIAL_UUID = GattUuidStrings.DEVICE_SERIAL_ID;
export const DEVICE_SERIAL_LEN = GattSizes.DEVICE_SERIAL_LEN;

/**
 * A characteristic value as a serial: exactly 32 bytes or null. A short read is never
 * zero-padded into a serial no hub holds, and a long one is never truncated into one.
 */
export function parseDeviceSerial(value: Uint8Array | null | undefined): Uint8Array | null {
  return value != null && value.length === DEVICE_SERIAL_LEN ? value.slice() : null;
}
