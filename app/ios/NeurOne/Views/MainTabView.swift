import Combine
import SwiftUI

// Top-level tab navigation.
// Tab order: Session (primary) · Setup · Consumables · Consent · Firmware.
// Blocking consumable reminders surface as badge counts on the Consumables tab.
// First-run redirects to Setup flow before Session tab is accessible.

struct MainTabView: View {

    @EnvironmentObject private var gatt:            NeurOneGATTManager
    @EnvironmentObject private var setup:           HardwareSetupManager
    @EnvironmentObject private var consumable:      ConsumableTracker
    @EnvironmentObject private var consent:         ConsentStore
    @EnvironmentObject private var ota:             OTAManager
    @EnvironmentObject private var protocolLibrary: NPProtocolLibrary

    @State private var selectedTab = 0

    // ISC-121: intercept tab selection before any render cycle. A Binding guard avoids the
    // one-frame flicker caused by onChange(of:), which fires after layout has already
    // placed the user on the Session tab.
    private var guardedTabSelection: Binding<Int> {
        Binding(
            get: { selectedTab },
            set: { newValue in
                if newValue == 0 && !setup.isFirstSetupComplete { return }
                selectedTab = newValue
            }
        )
    }

    var body: some View {
        TabView(selection: guardedTabSelection) {
            sessionTab
            setupTab
            consumableTab
            consentTab
            firmwareTab
        }
        .onReceive(NotificationCenter.default.publisher(for: UIApplication.didBecomeActiveNotification)) { _ in
            if !setup.isFirstSetupComplete { selectedTab = 1 }
        }
        // OI-HWFIT-01: what the protocol menu offers follows the connected helmet. Until this, nothing set the library's tier, so
        // it stayed `.none` and every protocol read as unavailable. The tier is the hub's verdict and only decides what the menu
        // offers, with the tile in each socket judged by the shared fit check; the hub gates a T2 protocol itself.
        .onReceive(gatt.$connectionState.combineLatest(gatt.$hubTier, gatt.$zoneModules)) { state, tier, modules in
            protocolLibrary.updateAvailability(
                tier: NPProtocolLibrary.DeviceTier.forHelmet(connected: state == .connected, hub: tier),
                tileTypes: modules.tileTypes)
        }
    }

    private var sessionTab: some View {
        SessionView()
            .tabItem {
                Label(String(localized: "SESSION_TITLE"), systemImage: "brain.head.profile")
            }
            .tag(0)
    }

    private var setupTab: some View {
        SetupView()
            .tabItem {
                Label("TAB_SETUP", systemImage: "gearshape")
            }
            .badge(setup.isFirstSetupComplete ? 0 : 1)
            .tag(1)
    }

    private var consumableTab: some View {
        ConsumableView()
            .tabItem {
                Label(String(localized: "TAB_SUPPLIES"), systemImage: "cross.case")
            }
            .badge(consumable.blockingReminders.count)
            .tag(2)
    }

    private var consentTab: some View {
        ConsentDashboardView()
            .tabItem {
                Label(String(localized: "SETUP_PRIVACY_CARD_TITLE"), systemImage: "lock.shield")
            }
            .badge(consent.pendingInvitations.filter(\.isOpen).count)
            .tag(3)
    }

    private var firmwareTab: some View {
        OTAView()
            .tabItem {
                Label("TAB_FIRMWARE", systemImage: "arrow.down.circle")
            }
            .tag(4)
    }
}
