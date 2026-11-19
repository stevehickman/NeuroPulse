package life.neurone.core.cloudauth

import kotlinx.datetime.Instant
import kotlinx.datetime.LocalDate
import kotlinx.datetime.TimeZone
import kotlinx.datetime.atStartOfDayIn

/**
 * A shared access signature the person pasted, checked before it is kept (SWR-MG-26, SWR-MG-30). It is the query string of a **service SAS on one
 * container**: what Azure shows after the `?` in "SAS token". The check is that it can do what a backup needs and no more than that, from the fields
 * Microsoft's "Create a service SAS" page documents (read 2026-11-18); everything else, the signature included, is Azure's to judge, so a wrong one
 * is found by the first request.
 *
 * Refused:
 * - anything that is not part of a query string, and a missing, empty or repeated `sig`;
 * - a `sr` that is missing or is not a container (`c`). The page calls `sr` required for Blob Storage, and an **account** SAS has none, so a
 *   signature for the whole storage account passed while `sr` was only checked when present. One carrying an account SAS's own fields (`ss`,
 *   `srt`) is refused as well;
 * - a `spr` that is not exactly `https`. The page's default when it is omitted is `https,http`, which lets the signature be sent in the clear;
 * - a `sp` that lacks read, delete, list and create-or-write, repeats a letter, or holds any letter beyond `rcwdl` (add, delete version, tags,
 *   move, execute, ownership, permissions, immutability policy, permanent delete): the store calls nothing that needs them;
 * - a `se` that is missing, repeated, unreadable or already past. The page lets `se` be left out only when a stored access policy (`si`) holds
 *   it, and the expiry is then unknowable here, so such a signature is refused too: an expiry that cannot be read used to be kept as "never".
 *
 * **No maximum lifetime is set.** Microsoft's page recommends a short one and names no figure; a figure here would be a threshold with no
 * derivation (`CLAUDE.md` §19). The form tells the person to choose a short expiry, and [expiresAtEpochSeconds] is always a real time.
 */
class AzureSas private constructor(val query: String, val expiresAtEpochSeconds: Long) {
    companion object {
        private val allowed = Regex("[A-Za-z0-9._~%+=&:/,-]+")

        /** The permission letters the store's five calls need; a signature may hold these and no other. */
        private const val PERMITTED = "rcwdl"

        /** The checked signature, or null. A leading `?` is dropped and surrounding spaces are not part of it. */
        fun parse(input: String, nowEpochSeconds: Long): AzureSas? {
            val query = input.trim().removePrefix("?")
            if (!allowed.matches(query)) return null
            val pairs = query.split('&').map { it.substringBefore('=', "") to it.substringAfter('=', "") }
            if (pairs.any { it.first.isEmpty() }) return null
            val byName = pairs.groupBy({ it.first }, { it.second })
            if (byName["sig"]?.size != 1 || byName.getValue("sig").single().isEmpty()) return null
            if (byName["sr"]?.singleOrNull() != "c") return null
            if ("ss" in byName || "srt" in byName) return null
            if (byName["spr"]?.singleOrNull() != "https") return null
            val sp = byName["sp"]?.singleOrNull() ?: return null
            if (sp.any { it !in PERMITTED } || sp.toSet().size != sp.length) return null
            if ('r' !in sp || 'd' !in sp || 'l' !in sp || ('c' !in sp && 'w' !in sp)) return null
            val expiry = byName["se"]?.singleOrNull()?.let { expiry(it) } ?: return null
            if (expiry <= nowEpochSeconds) return null
            return AzureSas(query, expiry)
        }

        /** Azure writes `se` as `YYYY-MM-DD` or `YYYY-MM-DDThh:mm:ssZ` (percent-encoded in the query); anything else cannot be read, and is refused. */
        private fun expiry(raw: String): Long? {
            val text = percentDecodeQuery(raw)
            return try {
                if ('T' in text) Instant.parse(text).epochSeconds else LocalDate.parse(text).atStartOfDayIn(TimeZone.UTC).epochSeconds
            } catch (e: IllegalArgumentException) {
                null
            }
        }

        private fun percentDecodeQuery(s: String): String = life.neurone.core.cloud.percentDecode(s)
    }
}
