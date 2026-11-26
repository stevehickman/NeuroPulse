package life.neurone.core.analytics

import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.protocol.PersistedKeys
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * The warranty owner's two prompts for general SHDR consent (`REQ-SHDR-12`; `OI-NVRAM-26`, `OI-NVRAM-28`; the principal,
 * 2026-11-26). The window-end signal from the hub and the helmet's enforcement of the content level are not built; these tests
 * drive the app's state and decisions directly.
 */
class GeneralShdrConsentPromptsTests {

    private class Rig(
        sets: List<GeneralShdrContentSet> = GeneralShdrContent.history,
        val kv: KeyValueStore = InMemoryKeyValueStore(),
    ) {
        val staged = mutableMapOf<String, ByteArray>()
        val records = WarrantyOwnerStore(kv, staging = object : ShdrStaging {
            override fun read(helmetKey: String) = staged[helmetKey]
            override fun stage(helmetKey: String, record: ByteArray) { staged[helmetKey] = record }
            override fun clear(helmetKey: String) { staged.remove(helmetKey) }
        })
        val gate = WarrantyAnalyticsGate(records)
        val char = CharacterisationEnrolmentGate(records, gate)
        val prompts = GeneralShdrConsentPrompts(records, gate, sets)

        /** A helmet whose owner turned reports on, enrolled it, and whose window has now ended. */
        fun windowEnded(hub: String = HUB) {
            assertTrue(gate.grant(hub, 0))
            assertNotNull(char.grant(hub))
            assertTrue(char.endWindow(hub))
        }
    }

    // @verifies REQ-SHDR-12#a
    @Test
    fun theWindowsEndMakesPromptOneDueShowingTheBaseline() {
        val r = Rig()
        r.gate.grant(HUB)
        r.char.grant(HUB)
        assertNull(r.prompts.due(HUB), "nothing is due while the window is open")
        assertTrue(r.char.endWindow(HUB))
        val p = assertNotNull(r.prompts.due(HUB))
        assertEquals(GeneralConsentPromptKind.WINDOW_ENDED, p.kind)
        assertEquals(GeneralShdrContent.BASELINE, p.shown, "until the review gate rules, general consent covers the baseline")
        assertTrue(p.added.isEmpty())
        assertTrue(p.reportsOn)
        assertNull(r.prompts.due(OTHER), "the question is this helmet's owner's only")
        assertNull(r.prompts.due(null), "with no helmet named nothing is due")
    }

    // Until the gate rules, the app carries the baseline and nothing else.
    // @verifies REQ-SHDR-12#a
    @Test
    fun theShippedHistoryIsTheBaselineAlone() {
        assertEquals(listOf(GeneralShdrContent.BASELINE), GeneralShdrContent.history)
        assertTrue(GeneralShdrContent.isWellFormed(GeneralShdrContent.history))
        assertEquals(0, GeneralShdrContent.shipped.version)
    }

    // @verifies REQ-SHDR-12#a
    @Test
    fun onlyAnEnrolmentInTheCurrentProgrammeCanEnd() {
        val r = Rig()
        assertFalse(r.char.endWindow(HUB), "a helmet never asked has no window")
        r.gate.grant(HUB)
        assertFalse(r.char.endWindow(HUB), "a helmet never enrolled has no window")
        r.char.grant(HUB)
        r.char.withdraw(HUB)
        assertFalse(r.char.endWindow(HUB), "a withdrawn enrolment has no window left to end")
        assertNull(r.prompts.due(HUB))
    }

    // @verifies REQ-SHDR-12#b
    @Test
    fun aContentSetTheGateFixedMakesPromptTwoDueNamingWhatIsAdded() {
        val r = Rig(GATED)
        r.gate.grant(HUB, 0) // an owner who said yes to the baseline, before this build
        val p = assertNotNull(r.prompts.due(HUB))
        assertEquals(GeneralConsentPromptKind.CONTENT_SET, p.kind)
        assertEquals(GATED.last(), p.shown)
        assertEquals(listOf("emitter_on_seconds", "throttle_count"), p.added)
        assertNull(Rig(GeneralShdrContent.history).prompts.due(HUB), "the baseline alone is no gate result: nothing is due")
    }

