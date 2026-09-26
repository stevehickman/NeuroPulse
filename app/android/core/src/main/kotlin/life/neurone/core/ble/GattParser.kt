package life.neurone.core.ble

import life.neurone.core.models.CervicalFaultStatus
import life.neurone.core.models.CervicalPadStatus
import life.neurone.core.models.FirmwareVersion
import life.neurone.core.models.HRVData
import life.neurone.core.models.OtaStatusPacket
import life.neurone.core.models.PacerPhase
import life.neurone.core.models.SessionStatus
import life.neurone.core.models.SocketMapFrame
import life.neurone.core.models.ZoneModuleFrame

// Port of iOS GATTParser — little-endian, matching hub firmware layout.
// All parsers return null (never throw) on short or malformed input.

object GattParser {

    private fun ByteArray.leUInt16(offset: Int): Int =
        (this[offset].toInt() and 0xFF) or ((this[offset + 1].toInt() and 0xFF) shl 8)

    private fun ByteArray.leUInt32(offset: Int): Long =
        (this[offset].toLong() and 0xFF) or
            ((this[offset + 1].toLong() and 0xFF) shl 8) or
            ((this[offset + 2].toLong() and 0xFF) shl 16) or
            ((this[offset + 3].toLong() and 0xFF) shl 24)

    /** SESSION_STATE: uint32 — Unix epoch milliseconds (truncated). UHDR-class. */
    fun parseSessionState(data: ByteArray): Long? {
        if (data.size < 4) return null
        return data.leUInt32(0)
    }

    /** SESSION_STATUS: uint8 protocolID + uint8 statusFlags */
    fun parseSessionStatus(data: ByteArray): Pair<Int, SessionStatus>? {
        if (data.size < 2) return null
        val pid = data[0].toInt() and 0xFF
        val status = SessionStatus.from(data[1].toInt() and 0xFF) ?: return null
        return pid to status
    }

    /** HRV_COHERENCE: uint16 coherence×100 + uint16 RMSSD ms */
    fun parseHrvCoherence(data: ByteArray): HRVData? {
        if (data.size < 4) return null
        val cohRaw = data.leUInt16(0)
        val rmssd = data.leUInt16(2)
        return HRVData(coherenceScore = cohRaw / 100.0f, rmssdMilliseconds = rmssd)
    }

    /** PACER_PHASE: uint8 phase + uint8 elapsed% */
    fun parsePacerPhase(data: ByteArray): Pair<PacerPhase, Int>? {
        if (data.size < 2) return null
        val phase = PacerPhase.from(data[0].toInt() and 0xFF) ?: return null
        return phase to (data[1].toInt() and 0xFF)
    }

    /** IMPEDANCE_RESULT: uint16 bitmask (bit n = electrode n passed). UHDR-class. */
    fun parseImpedanceResult(data: ByteArray): Int? {
        if (data.size < 2) return null
        return data.leUInt16(0)
    }

    /** CVNS_PAD_STATUS: uint8 failed mask + uint8 check + 2 × uint8 pad side. UHDR-class. null if malformed. */
    fun parseCervicalPadStatus(data: ByteArray): CervicalPadStatus? = CervicalPadStatus.fromWire(data)

    /** CVNS_FAULT_STATUS: version + re-enable state + n + flags + n × 8-byte record. UHDR-class. null if malformed. */
    fun parseCervicalFaultStatus(data: ByteArray): CervicalFaultStatus? = CervicalFaultStatus.fromWire(data)

    /** CONSUMABLE_STATUS: 4 × uint16 session counts (intranasal, hydrogel, VNS, audio) */
    fun parseConsumableStatus(data: ByteArray): List<Int>? {
        if (data.size < 8) return null
        return (0 until 4).map { data.leUInt16(it * 2) }
    }

    /**
     * ZONE_MODULE_STATUS: v2 header + n × 3-byte socket-keyed status records (np_zone_notify.h).
     * One fragment; feed it to ZoneModuleFrameAssembler. SHDR-class. null if malformed —
     * including the retired 5-byte one-byte-per-slot payload, which decodes as version 0.
     */
    fun parseZoneModuleStatus(data: ByteArray): ZoneModuleFrame? = ZoneModuleFrame.fromWire(data)

    /** SOCKET_MAP: v2 header + n × 8-byte socket geometry records. One fragment. SHDR-class. null if malformed. */
    fun parseSocketMap(data: ByteArray): SocketMapFrame? = SocketMapFrame.fromWire(data)

    /** FIRMWARE_VERSION: uint32 little-endian — bits [23:16]=major [15:8]=minor [7:0]=patch */
    fun parseFirmwareVersion(data: ByteArray): FirmwareVersion? =
        FirmwareVersion.fromGattBytes(data)

    /** OTA_STATUS: uint8 phase + uint8 progressPercent + uint16 errorCode */
    fun parseOtaStatus(data: ByteArray): OtaStatusPacket? {
        if (data.size < 4) return null
        return OtaStatusPacket(
            phaseRaw = data[0].toInt() and 0xFF,
            progressPercent = data[1].toInt() and 0xFF,
            errorCode = data.leUInt16(2),
        )
    }
}
