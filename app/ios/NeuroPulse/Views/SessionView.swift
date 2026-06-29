import SwiftUI
import OSLog

// Real-time session display — Mode 1 Connected (<1ms USB-C).
// Shows: connection status · session status · EEG/HRV live metrics ·
// HRV breathing ring · zone module status · session controls.

struct SessionView: View {

    @EnvironmentObject private var gatt:        NeuroPulseGATTManager
    @EnvironmentObject private var uploader:    SessionProtocolUploader
    @EnvironmentObject private var consumable:  ConsumableTracker
    @EnvironmentObject private var setup:       HardwareSetupManager
    @EnvironmentObject private var library:     NPProtocolLibrary
    @EnvironmentObject private var healthKit:   HealthKitSessionReader
    @EnvironmentObject private var history:     SessionHistoryStore
    @EnvironmentObject private var edfLoader:   EDFDownloader
    @EnvironmentObject private var bridge:      PhoneSessionManager

    @State private var showProtocolPicker    = false
    @State private var showStopConfirmation  = false
    @State private var stopErrorMessage:     String?
    @State private var showHistory           = false

    // Live snapshot for session history record — safe aggregated metrics only (ISC-49).
    @State private var runningStartedAt:     Date?
    @State private var runningProtocolID:    UInt8 = 0
    @State private var lastImpedancePass:    Int = 0
    @State private var lastCoherenceScore:   Float?
    @State private var lastRMSSD:            UInt16?

    // Reset to .idle when a new session starts so stale download state never carries across sessions.
    @State private var sessionDownloadState: SessionDownloadState = .idle
    @State private var downloadTask: Task<Void, Never>?

    private enum SessionDownloadState: Equatable {
        case idle
        case downloading
        case downloaded(URL)
        case failed(String)
    }

    // Biometric written-release acceptance (ISC-90). EEG features disabled until
    // the user has consented to brainwave-data collection (shown to all users once).
    @AppStorage("np.onboarding.bipa-accepted") private var bipaAccepted = false

    /// Whether EEG/closed-loop neurofeedback features may be shown and used.
    var eegConsentGranted: Bool { bipaAccepted }

    // True only while an HRV biofeedback session is live. The hub streams the HRV
    // coherence characteristic only for protocols that include HRV biofeedback, so
    // a non-nil session.hrv during a running session is the signal that HRV
    // biofeedback is the active modality. HealthKit is gated on this (ISC-94).
    private var hrvBiofeedbackActive: Bool {
        gatt.session.status == .running && gatt.session.hrv != nil
    }

    private static let logger = Logger(subsystem: "com.neuropulse.app", category: "SessionView")

    // VoiceOver coherence debounce (ISC-149): throttle GATT 100ms stream to 2s announcements.
    @State private var voiceOverCoherence:    Float? = nil
    @State private var coherenceDebounceTask: Task<Void, Never>? = nil

    // ISC-148: scale breathing ring with Dynamic Type so text inside never clips at AXL.
    @ScaledMetric(relativeTo: .title3) private var breathingRingMax: CGFloat = 120
    @ScaledMetric(relativeTo: .title3) private var breathingRingMin: CGFloat = 80