    // The notice is the same for every owner, so it is due for an owner who said stop as well as one who said yes; an owner never
    // asked is asked when they first turn reports on, and that grant answers it.
    // @verifies REQ-SHDR-12#b
    @Test
    fun promptTwoIsDueForEveryOwnerWhoAnsweredAndAGrantAtTheShownSetAnswersIt() {
        val r = Rig(GATED)
        r.gate.grant(HUB, 0)
        r.gate.revoke(HUB)
        val p = assertNotNull(r.prompts.due(HUB), "an owner who said stop is asked too")
        assertFalse(p.reportsOn)
        assertEquals(GATED.last().fields, p.added, "with reports off, everything beyond the baseline is new")
        assertNull(r.prompts.due(OTHER), "never asked: no prompt")
        assertTrue(r.gate.grant(OTHER, GATED.last().version))
        assertNull(r.prompts.due(OTHER), "the first grant was made for this set, so the question is answered")
    }

    // @verifies REQ-SHDR-12#b
    @Test
    fun promptTwoShowsOncePerOwnerAndHelmet() {
        val kv = InMemoryKeyValueStore()
        val r = Rig(GATED, kv)
        r.gate.grant(HUB, 0)
        r.gate.grant(OTHER, 0)
        val p = assertNotNull(r.prompts.due(HUB))
        assertTrue(r.prompts.markShown(HUB, p))
        assertNull(r.prompts.due(HUB), "shown once")
        assertNull(Rig(GATED, kv).prompts.due(HUB), "and not again after a restart")
        assertNotNull(r.prompts.due(OTHER), "another helmet's owner is asked for their own helmet")
        // A later set the gate fixes is a new question, naming only what it adds.
        val later = GATED + GeneralShdrContentSet(2, listOf("emitter_on_seconds", "throttle_count", "peak_ntc_celsius"))
        val again = assertNotNull(Rig(later, kv).prompts.due(HUB))
        assertEquals(listOf("emitter_on_seconds", "throttle_count", "peak_ntc_celsius"), again.added, "nothing beyond the baseline was agreed")
        // A change of custody forgets the helmet: its new owner has never been asked and is asked at their first grant.
        r.gate.endCustody(HUB)
        assertNull(r.prompts.due(HUB))
    }

    // @verifies REQ-SHDR-12#c
    @Test
    fun silenceKeepsTheConsentExactlyAtItsContentLevel() {
        for (answer in listOf(null, GeneralConsentAnswer.KEEP)) {
            val r = Rig(GATED)
            r.gate.grant(HUB, 0)
            val p = assertNotNull(r.prompts.due(HUB))
            assertTrue(r.prompts.answer(HUB, p, answer))
            assertTrue(r.gate.isOpen(HUB), "$answer: nothing is revoked")
            assertEquals(0, r.gate.contentLevel(HUB), "$answer: nothing is widened")
            // And an owner who said stop stays stopped.
            r.gate.grant(OTHER, 0)
            r.gate.revoke(OTHER)
            val q = assertNotNull(r.prompts.due(OTHER))
            assertTrue(r.prompts.answer(OTHER, q, answer))
            assertFalse(r.gate.isOpen(OTHER), "$answer: nothing is granted")
            assertTrue(r.gate.isDecided(OTHER))
        }
        // The same at prompt 1.
        val w = Rig()
        w.windowEnded()
        val p = assertNotNull(w.prompts.due(HUB))
        assertTrue(w.prompts.markShown(HUB, p))
        assertTrue(w.gate.isOpen(HUB))
        assertEquals(0, w.gate.contentLevel(HUB))
        assertNull(w.prompts.due(HUB), "prompt 1 is put once")
    }

    // @verifies REQ-SHDR-12#c
    @Test
    fun anAnswerSetsWhatTheOwnerChose() {
        val r = Rig(GATED)
        r.gate.grant(HUB, 0)
        r.records.stage(HUB, byteArrayOf(1))
        val p = assertNotNull(r.prompts.due(HUB))
        assertTrue(r.prompts.answer(HUB, p, GeneralConsentAnswer.STOP))
        assertFalse(r.gate.isOpen(HUB))
        assertTrue(r.gate.isDecided(HUB), "a stop is a decision, not never asked")
        assertNull(r.records.staged(HUB), "a record staged under the stopped consent is not sent")
        r.gate.grant(OTHER, 0)
        val q = assertNotNull(r.prompts.due(OTHER))
        assertTrue(r.prompts.answer(OTHER, q, GeneralConsentAnswer.AGREE))
        assertEquals(1, r.gate.contentLevel(OTHER))
    }

