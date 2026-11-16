package life.neurone.core.cloudauth

import kotlinx.datetime.LocalDate
import kotlinx.datetime.TimeZone
import kotlinx.datetime.atStartOfDayIn

/**
 * A shared access signature the person pasted, checked before it is kept (SWR-MG-26). It is the query string of a service SAS on the container: what
 * Azure shows after the `?` in "SAS token". It must be able to do what a backup needs and nothing wider, from the fields Azure documents
 * (Create a service SAS, Create an account SAS and Formatting DateTime values, read 2026-11-15); the signature itself is Azure's to judge, so a
 * wrong one is found by the first request.
 *
 * Refused:
 * - anything that is not part of a query string, and a missing or repeated `sig`;
 * - a `sr` that is absent, repeated or not a container (`c`). **An account SAS has no `sr`** (it has `ss` and `srt`) and reaches every container
 *   of the storage account, so a signature with no `sr`, or with `ss` or `srt`, is refused;
 * - a `spr` that is not exactly `https`: Azure's default when it is left out is `https,http`, so the signature would also be honoured over plain http;
 * - a `sp` without read, delete, list and create-or-write (read `r`, create `c`, write `w`, delete `d`, list `l`; Put Blob needs Create or Write,
 *   List Blobs needs List), **or with any letter beyond those five**;
 * - a `se` that is absent, repeated, not one of Azure's three forms, or already past. A signature with no expiry in it never lapses on this side,
 *   so "unknown" is not kept. (One whose expiry and permissions are in a stored access policy, `si`, carries neither and is refused for that.)
 *
 * **No longest lifetime is set.** A limit would be a figure with no derivation; the expiry is kept and shown to the host as
 * [CloudAccount.credentialExpiresAtEpochSeconds] so a screen can say when the signature lapses.
 */
class AzureSas private constructor(val query: String, val expiresAtEpochSeconds: Long) {
    companion object {
        private val allowed = Regex("[A-Za-z0-9._~%+=&:/,-]+")

        /** `YYYY-MM-DD`, `YYYY-MM-DDThh:mm` or `YYYY-MM-DDThh:mm:ss` with up to seven decimals, then nothing, `Z` or an offset (Formatting DateTime values). */
        private val expiryForm = Regex("""(\d{4}-\d{2}-\d{2})(?:T(\d{2}):(\d{2})(?::(\d{2})(?:\.\d{1,7})?)?(Z|[+-]\d{2}:\d{2})?)?""")

        /** The checked signature, or null. A leading `?` is dropped and surrounding spaces are not part of it. */
        fun parse(input: String, nowEpochSeconds: Long): AzureSas? {
            val query = input.trim().removePrefix("?")
            if (!allowed.matches(query)) return null
            val pairs = query.split('&').map { it.substringBefore('=', "") to it.substringAfter('=', "") }
            if (pairs.any { it.first.isEmpty() }) return null
            val byName = pairs.groupBy({ it.first }, { it.second })
            if (byName["sig"]?.size != 1 || byName.getValue("sig").single().isEmpty()) return null
            if (byName["sr"]?.singleOrNull() != "c" || "ss" in byName || "srt" in byName) return null
            if (byName["spr"]?.singleOrNull() != "https") return null
            val sp = byName["sp"]?.singleOrNull() ?: return null
            if ('r' !in sp || 'd' !in sp || 'l' !in sp || ('c' !in sp && 'w' !in sp) || sp.any { it !in "rcwdl" }) return null
            val expiry = byName["se"]?.singleOrNull()?.let { expiry(it) } ?: return null
            if (expiry <= nowEpochSeconds) return null
            return AzureSas(query, expiry)
        }

        /** Azure's three forms of `se` (percent-encoded in the query), in UTC unless an offset is written; anything else is null. A fraction of a second is dropped. */
        private fun expiry(raw: String): Long? {
            val m = expiryForm.matchEntire(percentDecodeQuery(raw)) ?: return null
            val (date, hh, mm, ss, zone) = m.destructured
            return try {
                val day = LocalDate.parse(date).atStartOfDayIn(TimeZone.UTC).epochSeconds
                fun n(text: String, below: Int): Long = (if (text.isEmpty()) 0 else text.toInt()).also { require(it < below) }.toLong()
                val offset = if (zone.isEmpty() || zone == "Z") 0L else (n(zone.substring(1, 3), 24) * 3600 + n(zone.substring(4, 6), 60) * 60) * (if (zone[0] == '-') -1 else 1)
                day + n(hh, 24) * 3600 + n(mm, 60) * 60 + n(ss, 60) - offset
            } catch (e: IllegalArgumentException) {
                null
            }
        }

        private fun percentDecodeQuery(s: String): String = life.neurone.core.cloud.percentDecode(s)
    }
}