    var body: some View {
        NavigationStack {
            sessionScrollView
                .navigationTitle(String(localized: "SESSION_TITLE"))
                .toolbar {
                    ToolbarItem(placement: .navigationBarLeading) {
                        Button { showHistory = true } label: {
                            Label("SESSION_HISTORY_LABEL", systemImage: "clock.arrow.circlepath")
                        }
                        .accessibilityLabel("SESSION_HISTORY_LABEL")
                    }
                    ToolbarItem(placement: .navigationBarTrailing) {
                        HStack(spacing: 8) {
                            watchConnectivityIcon
                            connectionIndicator
                        }
                    }
                }
                .sheet(isPresented: $showHistory) {
                    SessionHistoryListView()
                        .environmentObject(history)
                        .environmentObject(edfLoader)
                }
                .sheet(isPresented: $showProtocolPicker) {
                    ProtocolMenuView()
                        .environmentObject(library)
                        .environmentObject(uploader)
                }
                .confirmationDialog("SESSION_STOP_CONFIRM_TITLE",
                                    isPresented: $showStopConfirmation,
                                    titleVisibility: .visible) {
                    Button("SESSION_END_BUTTON", role: .destructive) { sendSessionStop() }
                    Button("COMMON_CANCEL", role: .cancel) { }
                } message: {
                    Text("SESSION_STOP_CONFIRM_MESSAGE")
                }
                .alert("SESSION_STOP_ERROR_TITLE",
                       isPresented: Binding(
                           get: { stopErrorMessage != nil },
                           set: { if !$0 { stopErrorMessage = nil } }
                       )) {
                    Button("COMMON_OK", role: .cancel) { stopErrorMessage = nil }
                } message: {
                    Text(stopErrorMessage ?? "")
                }
                .sessionObservers(
                    healthKit: healthKit,
                    gatt: gatt,
                    coherenceDebounceTask: $coherenceDebounceTask,
                    voiceOverCoherence: $voiceOverCoherence,
                    lastCoherenceScore: $lastCoherenceScore,
                    lastRMSSD: $lastRMSSD,
                    lastImpedancePass: $lastImpedancePass,
                    hrvBiofeedbackActive: hrvBiofeedbackActive,
                    onStatusChange: handleStatusChange
                )
                .onDisappear { downloadTask?.cancel() }
        }
    }

    private var sessionScrollView: some View {
        ScrollView {
            VStack(spacing: 20) {
                connectionBanner
                if consumable.sessionIsBlocked {
                    blockingConsumableAlert
                } else {
                    sessionStatusCard
                    if gatt.session.status == .running {
                        hrvBreathingRing
                        if eegConsentGranted {
                            liveMetricsGrid
                        } else {
                            eegConsentUnavailableCard
                        }
                    }
                    zoneModuleRow
                    sessionControls
                }
                regulatoryFooter
                voiceOverCoherenceAnnouncer
            }
            .padding()
        }
    }

    // Record session in history when hub confirms completion.
    private func handleStatusChange(from old: SessionStatus, to new: SessionStatus) {
        switch (old, new) {
        case (.idle, .running), (.paused, .running):
            runningStartedAt = Date()
            runningProtocolID = gatt.session.protocolID
            sessionDownloadState = .idle
        case (.running, .completed):
            let durationSecs = runningStartedAt.map { UInt32(max(0, Date().timeIntervalSince($0))) } ?? 0
            let record = CompletedSessionSummary(
                protocolName: "Protocol \(runningProtocolID)",
                durationSeconds: durationSecs,
                adaptationEvents: [],
                completedAt: Date(),
                averageCoherenceScore: lastCoherenceScore,
                rmssdMilliseconds: lastRMSSD,
                impedancePassCount: lastImpedancePass,
                edfSessionID: Self.edfSessionID(from: gatt.session.epoch)
            )
            history.record(record)
        default: break
        }
    }

