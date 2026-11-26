package life.neurone.core.analytics

import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.protocol.PersistedKeys
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

private class SpyAnalyticsBackend : AnalyticsBackend {
    val calls = mutableListOf<String>()
    val trackedEvents = mutableListOf<Pair<String, Map<String, String>>>()

    override fun configure() { calls += "configure" }
    override fun reset() { calls += "reset" }
    override fun track(event: String, properties: Map<String, String>) {
        calls += "track"
        trackedEvents += event to properties
    }
}

class AnalyticsGateTests {

    // ── ResearchAnalyticsGate (ISC-19, 20, 21) ───────────────────────────

    // @verifies REQ-CONSENT-14
    @Test
    fun configureNoOpsWhenConsentKeyUnset() {
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(InMemoryKeyValueStore(), backend)
        gate.configure()
        assertTrue(backend.calls.isEmpty())
    }

    // @verifies REQ-SW03-07
    @Test
    fun configureRunsExactlyOnceWhenGateOpen() {
        val store = InMemoryKeyValueStore()
        store.putBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(store, backend)
        gate.configure()
        gate.configure()
        assertEquals(listOf("configure"), backend.calls)
    }

    private fun openGate(): Pair<ResearchAnalyticsGate, SpyAnalyticsBackend> {
        val store = InMemoryKeyValueStore()
        store.putBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
        val backend = SpyAnalyticsBackend()
        return ResearchAnalyticsGate(store, backend) to backend
    }

    // @verifies REQ-SW03-07
    @Test
    fun trackSendsAnEventWhoseNameAndPropertiesAreAllOnThePermittedList() {
        val (gate, backend) = openGate()
        val props = mapOf(
            "app_version" to "1.0.3", "os_version" to "Android 15.0", "device_class" to "Android_mid",
            "screen_name" to "protocol_detail", "engagement_tier" to "established", "app_start_type" to "warm_start",
            "onboarding_step" to "consent_flow_step_2", "error_code" to "1001", "connectivity_type" to "cellular",
        )
        gate.track("app_opened", props)
        gate.track("screen_viewed", emptyMap())
        assertEquals(listOf("app_opened" to props, "screen_viewed" to emptyMap()), backend.trackedEvents)
    }

    // @verifies REQ-SW03-07
    @Test
    fun trackDropsAnEventWithAPropertyTheTelemetryPolicyDoesNotName() {
        val (gate, backend) = openGate()
        // The twelve keys the old denylist knew, and ones it never thought of: an allowlist drops all of them.
        for (key in listOf(
            "eeg", "hrv", "rmssd", "coherence", "session_id", "protocol_id", "session_count", "session_sequence",
            "imp", "impedance", "impedance_flags", "pass_flags",
            "heart_rate", "alpha_power", "electrode_site", "condition", "email", "duration_s", "timestamp",
        )) {
            gate.track("app_opened", mapOf(key to "1", "engagement_tier" to "new"))
        }
        assertTrue(backend.trackedEvents.isEmpty())
        gate.track("app_opened", mapOf("engagement_tier" to "new"))
        assertEquals(1, backend.trackedEvents.size)
    }

    // @verifies REQ-SW03-07
    @Test
    fun trackDropsAnEventWithAValueNotOfItsPropertysFormOrANameThatIsNotOnTheList() {
        val (gate, backend) = openGate()
        gate.track("app_opened", mapOf("engagement_tier" to "42"))            // the raw count the tier replaced
        gate.track("app_opened", mapOf("screen_name" to "tdcs_depression"))   // a protocol or condition as a screen
        gate.track("app_opened", mapOf("error_code" to "1746000000000"))      // a timestamp in an error code
        gate.track("app_opened", mapOf("os_version" to "Android 15.0.1"))     // a patch version
        gate.track("tdcs_started", emptyMap())                                // a modality in the event name
        gate.track("User Email Sent", emptyMap())                             // not a listed name
        // OI-PRIV-05: a well-shaped name nobody listed, a session or consent screen, and a helmet fault slot as an error code.
        gate.track("stim_2ma_done", emptyMap())
        gate.track("evt", mapOf("engagement_tier" to "new"))
        for (screen in listOf("session_setup", "session_active", "session_results", "research_consent", "clinical_consent")) {
            gate.track("screen_viewed", mapOf("screen_name" to screen))
        }
        for (code in listOf("3", "42", "251", "255")) {
            gate.track("error_reported", mapOf("error_code" to code))
        }
        assertTrue(backend.trackedEvents.isEmpty())
    }

