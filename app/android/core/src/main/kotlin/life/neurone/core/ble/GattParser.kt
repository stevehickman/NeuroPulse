package life.neurone.core.ble

import life.neurone.core.models.FirmwareVersion
import life.neurone.core.models.HRVData
import life.neurone.core.models.OtaStatusPacket
import life.neurone.core.models.PacerPhase
import life.neurone.core.models.SessionStatus

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

    /** CONSUMABLE_STATUS: 4 × uint16 session counts (intranasal, hydrogel, VNS, audio) */
    fun parseConsumableStatus(data: ByteArray): List<Int>? {
        if (data.size < 8) return null
        return (0 until 4).map { data.leUInt16(it * 2) }
    }

    /** ZONE_MODULE_STATUS: 5 bytes — one uint8 per zone slot (0=absent, 1–5=zone ID confirmed) */
    fun parseZoneModuleStatus(data: ByteArray): List<Int>? {
        if (data.size < 5) return null
        return (0 until 5).map { data[it].toInt() and 0xFF }
    }

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