    // @verifies REQ-SHDR-12#d
    @Test
    fun contentTheGateAddsIsNotCoveredByAGrantMadeForLess() {
        val r = Rig(GATED)
        r.gate.grant(HUB, 0)
        assertEquals(0, r.gate.contentLevel(HUB), "an update carrying a new set raises nothing by itself")
        assertEquals(0, WarrantyAnalyticsGate(WarrantyOwnerStore(r.kv)).contentLevel(HUB), "nor does a restart")
        val p = assertNotNull(r.prompts.due(HUB))
        r.prompts.markShown(HUB, p)
        assertEquals(0, r.gate.contentLevel(HUB), "nor does showing the question")
        assertTrue(r.prompts.answer(HUB, p, GeneralConsentAnswer.AGREE))
        assertEquals(1, r.gate.contentLevel(HUB), "only the owner's yes to the set shown")
        r.gate.revoke(HUB)
        assertNull(r.gate.contentLevel(HUB), "with reports off nothing is covered")
    }

    // @verifies REQ-SHDR-12#e
    @Test
    fun theEndOfTheWindowIsItsOwnState() {
        val r = Rig()
        r.windowEnded()
        val ended = assertNotNull(r.records.record(HUB))
        assertTrue(ended.windowEnded)
        assertFalse(ended.enrolled, "the opt-in turned from yes to no")
        assertTrue(r.gate.isOpen(HUB), "general consent is left as it was: the window's end is not a revoke")
        assertTrue(r.gate.isDecided(HUB), "nor a return to never asked")
        assertTrue(r.char.windowEnded(HUB))
        // A withdrawal by the owner is not a window's end, and a revoke is neither.
        r.gate.grant(OTHER)
        r.char.grant(OTHER)
        r.char.withdraw(OTHER)
        assertFalse(r.char.windowEnded(OTHER))
        assertNull(r.prompts.due(OTHER))
        assertNull(r.records.record("never-asked"))
        // The state survives a restart.
        assertTrue(CharacterisationEnrolmentGate(WarrantyOwnerStore(r.kv), r.gate).windowEnded(HUB))
    }

    // A record made by another of the owner's choices does not turn "never asked" into "said stop".
    // @verifies REQ-SHDR-12#e
    // @verifies REQ-SHDR-08#c
    @Test
    fun aRecordMadeByAnotherChoiceIsStillNeverAsked() {
        val r = Rig(GATED)
        r.records.update(HUB) { it.copy(stealthForbidden = true) }
        r.records.setOffered(OTHER, "fam", offered = false)
        assertFalse(r.gate.isDecided(HUB))
        assertFalse(r.gate.isDecided(OTHER))
        assertNull(r.prompts.due(HUB), "never asked: no prompt 2")
        r.gate.revoke(HUB)
        assertTrue(r.gate.isDecided(HUB), "said stop")
    }

    // @verifies REQ-SHDR-12#f
    @Test
    fun noPromptReEnrolsCharacterisation() {
        for (answer in listOf(null, GeneralConsentAnswer.AGREE, GeneralConsentAnswer.KEEP, GeneralConsentAnswer.STOP)) {
            val r = Rig()
            r.windowEnded()
            val p = assertNotNull(r.prompts.due(HUB))
            assertTrue(r.prompts.answer(HUB, p, answer))
            assertFalse(r.char.isOpen(HUB), "$answer")
            assertFalse(r.records.record(HUB)!!.enrolled, "$answer")
            assertNull(r.char.grant(HUB), "$answer: the programme is over for this helmet")
            r.gate.grant(HUB)
            assertFalse(r.char.isOpen(HUB), "$answer: a later grant does not enrol it either")
        }
    }

