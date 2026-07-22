package life.neurone.core.protocol

// ISC-90 (parity with iOS NPProtocolLibrary EEG-consent gate).
//
// A protocol that reads the user's EEG — to record neurofeedback or to close the
// stimulation loop — depends on brainwave (biometric) data. Under BIPA it must not
// run until the user grants the biometric written release. On both platforms the
// consent signal is the boolean key "np.onboarding.bipa-accepted" (see the Android
// onboarding flow's OnboardingKeys.BIPA_ACCEPTED and the iOS @AppStorage of the same
// name). This decision lives in the pure-JVM :core module so the future Session and
// Protocol-menu Compose screens consume one tested gate — identical to how iOS routes
// every protocol-selection path through NPProtocolLibrary.availability(for:).

/**
 * True when the protocol reads the user's EEG. Covers the explicit EEG/qEEG recording
 * modalities plus the three EEG-adaptive couplings (EEG closed-loop, EEG-adaptive
 * audio, HRV+EEG dual biofeedback). Mirrors iOS `NPProtocolDefinition.isEEGDependent`.
 */
val NPProtocolDefinition.isEEGDependent: Boolean
    get() = modalities.filter { it.enabled }.any { mod ->
        when (val p = mod.params) {
            is NPModalityParams.EegNeurofeedback -> true       // any EEG recording is brainwave data
            is NPModalityParams.Qeeg21ch -> true
            is NPModalityParams.AudioEntrainment -> p.params.eegAdaptive
            is NPModalityParams.VnsHRV ->
                p.params.hrvProtocol == NPVNSHRVParams.HRVProtocol.EEG_BIOFEEDBACK
            else -> false
        }
    }

/**
 * BIPA consent gate for protocol selection (ISC-90). Kept as a small pure-JVM object
 * rather than folded into a device-availability layer because Android has not yet
 * ported iOS `ProtocolAvailability`/device-tier — this is the load-bearing consent
 * decision the Session and Protocol-menu screens will call when they are built.
 */
object ProtocolConsentGate {

    /**
     * Android string-resource name the :app UI resolves to show the block reason.
     * Parity with the iOS localized key `SESSION_EEG_UNAVAILABLE_BODY`. Pure-JVM :core
     * cannot reference `R`, so it exposes the stable resource name instead.
     */
    const val EEG_UNAVAILABLE_MESSAGE_RES = "session_eeg_unavailable_body"

    /**
     * EEG-dependency for any entry, expanding composites against the library — a
     * composite is EEG-dependent if ANY member protocol is (parity with iOS
     * `NPProtocolLibrary.isEEGDependent(_:)`). [resolveSingle] maps a layer's protocol
     * name to its definition; return null for an unknown name.
     */
    fun isEEGDependent(
        entry: NPProtocolEntry,
        resolveSingle: (String) -> NPProtocolDefinition? = { null },
    ): Boolean = when (entry) {
        is NPProtocolEntry.Single -> entry.protocol.isEEGDependent
        is NPProtocolEntry.Composite -> entry.composite.layers.any { layer ->
            resolveSingle(layer.protocolName)?.isEEGDependent ?: false
        }
        is NPProtocolEntry.Limits -> false
    }

    /**
     * True when [entry] must be blocked from selection because it reads EEG and the
     * BIPA biometric written release was not granted. The UI shows
     * [EEG_UNAVAILABLE_MESSAGE_RES] when this returns true.
     */
    fun isBlockedByBipa(
        entry: NPProtocolEntry,
        consentGranted: Boolean,
        resolveSingle: (String) -> NPProtocolDefinition? = { null },
    ): Boolean = !consentGranted && isEEGDependent(entry, resolveSingle)
}
