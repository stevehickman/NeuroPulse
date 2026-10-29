package life.neurone.core.models

import life.neurone.core.protocol.NPLibraryRules

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
    /** The `helmetRefusal` the library's upload rule takes for this refusal, or null when it is none of the three it names. The core decides. */
    val refusalKind: String?
        get() = NPLibraryRules.helmetCause(statusRule, reason, null, null).refusalKind

    /** The report's status as the core takes it: `accepted`, `refused`, or null while there is no final answer. */
    internal val statusRule: String?
        get() = when (status) {
            UploadCheckStatus.ACCEPTED -> "accepted"
            UploadCheckStatus.REFUSED -> "refused"
            else -> null
        }

    companion object {
        const val LENGTH = 34

        /** The library's `helmetRefusal` codes (`SWR-UP-02`) for a refusal the helmet gives for its tier or hardware version. */
        const val REFUSAL_TIER = "tier"
        const val REFUSAL_HARDWARE_VERSION = "hardware_version"

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
    /**
     * The helmet's refusal reason (`np_hub_status_t` as a positive number) when it refused, else null. The library needs only
     * the kind ([helmetRefusal]); a screen needs the reason to say whether to try again
     * ([helmetNotReady]) or not.
     */
    val helmetReason: Int? = null,
) {
    /**
     * True when the helmet refused only because it had not yet heard its own tier or hardware version from the safety MCU
     * (`NP_HUB_ERR_TIER_UNVERIFIED`, `NP_HUB_ERR_HW_VERSION_UNVERIFIED`): it checked nothing, and sending the same protocol
     * again can succeed. The other refusals will not change on a second try. The core decides (`helmetCause`).
     */
    val helmetNotReady: Boolean
        get() = NPLibraryRules.helmetCause(helmetStatus, helmetReason, null, null).notReady

    /**
     * Why the helmet refused, for a screen to say so in words; null when it did not refuse. The reasons are the
     * `np_hub_status_t` values `np_runner_load()` can return, grouped by what the person can do about them. The core decides
     * (`helmetCause` in `common/npps-core/src/library.rs`), so every app groups them the same way.
     */
    val refusalCause: HelmetRefusalCause?
        get() = HelmetRefusalCause.fromRule(NPLibraryRules.helmetCause(helmetStatus, helmetReason, null, null).cause)

    /**
     * What a screen should say about the upload whose bytes hash to [sentHash]: why the helmet refused it, or
     * [HelmetRefusalCause.UNMATCHED] when the helmet's final answer is for other bytes (it holds something else, or answered an
     * earlier upload), so nothing is known about this one. Null for an accepted upload and for no answer at all.
     */
    fun causeFor(sentHash: String): HelmetRefusalCause? =
        HelmetRefusalCause.fromRule(NPLibraryRules.helmetCause(helmetStatus, helmetReason, helmetHash, sentHash).cause)

    /** The helmet refused for its tier (`NP_HUB_ERR_TIER_F4` or `_UNVERIFIED`). */
    val refusedForTier: Boolean
        get() = helmetRefusal == UploadCheck.REFUSAL_TIER

    /** The helmet refused for its hardware version (`NP_HUB_ERR_HW_VERSION_MISMATCH` or `_UNVERIFIED`). */
    val refusedForHardwareVersion: Boolean
        get() = helmetRefusal == UploadCheck.REFUSAL_HARDWARE_VERSION

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
                helmetReason = if (check.status == UploadCheckStatus.REFUSED) check.reason else null,
            )
        }
    }
}

/**
 * What a screen tells the person when the helmet refuses a protocol (`UploadVerdict.refusalCause`). Each is a different thing
 * to do next: try again, send the protocol again from a reconnected app, update, wait for the session to end, restart the
 * helmet, or nothing the person can do. A tier refusal (F4) never names a purchase (`NP-PWRSRC-001` §6.3).
 */
enum class HelmetRefusalCause(val rule: String) {
    TIER("tier"),
    HARDWARE_VERSION("hardware_version"),
    NOT_READY("not_ready"),
    SIGNATURE("signature"),
    WRONG_DEVICE("wrong_device"),
    FORMAT_VERSION("format_version"),
    DAMAGED("damaged"),
    MALFORMED("malformed"),
    SESSION_ACTIVE("session_active"),
    BOOT_REFUSED("boot_refused"),
    OTHER("other"),

    /** Not a refusal: the helmet's final answer is for other bytes than the ones just sent (`UploadVerdict.causeFor`). */
    UNMATCHED("unmatched"),
    ;

    companion object {
        /** The cause the core named (`helmetCause`), or null for none. */
        fun fromRule(rule: String?): HelmetRefusalCause? = rule?.let { r -> entries.first { it.rule == r } }
    }
}
