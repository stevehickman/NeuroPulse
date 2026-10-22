package life.neurone.core.consent

import life.neurone.core.models.ClinicianUseCaseTier
import life.neurone.core.models.UHDRElement
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** Ports of iOS `ConsentEngineTests` that had no Kotlin counterpart: minimum-necessary derivation and per-element tiers. */
class ConsentEngineTests {

    private fun elementsOf(id: String) = USE_CASE_LIBRARY.first { it.id == id }.requiredElements

    // @verifies REQ-CONSENT-15#a
    @Test
    fun minimumNecessaryIsTheExactUnionOfTheSelectedUseCases() {
        val union = ConsentEngine.minimumNecessaryElements(setOf("adherence_monitoring", "eeg_review"))
        assertEquals(elementsOf("adherence_monitoring") + elementsOf("eeg_review"), union)
        assertTrue(UHDRElement.EEG_WAVEFORMS in union)
        assertFalse(UHDRElement.HRV_TIME_SERIES in union, "no selected use case needs HRV")
    }

    // @verifies REQ-CONSENT-15#a
    @Test
    fun anEmptySelectionGrantsNothing() {
        assertTrue(ConsentEngine.minimumNecessaryElements(emptySet()).isEmpty())
    }

    // @verifies REQ-CONSENT-15#a
    @Test
    fun hrvOutcomesCarriesTheFullClinicalElementsAndInheritsTheLowerTiers() {
        val e = ConsentEngine.minimumNecessaryElements(setOf("hrv_outcomes"))
        listOf(
            UHDRElement.HRV_TIME_SERIES, UHDRElement.PPG_OPTICAL_SIGNAL, UHDRElement.CLOSED_LOOP_EVENTS,
            UHDRElement.OUTCOME_LOGS, UHDRElement.SESSION_TIMESTAMPS, UHDRElement.EEG_WAVEFORMS,
            UHDRElement.PBM_DOSE_LOGS,
        ).forEach { assertTrue(it in e, "$it") }
    }

    // @verifies REQ-CONSENT-15#a
    @Test
    fun selectingAllThreeUseCasesEqualsSelectingTheSupersetAlone() {
        assertEquals(
            ConsentEngine.minimumNecessaryElements(setOf("hrv_outcomes")),
            ConsentEngine.minimumNecessaryElements(setOf("adherence_monitoring", "eeg_review", "hrv_outcomes")),
        )
    }

    // @verifies REQ-CONSENT-15#a
    @Test
    fun noUseCaseIsEmptyAndNothingBeyondTheLibraryIsEverReturned() {
        USE_CASE_LIBRARY.forEach { assertTrue(it.requiredElements.isNotEmpty(), it.id) }
        val all = ConsentEngine.minimumNecessaryElements(USE_CASE_LIBRARY.map { it.id }.toSet())
        assertEquals(USE_CASE_LIBRARY.flatMap { it.requiredElements }.toSet(), all)
    }

    // @verifies REQ-CONSENT-15
    @Test
    fun eachTierExposesExactlyItsElementsAndEachIsASupersetOfTheLower() {
        val monitor = ClinicianUseCaseTier.MONITOR.uhdrElements
        val assess = ClinicianUseCaseTier.ASSESS.uhdrElements
        val full = ClinicianUseCaseTier.FULL_CLINICAL.uhdrElements
        assertEquals(
            setOf(UHDRElement.SESSION_TIMESTAMPS, UHDRElement.SESSION_DURATION, UHDRElement.PROTOCOL_PARAMETERS),
            monitor,
        )
        assertFalse(UHDRElement.EEG_WAVEFORMS in monitor)
        assertFalse(UHDRElement.HRV_TIME_SERIES in monitor)
        assertTrue(UHDRElement.EEG_WAVEFORMS in assess && assess.containsAll(monitor))
        assertFalse(UHDRElement.HRV_TIME_SERIES in assess)
        assertTrue(full.containsAll(assess) && UHDRElement.HRV_TIME_SERIES in full)
        assertEquals(UHDRElement.values().toSet(), full)
    }

    // @verifies REQ-CONSENT-15
    @Test
    fun minimumTierPerElementMatchesTheLadder() {
        listOf(UHDRElement.SESSION_TIMESTAMPS, UHDRElement.SESSION_DURATION, UHDRElement.PROTOCOL_PARAMETERS)
            .forEach { assertEquals(ClinicianUseCaseTier.MONITOR, it.minimumTier, "$it") }
        listOf(UHDRElement.EEG_WAVEFORMS, UHDRElement.NEUROFEEDBACK_SCORES, UHDRElement.PBM_DOSE_LOGS)
            .forEach { assertEquals(ClinicianUseCaseTier.ASSESS, it.minimumTier, "$it") }
        listOf(
            UHDRElement.HRV_TIME_SERIES, UHDRElement.PPG_OPTICAL_SIGNAL, UHDRElement.CLOSED_LOOP_EVENTS,
            UHDRElement.OUTCOME_LOGS, UHDRElement.EYE_STATE_LOGS,
        ).forEach { assertEquals(ClinicianUseCaseTier.FULL_CLINICAL, it.minimumTier, "$it") }
    }

    // @verifies REQ-CONSENT-15
    @Test
    fun noTierContainsAnElementBelowItsMinimumTier() {
        ClinicianUseCaseTier.values().filter { it != ClinicianUseCaseTier.RESEARCH }.forEach { tier ->
            tier.uhdrElements.forEach {
                assertTrue(it.minimumTier.rank <= tier.rank, "${tier.name} over-grants $it")
            }
        }
    }
}