    // @verifies REQ-CONSENT-14
    @Test
    fun trackNoOpsWhenGateClosed() {
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(InMemoryKeyValueStore(), backend)
        gate.track("evt", mapOf("engagement_tier" to "new"))
        assertTrue(backend.trackedEvents.isEmpty())
    }

    // @verifies REQ-CONSENT-14
    @Test
    fun theGateIsClosedWhenTheConsentKeyIsAbsentOrFalseAndOpenOnceGranted() {
        val store = InMemoryKeyValueStore()
        val gate = ResearchAnalyticsGate(store, SpyAnalyticsBackend())
        assertFalse(gate.isOpen)
        store.putBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY, false)
        assertFalse(gate.isOpen)
        store.putBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
        assertTrue(gate.isOpen)
    }

    // @verifies REQ-CONSENT-14
    @Test
    fun onlyTheResearchConsentKeyOpensTheGateNotTheOnboardingKeys() {
        val store = InMemoryKeyValueStore()
        store.putBoolean("np.onboarding.consent-shown", true)
        store.putBoolean("np.onboarding.consent-accepted", true)
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(store, backend)
        assertFalse(gate.isOpen, "seeing or skipping the screen, or the retired key, is not research consent")
        gate.configure()
        assertTrue(backend.calls.isEmpty())
        assertEquals("np.research.consent-granted", PersistedKeys.RESEARCH_ANALYTICS_KEY)
    }

    // @verifies REQ-CONSENT-14
    @Test
    fun aClosedGateLeavesItsStateAndTheBackendUntouchedOnConfigure() {
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(InMemoryKeyValueStore(), backend)
        gate.configure()
        assertFalse(gate.isOpen)
        assertTrue(backend.calls.isEmpty())
    }

    // @verifies REQ-CONSENT-03
    @Test
    fun resetAllowsTheBackendToBeConfiguredAgainAfterwards() {
        val store = InMemoryKeyValueStore()
        store.putBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(store, backend)
        gate.configure()
        gate.reset()
        gate.configure()
        assertEquals(listOf("configure", "reset", "configure"), backend.calls)
    }

    // @verifies REQ-SW03-07
    @Test
    fun resetTearsDownBackendBeforeClearingConfiguredState() {
        val store = InMemoryKeyValueStore()
        store.putBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
        val backend = SpyAnalyticsBackend()
        val gate = ResearchAnalyticsGate(store, backend)
        gate.configure()
        gate.reset()
        assertEquals(listOf("configure", "reset"), backend.calls)
        // reset() when not configured still tells the backend to forget its ID: the ID outlives the process, so a withdrawal
        // made before configure() ran has one to erase (OI-PRIV-08 (1)). It never configures.
        gate.reset()
        assertEquals(listOf("configure", "reset", "reset"), backend.calls)
    }

    // ── Two consent subjects never share keys (ISC-24) ───────────────────

    // @verifies REQ-SW02-04
    @Test
    fun warrantyAndResearchGatesUseDistinctKeys() {
        assertTrue(
            WarrantyAnalyticsGate.WARRANTY_CONSENT_KEY !=
                PersistedKeys.RESEARCH_ANALYTICS_KEY,
        )
    }

    // @verifies REQ-CONSENT-01#b
    @Test
    fun warrantyGateIndependentOfResearchKey() {
        val store = InMemoryKeyValueStore()
        val warranty = WarrantyAnalyticsGate(WarrantyOwnerStore(store))
        warranty.grant(HELMET_A)
        assertTrue(warranty.isOpen(HELMET_A))
        // Clearing the research key has no effect on warranty consent.
        store.remove(PersistedKeys.RESEARCH_ANALYTICS_KEY)
        assertTrue(warranty.isOpen(HELMET_A))
    }
}

private const val HELMET_A = "AA:BB:CC:00:00:01"
private const val HELMET_B = "AA:BB:CC:00:00:02"

// ── WarrantyAnalyticsGate and the warranty owner's store (OI-PRIV-04) ─────
//
// As below, every refusal is asserted beside a control that succeeds.

class WarrantyOwnerConsentTests {

    private fun gate(store: InMemoryKeyValueStore = InMemoryKeyValueStore()) = WarrantyAnalyticsGate(WarrantyOwnerStore(store))

