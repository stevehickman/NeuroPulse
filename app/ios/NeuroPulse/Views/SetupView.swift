import SwiftUI

// Hardware setup flow — first-session onboarding.
// Steps are measurement-confirmed; the app does not advance past hardware-gated steps
// until the hub reports the correct condition via GATT notification.

struct SetupView: View {

    @EnvironmentObject private var setup: HardwareSetupManager

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

                if let error = setup.lastError {
                    errorBanner(error.localizedDescription)
                }

                stepSpecificContent
            }
            .padding()
        }
    }

    private var stepIcon: some View {
        Image(systemName: iconName(for: setup.currentStep))
            .font(.system(size: 48))
            .foregroundColor(.accentColor)
            .padding(.top, 8)
    }

    private func iconName(for step: SetupStep) -> String {
        switch step {
        case .welcome:            return "sparkles"
        case .boaDial:            return "dial.high"
        case .electrodePods:      return "brain.head.profile"
        case .zoneModules:        return "rectangle.3.group"
        case .impedanceCheck:     return "waveform.path.ecg"
        case .ads1299Calibration: return "slider.horizontal.3"
        case .hydrationCaps:      return "drop.circle"
        case .complete:           return "checkmark.circle.fill"
        }
    }

    @ViewBuilder
    private var stepSpecificContent: some View {
        switch setup.currentStep {
        case .zoneModules:
            ZoneModuleStatusGrid(configuration: setup.zoneConfiguration)
        case .impedanceCheck:
            ImpedanceStatusGrid(flags: setup.impedanceFlags)
        case .ads1299Calibration:
            if setup.isProcessing {
                ProgressView("Calibrating EEG amplifier…")
                    .frame(maxWidth: .infinity)
            }
        case .complete:
            VStack(spacing: 16) {
                Image(systemName: "checkmark.circle.fill")
                    .font(.system(size: 64))
                    .foregroundColor(.green)
                Text("Your NeuroPulse is ready.")
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
            Text(message)
                .font(.subheadline)
                .foregroundColor(.primary)
        }
        .padding()
        .background(Color.orange.opacity(0.1))
        .clipShape(RoundedRectangle(cornerRadius: 10))
    }

    // MARK: - Navigation controls

    @ViewBuilder
    private var navigationControls: some View {
        if setup.currentStep == .complete {
            Text("Head to the Session tab to begin your first session.")
                .font(.caption)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
        } else if setup.currentStep.requiresHardwareConfirmation {
            hardwareConfirmButton
        } else {
            Button("Continue") { setup.advance() }
                .buttonStyle(.borderedProminent)
                .frame(maxWidth: .infinity)
        }
    }

    @ViewBuilder
    private var hardwareConfirmButton: some View {
        switch setup.currentStep {
        case .impedanceCheck:
            VStack(spacing: 12) {
                if setup.isProcessing {
                    ProgressView("Checking electrode contacts…")
                } else {
                    Button("Check Signal Quality") {
                        Task { await setup.confirmImpedanceCheck() }
                    }
                    .buttonStyle(.borderedProminent)
                    .frame(maxWidth: .infinity)
                }
                if setup.lastError != nil {
                    Button("Retry") { setup.retry() }
                        .buttonStyle(.bordered)
                }
            }
        case .ads1299Calibration:
            if setup.isProcessing {
                ProgressView("Calibrating…")
            } else {
                Button("Calibrate Amplifier") {
                    Task { await setup.confirmADS1299Calibration() }
                }
                .buttonStyle(.borderedProminent)
                .frame(maxWidth: .infinity)
            }
        case .zoneModules:
            Button("Confirm Zone Modules") {
                setup.confirmZoneModules()
            }
            .buttonStyle(.borderedProminent)
            .frame(maxWidth: .infinity)
        default:
            EmptyView()
        }
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
                    Text(channelNames[idx])
                        .font(.caption2)
                        .foregroundColor(.secondary)
                }
            }
        }
        .padding()
        .background(Color(.systemGray6))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }
}
