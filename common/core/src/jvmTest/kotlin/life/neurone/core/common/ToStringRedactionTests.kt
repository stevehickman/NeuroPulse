package life.neurone.core.common

import kotlinx.datetime.LocalDate
import life.neurone.core.migration.PairingOffer
import life.neurone.core.session.AdaptTrigger
import life.neurone.core.session.AdaptationEvent
import life.neurone.core.session.CompletedSessionSummary
import life.neurone.core.session.SessionRecord
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * A value that holds a secret or UHDR does not print it (`SWR-MG-28` (f)). No host logs these today; a generated `toString`
 * is how one would start to without anyone deciding it, through a log line, an exception message or a string template.
 */
class ToStringRedactionTests {

    // @verifies SWR-MG-28#f
    @Test
    fun aPairingOfferPrintsItsLifeAndItsTriesAndNotItsCode() {
        val offer = PairingOffer(code = "48151623", expiresAt = 1_700_000_120, triesLeft = 3)
        val text = "$offer"
        assertFalse(text.contains("48151623"), text)
        assertEquals("PairingOffer(code=<redacted>, expiresAt=1700000120, triesLeft=3)", text)
        // The same through a collection and a copy, which format their elements with toString.
        assertFalse(listOf(offer, offer.copy(triesLeft = 2)).toString().contains("48151623"))
        // The code is still there to be shown and served, and equality still sees it.
        assertEquals("48151623", offer.code)
        assertTrue(offer != offer.copy(code = "00000000"))
    }

    // @verifies SWR-MG-28#f
    @Test
    fun aSessionRecordAndASummaryPrintNothingAboutThePerson() {
        val record = SessionRecord(
            id = "record-7f3a", protocolName = "Evening wind-down", sessionDay = "2026-03-09", insertionIndex = 4,
            durationSeconds = 1234.0, averageCoherenceScore = 6.25f, rmssdMilliseconds = 47, impedancePassCount = 7, edfSessionId = 1_741_500_000_123,
        )
        val summary = CompletedSessionSummary.fromRecord(record)
        for (text in listOf(record.toString(), summary.toString(), listOf(record).toString(), "failed for $summary")) {
            for (leak in listOf("Evening", "2026-03-09", "6.25", "47", "1234", "1741500000123", "record-7f3a")) {
                assertFalse(text.contains(leak), "'$leak' is printed: $text")
            }
        }
        assertEquals("SessionRecord(<redacted>)", record.toString())
        assertEquals("CompletedSessionSummary(<redacted>)", summary.toString())
        // What the screens read is unchanged.
        assertEquals(47, summary.rmssdMilliseconds)
        assertEquals(LocalDate(2026, 3, 9), summary.completedAt)
    }

    // @verifies SWR-MG-28#f
    @Test
    fun anAdaptationEventPrintsNeitherItsTriggerNorItsValues() {
        val event = AdaptationEvent(
            sessionOffsetMs = 61_000, trigger = AdaptTrigger.HRV_COHERENCE_LOW, modType = 5, paramId = 2,
            valueBeforeX100 = 4321, valueAfterX100 = 5678, confidencePct = 91,
        )
        val summary = CompletedSessionSummary(
            protocolName = "p", completedAt = LocalDate(2026, 3, 9), durationSeconds = 1, averageCoherenceScore = null,
            rmssdMilliseconds = null, impedancePassCount = 0, adaptationEvents = listOf(event),
        )
        for (text in listOf(event.toString(), listOf(event).toString(), summary.toString())) {
            for (leak in listOf("HRV", "4321", "5678", "61000", "91")) {
                assertFalse(text.contains(leak), "'$leak' is printed: $text")
            }
        }
        assertEquals("AdaptationEvent(<redacted>)", event.toString())
        assertEquals(4321, event.valueBeforeX100)
    }
}