    // @verifies REQ-SHDR-08#a
    @Test
    fun consentForOneHelmetOpensNothingForAnother() {
        val warranty = gate()
        assertTrue(warranty.grant(HELMET_A))
        assertTrue(warranty.isOpen(HELMET_A))
        assertFalse(warranty.isOpen(HELMET_B), "a clinic's helmet met by a phone whose own helmet is consented")
        assertFalse(warranty.isDecided(HELMET_B), "the second helmet's owner has not been asked")
        assertTrue(warranty.isDecided(HELMET_A))
    }

    // @verifies REQ-SHDR-08#b
    @Test
    fun withNoHelmetNamedTheGateIsClosedAndAGrantIsRefused() {
        val store = InMemoryKeyValueStore()
        val warranty = gate(store)
        assertFalse(warranty.grant(null), "consent with no helmet would be consent for whichever connects next")
        assertFalse(warranty.isOpen(null))
        assertNull(store.getString(WarrantyOwnerStore.RECORDS_KEY), "nothing was recorded")
        warranty.revoke(null)
        assertNull(store.getString(WarrantyOwnerStore.RECORDS_KEY))
        // Control: the same gate records a grant once a helmet is named, and it does not open the gate for "no helmet".
        assertTrue(warranty.grant(HELMET_A))
        assertTrue(warranty.isOpen(HELMET_A))
        assertFalse(warranty.isOpen(null))
    }

    // @verifies REQ-SHDR-08#c
    @Test
    fun revokingOneHelmetLeavesTheOtherAsItWas() {
        val warranty = gate()
        warranty.grant(HELMET_A)
        warranty.grant(HELMET_B)
        warranty.revoke(HELMET_A)
        assertFalse(warranty.isOpen(HELMET_A))
        assertTrue(warranty.isOpen(HELMET_B))
        assertTrue(warranty.isDecided(HELMET_A), "a revoke is a decision, not a return to never asked")
    }

    // @verifies REQ-SHDR-08#d
    @Test
    fun theOneFlagAnEarlierVersionKeptOpensNothingAndIsRemoved() {
        val store = InMemoryKeyValueStore()
        store.putBoolean(PersistedKeys.WARRANTY_CONSENT_KEY, true)
        val warranty = gate(store)
        assertFalse(warranty.isOpen(HELMET_A), "the old consent cannot be attributed to a helmet, so the owner is asked again")
        assertFalse(warranty.isDecided(HELMET_A))
        assertFalse(store.getBoolean(PersistedKeys.WARRANTY_CONSENT_KEY), "the old flag is removed")
        // Even if the flag comes back (a run that could not write), nothing reads it.
        store.putBoolean(PersistedKeys.WARRANTY_CONSENT_KEY, true)
        assertFalse(warranty.isOpen(HELMET_A))
        // Control: asking again works.
        warranty.grant(HELMET_A)
        assertTrue(warranty.isOpen(HELMET_A))
    }

    // @verifies REQ-SHDR-10#a
    @Test
    fun theOwnersRecordsAreOneValueOfTheirOwnKeyedByASaltedHashAndNeverByTheIdentifier() {
        val store = InMemoryKeyValueStore()
        val warranty = gate(store)
        val enrolment = CharacterisationEnrolmentGate(WarrantyOwnerStore(store), warranty)
        warranty.grant(HELMET_A)
        enrolment.grant(HELMET_A)
        warranty.grant(HELMET_B)
        val value = store.getString(WarrantyOwnerStore.RECORDS_KEY)!!
        assertFalse(HELMET_A in value || HELMET_B in value, "the helmet's Bluetooth address is not stored")
        val lines = value.lines()
        assertEquals(3, lines.size, "a salt and one line per helmet")
        assertTrue(lines.drop(1).all { it.substringBefore('\t').matches(Regex("^[0-9a-f]{64}$")) })
        // Consent and enrolment are in this one value and under none of the keys an earlier version used.
        WarrantyOwnerStore.LEGACY_KEYS.forEach { assertNull(store.getString(it)); assertFalse(store.getBoolean(it)); assertEquals(0, store.getInt(it)) }
        // The device tokens are another value with another salt: the two tables do not share a line.
        assertNull(store.getString(FleetUploader.TOKEN_PREF))
        // The key is no user's or research key.
        assertTrue(WarrantyOwnerStore.RECORDS_KEY !in listOf(PersistedKeys.RESEARCH_ANALYTICS_KEY, FleetUploader.TOKEN_PREF))
    }

