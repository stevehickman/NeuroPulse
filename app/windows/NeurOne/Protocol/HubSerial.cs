// HubSerial.cs — the hub's replay-guard serial (NP-FW-HUB-001 §4.2, OI-AND-WIRE-02).
//
// The hub publishes it as the DEVICE_SERIAL GATT characteristic (0x0017, READ 32 B, encrypted link
// only). Windows has no hub transport yet, so nothing reads it here; this is the seam the transport
// will fill and `HubDescriptorCompiler.Compile(deviceSerial: …)` will consume, matching iOS
// (`NeurOneGATTManager.deviceSerial`) and Android (`NeurOneGattManager.deviceSerial`).
//
// Memory only: never persist, upload or display it.

namespace NeurOne.Protocol;

static class HubSerial
{
    /// 4E455550-0017-1000-8000-00805F9B34FB — byte-identical to the apps and the hub.
    public static readonly Guid CharacteristicUuid = new("4E455550-0017-1000-8000-00805F9B34FB");

    public const int Length = 32;

    /// A characteristic value as a serial: exactly 32 bytes or null. A short read is never
    /// zero-padded into a serial no hub holds, and a long one is never truncated into one.
    public static byte[]? Parse(byte[]? value)
        => value is { Length: Length } ? (byte[])value.Clone() : null;
}
