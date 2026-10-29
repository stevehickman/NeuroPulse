package life.neurone.core.cloudauth

import kotlinx.datetime.Instant
import kotlinx.datetime.LocalDate
import kotlinx.datetime.TimeZone
import kotlinx.datetime.atStartOfDayIn

/**
 * A shared access signature the person pasted, checked before it is kept (SWR-MG-26). It is the query string of a service SAS on the container: what
 * Azure shows after the `?` in "SAS token". The check is only that it can do what a backup needs, from the permission letters Azure documents
 * (read `r`, create `c`, write `w`, delete `d`, list `l`; Put Blob needs Create or Write, List Blobs needs List); everything else, the signature
 * included, is Azure's to judge, so a wrong one is found by the first request.
 *
 * Refused: nothing in it that is not part of a query string, a missing or repeated `sig`, a `sr` that is not a container (`c`), a `sp` without
 * read, delete, list and create-or-write, and an expiry (`se`) already past. An expiry that cannot be read is kept as unknown and left to Azure.
 */
class AzureSas private constructor(val query: String, val expiresAtEpochSeconds: Long) {
    companion object {
        private val allowed = Regex("[A-Za-z0-9._~%+=&:/,-]+")

        /** The checked signature, or null. A leading `?` is dropped and surrounding spaces are not part of it. */
        fun parse(input: String, nowEpochSeconds: Long): AzureSas? {
            val query = input.trim().removePrefix("?")
            if (!allowed.matches(query)) return null
            val pairs = query.split('&').map { it.substringBefore('=', "") to it.substringAfter('=', "") }
            if (pairs.any { it.first.isEmpty() }) return null
            val byName = pairs.groupBy({ it.first }, { it.second })
            if (byName["sig"]?.size != 1 || byName.getValue("sig").single().isEmpty()) return null
            byName["sr"]?.let { if (it.size != 1 || it.single() != "c") return null }
            val sp = byName["sp"]?.singleOrNull() ?: return null
            if ('r' !in sp || 'd' !in sp || 'l' !in sp || ('c' !in sp && 'w' !in sp)) return null
            // Null is "unknown", kept apart from a real expiry so that one at the epoch itself is still an expiry, and past.
            val expiry: Long? = byName["se"]?.singleOrNull()?.let { expiry(it) }
            if (expiry != null && expiry <= nowEpochSeconds) return null
            return AzureSas(query, expiry ?: 0L)
        }

        /** Azure writes `se` as `YYYY-MM-DD` or `YYYY-MM-DDThh:mm:ssZ` (percent-encoded in the query); anything else reads as unknown. */
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
