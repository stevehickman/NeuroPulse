package life.neurone.core.session

import kotlinx.datetime.LocalDate
import life.neurone.core.migration.PairingOffer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * The text of an object that holds the pairing code or health values holds neither (SWR-MG-28 (e), OI-PRIV-12): what a
 * log line, a crash report or a failed assertion would print. A `data class` prints every field unless it says otherwise.
 */
class RedactedTextTests {

    // @verifies SWR-MG-28#e
    @Test
    fun aPairingOfferPrintsNoCode() {
        val offer = PairingOffer(code = "48151623", expiresAt = 1_700_000_120, triesLeft = 3)
        val text = offer.toString()
        assertFalse(text.contains("48151623"), text)
        assertFalse("$offer and ${listOf(offer)}".contains("48151623"), "a template or a list prints it the same way")
        assertTrue(text.contains("triesLeft=3"), "what is not secret is still there to read: $text")
        // The code is still the object's, for the screen and the core: only the text is redacted.
        assertEquals("48151623", offer.code)
        assertEquals(offer, offer.copy())
    }

    // @verifies SWR-MG-28#e
    @Test
    fun aSessionRecordAndASummaryPrintNoProtocolDayOrHrvValue() {
        val record = SessionRecord(
            id = "rec-1", protocolName = "Evening Wind-Down", sessionDay = "2026-11-15", insertionIndex = 7,
            durationSeconds = 1234.0, averageCoherenceScore = 6.25f, rmssdMilliseconds = 4711, impedancePassCount = 8,
            edfSessionId = 1_763_164_800_123,
        )
        val summary = CompletedSessionSummary.fromRecord(record).copy(
            adaptationEvents = listOf(event()),
        )
        for (text in listOf(record.toString(), summary.toString(), "${listOf(record)}")) {
            for (held in listOf("Evening Wind-Down", "2026-11-15", "1234", "6.25", "4711", "1763164800123", "97531")) {
                assertFalse(text.contains(held), "'$held' is in: $text")
            }
        }
        assertEquals(4711, record.rmssdMilliseconds)
        assertEquals(LocalDate(2026, 11, 15), summary.completedAt)
    }

    // @verifies SWR-MG-28#e
    @Test
    fun anAdaptationEventPrintsNoTriggerOffsetOrValue() {
        val text = event().toString()
        for (held in listOf("HRV", "97531", "8642", "1357", "86")) {
            assertFalse(text.contains(held), "'$held' is in: $text")
        }
        assertEquals(event(), event(), "equality is still by content")
        assertEquals("97531-8-3-1", event().id)
    }

    private fun event() = AdaptationEvent(
        sessionOffsetMs = 97_531, trigger = AdaptTrigger.HRV_COHERENCE_LOW,
        modType = 3, paramId = 1, valueBeforeX100 = 8642, valueAfterX100 = 1357, confidencePct = 86,
    )
}
