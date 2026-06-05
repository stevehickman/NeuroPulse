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
        _consumable      = StateObject(wrappedValue: ConsumableTracker(gatt: g))
        _ota             = StateObject(wrappedValue: OTAManager(gatt: g))
        _setupMgr        = StateObject(wrappedValue: HardwareSetupManager(gatt: g))
        _protocolLibrary = StateObject(wrappedValue: NPProtocolLibrary())
        _limitsStore     = StateObject(wrappedValue: NPLimitsStore())
        _healthKit       = StateObject(wrappedValue: HealthKitSessionReader())

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
                .onAppear {
                    UIDevice.current.isBatteryMonitoringEnabled = true
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
                .sheet(isPresented: $showConsentOnboarding) {
                    ConsentOnboardingView(isPresented: $showConsentOnboarding)
                        .environmentObject(consentStore)
                }
        }
    }

    /// Drives the onboarding sequence: age gate first, then research consent.
    /// Age confirmation gates everything that collects or displays personal data.
    private func presentNextOnboardingStep() {
        if !ageConfirmed {
            showAgeGate = true
            return
        }
        if !consentOnboardingShown {
            showConsentOnboarding = true
            consentOnboardingShown = true
        }
    }
}
