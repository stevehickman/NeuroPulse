package com.neuropulse.core.session

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** Port of the iOS AdaptTrigger/AdaptationEvent assertions. */
class AdaptationEventTests {

    @Test
    fun allSeventeenTriggersHavePlainLanguageCopy() {
        assertEquals(17, AdaptTrigger.entries.size)
        for (t in AdaptTrigger.entries) {
            assertTrue(t.plainLanguageDescription.isNotBlank(), "${t.name} must have plain-language copy")
        }
    }

    @Test
    fun rawValuesAreContiguousZeroToSixteen() {
        assertEquals((0x00..0x10).toList(), AdaptTrigger.entries.map { it.rawValue })
    }

    @Test
    fun fromRawValueRoundTrips() {
        assertEquals(AdaptTrigger.HRV_COHERENCE_LOW, AdaptTrigger.from(0x08))
        assertEquals(AdaptTrigger.SESSION_END_RAMP, AdaptTrigger.from(0x10))
        assertNull(AdaptTrigger.from(0x11))
    }

    @Test
    fun eventIdIsStableAndContentDerived() {
        val e = AdaptationEvent(
            sessionOffsetMs = 12_000, trigger = AdaptTrigger.EEG_ALPHA_LOW,
            modType = 3, paramId = 1, valueBeforeX100 = 1000, valueAfterX100 = 1200, confidencePct = 80,
        )
        assertEquals("12000-0-3-1", e.id)
        assertEquals(12.0, e.sessionOffsetSeconds)
        // Same content → same id (SwiftUI/Compose diffing correctness).
        val again = e.copy()
        assertEquals(e.id, again.id)
        assertNotNull(AdaptTrigger.from(e.trigger.rawValue))
    }
}
