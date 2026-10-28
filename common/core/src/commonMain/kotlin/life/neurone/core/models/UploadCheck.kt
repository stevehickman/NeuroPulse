package life.neurone.core.models

// UPLOAD_CHECK (SWR-UP-01, OI-UI-KMP-10 U1): what the helmet says it received. After every protocol upload the helmet
// hashes the buffer it received, before parsing, and reports that hash with an accept or refuse status. The app compares
// the hash with the SHA-256 of the exact bytes it sent. Wire layout: firmware/hub_control/include/np_upload_check.h,
// 34 bytes: status, reason, SHA-256. The hash is of protocol bytes the person authored and carries no user biology.

enum class UploadCheckStatus(val rawValue: Int) {
    /** Nothing received since power-on. */
    NONE(0),
    ACCEPTED(1),
    REFUSED(2),

    /** Hashed, verdict not yet reached: the app reads again, and never sends the protocol again (SWR-UP-03). */
    PENDING(3);

    companion object {
        fun from(rawValue: Int): UploadCheckStatus? = entries.firstOrNull { it.rawValue == rawValue }
    }
}

data class UploadCheck(
    val status: UploadCheckStatus,
    /** The refusal's `np_hub_status_t` code as a positive number; 0 unless [status] is [UploadCheckStatus.REFUSED]. */
    val reason: Int,
    /** SHA-256 of the received bytes, 64 lowercase hex digits; all zeros while [status] is [UploadCheckStatus.NONE]. */
    val receivedHash: String,
) {
    /** The `helmetRefusal` the library's upload rule takes for this refusal, or null when it is none of the three it names. */
    val refusalKind: String?
        get() = if (status == UploadCheckStatus.REFUSED && reason in TIER_REFUSAL_CODES) "tier" else null

    companion object {
        const val LENGTH = 34

        /**
         * `NP_HUB_ERR_TIER_F4` (19) and `NP_HUB_ERR_TIER_UNVERIFIED` (20): the helmet refused a protocol for its tier. A
         * refusal for the hardware version is the hub's too (`NP_HUB_ERR_HW_VERSION_MISMATCH` 27 and `..._UNVERIFIED` 28, `SWR-HV-08`),
         * but no app maps those yet; an unmodified clone is not made by the hub, so no code names it.
         */
        private val TIER_REFUSAL_CODES = setOf(19, 20)

        /** Null for a frame of the wrong length or an unknown status: no part of it is believed. */
        fun fromWire(data: ByteArray): UploadCheck? {
            if (data.size != LENGTH) return null
            val status = UploadCheckStatus.from(data[0].toInt() and 0xFF) ?: return null
            val hex = buildString(64) {
                for (i in 2 until LENGTH) {
                    val v = data[i].toInt() and 0xFF
                    append("0123456789abcdef"[v shr 4]).append("0123456789abcdef"[v and 0x0F])
                }
            }
            return UploadCheck(status, data[1].toInt() and 0xFF, hex)
        }

        /** The hash of the exact bytes the app sent, in the form [receivedHash] takes. */
        fun hashOf(sent: ByteArray): String {
            val digest = life.neurone.core.platform.Digests.sha256(sent)
            return buildString(64) {
                for (b in digest) {
                    val v = b.toInt() and 0xFF
                    append("0123456789abcdef"[v shr 4]).append("0123456789abcdef"[v and 0x0F])
                }
            }
        }
    }
}

/**
 * What the library's upload rule is told (`SWR-UP-02`). Whether it was confirmed, unknown or failed is the core's decision
 * (`NPProtocolLibrary.recordUploadResult`); this only reads the helmet's answer.
 */
data class UploadVerdict(
    val helmetStatus: String?,
    val helmetHash: String?,
    val helmetRefusal: String?,
) {
    companion object {
        /**
         * The verdict from the last report the helmet gave ([check], null if it never gave one). A final report (accepted
         * or refused) is passed on with its hash; the library compares that hash with the one the app sent, so a final report
         * for other bytes is a failed upload and not a guess here. No report, an empty one (`NONE`) or one still `PENDING`
         * is no answer, which the library treats as unknown (`SWR-UP-03`).
         */
        fun of(check: UploadCheck?): UploadVerdict = when {
            check == null || check.status == UploadCheckStatus.NONE || check.status == UploadCheckStatus.PENDING ->
                UploadVerdict(null, null, null)
            else -> UploadVerdict(
                helmetStatus = if (check.status == UploadCheckStatus.ACCEPTED) "accepted" else "refused",
                helmetHash = check.receivedHash,
                helmetRefusal = check.refusalKind,
            )
        }
    }
}