    // @verifies REQ-SHDR-10#a
    @Test
    fun aLineThatDoesNotReadAsARecordGrantsNothing() {
        val store = InMemoryKeyValueStore()
        val warranty = gate(store)
        warranty.grant(HELMET_A)
        assertTrue(warranty.isOpen(HELMET_A))
        val value = store.getString(WarrantyOwnerStore.RECORDS_KEY)!!
        for (bad in listOf(
            "v2,2,0,0,0,0", "v3,1,0,0,0,0", "v2,1,0,-1,0,0", "v2,1,0,0,0", "v2,1,0,0,0,2", "v1,1,0,0,0,0", "v1,2,0,0,0",
            "v1,1,0,-1,0", "v1,1,0,0", "true", "v2,1,0,0,0,0,", "v4,1,0,0,0,0,", "v3,1,0,0,0,0,zz", "v3,1,0,0,0,0,616",
            "v3,1,0,0,0,0,61.", "v3,1,0,0,0,0,6A",
            // v4 (OI-NVRAM-26): twelve fields, every flag 0 or 1, the two versions whole and not negative.
            "v4,1,0,0,0,0,,1,0,0,0", "v4,1,0,0,0,0,,2,0,0,0,0", "v4,1,0,0,0,0,,1,-1,0,0,0", "v4,1,0,0,0,0,,1,0,2,0,0",
            "v4,1,0,0,0,0,,1,0,0,x,0", "v4,1,0,0,0,0,,1,0,0,0,-1", "v4,1,0,0,0,0,,1,a,0,0,0", "v4,1,0,0,0,0,,1,0,0,0,0,",
        )) {
            store.putString(WarrantyOwnerStore.RECORDS_KEY, value.replace(WRITTEN, bad))
            assertFalse(warranty.isOpen(HELMET_A), bad)
        }
    }

    // The owner's stealth lock is per helmet, kept across a restart, and changes nothing else the owner decided.
    // @verifies SWR-IND-02#b
    @Test
    fun theStealthLockIsPerHelmetAndLeavesTheOtherChoicesAlone() {
        val store = InMemoryKeyValueStore()
        val warranty = gate(store)
        warranty.grant(HELMET_A)
        WarrantyOwnerStore(store).update(HELMET_A) { it.copy(stealthForbidden = true) }
        val reread = WarrantyOwnerStore(store)
        assertTrue(reread.record(HELMET_A)!!.stealthForbidden)
        assertTrue(reread.record(HELMET_A)!!.fleetConsent, "the lock does not touch the reports choice")
        assertFalse(reread.record(HELMET_B)?.stealthForbidden ?: false, "a lock on one helmet locks no other")
    }

    // A record written before the stealth lock existed (v1, five fields) still says what it said, and forbids no stealth.
    // @verifies REQ-SHDR-10#a
    @Test
    fun aRecordWrittenBeforeTheStealthLockStillReads() {
        val store = InMemoryKeyValueStore()
        val warranty = gate(store)
        warranty.grant(HELMET_A)
        val value = store.getString(WarrantyOwnerStore.RECORDS_KEY)!!
        assertTrue(WRITTEN in value)
        store.putString(WarrantyOwnerStore.RECORDS_KEY, value.replace(WRITTEN, "v1,1,0,0,0"))
        assertTrue(warranty.isOpen(HELMET_A))
        assertFalse(WarrantyOwnerStore(store).record(HELMET_A)!!.stealthForbidden)
    }

    // A record written before the owner's offered set moved into it (v2, six fields, OI-PRIV-60) still says what it said, and
    // withdraws no family.
    // @verifies REQ-SHDR-10#a
    @Test
    fun aRecordWrittenBeforeTheOfferedSetMovedHereStillReads() {
        val store = InMemoryKeyValueStore()
        val warranty = gate(store)
        warranty.grant(HELMET_A)
        val value = store.getString(WarrantyOwnerStore.RECORDS_KEY)!!
        store.putString(WarrantyOwnerStore.RECORDS_KEY, value.replace(WRITTEN, "v2,1,0,0,0,1"))
        assertTrue(warranty.isOpen(HELMET_A))
        val record = WarrantyOwnerStore(store).record(HELMET_A)!!
        assertTrue(record.stealthForbidden)
        assertTrue(record.withdrawnFamilies.isEmpty())
    }

    private companion object {
        /** The line this version writes for a helmet whose owner granted the reports and decided nothing else. */
        const val WRITTEN = "v4,1,0,0,0,0,,1,0,0,0,0"
    }
}

