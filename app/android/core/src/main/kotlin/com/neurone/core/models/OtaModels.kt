package com.neurone.core.models

// Port of app/ios/NeurOne/Models/OTAModels.swift.
// Wire values are frozen — hub firmware contract (CLAUDE.md §13.5 OTA entry).

enum class OtaPhase(val rawValue: Int) {
    IDLE(0x00),
    RECEIVING(0x01),
    VERIFYING(0x02),
    COMMITTING(0x03),
    VERIFIED(0x04),
    REBOOTING(0x05),
    COMPLETE(0x06),
    FAILED(0xFF);

    val isBusy: Boolean
        get() = this == RECEIVING || this == VERIFYING || this == COMMITTING || this == REBOOTING

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
        const val CHUNK_SIZE: Int = 496

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