    // Shown when the user declined the biometric data consent (ISC-90).
    private var eegConsentUnavailableCard: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label("SESSION_EEG_UNAVAILABLE_TITLE", systemImage: "brain")
                .font(.headline)
                .foregroundColor(.orange)
            Text("SESSION_EEG_UNAVAILABLE_BODY")
                .font(.subheadline)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding()
        .background(Color.orange.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    // Requests the hub to stop the session. Does NOT update session.status locally —
    // the UI reflects new state only after the hub reports it via GATT (ISC-34).
    private func sendSessionStop() {
        gatt.sendSessionStop { result in
            if case .failure(let error) = result {
                Self.logger.error("Session stop failed: \(String(describing: error))")
                stopErrorMessage = String(localized: "SESSION_STOP_ERROR")
            }
        }
    }

    // MARK: - Subviews

    private var connectionBanner: some View {
        HStack {
            Circle()
                .fill(connectionColor)
                .frame(width: 8, height: 8)
                .accessibilityHidden(true)
            Text(connectionLabel)
                .font(.footnote)
                .foregroundColor(.secondary)
            Spacer()
            if gatt.connectionState == .connected {
                Text(connectionModeLabel)
                    .font(.caption2)
                    .padding(.horizontal, 8).padding(.vertical, 2)
                    .background(Color.green.opacity(0.15))
                    .clipShape(Capsule())
            }
        }
        .padding(.horizontal, 4)
    }

    private var connectionColor: Color {
        switch gatt.connectionState {
        case .connected:    return .green
        case .connecting:   return .yellow
        case .scanning:     return .orange
        case .disconnected: return .red
        }
    }

    private var connectionLabel: String {
        switch gatt.connectionState {
        case .disconnected: return String(localized: "SESSION_CONN_SEARCHING")
        case .scanning:     return String(localized: "SESSION_CONN_SCANNING")
        case .connecting:   return String(localized: "SESSION_CONN_CONNECTING")
        case .connected:    return String(localized: "SESSION_CONN_CONNECTED")
        }
    }

    // Distinguishes the wired USB-C transport (sub-millisecond, "Live") from the
    // BLE transport ("Wireless"). The BLE path deliberately makes no latency claim
    // because the <1ms guarantee only holds over USB-C (CLAUDE.md §4.1).
    //
    // USB-C detection: iOS exposes no public IOKit accessory API for this, so the
    // device charging state is used as a reasonable proxy — the hub powers the
    // phone over USB-C when wired. Battery monitoring is enabled app-wide in
    // NeuroPulseApp.swift (UIDevice.current.isBatteryMonitoringEnabled = true).
    private var connectionModeLabel: String {
        let isWired = UIDevice.current.batteryState == .charging
            || UIDevice.current.batteryState == .full
        return isWired ? String(localized: "SESSION_MODE_LIVE") : String(localized: "SESSION_MODE_WIRELESS")
    }

    private var watchConnectivityIcon: some View {
        ZStack {
            Image(systemName: "applewatch")
                .font(.footnote)
                .foregroundColor(.secondary)
            if !bridge.watchConnectivityAvailable {
                ZStack {
                    Circle()
                        .stroke(Color.red, lineWidth: 1)
                    Image(systemName: "line.diagonal")
                        .foregroundColor(.red)
                }
                .font(.footnote)
            }
        }
        .accessibilityLabel(bridge.watchConnectivityAvailable
                            ? String(localized: "SESSION_WATCH_CONNECTED_A11Y")
                            : String(localized: "SESSION_WATCH_DISCONNECTED_A11Y"))
    }

    private var connectionIndicator: some View {
        Image(systemName: gatt.connectionState == .connected ? "wifi" : "wifi.slash")
            .foregroundColor(connectionColor)
            .accessibilityLabel(gatt.connectionState == .connected
                                ? String(localized: "SESSION_CONN_INDICATOR_CONNECTED")
                                : String(localized: "SESSION_CONN_INDICATOR_DISCONNECTED"))
    }

    // Hidden VoiceOver-only announcer for the debounced coherence score (ISC-149).
    private var voiceOverCoherenceAnnouncer: some View {
        Text(voiceOverCoherence.map {
            String(format: String(localized: "SESSION_COHERENCE_SCORE_A11Y_FORMAT"),
                   NPNumberFormatter.decimal1($0))
        } ?? "")
            .frame(width: 0, height: 0)
            .accessibilityHidden(false)
            .onChange(of: voiceOverCoherence) { _, newValue in
                guard newValue != nil else { return }
                UIAccessibility.post(notification: .announcement,
                                     argument: voiceOverCoherence.map {
                                         String(format: String(localized: "SESSION_COHERENCE_A11Y_FORMAT"),
                                                NPNumberFormatter.decimal1($0))
                                     })
            }
    }

    private var blockingConsumableAlert: some View {
        VStack(alignment: .leading, spacing: 12) {
            Label(String(localized: "SESSION_BLOCKED_TITLE"), systemImage: "exclamationmark.triangle.fill")
                .foregroundColor(.red)
                .font(.headline)
            if let reason = consumable.sessionBlockReason {
                Text(reason)
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }
            Text(String(localized: "SESSION_BLOCKED_GUIDANCE"))
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .padding()
        .background(Color.red.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    private var sessionStatusCard: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                statusIndicator
                Spacer()
                if gatt.session.status == .running {
                    Text(String(localized: "SESSION_PROTOCOL_LABEL").replacingOccurrences(of: "{0}", with: gatt.session.protocolID))
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            }

            if gatt.session.status == .idle {
                Text(String(localized: "SESSION_IDLE_PROMPT"))
                    .font(.subheadline)
                    .foregroundColor(.secondary)
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    private var statusIndicator: some View {
        HStack(spacing: 8) {
            Circle()
                .fill(sessionStatusColor)
                .frame(width: 10, height: 10)
                .overlay {
                    if gatt.session.status == .running {
                        Circle().fill(sessionStatusColor).opacity(0.4)
                            .scaleEffect(1.5)
                            .animation(.easeInOut(duration: 1).repeatForever(), value: gatt.session.status)
                    }
                }
                .accessibilityHidden(true)
            Text(sessionStatusLabel)
                .font(.headline)
        }
    }

    private var sessionStatusColor: Color {
        switch gatt.session.status {
        case .running:   return .green
        case .paused:    return .yellow
        case .completed: return .blue
        case .idle:      return .gray
        }
    }

    private var sessionStatusLabel: String {
        switch gatt.session.status {
        case .idle:      return String(localized: "SESSION_STATUS_READY")
        case .running:   return String(localized: "SESSION_STATUS_ACTIVE")
        case .paused:    return String(localized: "SESSION_STATUS_PAUSED")
        case .completed: return String(localized: "SESSION_STATUS_COMPLETE")
        }
    }

    // Breathing ring — expands on inhale, contracts on exhale.
    private var hrvBreathingRing: some View {
        VStack(spacing: 8) {
            ZStack {
                Circle()
                    .stroke(Color.blue.opacity(0.2), lineWidth: 6)
                    .frame(width: breathingRingMax, height: breathingRingMax)
                Circle()
                    .stroke(Color.blue, lineWidth: 6)
                    .frame(
                        width: gatt.session.pacerPhase == .inhale ? breathingRingMax : breathingRingMin,
                        height: gatt.session.pacerPhase == .inhale ? breathingRingMax : breathingRingMin
                    )
                    .animation(.easeInOut(duration: 2.5), value: gatt.session.pacerPhase)

                VStack(spacing: 2) {
                    Text(gatt.session.pacerPhase == .inhale ? "SESSION_BREATH_INHALE" : "SESSION_BREATH_EXHALE")
                        .font(.caption2).foregroundColor(.blue)
                    if let hrv = gatt.session.hrv {
                        Text(NPNumberFormatter.decimal1(hrv.coherenceScore))
                            .font(.title3.bold())
                        Text("SESSION_COHERENCE_LABEL").font(.caption2).foregroundColor(.secondary)
                    }
                }
            }

            if let hrv = gatt.session.hrv {
                Text(String(format: String(localized: "SESSION_RMSSD_FORMAT"), hrv.rmssdMilliseconds))
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
        }
    }

    private var liveMetricsGrid: some View {
        LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible())], spacing: 12) {
            if let hrv = gatt.session.hrv {
                MetricCard(
                    title: String(localized: "SESSION_METRIC_COHERENCE"),
                    value: String(format: String(localized: "SESSION_COHERENCE_VALUE_FORMAT"),
                                  NPNumberFormatter.decimal1(hrv.coherenceScore)),
                    icon: "waveform.path.ecg", color: coherenceColor(hrv.coherenceScore))
                MetricCard(
                    title: String(localized: "SESSION_METRIC_RMSSD"),
                    value: String(format: String(localized: "SESSION_RMSSD_VALUE_FORMAT"),
                                  NPNumberFormatter.decimal0(Double(hrv.rmssdMilliseconds))),
                    icon: "heart.fill", color: .pink)
            }
            MetricCard(title: String(localized: "SESSION_METRIC_EEG_CONTACTS"),
                       value: "\(impedancePassCount) / 8",
                       icon: "brain", color: impedancePassCount == 8 ? .green : .orange)
            if let healthKitHRV = healthKit.latestHRVSDNN {
                MetricCard(
                    title: String(localized: "SESSION_METRIC_HEALTHKIT_HRV"),
                    value: String(format: String(localized: "SESSION_RMSSD_VALUE_FORMAT"),
                                  NPNumberFormatter.decimal0(healthKitHRV)),
                    icon: "heart.text.square", color: .purple)
            }
        }
    }

    private var impedancePassCount: Int {
        (0..<8).filter { gatt.session.impedancePassFlags & (1 << $0) != 0 }.count
    }

    private func coherenceColor(_ score: Float) -> Color {
        score >= 7 ? .green : score >= 4 ? .yellow : .orange
    }

    private var zoneModuleRow: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(String(localized: "SESSION_ZONE_MODULES")).font(.caption).foregroundColor(.secondary)
            HStack(spacing: 8) {
                ForEach(0..<5, id: \.self) { slot in
                    let isPresent = slot < gatt.zoneModules.count && gatt.zoneModules[slot] != 0
                    RoundedRectangle(cornerRadius: 6)
                        .fill(isPresent ? Color.green.opacity(0.2) : Color(.systemGray5))
                        .overlay {
                            Text("\(slot + 1)")
                                .font(.caption2.bold())
                                .foregroundColor(isPresent ? .green : .secondary)
                        }
                        .frame(minHeight: 36)
                }
            }
        }
    }

    private var sessionControls: some View {
        VStack(spacing: 12) {
            Button {
                showProtocolPicker = true
            } label: {
                Label(String(localized: "SESSION_CHOOSE_PROTOCOL_BUTTON"), systemImage: "list.bullet.rectangle")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            .disabled(!setup.isFirstSetupComplete || gatt.connectionState != .connected)

            if gatt.session.status == .running {
                Button(role: .destructive) {
                    showStopConfirmation = true
                } label: {
                    Label(String(localized: "SESSION_END_BUTTON"), systemImage: "stop.circle")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .disabled(gatt.connectionState != .connected)
            }

            if Self.shouldShowSessionDownload(status: gatt.session.status, epoch: gatt.session.epoch) {
                sessionDownloadControl
            }
        }
    }

    @ViewBuilder
    private var sessionDownloadControl: some View {
        VStack(spacing: 8) {
            switch sessionDownloadState {
            case .idle:
                Button { startCompletedSessionDownload() } label: {
                    Label("SESSION_DOWNLOAD_BUTTON", systemImage: "arrow.down.doc")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
                .accessibilityLabel("SESSION_DOWNLOAD_A11Y")

            case .downloading:
                HStack(spacing: 10) {
                    ProgressView()
                    Text("SESSION_DOWNLOADING")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 4)

            case .downloaded(let url):
                VStack(spacing: 4) {
                    Label("SESSION_DOWNLOADED_LABEL", systemImage: "checkmark.circle.fill")
                        .font(.subheadline.bold())
                        .foregroundColor(.green)
                    Text(url.lastPathComponent)
                        .font(.caption2.monospaced())
                        .foregroundColor(.secondary)
                    Text("SESSION_DOWNLOADED_FILES_HINT")
                        .font(.caption2)
                        .foregroundColor(.secondary)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 4)

            case .failed(let message):
                VStack(spacing: 8) {
                    Label(message, systemImage: "exclamationmark.triangle")
                        .font(.caption)
                        .foregroundColor(.orange)
                        .multilineTextAlignment(.center)
                    Button("SESSION_DOWNLOAD_RETRY") { startCompletedSessionDownload() }
                        .font(.caption.bold())
                }
                .frame(maxWidth: .infinity)
            }
        }
    }

    private func startCompletedSessionDownload() {
        guard sessionDownloadState != .downloading else { return }
        let epoch = gatt.session.epoch
        guard epoch != 0 else {
            // epoch == 0 means no hub session ID. shouldShowSessionDownload already
            // guards against this, so reaching here means a GATT state race occurred.
            Self.logger.error("startCompletedSessionDownload called with epoch == 0; GATT state race")
            sessionDownloadState = .failed(String(localized: "SESSION_DOWNLOAD_UNAVAILABLE"))
            return
        }
        sessionDownloadState = .downloading
        downloadTask = Task {
            defer { if sessionDownloadState == .downloading { sessionDownloadState = .idle } }
            do {
                let session = try await edfLoader.requestDownload(sessionID: epoch)
                if let url = session.localURL {
                    sessionDownloadState = .downloaded(url)
                } else {
                    sessionDownloadState = .failed(String(localized: "SESSION_DOWNLOAD_NO_FILE"))
                }
            } catch is CancellationError {
                // Task was cancelled (view dismissed mid-download). defer resets to .idle.
                Self.logger.info("Post-session EDF download cancelled")
            } catch let error as EDFDownloadError {
                let desc = error.errorDescription ?? String(localized: "SESSION_DOWNLOAD_GENERIC_ERROR")
                Self.logger.error("Post-session EDF download failed: \(desc, privacy: .public)")
                sessionDownloadState = .failed(desc)
            } catch {
                Self.logger.error("Post-session EDF download failed (unexpected): \(String(describing: type(of: error)), privacy: .public)")
                sessionDownloadState = .failed(String(localized: "SESSION_DOWNLOAD_GENERIC_ERROR"))
            }
        }
    }

    // MARK: - Download predicate helpers (static for unit testability)

    // epoch == 0 means the hub never assigned a session ID for this session — no EDF file to offer.
    static func shouldShowSessionDownload(status: SessionStatus, epoch: UInt32) -> Bool {
        status == .completed && epoch != 0
    }

    // epoch == 0 maps to nil so CompletedSessionSummary.edfSessionID is nil, not Optional(0).
    static func edfSessionID(from epoch: UInt32) -> UInt32? {
        epoch == 0 ? nil : epoch
    }

    private var regulatoryFooter: some View {
        Text("REGULATORY_FOOTER")
            .font(.caption2)
            .foregroundColor(.secondary)
            .multilineTextAlignment(.center)
    }
}

// MARK: - Session observer modifier

private struct SessionObserversModifier: ViewModifier {
    let healthKit: HealthKitSessionReader
    let gatt: NeuroPulseGATTManager
    @Binding var coherenceDebounceTask: Task<Void, Never>?
    @Binding var voiceOverCoherence: Float?
    @Binding var lastCoherenceScore: Float?
    @Binding var lastRMSSD: UInt16?
    @Binding var lastImpedancePass: Int
    let hrvBiofeedbackActive: Bool
    let onStatusChange: (SessionStatus, SessionStatus) -> Void

    func body(content: Content) -> some View {
        let v1 = content
            .onChange(of: hrvBiofeedbackActive) { _, isActive in
                if isActive { Task { await healthKit.requestAuthorizationAndStart() } }
                else { healthKit.stopAndClear() }
            }
            .onDisappear { healthKit.stopAndClear(); coherenceDebounceTask?.cancel() }
        let v2 = v1
            .onChange(of: gatt.session.status) { old, new in onStatusChange(old, new) }
            .onChange(of: gatt.session.impedancePassFlags) { _, flags in
                lastImpedancePass = (0..<8).filter { flags & (1 << $0) != 0 }.count
            }
        return v2.onChange(of: gatt.session.hrv) { _, hrv in updateHRVSnapshot(hrv: hrv) }
    }

    private func updateHRVSnapshot(hrv: HRVData?) {
        coherenceDebounceTask?.cancel()
        coherenceDebounceTask = Task {
            try? await Task.sleep(nanoseconds: 2_000_000_000)
            guard !Task.isCancelled else { return }
            await MainActor.run { voiceOverCoherence = hrv?.coherenceScore }
        }
        if let hrv { lastCoherenceScore = hrv.coherenceScore; lastRMSSD = hrv.rmssdMilliseconds }
    }
}

private extension View {
    func sessionObservers(
        healthKit: HealthKitSessionReader,
        gatt: NeuroPulseGATTManager,
        coherenceDebounceTask: Binding<Task<Void, Never>?>,
        voiceOverCoherence: Binding<Float?>,
        lastCoherenceScore: Binding<Float?>,
        lastRMSSD: Binding<UInt16?>,
        lastImpedancePass: Binding<Int>,
        hrvBiofeedbackActive: Bool,
        onStatusChange: @escaping (SessionStatus, SessionStatus) -> Void
    ) -> some View {
        modifier(SessionObserversModifier(
            healthKit: healthKit,
            gatt: gatt,
            coherenceDebounceTask: coherenceDebounceTask,
            voiceOverCoherence: voiceOverCoherence,
            lastCoherenceScore: lastCoherenceScore,
            lastRMSSD: lastRMSSD,
            lastImpedancePass: lastImpedancePass,
            hrvBiofeedbackActive: hrvBiofeedbackActive,
            onStatusChange: onStatusChange
        ))
    }
}

// MARK: - Supporting subviews

struct MetricCard: View {
    let title: String
    let value: String
    let icon: String
    let color: Color

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Label(title, systemImage: icon)
                .font(.caption)
                .foregroundColor(color)
            Text(value)
                .font(.title3.bold())
                .foregroundColor(.primary)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(color.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 10))
    }
}