// ── CharacterisationEnrolmentGate (NP-FW-EMMC-002 §H) ────────────────────
//
// Every negative below is paired with the positive that proves the path exists.
// "No enrolment was recorded" is satisfied just as well by a gate that records
// nothing at all, so each refusal is asserted against a control that succeeds.

class CharacterisationEnrolmentGateTests {

    private fun gates(warrantyOpen: Boolean = true):
        Triple<InMemoryKeyValueStore, WarrantyAnalyticsGate, CharacterisationEnrolmentGate> {
        val store = InMemoryKeyValueStore()
        val records = WarrantyOwnerStore(store)
        val warranty = WarrantyAnalyticsGate(records)
        if (warrantyOpen) warranty.grant(HELMET_A)
        return Triple(store, warranty, CharacterisationEnrolmentGate(records, warranty))
    }

    // @verifies REQ-SW02-04
    @Test
    fun closedUntilAffirmativelyGranted() {
        val (_, _, char) = gates()
        assertFalse(char.isOpen(HELMET_A), "warranty consent alone must not enrol a device")
        assertEquals(0, char.consentEpoch(HELMET_A))

        // Control: the path works when the owner actually opts in.
        assertEquals(1, char.grant(HELMET_A))
        assertTrue(char.isOpen(HELMET_A))
    }

    // @verifies REQ-SW02-04
    @Test
    fun grantIsRefusedWithoutWarrantyConsent() {
        val (_, _, char) = gates(warrantyOpen = false)
        assertNull(char.grant(HELMET_A), "enrolling a device with no SHDR consent is incoherent")
        assertFalse(char.isOpen(HELMET_A))
    }

    // @verifies REQ-SHDR-09#a
    @Test
    fun enrolmentOfOneHelmetEnrolsNoOtherAndNeedsThatHelmetsWarrantyConsent() {
        val (_, warranty, char) = gates()
        assertEquals(1, char.grant(HELMET_A))
        assertFalse(char.isOpen(HELMET_B))
        assertNull(char.grant(HELMET_B), "helmet A's warranty consent does not let helmet B be enrolled")
        assertNull(char.grant(null))
        assertFalse(char.isOpen(null))
        // Even with its own warranty consent, B is not enrolled by A's enrolment.
        warranty.grant(HELMET_B)
        assertFalse(char.isOpen(HELMET_B))
        assertEquals(0, char.consentEpoch(HELMET_B))
        // Control: B enrols by its own opt-in, with its own epoch, and A is untouched.
        assertEquals(1, char.grant(HELMET_B))
        assertTrue(char.isOpen(HELMET_B) && char.isOpen(HELMET_A))
        char.withdraw(HELMET_B)
        assertTrue(char.isOpen(HELMET_A))
    }

    // @verifies REQ-SHDR-09#b
    @Test
    fun revokingWarrantyConsentWithdrawsTheEnrolmentAndARegrantDoesNotBringItBack() {
        val (_, warranty, char) = gates()
        char.grant(HELMET_A)
        assertTrue(char.isOpen(HELMET_A))
        warranty.revoke(HELMET_A)
        assertFalse(char.isOpen(HELMET_A), "the extended set must not outlive SHDR consent itself")

        // The re-grant may be a new owner's: it opens ordinary reports and nothing more.
        warranty.grant(HELMET_A)
        assertTrue(warranty.isOpen(HELMET_A))
        assertFalse(char.isOpen(HELMET_A), "no fresh opt-in was given")
        assertEquals(1, char.consentEpoch(HELMET_A), "the epoch is kept")

        // Control: a fresh opt-in enrols again, as a later grant.
        assertEquals(2, char.grant(HELMET_A))
        assertTrue(char.isOpen(HELMET_A))
    }

    // @verifies REQ-SHDR-09#b
    @Test
    fun revokingOneHelmetsWarrantyConsentLeavesAnotherHelmetsEnrolment() {
        val (_, warranty, char) = gates()
        warranty.grant(HELMET_B)
        char.grant(HELMET_A)
        char.grant(HELMET_B)
        warranty.revoke(HELMET_A)
        assertFalse(char.isOpen(HELMET_A))
        assertTrue(char.isOpen(HELMET_B))
    }

