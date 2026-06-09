import SwiftUI
import BackgroundTasks

@main
struct NeuroPulseApp: App {

    // All services declared without default values; initialized together in init()
    // so dependent services share the same NeuroPulseGATTManager instance.
    @StateObject private var gatt:        NeuroPulseGATTManager
    @StateObject private var bridge:      PhoneSessionManager
    @StateObject private var keyManager:  UHDRKeyManager
    @StateObject private var consentStore: ConsentStore
    @StateObject private var uploader:    SessionProtocolUploader
    @StateObject private var edfLoader:   EDFDownloader
    @StateObject private var shdrUpload:  SHDRUploader
    @StateObject private var backup:      UHDRBackupScheduler
    @StateObject private var consumable:  ConsumableTracker
    @StateObject private var ota:             OTAManager
    @StateObject private var setupMgr:        HardwareSetupManager
    @StateObject private var protocolLibrary: NPProtocolLibrary
    @StateObject private var limitsStore:     NPLimitsStore
    @StateObject private var healthKit:       HealthKitSessionReader
    @StateObject private var historyStore:    SessionHistoryStore

    init() {
        let g  = NeuroPulseGATTManager()
        let km = UHDRKeyManager()

        _gatt            = StateObject(wrappedValue: g)
        _bridge          = StateObject(wrappedValue: PhoneSessionManager(gatt: g))
        _keyManager      = StateObject(wrappedValue: km)
        _consentStore    = StateObject(wrappedValue: ConsentStore())
        _uploader        = StateObject(wrappedValue: SessionProtocolUploader(gatt: g))
        _edfLoader       = StateObject(wrappedValue: EDFDownloader(gatt: g))
        _shdrUpload      = StateObject(wrappedValue: SHDRUploader(gatt: g))
        _backup          = StateObject(wrappedValue: UHDRBackupScheduler(keyManager: km))
        _consumable      = StateObject(wrappedValue: ConsumableTracker(countsProvider: g))
        _ota             = StateObject(wrappedValue: OTAManager(gatt: g))
        _setupMgr        = StateObject(wrappedValue: HardwareSetupManager(gatt: g))
        _protocolLibrary = StateObject(wrappedValue: NPProtocolLibrary())
        _limitsStore     = StateObject(wrappedValue: NPLimitsStore())
        _healthKit       = StateObject(wrappedValue: HealthKitSessionReader())
        _historyStore    = StateObject(wrappedValue: SessionHistoryStore())

        // Register background task for nightly UHDR backup.
        // Must be registered before app finishes launching.
        BGTaskScheduler.shared.register(
            forTaskWithIdentifier: "com.neuropulse.uhdr-backup",
            using: nil
        ) { task in
            guard let refreshTask = task as? BGAppRefreshTask else { return }
            let localKM = km  // capture for async context
            Task { @MainActor in
                await UHDRBackupScheduler(keyManager: localKM).performBackupIfNeeded()
                refreshTask.setTaskCompleted(success: true)
            }
        }
    }

    // Age gate is the FIRST onboarding screen, before any personal data is
    // collected or any consent layer is presented (ISC-83, ISC-127).
    // Launch-blocking privacy requirement (NP-PRIV-001 Rev B MEDIUM-03).
    @AppStorage("np.onboarding.age-confirmed") private var ageConfirmed = false
    @State private var showAgeGate = false

    @AppStorage("np.onboarding.consent-shown") private var consentOnboardingShown = false
    @State private var showConsentOnboarding = false

    // BIPA written-release state (ISC-88..91). Wiring into the onboarding sequence
    // is handled by the separate `onboarding-sequence` feature; these keys exist so
    // both features share the same persisted state.
    @AppStorage("np.onboarding.bipa-shown") private var bipaShown = false
    @AppStorage("np.onboarding.bipa-accepted") private var bipaAccepted = false
    @State private var showBIPADisclosure = false

    var body: some Scene {
        WindowGroup {
            MainTabView()
                .environmentObject(gatt)
                .environmentObject(bridge)
                .environmentObject(keyManager)
                .environmentObject(consentStore)
                .environmentObject(uploader)
                .environmentObject(edfLoader)
                .environmentObject(shdrUpload)
                .environmentObject(backup)
                .environmentObject(consumable)
                .environmentObject(ota)
                .environmentObject(setupMgr)
                .environmentObject(protocolLibrary)
                .environmentObject(limitsStore)
                .environmentObject(healthKit)
                .environmentObject(historyStore)
                .onAppear {
                    UIDevice.current.isBatteryMonitoringEnabled = true
                    EngagementTier.incrementLaunchCount()
                    presentNextOnboardingStep()
                }
                // Age gate first — full-screen, no Skip (ISC-83, ISC-130).
                .fullScreenCover(isPresented: $showAgeGate) {
                    AgeGateView {
                        ageConfirmed = true
                        showAgeGate = false
                        presentNextOnboardingStep()
                    }
                }
                // BIPA disclosure — Illinois only, between age gate and consent (ISC-127).
                .fullScreenCover(isPresented: $showBIPADisclosure) {
                    BIPADisclosureView(
                        onAccept: {
                            bipaAccepted = true; bipaShown = true
                            showBIPADisclosure = false
                            presentNextOnboardingStep()
                        },
                        onDecline: {
                            bipaAccepted = false; bipaShown = true
                            showBIPADisclosure = false
                            presentNextOnboardingStep()
                        }
                    )
                }
                .sheet(isPresented: $showConsentOnboarding) {
                    ConsentOnboardingView(isPresented: $showConsentOnboarding)
                        .environmentObject(consentStore)
                }
        }
    }

    /// Single-entry onboarding chain: age gate → biometric consent (all users, once) →
    /// research consent L1–L4. Each screen re-invokes this on completion (ISC-127).
    private func presentNextOnboardingStep() {
        // Step 1: Age gate — must be first, all users.
        if !ageConfirmed {
            showAgeGate = true
            return
        }
        // Step 2: Biometric consent — all users, shown once.
        // Satisfies BIPA (IL), GDPR Art. 9 (EU), WA MHMD, and is good practice everywhere.
        // A prior decision (accept or decline) sets bipaShown = true permanently.
        if !bipaShown {
            showBIPADisclosure = true
            return
        }
        // Step 3: Research consent L1–L4.
        if !consentOnboardingShown {
            showConsentOnboarding = true
            consentOnboardingShown = true
            return
        }
        // All onboarding gates passed. For returning users whose analytics consent
        // key (np.analytics.consent-granted) is already set, configure() starts
        // the SDK. For users who previously skipped, isOpen is false and configure()
        // no-ops — the gate stays closed until they actively consent via the
        // Research Preferences sheet (which calls configure() directly on Done).
        AnalyticsGate.configure()
    }
}
