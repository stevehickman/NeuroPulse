import SwiftUI

// OTA firmware update UI.
// Shows available firmware, Ed25519 fingerprint before apply,
// transfer progress, and Safety MCU separate confirmation gate.

struct OTAView: View {

    @EnvironmentObject private var ota:  OTAManager
    @EnvironmentObject private var gatt: NeuroPulseGATTManager

    @State private var showSafetyMCUConfirmation = false
    @State private var availableFirmware: FirmwareImage? = nil
    @State private var availableData: Data? = nil

    var body: some View {
        NavigationStack {
            List {
                connectionSection
                if let fw = availableFirmware {
                    availableUpdateSection(fw)
                } else {
                    upToDateSection
                }
                if let session = ota.currentSession {
                    progressSection(session)
                }
                rollbackNotice
            }
            .listStyle(.insetGrouped)
            .navigationTitle("Firmware")
            .onAppear { checkForUpdates() }
            .alert("Update Safety MCU?", isPresented: $showSafetyMCUConfirmation) {
                Button("Cancel", role: .cancel) {}
                Button("Update Safety MCU", role: .destructive) {
                    if let fw = availableFirmware, let data = availableData {
                        Task { try? await ota.beginSafetyMCUUpdate(image: fw, imageData: data) }
                    }
                }
            } message: {
                Text("The Safety MCU controls all stimulation enable lines. This update will pause all stimulation. Do not disconnect during the update.")
            }
        }
    }

    // MARK: - Sections

    private var connectionSection: some View {
        Section {
            HStack {
                Image(systemName: gatt.connectionState == .connected ? "wifi" : "wifi.slash")
                    .foregroundColor(gatt.connectionState == .connected ? .green : .secondary)
                Text(gatt.connectionState == .connected ? "Hub connected" : "Hub not connected")
                    .font(.subheadline)
            }
        } header: {
            Text("Hub Status")
        }
    }

    private var upToDateSection: some View {
        Section {
            HStack {
                Image(systemName: "checkmark.circle.fill").foregroundColor(.green)
                Text("Firmware is up to date").font(.subheadline)
            }
        } header: {
            Text("Updates")
        }
    }

    private func availableUpdateSection(_ fw: FirmwareImage) -> some View {
        Section {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text("Version \(fw.version)").font(.headline)
                    Spacer()
                    Text(fw.buildDate.formatted(.dateTime.month().day().year()))
                        .font(.caption).foregroundColor(.secondary)
                }
                Text(fw.releaseNotes)
                    .font(.caption).foregroundColor(.secondary)

                VStack(alignment: .leading, spacing: 4) {
                    Text("Security fingerprint (Ed25519):").font(.caption2).foregroundColor(.secondary)
                    Text(fw.ed25519PublicKeyFingerprint)
                        .font(.system(.caption2, design: .monospaced))
                        .foregroundColor(.secondary)
                }
                .padding(8)
                .background(Color(.systemGray6))
                .clipShape(RoundedRectangle(cornerRadius: 6))

                Text("Verify this fingerprint matches your purchase confirmation email before applying.")
                    .font(.caption2).foregroundColor(.orange)

                if let error = ota.lastError {
                    Text(error.localizedDescription)
                        .font(.caption).foregroundColor(.red)
                }
            }

            if fw.isSafetyMCUFirmware {
                Button("Update Safety MCU Firmware") {
                    showSafetyMCUConfirmation = true
                }
                .buttonStyle(.borderedProminent)
                .tint(.red)
                .disabled(!isReadyForUpdate || ota.phase.description != "Ready")
            } else {
                Button("Install Update") {
                    if let data = availableData {
                        Task { try? await ota.beginUpdate(image: fw, imageData: data) }
                    }
                }
                .buttonStyle(.borderedProminent)
                .disabled(!isReadyForUpdate || ota.phase.description != "Ready")
            }
        } header: {
            Label("Update Available", systemImage: "arrow.down.circle.fill")
                .foregroundColor(.accentColor)
        }
    }

    private func progressSection(_ session: OTASession) -> some View {
        Section("Update Progress") {
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    Text(ota.phase.description).font(.subheadline.bold())
                    Spacer()
                    Text("\(ota.progressPercent)%").font(.subheadline).foregroundColor(.secondary)
                }
                ProgressView(value: Double(ota.progressPercent), total: 100)
                    .progressViewStyle(.linear)
                    .tint(ota.phase == .failed ? .red : .accentColor)

                if ota.phase == .applying {
                    Text("Do not disconnect the hub. The device will restart automatically.")
                        .font(.caption).foregroundColor(.orange)
                } else if ota.phase == .complete {
                    Label("Update complete — hub restarted successfully.", systemImage: "checkmark.circle.fill")
                        .font(.caption).foregroundColor(.green)
                }

                if session.isActive && ota.phase != .applying {
                    Button("Abort", role: .destructive) { ota.abort() }
                        .buttonStyle(.bordered).controlSize(.small)
                }
            }
        }
    }

    private var rollbackNotice: some View {
        Section {
            Text("If the hub fails to boot after an update, it will automatically roll back to the previous firmware version. You can also force USB-C DFU recovery from this screen.")
                .font(.caption).foregroundColor(.secondary)
        } header: {
            Text("Safety")
        }
    }

    // MARK: - Helpers

    private var isReadyForUpdate: Bool {
        gatt.connectionState == .connected && !ota.phase.description.contains("…")
    }

    private func checkForUpdates() {
        // Production: fetch firmware manifest from fleet update server.
        // Placeholder: no update available.
        availableFirmware = nil
        availableData = nil
    }
}
