package life.neurone.core.session

import life.neurone.core.npps.NppsCore
import life.neurone.core.npps.namedZoneRefs
import life.neurone.core.npps.toNppsCoreJson
import life.neurone.core.protocol.HubDescriptorWire
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPZoneRegistry
import life.neurone.core.platform.epochSeconds
import life.neurone.core.platform.secureRandomBytes

// The hub session descriptor (OI-AND-WIRE-01, OI-NPPS-CORE-01).
//
// There is ONE wire format: the binary descriptor of NP-FW-HUB-001 §4, which
// firmware/hub_control/src/np_protocol.c parses. It has ONE writer: the shared NPPS core
// (common/npps-core), which this class calls through JNI (NppsCore). This class used to be a Kotlin
// port of the web compiler, one of four hand-written ports that drifted; it now maps the Android
// models to the core's protocol shape, hands over the clock, the session UUID and the zone
// namespace, and signs the result. What a protocol compiles to, and what refuses it, is decided in
// the core and is the same on every runtime. app/NeurOneShared/TestData/hub-descriptor-golden.json
// holds the web compiler's output for the same definitions; HubDescriptorCompilerTests diffs this
// class against it, so the core is held to the web reference from here as well as from Rust.
//
// A refusal is an IllegalArgumentException carrying the core's message.
//
// Layout (little-endian, packed):
//   [64]  header   magic u32 | version u16 | flags u8 | cmd_count u8 | uuid[16] |
//                  compiled_at_unix u32 | serial[32] | session_duration_ms u32
//   [N]   commands cmd_hdr(14) + target[target_len] + params[params_len]
//   [64]  Ed25519 signature over everything before it (the raw region, not a digest)

/** A compiled, signed descriptor. [blob] is the whole wire image; the hub verifies it as is. */
class HubDescriptor(
    val blob: ByteArray,
    val sessionUuid: ByteArray,
    val isT2: Boolean,
    val cmdCount: Int,
    val publicKeyFingerprint: String,
)

/** Ed25519 over the descriptor's signed region (NP-FW-HUB-001 §4.1). */
fun interface ProtocolSigner {
    fun sign(message: ByteArray): SignatureResult
}

class SignatureResult(val signature: ByteArray, val publicKeyFingerprint: String)

class HubDescriptorCompiler(
    private val signer: ProtocolSigner,
    private val clockSeconds: () -> Long = { epochSeconds() },
    private val randomBytes: (Int) -> ByteArray = { n -> secureRandomBytes(n) },
) {
    /**
     * @param deviceSerial the 32-byte replay guard the hub checks (§4.2). Omitted only for
     *   bench use: a hub refuses a descriptor whose serial is not its own.
     * @param clinicianSockets operator-chosen 1-based socket ids for `clinician_selected` targets.
     * @param acknowledgedCautions the ids of the cautions the author acknowledged (`NPZoneCaution.ackId`); a protocol
     *   in the caution zone is refused without them.
     */
    fun compile(
        definition: NPProtocolDefinition,
        deviceSerial: ByteArray? = null,
        clinicianSockets: List<Int>? = null,
        acknowledgedCautions: List<String> = emptyList(),
    ): HubDescriptor {
        val unsigned = build(definition, deviceSerial, clinicianSockets, acknowledgedCautions)
        val sig = signer.sign(unsigned.blob.copyOfRange(0, unsigned.blob.size - HubDescriptorWire.SIG_LEN))
        require(sig.signature.size == HubDescriptorWire.SIG_LEN) { "Ed25519 signature must be ${HubDescriptorWire.SIG_LEN} bytes, got ${sig.signature.size}." }
        sig.signature.copyInto(unsigned.blob, unsigned.blob.size - HubDescriptorWire.SIG_LEN)
        return HubDescriptor(unsigned.blob, unsigned.sessionUuid, unsigned.isT2, unsigned.cmdCount, sig.publicKeyFingerprint)
    }

    internal class Unsigned(val blob: ByteArray, val sessionUuid: ByteArray, val isT2: Boolean, val cmdCount: Int)

    /** The descriptor with a zeroed signature (the web compiler's output; the golden test diffs it). */
    internal fun build(
        definition: NPProtocolDefinition,
        deviceSerial: ByteArray?,
        clinicianSockets: List<Int>?,
        acknowledgedCautions: List<String> = emptyList(),
    ): Unsigned {
        val uuid = randomBytes(HubDescriptorWire.UUID_LEN)
        require(uuid.size == HubDescriptorWire.UUID_LEN)
        // The zone namespace is the loaded .npps one (NP-NPPS-REF-001 §8). A name it does not hold is
        // left out, and the core refuses the protocol naming it.
        val zones = definition.namedZoneRefs()
            .mapNotNull { name -> NPZoneRegistry.sockets(name)?.let { name to it } }.toMap()
        val blob = NppsCore.compile(
            definition.toNppsCoreJson(),
            NppsCore.CompileOptions(
                zones = zones,
                clinicianSockets = clinicianSockets,
                deviceSerial = deviceSerial,
                nowUnix = clockSeconds(),
                sessionUuid = uuid,
                acknowledgedCautions = acknowledgedCautions,
            ),
        )
        return Unsigned(blob, uuid, (blob[6].toInt() and HubDescriptorWire.FLAG_T2_TIER) != 0, blob[7].toInt() and 0xFF)
    }

    companion object {
    }
}
