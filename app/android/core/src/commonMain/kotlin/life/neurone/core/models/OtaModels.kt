package life.neurone.core.models

// Port of app/ios/NeurOne/Models/OTAModels.swift (docs/status/completed-decisions.md OTA entry).

// OTA_STATUS notify byte 0. The hub firmware does not yet emit this characteristic, so the
// contract is the iOS `OTAPhase` enum; `ModelsParityTests` reads that Swift source and fails
// on any divergence in name or value. Order follows the app-driven sequence
// INITIATE → CHUNK… → VERIFY → (hub: VERIFIED) → COMMIT → reboot → COMPLETE, so COMMIT is only
// ever sent after 0x04 and nothing "committing" can precede it.
enum class OtaPhase(val rawValue: Int) {
    IDLE(0x00),
    PREPARING(0x01),     // Hub clearing Scratch partition
    TRANSFERRING(0x02),  // Receiving firmware chunks
    VERIFYING(0x03),     // Hub computing SHA-256 + Ed25519 check
    VERIFIED(0x04),      // Signature OK; awaiting COMMIT opcode from app
    APPLYING(0x05),      // Writing inactive bank + staging boot swap + about to reset
    COMPLETE(0x06),      // Post-reboot, running from new bank
    FAILED(0xFF);

    // Matches iOS: every non-idle, non-terminal phase is busy, including VERIFIED — the
    // update is mid-flight there, waiting on the app's COMMIT.
    val isBusy: Boolean
        get() = this != IDLE && !isTerminal

    val isTerminal: Boolean
        get() = this == COMPLETE || this == FAILED

    companion object {
        fun from(rawValue: Int): OtaPhase? = entries.firstOrNull { it.rawValue == rawValue }
    }
}

data class OtaStatusPacket(
    val phaseRaw: Int,
    val progressPercent: Int,
    val errorCode: Int,
) {
    val isError: Boolean get() = errorCode != 0
    val phase: OtaPhase? get() = OtaPhase.from(phaseRaw)
}

// FIRMWARE_VERSION GATT encoding: uint32 little-endian —
// bits [23:16]=major, [15:8]=minor, [7:0]=patch.
data class FirmwareVersion(
    val major: Int,
    val minor: Int,
    val patch: Int,
) : Comparable<FirmwareVersion> {

    override fun compareTo(other: FirmwareVersion): Int =
        compareValuesBy(this, other, { it.major }, { it.minor }, { it.patch })

    override fun toString(): String = "$major.$minor.$patch"

    companion object {
        /** Parse "1.2.3". Returns null on any malformed input. */
        fun parse(versionString: String): FirmwareVersion? {
            val parts = versionString.split(".")
            if (parts.size != 3) return null
            val nums = parts.map { it.toIntOrNull() ?: return null }
            if (nums.any { it < 0 }) return null
            return FirmwareVersion(nums[0], nums[1], nums[2])
        }

        /** Parse the 4-byte little-endian GATT payload. */
        fun fromGattBytes(data: ByteArray): FirmwareVersion? {
            if (data.size < 4) return null
            val raw = (data[0].toLong() and 0xFF) or
                ((data[1].toLong() and 0xFF) shl 8) or
                ((data[2].toLong() and 0xFF) shl 16) or
                ((data[3].toLong() and 0xFF) shl 24)
            return FirmwareVersion(
                major = ((raw shr 16) and 0xFF).toInt(),
                minor = ((raw shr 8) and 0xFF).toInt(),
                patch = (raw and 0xFF).toInt(),
            )
        }
    }
}

// In-flight OTA transfer bookkeeping. Chunk payload size matches the iOS app
// and hub firmware: 496 bytes of firmware data per BLE write (2B index + data).
data class OtaSession(
    val totalBytes: Long,
    val sentBytes: Long = 0,
) {
    val progressFraction: Double
        get() = if (totalBytes <= 0) 0.0 else sentBytes.toDouble() / totalBytes.toDouble()

    companion object {

        /**
         * Human-readable byte count with integer round-half-up KB/MB —
         * mirrors iOS `formattedBytes`, which avoids banker's-rounding
         * "0 KB" for sub-kilobyte transfers.
         */
        fun formattedBytes(bytes: Long): String {
            if (bytes < 1024) return "$bytes B"
            val kb = (bytes + 512) / 1024      // round half up
            if (kb < 1024) return "$kb KB"
            val mb = (kb + 512) / 1024
            return "$mb MB"
        }
    }
}