    // @verifies REQ-SHDR-09#c
    @Test
    fun theEnrolmentAnEarlierVersionKeptEnrolsNothingAndIsRemoved() {
        val store = InMemoryKeyValueStore()
        store.putBoolean(PersistedKeys.WARRANTY_CONSENT_KEY, true)
        store.putBoolean(PersistedKeys.ENROLMENT_KEY, true)
        store.putInt(PersistedKeys.CONSENT_EPOCH_KEY, 3)
        store.putInt(PersistedKeys.PROGRAMME_ID_KEY, PersistedKeys.CURRENT_PROGRAMME_ID)
        val records = WarrantyOwnerStore(store)
        val warranty = WarrantyAnalyticsGate(records)
        val char = CharacterisationEnrolmentGate(records, warranty)
        warranty.grant(HELMET_A)
        assertFalse(char.isOpen(HELMET_A), "an install-wide enrolment cannot be attributed to a helmet")
        assertEquals(0, char.consentEpoch(HELMET_A))
        assertFalse(store.getBoolean(PersistedKeys.ENROLMENT_KEY))
        assertEquals(0, store.getInt(PersistedKeys.CONSENT_EPOCH_KEY))
        assertEquals(0, store.getInt(PersistedKeys.PROGRAMME_ID_KEY))
        // Control: the owner's own opt-in for this helmet enrols it.
        assertEquals(1, char.grant(HELMET_A))
        assertTrue(char.isOpen(HELMET_A))
    }

    // @verifies REQ-SW02-04
    @Test
    fun withdrawalStopsCollectionButKeepsTheEpochMonotone() {
        val (_, _, char) = gates()
        assertEquals(1, char.grant(HELMET_A))
        char.withdraw(HELMET_A)
        assertFalse(char.isOpen(HELMET_A))
        assertEquals(1, char.consentEpoch(HELMET_A), "withdrawal must not reset the epoch")

        // Re-enrolment is a NEW grant and must be distinguishable from the first.
        assertEquals(2, char.grant(HELMET_A))
        assertTrue(char.isOpen(HELMET_A))
    }

    // @verifies REQ-SW02-04
    @Test
    fun enrolmentDoesNotSurviveDeviceTransfer() {
        val (_, warranty, char) = gates()
        char.grant(HELMET_A)
        assertTrue(char.isOpen(HELMET_A))
        warranty.endCustody(HELMET_A)
        assertFalse(char.isOpen(HELMET_A), "a previous registrant's consent must not bind a new owner")
        assertEquals(0, char.consentEpoch(HELMET_A), "the new owner's first enrolment is genuinely their first")
    }

    // @verifies REQ-SW02-04
    @Test
    fun consentToOneProgrammeDoesNotAuthoriseAnother() {
        val (store, _, char) = gates()
        char.grant(HELMET_A)
        assertTrue(char.isOpen(HELMET_A))
        // The record as a later programme's enrolment would be written: the last field is the programme.
        val value = store.getString(WarrantyOwnerStore.RECORDS_KEY)!!
        val enrolled = "v4,1,1,1,${PersistedKeys.CURRENT_PROGRAMME_ID},0,,1,0,0,0,0"
        assertTrue(enrolled in value)
        store.putString(WarrantyOwnerStore.RECORDS_KEY, value.replace(enrolled, "v4,1,1,1,${PersistedKeys.CURRENT_PROGRAMME_ID + 1},0,,1,0,0,0,0"))
        assertFalse(char.isOpen(HELMET_A), "enrolment in programme N must not carry into N+1")
    }

    // @verifies REQ-SW02-04
    @Test
    fun grantingCharacterisationNeverGrantsWarrantyConsent() {
        // The subset relation runs one way only.
        val (_, warranty, char) = gates(warrantyOpen = false)
        char.grant(HELMET_A)
        assertFalse(warranty.isOpen(HELMET_A))
        assertFalse(warranty.isDecided(HELMET_A), "a refused enrolment records nothing")
    }

    // @verifies REQ-SW02-04
    @Test
    fun keysAreDistinctFromEveryOtherConsentSurface() {
        // CLAUDE.md §6.0: a shared key would make two consents one consent. Warranty consent and enrolment are two fields of
        // the warranty owner's one record (the same subject); neither shares a key with a user's consent.
        val keys = WarrantyOwnerStore.LEGACY_KEYS + WarrantyOwnerStore.RECORDS_KEY
        assertEquals(keys.size, keys.toSet().size)
        assertFalse(PersistedKeys.RESEARCH_ANALYTICS_KEY in keys)
    }
}
