package life.neurone.core.session

import life.neurone.core.protocol.NPAudioEntrainmentParams
import life.neurone.core.protocol.NPBESTacsParams
import life.neurone.core.protocol.NPHardwareLimits
import life.neurone.core.protocol.NPHDTdcsParams
import life.neurone.core.protocol.NPIntervalConfig
import life.neurone.core.protocol.NPModalityParams
import life.neurone.core.protocol.NPPBMChannelElement
import life.neurone.core.protocol.NPPBMTranscranialParams
import life.neurone.core.protocol.NPPbmChannelResolution
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPTimingMode
import life.neurone.core.protocol.NPVNSHRVParams
import life.neurone.core.protocol.NPWavelengthRulesEngine
import life.neurone.core.protocol.SocketLattice
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.SecureRandom
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.max
import kotlin.math.min

// The hub session descriptor (OI-AND-WIRE-01).
//
// There is ONE wire format: the binary descriptor of NP-FW-HUB-001 §4, which
// firmware/hub_control/src/np_protocol.c parses and app/web/src/lib/hubCompiler.ts
// writes. This is a port of that compiler. Where the two disagree §4 is right and this
// file is wrong (REQ-FWHUB-08); app/NeurOneShared/TestData/hub-descriptor-golden.json
// holds the web compiler's own output for the same definitions and the test diffs against it.
//
// The hub carries no JSON parser. The "NPPR" JSON blobs the iOS, Android and Windows apps
// used to send were a format no hub could read.
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
    private val clockSeconds: () -> Long = { System.currentTimeMillis() / 1000 },
    private val randomBytes: (Int) -> ByteArray = { n -> ByteArray(n).also { SecureRandom().nextBytes(it) } },
) {
    /**
     * @param deviceSerial the 32-byte replay guard the hub checks (§4.2). Omitted only for
     *   bench use: a hub refuses a descriptor whose serial is not its own.
     * @param clinicianSockets operator-chosen 1-based socket ids for `clinician_selected` targets.
     */
    fun compile(
        definition: NPProtocolDefinition,
        deviceSerial: ByteArray? = null,
        clinicianSockets: List<Int>? = null,
    ): HubDescriptor {
        val unsigned = build(definition, deviceSerial, clinicianSockets)
        val sig = signer.sign(unsigned.blob.copyOfRange(0, unsigned.blob.size - SIG_LEN))
        require(sig.signature.size == SIG_LEN) { "Ed25519 signature must be $SIG_LEN bytes, got ${sig.signature.size}." }
        sig.signature.copyInto(unsigned.blob, unsigned.blob.size - SIG_LEN)
        return HubDescriptor(unsigned.blob, unsigned.sessionUuid, unsigned.isT2, unsigned.cmdCount, sig.publicKeyFingerprint)
    }

    internal class Unsigned(val blob: ByteArray, val sessionUuid: ByteArray, val isT2: Boolean, val cmdCount: Int)

    /** The descriptor with a zeroed signature (the web compiler's output; the golden test diffs it). */
    internal fun build(
        definition: NPProtocolDefinition,
        deviceSerial: ByteArray?,
        clinicianSockets: List<Int>?,
    ): Unsigned {
        val sessionDurationMs = when (val t = definition.timingMode) {
            is NPTimingMode.Duration -> t.seconds.toLong() * 1000
            is NPTimingMode.IntervalCount -> 0L // runs until the repeats are exhausted
        }

        val cmds = mutableListOf<Cmd>()
        var isT2 = false
        definition.modalities.forEachIndexed { blockIndex, m ->
            if (!m.enabled) return@forEachIndexed
            if (m.params.isT2()) isT2 = true
            for (c in buildCommands(m.params, m.interval, sessionDurationMs, clinicianSockets)) {
                cmds.add(c.also { it.block = blockIndex })
                // Refuse, never truncate: a dropped tail is a missing modality, or a missing STOP.
                if (cmds.size > CMD_MAX) {
                    throw IllegalArgumentException(
                        "Protocol exceeds maximum command count ($CMD_MAX). " +
                            "Reduce interval repeats or the number of enabled modalities.",
                    )
                }
            }
        }

        mergeOverlappingPbm(cmds, sessionDurationMs)
        if (cmds.isEmpty()) throw IllegalArgumentException("Protocol produces no commands — no enabled modalities")

        // The hub needs start order; ties keep declaration order (sortedBy is stable).
        val sorted = cmds.sortedBy { it.startMs }
        val targets = sorted.map { serializeTarget(it.target) }

        val bodyLen = sorted.indices.sumOf { CMD_HDR_LEN + targets[it].block.size + sorted[it].params.size }
        val buf = ByteBuffer.allocate(HEADER_LEN + bodyLen + SIG_LEN).order(ByteOrder.LITTLE_ENDIAN)

        val uuid = randomBytes(UUID_LEN)
        require(uuid.size == UUID_LEN)
        val serial = ByteArray(SERIAL_LEN)
        deviceSerial?.let { it.copyInto(serial, 0, 0, min(it.size, SERIAL_LEN)) }

        buf.putInt(MAGIC)
        buf.putShort(VERSION.toShort())
        buf.put((if (isT2) FLAG_T2_TIER else 0).toByte())
        buf.put(sorted.size.toByte())
        buf.put(uuid)
        buf.putInt(clockSeconds().toInt())
        buf.put(serial)
        buf.putInt(sessionDurationMs.toInt())
        check(buf.position() == HEADER_LEN)

        sorted.forEachIndexed { i, c ->
            val t = targets[i]
            buf.put(c.modType.toByte())
            buf.put(t.slotId.toByte())
            buf.putInt(c.startMs.toInt())
            buf.putInt(c.durationMs.toInt())
            buf.putShort(c.params.size.toShort())
            buf.put(t.kind.toByte())
            buf.put(t.block.size.toByte())
            buf.put(t.block)
            buf.put(c.params)
        }
        return Unsigned(buf.array(), uuid, isT2, sorted.size)
    }

    // ── command generation ───────────────────────────────────────────────────

    private fun buildCommands(
        mp: NPModalityParams,
        interval: NPIntervalConfig,
        sessionDurationMs: Long,
        clinicianSockets: List<Int>?,
    ): List<Cmd> {
        val enc = encode(mp, clinicianSockets)

        // A block's `start` shifts its whole schedule (NP-NPPS-REF-001 §5).
        val offsetMs = interval.startOffsetSeconds.toLong() * 1000
        if (sessionDurationMs > 0 && offsetMs >= sessionDurationMs) {
            throw IllegalArgumentException(
                "A block starts at ${offsetMs / 1000}s, at or after the session's end " +
                    "(${sessionDurationMs / 1000}s). It would never run; remove it or lengthen the session.",
            )
        }

        if (interval.isContinuous) return listOf(Cmd(enc.modType, enc.target, offsetMs, 0, enc.params))

        val onMs = interval.intervalOnSeconds.toLong() * 1000
        val periodMs = onMs + interval.intervalOffSeconds.toLong() * 1000
        val maxRepeats = interval.repeatCount?.toLong()
            ?: if (sessionDurationMs > 0) ceil(sessionDurationMs.toDouble() / periodMs).toLong() else 1L

        val out = mutableListOf<Cmd>()
        var i = 0L
        while (i < maxRepeats) {
            val startMs = offsetMs + i * periodMs
            if (sessionDurationMs > 0 && startMs >= sessionDurationMs) break
            out.add(Cmd(enc.modType, enc.target, startMs, onMs, enc.params))
            val stopMs = startMs + onMs
            if (sessionDurationMs == 0L || stopMs < sessionDurationMs) {
                // The stop carries the SAME target as the on: a stop that reached other
                // sockets would leave the difference running.
                out.add(Cmd(enc.modType, enc.target, stopMs, 0, STOP))
            }
            if (out.size > CMD_MAX) break // the caller refuses; do not run an unbounded expansion
            i++
        }
        return out
    }

    // ── parallel PBM blocks on one tile ──────────────────────────────────────

    private fun isPbmOn(c: Cmd) =
        (c.modType == MOD_PBM_BASE || c.modType == MOD_PBM_SMART || c.modType == MOD_INTRANASAL) && c.params.isNotEmpty()

    private fun socketsOf(c: Cmd): List<Int> = when (val t = c.target) {
        is Target.Sockets -> t.ids
        is Target.Slot -> listOf(10_000 + t.slot)
    }

    /**
     * Two blocks that overlap on a socket are MERGED when they have the same window, sockets,
     * frequency and duty and drive different channels; otherwise the protocol is REFUSED. A
     * tile takes one frequency and one duty for all its channels, and delivering one block or
     * averaging both would be a stimulus nobody authored (CLAUDE.md §3).
     */
    private fun mergeOverlappingPbm(cmds: MutableList<Cmd>, sessionDurationMs: Long) {
        fun endOf(c: Cmd): Double =
            if (c.durationMs > 0) (c.startMs + c.durationMs).toDouble()
            else if (sessionDurationMs > 0) sessionDurationMs.toDouble() else Double.POSITIVE_INFINITY

        val drop = mutableSetOf<Cmd>()
        for (i in cmds.indices) {
            val a = cmds[i]
            if (!isPbmOn(a) || a in drop) continue
            for (j in i + 1 until cmds.size) {
                val b = cmds[j]
                if (!isPbmOn(b) || b in drop || a.block == b.block) continue
                val sa = socketsOf(a).toSet()
                val sbList = socketsOf(b)
                val shared = sbList.filter { it in sa }
                val overlapInTime = a.startMs < endOf(b) && b.startMs < endOf(a)
                if (shared.isEmpty() || !overlapInTime) continue

                val sameSockets = shared.size == sa.size && sbList.size == sa.size
                val sameWindow = a.startMs == b.startMs && a.durationMs == b.durationMs
                val (fo, dutyOff) = if (a.modType == MOD_INTRANASAL) 1 to 2 else 0 to 1
                val sameTiming = a.modType == b.modType &&
                    a.params[fo] == b.params[fo] && a.params[dutyOff] == b.params[dutyOff]
                val curs = PBM_CUR_OFFSETS.getValue(a.modType)
                val disjoint = a.modType == b.modType && curs.all { a.params[it] == 0.toByte() || b.params[it] == 0.toByte() }

                if (!(sameSockets && sameWindow && sameTiming && disjoint)) {
                    throw IllegalArgumentException(
                        "Two PBM blocks overlap on socket${if (shared.size == 1) "" else "s"} " +
                            "${shared.take(6).joinToString(", ")}${if (shared.size > 6) ", …" else ""} " +
                            "from ${max(a.startMs, b.startMs) / 1000.0}s. A tile takes one frequency, " +
                            "one duty and one schedule for all its channels, so blocks sharing a tile must " +
                            "match in timing, frequency and duty and drive different wavelengths. " +
                            "The protocol is refused rather than reshaped.",
                    )
                }
                val merged = a.params.copyOf()
                for (k in curs) merged[k] = if (a.params[k] != 0.toByte()) a.params[k] else b.params[k]
                if (a.modType == MOD_PBM_SMART) merged[5] = (a.params[5].toInt() or b.params[5].toInt()).toByte()
                a.params = merged
                drop.add(b)
                cmds.firstOrNull {
                    it.block == b.block && it.params.isEmpty() && it.modType == b.modType && it.startMs.toDouble() == endOf(b)
                }?.let { drop.add(it) }
            }
        }
        cmds.removeAll(drop)
    }

    // ── targets ──────────────────────────────────────────────────────────────

    private class SerializedTarget(val kind: Int, val slotId: Int, val block: ByteArray)

    private fun serializeTarget(t: Target): SerializedTarget = when (t) {
        is Target.Slot -> {
            if (t.slot < SLOT_FIRST_VALID || t.slot >= SLOT_MAX) {
                throw IllegalArgumentException(
                    "Slot ${t.slot} is not addressable: slots below $SLOT_FIRST_VALID are the " +
                        "retired zone-module slots (cranial targets use sockets), and $SLOT_MAX is " +
                        "the end of the slot domain.",
                )
            }
            SerializedTarget(TARGET_SLOT, t.slot, ByteArray(0))
        }
        is Target.Sockets -> {
            // An empty target is a session that reports a delivered dose while lighting nothing.
            if (t.ids.isEmpty()) throw IllegalArgumentException("Command targets no sockets — resolve the zone to at least one socket.")
            SerializedTarget(TARGET_SOCKET_MASK, SLOT_NONE, socketBitmap(t.ids))
        }
    }

    /** 1-based NPPS socket ids → the firmware's 0-based LSB-first bitmap; duplicates set one bit. */
    private fun socketBitmap(ids: List<Int>): ByteArray {
        val mask = ByteArray(SOCKET_MASK_BYTES)
        for (id in ids) {
            if (!SocketLattice.isValid(id)) {
                throw IllegalArgumentException(
                    "Socket $id is not a socket on this helmet (valid ids are ${SocketLattice.rangeLabel}). " +
                        "Check the zone's socket list.",
                )
            }
            val bit = id - 1
            mask[bit shr 3] = (mask[bit shr 3].toInt() or (1 shl (bit and 7))).toByte()
        }
        return mask
    }

    // ── parameter encoding ───────────────────────────────────────────────────

    private fun encode(mp: NPModalityParams, clinicianSockets: List<Int>?): Encoded = when (mp) {
        is NPModalityParams.PbmTranscranial -> encodePbm(mp.params, clinicianSockets)
        is NPModalityParams.PbmIntranasal -> {
            val p = mp.params
            val ch = oneChannel(p.wavelength.rawValue, listOf(NPPBMChannelElement.LED_660, NPPBMChannelElement.LED_808))
            val cur = PbmDrive.irradianceToRegister(p.irradianceMWcm2, PbmDrive.INTRANASAL_FULL_SCALE_MW_CM2, "the intranasal probe")
            Encoded(
                MOD_INTRANASAL, Target.Slot(SLOT_INTRANASAL),
                bytes(0x00, freqCode(p.frequencyHz), dutyReg(p.dutyCyclePercent),
                    if (ch == NPPBMChannelElement.LED_660) cur else 0, if (ch == NPPBMChannelElement.LED_808) cur else 0),
            )
        }
        is NPModalityParams.EegNeurofeedback -> {
            val p = mp.params
            var chMask = when (p.channels) {
                life.neurone.core.protocol.NPEEGNeurofeedbackParams.ChannelSelection.ALL -> 0xFF
                life.neurone.core.protocol.NPEEGNeurofeedbackParams.ChannelSelection.FRONT -> 0x03
                life.neurone.core.protocol.NPEEGNeurofeedbackParams.ChannelSelection.CENTRAL -> 0x3C
                life.neurone.core.protocol.NPEEGNeurofeedbackParams.ChannelSelection.CUSTOM -> 0xFF
            }
            if (p.channels == life.neurone.core.protocol.NPEEGNeurofeedbackParams.ChannelSelection.CUSTOM && p.customChannels != null) {
                val labels = listOf("Fp1", "Fp2", "F3", "F4", "C3", "C4", "P3", "P4")
                chMask = p.customChannels!!.fold(0) { m, ch -> labels.indexOf(ch).let { i -> if (i >= 0) m or (1 shl i) else m } }
            }
            // gain=24×, notch=60 Hz, ref=linked_ear; adaptive_out 3 = drive both PBM and audio.
            Encoded(MOD_EEG, Target.Slot(SLOT_EEG), bytes(chMask, 6, 2, 0, if (p.closedLoopEnabled) 0x03 else 0x00))
        }
        is NPModalityParams.BesTacs -> {
            val p = mp.params
            val wf = if (p.waveform == NPBESTacsParams.Waveform.SQUARE) 1 else 0
            // 7 bytes, not 8: the struct is packed and the driver checks the length exactly.
            val b = le(7)
            b.put(0)
            b.putShort(min(jsRound(p.frequencyHz * 1000), 0xFFFF).toShort())
            b.putShort(min(jsRound(p.intensityMilliamps * 1000), 1000).toShort())
            b.put(wf.toByte()); b.put(0)
            Encoded(MOD_BES_TACS, Target.Slot(SLOT_BES_TACS), b.array())
        }
        is NPModalityParams.Tdcs -> {
            val p = mp.params
            val b = le(8)
            b.put(electrodePair(p.electrodePairs.firstOrNull()).toByte())
            b.putShort(min(jsRound(p.intensityMilliamps * 1000), 2000).toShort())
            b.put(0)
            b.putShort(max(p.rampSeconds, 30).toShort()) // firmware enforces ≥30 s
            // OI-CHARGE-04: FLOORED, never rounded — the safety MCU derives 40 µC/cm² × area,
            // so rounding up would hand it a limit above the true ceiling.
            b.putShort(min(floor(p.electrodeAreaCm2 * 1000).toInt(), 0xFFFF).toShort())
            Encoded(MOD_TDCS, Target.Slot(SLOT_TDCS), b.array())
        }
        is NPModalityParams.VnsHRV -> {
            val p = mp.params
            val proto = when (p.hrvProtocol) {
                NPVNSHRVParams.HRVProtocol.STANDALONE -> 0
                NPVNSHRVParams.HRVProtocol.COMBINED_PBM -> 1
                NPVNSHRVParams.HRVProtocol.TAVNS_SYNC -> 2
                NPVNSHRVParams.HRVProtocol.EEG_BIOFEEDBACK -> 3
            }
            val b = le(9)
            b.put(0)
            b.putShort(min(jsRound(p.frequencyHz * 1000), 25000).toShort())
            b.putShort(min(jsRound(p.intensityMilliamps * 1000), 2000).toShort())
            b.put(0); b.put(1); b.put(1); b.put(proto.toByte())
            Encoded(MOD_VNS_HRV, Target.Slot(SLOT_VNS_HRV), b.array())
        }
        is NPModalityParams.AudioEntrainment -> encodeAudio(mp.params)
        is NPModalityParams.VisualStimulation -> {
            val p = mp.params
            val mode = p.mode.ordinal // binocular, emdr, retinal_pbm, mode_f
            val shadeReq = if (mode == 0) 1 else 0
            Encoded(
                MOD_VISUAL, Target.Slot(SLOT_VISUAL),
                bytes(mode, min(jsRound(p.frequencyHz), 100), 50, 0x02, 0xFF, 0x0F,
                    min(jsRound(p.emdrCadenceHz * 10), 255), if (p.enableModeF) 1 else 0, shadeReq),
            )
        }
        is NPModalityParams.Qeeg21ch -> {
            val p = mp.params
            val b = le(8)
            b.putInt(0x1FFFFF)
            b.put(6); b.put(2)
            b.put(p.reference.ordinal.toByte()) // linked_ear 0, cz 1, average 2
            b.put(if (p.sloretaEnabled) 1 else 0)
            Encoded(MOD_QEEG_21CH, Target.Slot(SLOT_QEEG), b.array())
        }
        is NPModalityParams.Tms -> {
            val p = mp.params
            val isTbs = p.tmsProtocol != life.neurone.core.protocol.NPTMSParams.TMSProtocol.RTMS
            val interTrain = if (isTbs) 200 else jsRound(1000.0 / max(p.frequencyHz, 0.1))
            val b = le(10)
            b.put(p.tmsProtocol.ordinal.toByte()) // rTMS 0, TBS 1, iTBS 2
            b.put(p.target.ordinal.toByte())
            b.putShort(min(jsRound(p.frequencyHz * 1000), 0xFFFF).toShort())
            b.put(min(p.intensityPercentMT, 255).toByte())
            b.putShort(min(p.pulseCount, 0xFFFF).toShort())
            b.putShort(min(interTrain, 0xFFFF).toShort())
            b.put((if (isTbs) 50 else 0).toByte())
            Encoded(MOD_TMS, Target.Slot(SLOT_TMS), b.array())
        }
        is NPModalityParams.PbmDeep1170nm -> {
            val p = mp.params
            val b = le(5)
            b.putShort(min(jsRound(p.intensityMWcm2), 1000).toShort())
            b.put(freqCode(p.frequencyHz).toByte())
            b.put(dutyReg(p.dutyCyclePercent).toByte())
            b.put(0)
            Encoded(MOD_PBM_1170NM, Target.Slot(SLOT_PBM_1170NM), b.array())
        }
        is NPModalityParams.ClinicalTacs -> {
            val p = mp.params
            // The first channelCount channels; bits 5–7 of the extension byte stay clear (OI-TACS-01).
            val n = min(max(jsRound(p.channelCount.toDouble()), 0), NPHardwareLimits.CLINICAL_TACS_MAX_CHANNELS)
            val fullMask = if (n == 0) 0 else (1 shl n) - 1
            val b = le(8)
            b.putShort(min(jsRound(p.frequencyHz * 1000), 0xFFFF).toShort())
            b.putShort(min(jsRound(p.intensityMilliamps * 1000), 4000).toShort())
            b.put((fullMask and 0xFF).toByte())
            b.put(((fullMask ushr 8) and 0xFF).toByte())
            b.put(((fullMask ushr 16) and 0x1F).toByte())
            b.put((when (p.waveform) { NPBESTacsParams.Waveform.SINUSOIDAL -> 0; NPBESTacsParams.Waveform.SQUARE -> 1; else -> 2 }).toByte())
            Encoded(MOD_CLIN_TACS, Target.Slot(SLOT_CLIN_TACS), b.array())
        }
        is NPModalityParams.HdTdcs -> {
            val p = mp.params
            val b = le(6)
            b.put(p.target.ordinal.toByte())
            b.put(p.montage.ordinal.toByte()) // ring_4x1 0, bilateral_4x1 1, standard_2_electrode 2
            b.putShort(min(jsRound(p.intensityMilliamps * 1000), 2000).toShort())
            b.putShort(30) // ramp_s; firmware enforces ≥30 s
            Encoded(MOD_HD_TDCS, Target.Slot(SLOT_HD_TDCS), b.array())
        }
        is NPModalityParams.CervicalVns -> {
            val p = mp.params
            val b = le(10)
            b.put(0)
            b.putShort(min(jsRound(p.frequencyHz * 1000), 25000).toShort())
            b.putShort(min(jsRound(p.intensityMilliamps * 1000), 2000).toShort())
            b.putShort(0)  // pulse_width_us 0 → firmware default 250 µs
            b.putShort(10) // ramp_s; firmware enforces ≥10 s
            b.put(1)       // baseline_req: the cardiac interlock is required
            Encoded(MOD_CVNS, Target.Slot(SLOT_CVNS), b.array())
        }
        is NPModalityParams.Vibrotactile40hz -> {
            val p = mp.params
            val clamped = max(0.6, min(1.2, p.intensityG))
            val gain = jsRound((clamped - 0.6) / 0.6 * 0x7F)
            val b = le(4)
            b.put(gain.toByte())
            b.put(((if (p.syncToAudio) 0x01 else 0) or (if (p.syncToVisual) 0x02 else 0)).toByte())
            b.putShort(40000.toShort()) // 40 Hz locked
            Encoded(MOD_VIBROTACTILE, Target.Slot(SLOT_VIBROTACTILE), b.array())
        }
    }

    private fun encodePbm(p: NPPBMTranscranialParams, clinicianSockets: List<Int>?): Encoded {
        // Throws on an unknown zone or a missing clinician selection: a substituted target is wrong-site stimulation.
        val target = Target.Sockets(p.target.resolveSockets(clinicianSockets))
        val duty = dutyReg(p.dutyCyclePercent)
        val fc = freqCode(p.frequencyHz)
        // One wavelength per block: the rules pick the channel, every other channel is commanded 0.
        val ch = oneChannel(p.wavelength.rawValue, NPPBMChannelElement.entries)
        val cur = PbmDrive.irradianceToRegister(p.irradianceMWcm2, PbmDrive.PBM_FULL_SCALE_MW_CM2.getValue(ch), ch.rawValue)
        return when (ch) {
            NPPBMChannelElement.LED_660 -> Encoded(MOD_PBM_BASE, target, bytes(fc, duty, cur, 0))
            NPPBMChannelElement.LED_808 -> Encoded(MOD_PBM_BASE, target, bytes(fc, duty, 0, cur))
            NPPBMChannelElement.LED_1064 -> Encoded(MOD_PBM_SMART, target, bytes(fc, duty, 0, 0, cur, 0x04))
        }
    }

    private fun encodeAudio(p: NPAudioEntrainmentParams): Encoded {
        var mode = 4 // off
        var beatMhz = 0
        val bin = p.binauralBeatsHz
        val iso = p.isochronicTonesHz
        when {
            bin != null -> { mode = 0; beatMhz = jsRound(bin * 1000) }
            iso != null -> { mode = 1; beatMhz = jsRound(iso * 1000) }
            p.noiseType == NPAudioEntrainmentParams.NoiseType.PINK -> mode = 2
            p.noiseType == NPAudioEntrainmentParams.NoiseType.BROWN -> mode = 3
        }
        val b = le(8)
        b.put(mode.toByte())
        b.putShort(min(p.carrierHz, 65535.0).toInt().toShort())
        b.putShort(min(beatMhz, 0xFFFF).toShort())
        b.put(PbmDrive.dbToVolumePercent(p.volumeDb).toByte())
        b.put(if (p.boneConductionPacer) 1 else 0)
        b.put(if (p.eegAdaptive) 1 else 0)
        return Encoded(MOD_AUDIO, Target.Slot(SLOT_AUDIO), b.array())
    }

    /** The one emitter channel that delivers [wavelength], or a refusal that names why. */
    private fun oneChannel(wavelength: String, allowed: List<NPPBMChannelElement>): NPPBMChannelElement =
        when (val r = NPWavelengthRulesEngine.resolveChannels(wavelength)) {
            is NPPbmChannelResolution.Refused -> throw IllegalArgumentException(
                when (r.reason) {
                    "retired" -> NPWavelengthRulesEngine.retiredMessage(r.value)
                    "invalid" -> "PBM wavelength '${r.value}' is not a wavelength: write one value such as \"810nm\"."
                    else -> unmappedMessage(r.value)
                },
            )
            is NPPbmChannelResolution.Ok -> r.elements.first().also {
                // Same text as iOS: a channel this modality does not carry reads as "unmapped".
                if (it !in allowed) throw IllegalArgumentException(unmappedMessage(wavelength))
            }
        }

    private fun unmappedMessage(value: String) =
        "No emitter channel delivers $value under the wavelength rules in force. " +
            "Refused, not moved to the nearest channel."

    private fun electrodePair(pair: List<String>?): Int {
        if (pair == null || pair.size < 2) return 0
        val a = pair[0].uppercase()
        val b = pair[1].uppercase()
        return when {
            (a == "F3" && b == "F4") || (a == "F4" && b == "F3") -> 0
            (a == "P3" && b == "P4") || (a == "P4" && b == "P3") -> 1
            (a == "FZ" && b == "PZ") || (a == "PZ" && b == "FZ") -> 2
            else -> 0
        }
    }

    private fun NPModalityParams.isT2() = this is NPModalityParams.Qeeg21ch || this is NPModalityParams.Tms ||
        this is NPModalityParams.PbmDeep1170nm || this is NPModalityParams.ClinicalTacs ||
        this is NPModalityParams.HdTdcs || this is NPModalityParams.CervicalVns

    // ── helpers ──────────────────────────────────────────────────────────────

    private class Encoded(val modType: Int, val target: Target, val params: ByteArray)

    private sealed class Target {
        data class Slot(val slot: Int) : Target()
        data class Sockets(val ids: List<Int>) : Target()
    }

    private class Cmd(val modType: Int, val target: Target, val startMs: Long, val durationMs: Long, var params: ByteArray) {
        var block: Int = -1
    }

    private fun le(n: Int): ByteBuffer = ByteBuffer.allocate(n).order(ByteOrder.LITTLE_ENDIAN)
    private fun bytes(vararg v: Int) = ByteArray(v.size) { v[it].toByte() }

    /** JavaScript's Math.round — half rounds up — so this encodes what hubCompiler.ts encodes. */
    private fun jsRound(x: Double): Int = floor(x + 0.5).toInt()
    private fun freqCode(hz: Double) = if (hz <= 0) 0 else jsRound(hz) and 0xFF
    private fun dutyReg(pct: Int) = min(pct * 2, 0x32)

    companion object {
        // Mirrors np_hub_config.h; scripts/check-hub-wire-format.ts pins the web compiler's copy.
        const val MAGIC = 0x4E504850
        const val VERSION = 1
        const val UUID_LEN = 16
        const val SERIAL_LEN = 32
        const val SIG_LEN = 64
        const val CMD_MAX = 64
        const val HEADER_LEN = 64
        const val CMD_HDR_LEN = 14
        const val SOCKET_MASK_BYTES = 16
        const val FLAG_T2_TIER = 1

        private const val TARGET_SLOT = 0x00
        private const val TARGET_SOCKET_MASK = 0x01
        private const val SLOT_NONE = 0xFF
        private const val SLOT_FIRST_VALID = 5
        private const val SLOT_MAX = 19

        private const val SLOT_EEG = 5
        private const val SLOT_AUDIO = 6
        private const val SLOT_VISUAL = 7
        private const val SLOT_VNS_HRV = 8
        private const val SLOT_INTRANASAL = 9
        private const val SLOT_CVNS = 10
        private const val SLOT_QEEG = 11
        private const val SLOT_TMS = 12
        private const val SLOT_PBM_1170NM = 13
        private const val SLOT_CLIN_TACS = 14
        private const val SLOT_HD_TDCS = 15
        private const val SLOT_VIBROTACTILE = 16
        private const val SLOT_BES_TACS = 17
        private const val SLOT_TDCS = 18

        private const val MOD_PBM_BASE = 0x01
        private const val MOD_PBM_SMART = 0x02
        private const val MOD_INTRANASAL = 0x03
        private const val MOD_EEG = 0x04
        private const val MOD_BES_TACS = 0x05
        private const val MOD_TDCS = 0x06
        private const val MOD_VNS_HRV = 0x07
        private const val MOD_AUDIO = 0x08
        private const val MOD_VISUAL = 0x09
        private const val MOD_CVNS = 0x0A
        private const val MOD_QEEG_21CH = 0x0B
        private const val MOD_TMS = 0x0C
        private const val MOD_PBM_1170NM = 0x0D
        private const val MOD_CLIN_TACS = 0x0E
        private const val MOD_HD_TDCS = 0x0F
        private const val MOD_VIBROTACTILE = 0x10

        private val STOP = ByteArray(0)

        /** Channel-current byte offsets inside each PBM params struct (NP-FW-HUB-001 §4.6). */
        private val PBM_CUR_OFFSETS = mapOf(
            MOD_PBM_BASE to intArrayOf(2, 3),
            MOD_PBM_SMART to intArrayOf(2, 3, 4),
            MOD_INTRANASAL to intArrayOf(3, 4),
        )
    }
}
