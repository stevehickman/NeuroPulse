import SwiftUI

// Hardware setup flow — first-session onboarding.
// Steps are measurement-confirmed; the app does not advance past hardware-gated steps
// until the hub reports the correct condition via GATT notification.

struct SetupView: View {

    @EnvironmentObject private var setup: HardwareSetupManager
    @EnvironmentObject private var library: NPProtocolLibrary
    @EnvironmentObject private var uploader: SessionProtocolUploader
    @EnvironmentObject private var limitsStore: NPLimitsStore

    @State private var showAutonomousPicker = false
    @State private var showProgramConfirmation = false

    // BIPA written-release re-presentation (ISC-91). Illinois users can re-open the
    // brainwave-data consent screen here to grant or revoke EEG data consent.
    @AppStorage("np.onboarding.bipa-shown") private var bipaShown = false
    @AppStorage("np.onboarding.bipa-accepted") private var bipaAccepted = false
    @State private var showBIPADisclosure = false

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                progressBar
                Divider()
                stepContent
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                Divider()
                navigationControls
                    .padding()
            }
            .navigationTitle(setup.currentStep.title)
            .navigationBarTitleDisplayMode(.inline)
            .sheet(isPresented: $showAutonomousPicker) {
                ProtocolMenuView(programMode: true, onProgrammed: {
                    showProgramConfirmation = true
                })
                .environmentObject(library)
                .environmentObject(uploader)
                .environmentObject(limitsStore)
            }
            .alert("SETUP_PROTOCOL_STORED_TITLE", isPresented: $showProgramConfirmation) {
                Button("COMMON_OK", role: .cancel) {}
            } message: {
                Text("SETUP_PROTOCOL_STORED_MESSAGE")
            }
            .sheet(isPresented: $showBIPADisclosure) {
                BIPADisclosureView(
                    onAccept: {
                        bipaAccepted = true
                        bipaShown = true
                        showBIPADisclosure = false
                    },
                    onDecline: {
                        bipaAccepted = false
                        bipaShown = true
                        showBIPADisclosure = false
                    }
                )
            }
        }
    }

    // MARK: - Progress bar

    private var progressBar: some View {
        let total = SetupStep.allCases.count - 1  // exclude .complete
        let current = min(setup.currentStep.rawValue, total)
        return GeometryReader { geo in
            ZStack(alignment: .leading) {
                Rectangle().fill(Color(.systemGray5)).frame(height: 4)
                Rectangle()
                    .fill(Color.accentColor)
                    .frame(width: geo.size.width * CGFloat(current) / CGFloat(total), height: 4)
                    .animation(.easeInOut, value: current)
            }
        }
        .frame(height: 4)
    }

    // MARK: - Step content

    @ViewBuilder
    private var stepContent: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 24) {
                stepIcon
                    .frame(maxWidth: .infinity)

                Text(setup.currentStep.instruction)
                    .font(.body)
                    .multilineTextAlignment(.leading)
                    .fixedSize(horizontal: false, vertical: true)

                if let error = setup.lastError {
                    errorBanner(error.localizedDescription)
                }

                stepSpecificContent

                // Mode 3 programming is offered once first-run setup is complete.
                if setup.currentStep == .complete {
                    autonomousModeCard
                    privacyConsentCard
                }
            }
            .padding()
        }
    }

    // MARK: - Autonomous Mode (Mode 3) card

    private var autonomousModeCard: some View {
        VStack(alignment: .leading, spacing: 12) {
            Label("SETUP_AUTONOMOUS_TITLE", systemImage: "bolt.badge.clock")
                .font(.headline)
                .foregroundColor(.accentColor)

            Text("SETUP_AUTONOMOUS_BODY")
                .font(.subheadline)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            Button {
                showAutonomousPicker = true
            } label: {
                Label("SETUP_AUTONOMOUS_CHOOSE_BUTTON", systemImage: "list.bullet.rectangle")
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    // MARK: - Privacy consent card (ISC-91)

    // Re-presentable entry point for the biometric data written release.
    // Shown to all users. Tapping re-opens BIPADisclosureView so consent
    // can be granted or revoked at any time.
    private var privacyConsentCard: some View {
        VStack(alignment: .leading, spacing: 12) {
            Label("SETUP_PRIVACY_CARD_TITLE", systemImage: "hand.raised.fill")
                .font(.headline)
                .foregroundColor(.accentColor)

            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text("SETUP_PRIVACY_MANAGE_EEG")
                        .font(.subheadline)
                        .fixedSize(horizontal: false, vertical: true)
                    Text(bipaAccepted ? "SETUP_PRIVACY_STATUS_GRANTED" : "SETUP_PRIVACY_STATUS_NOT_GRANTED")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                Spacer()
                Image(systemName: "chevron.right")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            .contentShape(Rectangle())
            .onTapGesture { showBIPADisclosure = true }
            .accessibilityElement(children: .combine)
            .accessibilityLabel("SETUP_PRIVACY_MANAGE_EEG_A11Y")
            .accessibilityAddTraits(.isButton)
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    private var stepIcon: some View {
        Image(systemName: iconName(for: setup.currentStep))
            .font(.system(size: 48))
            .foregroundColor(.accentColor)
            .padding(.top, 8)
            .accessibilityHidden(true)
    }

    private func iconName(for step: SetupStep) -> String {
        switch step {
        case .welcome:               return "sparkles"
        case .bleConfirmation:       return "antenna.radiowaves.left.and.right"
        case .boaDial:               return "dial.high"
        case .electrodePods:         return "brain.head.profile"
        case .zoneModules:           return "rectangle.3.group"
        case .impedanceCheck:        return "waveform.path.ecg"
        case .ads1299Calibration:    return "slider.horizontal.3"
        case .hydrationCaps:         return "drop.circle"
        case .safetyAcknowledgement: return "shield.checkered"
        case .protocolSelection:     return "list.bullet.rectangle.portrait"
        case .complete:              return "checkmark.circle.fill"
        }
    }

    @ViewBuilder
    private var stepSpecificContent: some View {
        switch setup.currentStep {
        case .bleConfirmation:
            BLEConnectionStatusCard()
        case .zoneModules:
            ZoneModuleStatusGrid(configuration: setup.zoneConfiguration)
        case .impedanceCheck:
            ImpedanceStatusGrid(flags: setup.impedanceFlags)
        case .ads1299Calibration:
            if setup.isProcessing {
                ProgressView(String(localized: "SETUP_CALIBRATING_AMPLIFIER"))
                    .frame(maxWidth: .infinity)
            }
        case .safetyAcknowledgement:
            SafetyAcknowledgementCard(acknowledged: setup.safetyAcknowledged) {
                setup.acknowledgeSafety()
            }
        case .protocolSelection:
            ProtocolSelectionCard()
        case .complete:
            VStack(spacing: 16) {
                Image(systemName: "checkmark.circle.fill")
                    .font(.system(size: 64))
                    .foregroundColor(.green)
                    .accessibilityHidden(true)
                Text("SETUP_COMPLETE_HEADING")
                    .font(.title2.bold())
                    .multilineTextAlignment(.center)
            }
            .frame(maxWidth: .infinity)
        default:
            EmptyView()
        }
    }

    private func errorBanner(_ message: String) -> some View {
        HStack(alignment: .top, spacing: 8) {
            Image(systemName: "exclamationmark.triangle.fill")
                .foregroundColor(.orange)
                .accessibilityHidden(true)
            Text(message)
                .font(.subheadline)
                .foregroundColor(.primary)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding()
        .background(Color.orange.opacity(0.1))
        .clipShape(RoundedRectangle(cornerRadius: 10))
    }

    // MARK: - Navigation controls

    @ViewBuilder
    private var navigationControls: some View {
        if setup.currentStep == .complete {
            Text("SETUP_COMPLETE_HINT")
                .font(.caption)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
        } else if setup.currentStep.requiresHardwareConfirmation {
            hardwareConfirmButton
        } else {
            Button("SETUP_CONTINUE_BUTTON") { setup.advance() }
                .buttonStyle(.borderedProminent)
                .frame(maxWidth: .infinity)
        }
    }

    @ViewBuilder
    private var hardwareConfirmButton: some View {
        switch setup.currentStep {
        case .bleConfirmation:
            VStack(spacing: 12) {
                Button("SETUP_CONFIRM_BLE_BUTTON") {
                    setup.confirmBLEPairing()
                }
                .buttonStyle(.borderedProminent)
                .frame(maxWidth: .infinity)
                if setup.lastError != nil {
                    Button("SETUP_RETRY_BUTTON") { setup.retry() }
                        .buttonStyle(.bordered)
                }
            }
        case .impedanceCheck:
            VStack(spacing: 12) {
                if setup.isProcessing {
                    ProgressView(String(localized: "SETUP_CHECKING_CONTACTS"))
                } else {
                    Button("SETUP_CHECK_SIGNAL_BUTTON") {
                        Task { await setup.confirmImpedanceCheck() }
                    }
                    .buttonStyle(.borderedProminent)
                    .frame(maxWidth: .infinity)
                }
                if setup.lastError != nil {
                    Button("SETUP_RETRY_BUTTON") { setup.retry() }
                        .buttonStyle(.bordered)
                }
            }
        case .ads1299Calibration:
            if setup.isProcessing {
                ProgressView(String(localized: "SETUP_CALIBRATING"))
            } else {
                Button("SETUP_CALIBRATE_BUTTON") {
                    Task { await setup.confirmADS1299Calibration() }
                }
                .buttonStyle(.borderedProminent)
                .frame(maxWidth: .infinity)
            }
        case .zoneModules:
            Button("SETUP_CONFIRM_ZONE_BUTTON") {
                setup.confirmZoneModules()
            }
            .buttonStyle(.borderedProminent)
            .frame(maxWidth: .infinity)
        default:
            EmptyView()
        }
    }
}

// MARK: - BLE connection status card

private struct BLEConnectionStatusCard: View {
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Label("SETUP_BLE_CARD_TITLE", systemImage: "antenna.radiowaves.left.and.right")
                .font(.headline)
                .foregroundColor(.accentColor)
            Text("SETUP_BLE_CARD_BODY")
                .font(.subheadline)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }
}

// MARK: - Safety acknowledgement card (ISC-118/119)

struct SafetyAcknowledgementCard: View {
    let acknowledged: Bool
    let onAcknowledge: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Label("SETUP_SAFETY_CARD_TITLE", systemImage: "exclamationmark.shield")
                .font(.headline)
                .foregroundColor(.primary)

            VStack(alignment: .leading, spacing: 8) {
                contraindication(String(localized: "SETUP_SAFETY_CONTRAIN_1"))
                contraindication(String(localized: "SETUP_SAFETY_CONTRAIN_2"))
                contraindication(String(localized: "SETUP_SAFETY_CONTRAIN_3"))
                contraindication(String(localized: "SETUP_SAFETY_CONTRAIN_4"))
                contraindication(String(localized: "SETUP_SAFETY_CONTRAIN_5"))
                contraindication(String(localized: "SETUP_SAFETY_CONTRAIN_6"))
            }

            Divider()

            Button {
                onAcknowledge()
            } label: {
                HStack(spacing: 12) {
                    Image(systemName: acknowledged ? "checkmark.square.fill" : "square")
                        .font(.title3)
                        .foregroundColor(acknowledged ? .accentColor : .secondary)
                    Text("SETUP_SAFETY_ACK_TEXT")
                        .font(.subheadline)
                        .multilineTextAlignment(.leading)
                        .foregroundColor(.primary)
                }
            }
            .buttonStyle(.plain)
            .accessibilityLabel(String(localized: "SETUP_SAFETY_ACK_A11Y_LABEL"))
            .accessibilityValue(
                acknowledged ? String(localized: "SETUP_SAFETY_ACK_CHECKED")
                    : String(localized: "SETUP_SAFETY_ACK_UNCHECKED")
            )
            .accessibilityAddTraits(.isButton)
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    private func contraindication(_ text: String) -> some View {
        HStack(alignment: .top, spacing: 8) {
            Image(systemName: "xmark.circle.fill")
                .foregroundColor(.red)
                .font(.caption)
                .padding(.top, 2)
                .accessibilityHidden(true)
            Text(text)
                .font(.subheadline)
                .fixedSize(horizontal: false, vertical: true)
        }
    }
}

// MARK: - Protocol selection card (ISC-114 item 3)

private struct ProtocolSelectionCard: View {
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Label("SETUP_PROTOCOL_CARD_TITLE", systemImage: "list.bullet.rectangle.portrait")
                .font(.headline)
                .foregroundColor(.accentColor)

            Text("SETUP_PROTOCOL_CARD_BODY")
                .font(.subheadline)
                .foregroundColor(.secondary)
                .fixedSize(horizontal: false, vertical: true)

            ForEach(StarterProtocol.all) { proto in
                HStack(spacing: 12) {
                    Image(systemName: proto.icon)
                        .foregroundColor(.accentColor)
                        .frame(width: 24)
                    VStack(alignment: .leading, spacing: 2) {
                        Text(proto.name)
                            .font(.subheadline.bold())
                        Text(proto.summary)
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                }
                .padding(.vertical, 4)
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }
}

private struct StarterProtocol: Identifiable {
    let id: String
    let name: String
    let summary: String
    let icon: String

    static var all: [StarterProtocol] {
        [
            StarterProtocol(id: "alpha_calm",
                            name: String(localized: "SETUP_STARTER_ALPHA_CALM_NAME"),
                            summary: String(localized: "SETUP_STARTER_ALPHA_CALM_SUMMARY"),
                            icon: "waveform"),
            StarterProtocol(id: "focus_prime",
                            name: String(localized: "SETUP_STARTER_FOCUS_PRIME_NAME"),
                            summary: String(localized: "SETUP_STARTER_FOCUS_PRIME_SUMMARY"),
                            icon: "bolt.circle"),
            StarterProtocol(id: "sleep_deep",
                            name: String(localized: "SETUP_STARTER_SLEEP_DEEP_NAME"),
                            summary: String(localized: "SETUP_STARTER_SLEEP_DEEP_SUMMARY"),
                            icon: "moon.stars"),
        ]
    }
}

// MARK: - Supporting subviews

struct ZoneModuleStatusGrid: View {
    let configuration: ZoneModuleConfiguration

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            ForEach(configuration.slots) { slot in
                HStack {
                    Image(systemName: slot.isPresent ? "checkmark.circle.fill" : "circle.dashed")
                        .foregroundColor(slot.isPresent ? .green : .secondary)
                        .accessibilityHidden(true)
                    Text(slot.anatomicalLabel)
                        .font(.subheadline)
                    Spacer()
                    if slot.isPresent {
                        Text(slot.displayName)
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                }
                .padding(.vertical, 4)
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }
}

struct ImpedanceStatusGrid: View {
    let flags: UInt16

    private let channelNames = ["Fp1", "Fp2", "F3", "F4", "C3", "C4", "P3", "P4"]

    var body: some View {
        LazyVGrid(columns: Array(repeating: GridItem(.flexible()), count: 4), spacing: 10) {
            ForEach(0..<8, id: \.self) { idx in
                let passed = flags & (1 << idx) != 0
                VStack(spacing: 4) {
                    Circle()
                        .fill(passed ? Color.green : Color.orange)
                        .frame(width: 20, height: 20)
                        .accessibilityHidden(true)
                    Text(channelNames[idx])
                        .font(.caption2)
                        .foregroundColor(.secondary)
                }
                .accessibilityElement(children: .combine)
                .accessibilityLabel(
                    String(
                        format: String(
                            localized: passed
                                ? "SETUP_IMPEDANCE_PASS_A11Y"
                                : "SETUP_IMPEDANCE_FAIL_A11Y"
                        ),
                        channelNames[idx]
                    )
                )                                                        
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }
}