    // @verifies REQ-SHDR-12#g
    @Test
    fun thePromptsAreTheWarrantyOwnersAloneAndTouchNoOtherKey() {
        val written = mutableSetOf<String>()
        val inner = InMemoryKeyValueStore()
        val kv = object : KeyValueStore by inner {
            override fun putString(key: String, value: String) { written += key; inner.putString(key, value) }
            override fun putInt(key: String, value: Int) { written += key; inner.putInt(key, value) }
            override fun putBoolean(key: String, value: Boolean) { written += key; inner.putBoolean(key, value) }
        }
        val r = Rig(GATED, kv)
        r.windowEnded()
        r.prompts.answer(HUB, r.prompts.due(HUB)!!, GeneralConsentAnswer.AGREE)
        r.gate.grant(OTHER, 0)
        r.prompts.answer(OTHER, r.prompts.due(OTHER)!!, GeneralConsentAnswer.STOP)
        assertEquals(setOf(WarrantyOwnerStore.RECORDS_KEY), written, "the owner's one value, and no user's or research key")
        assertNull(kv.getString(PersistedKeys.RESEARCH_ANALYTICS_KEY))
        // The prompt logic is built on the owner's store and gate alone: nothing of a user's consent is reachable from it.
        val types = GeneralShdrConsentPrompts::class.java.declaredFields.map { it.type.simpleName }
        assertFalse(types.any { it == "ConsentStore" || it == "ResearchAnalyticsGate" }, "$types")
    }

    // With both due, prompt 1 is asked about the set in force, and its answer answers prompt 2 for it.
    // @verifies REQ-SHDR-12#a
    // @verifies REQ-SHDR-12#b
    @Test
    fun withBothDueTheOwnerIsAskedAboutOneContentOnce() {
        val r = Rig(GATED)
        r.windowEnded()
        val p = assertNotNull(r.prompts.due(HUB))
        assertEquals(GeneralConsentPromptKind.WINDOW_ENDED, p.kind)
        assertEquals(GATED.last(), p.shown)
        assertEquals(GATED.last().fields, p.added, "the window's end shows what general consent covers now, additions named")
        assertTrue(r.prompts.markShown(HUB, p))
        assertNull(r.prompts.due(HUB), "prompt 2 for the same set is not asked again")
        assertEquals(0, r.gate.contentLevel(HUB))
    }

    // A content history that is not well formed is never shown and changes nothing.
    // @verifies REQ-SHDR-12#d
    @Test
    fun aMalformedHistoryMakesNoPromptDueAndChangesNothing() {
        val bad = listOf(
            listOf(GeneralShdrContentSet(1, listOf("a"))),
            listOf(GeneralShdrContent.BASELINE, GeneralShdrContentSet(2, listOf("a"))),
            listOf(GeneralShdrContentSet(0, listOf("a"))),
            listOf(GeneralShdrContent.BASELINE, GeneralShdrContentSet(1, listOf("a", "a"))),
            listOf(GeneralShdrContent.BASELINE, GeneralShdrContentSet(1, listOf("Not An Id"))),
            emptyList(),
        )
        for (sets in bad) {
            assertFalse(GeneralShdrContent.isWellFormed(sets), "$sets")
            val r = Rig(sets)
            r.gate.grant(HUB, 0)
            assertNull(r.prompts.due(HUB), "$sets")
            val forged = GeneralConsentPrompt(GeneralConsentPromptKind.CONTENT_SET, sets.lastOrNull() ?: GeneralShdrContent.BASELINE, emptyList(), true)
            assertFalse(r.prompts.answer(HUB, forged, GeneralConsentAnswer.AGREE), "$sets")
            assertEquals(0, r.gate.contentLevel(HUB), "$sets")
        }
    }

    // A record written before the prompts (v3) reads as decided, at the baseline, with no window ended and nothing asked.
    // @verifies REQ-SHDR-10#a
    @Test
    fun aRecordWrittenBeforeThePromptsStillReads() {
        val kv = InMemoryKeyValueStore()
        val r = Rig(GATED, kv)
        r.gate.grant(HUB, 0)
        val value = kv.getString(WarrantyOwnerStore.RECORDS_KEY)!!
        kv.putString(WarrantyOwnerStore.RECORDS_KEY, value.replace("v4,1,0,0,0,0,,1,0,0,0,0", "v3,0,0,0,0,0,"))
        val record = assertNotNull(WarrantyOwnerStore(kv).record(HUB))
        assertTrue(record.fleetDecided, "a v3 record was made by a decision, as before")
        assertFalse(record.fleetConsent)
        assertEquals(0, record.contentLevel)
        assertFalse(record.windowEnded)
        assertEquals(0, record.contentAsked)
    }

    private companion object {
        const val HUB = "AA:BB:CC:00:00:01"
        const val OTHER = "AA:BB:CC:00:00:02"

        /** A history as an update carrying a gate result might ship it (the ids are the §7.2 / Map 3 names, for the test only). */
        val GATED = listOf(GeneralShdrContent.BASELINE, GeneralShdrContentSet(1, listOf("emitter_on_seconds", "throttle_count")))
    }
}
