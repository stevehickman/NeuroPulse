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
                .onAppear {
                    UIDevice.current.isBatteryMonitoringEnabled = true
                    if !consentOnboardingShown {
                        showConsentOnboarding = true
                        consentOnboardingShown = true
                    }
                }
                .sheet(isPresented: $showConsentOnboarding) {
                    ConsentOnboardingView(isPresented: $showConsentOnboarding)
                        .environmentObject(consentStore)
                }
        }
    }
}
