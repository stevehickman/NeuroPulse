import SwiftUI

// OTA firmware update UI.
// Shows current hub version (ISC-108) and available update from manifest (ISC-107).
// Displays Ed25519 fingerprint before confirmation (ISC-113).
// Progress section shows bytes/total bytes (ISC-110) and reconnect wait (ISC-112).
// Safety MCU update requires an explicit confirmation alert.

struct OTAView: View {

    @EnvironmentObject private var ota:  OTAManager
    @EnvironmentObject private var gatt: NeuroPulseGATTManager

    @State private var showSafetyMCUConfirmation = false

    var body: some View {
        NavigationStack {
            List {
                hubStatusSection
                versionSection
                if let fw = ota.availableUpdate {
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
            .onAppear { ota.checkForUpdates() }
            .alert("Update Safety MCU?", isPresented: $showSafetyMCUConfirmation) {
                Button("Cancel", role: .cancel) {}
                Button("Update Safety MCU", role: .destructive) {
                    if let fw = ota.availableUpdate {
                        Task { try? await ota.beginSafetyMCUUpdate(image: fw) }
                    }
                }
            } message: {
                Text(String(localized: "OTA_SAFETY_MCU_WARNING"))
            }
        }
    }

    // MARK: - Sections

    private var hubStatusSection: some View {
        Section("Hub Status") {
            HStack {
                Image(systemName: gatt.connectionState == .connected ? "wifi" : "wifi.slash")
                    .foregroundColor(gatt.connectionState == .connected ? .green : .secondary)
                    .accessibilityHidden(true)
                Text(gatt.connectionState == .connected ? "Hub connected" : "Hub not connected")
                    .font(.subheadline)
            }
        }
    }

    // ISC-108: show current hub firmware version alongside available version.
    private var versionSection: some View {
        Section("Version") {
            LabeledContent("Installed") {
                Text(gatt.hubFirmwareVersion ?? (gatt.connectionState == .connected ? "Reading…" : "—"))
                    .foregroundColor(.secondary)
            }
            if let available = ota.availableUpdate {
                LabeledContent("Available") {
                    Text(available.version).foregroundColor(.accentColor)
                }
            }
        }
    }

    private var upToDateSection: some View {
        Section("Updates") {
            HStack {
                Image(systemName: "checkmark.circle.fill").foregroundColor(.green)
                Text(String(localized: "OTA_UP_TO_DATE")).font(.subheadline)
            }
        }
    }

    private func availableUpdateSection(_ fw: FirmwareImage) -> some View {
        Section {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text(String(localized: "OTA_VERSION_LABEL").replacingOccurrences(of: "{0}", with: fw.version)).font(.headline)
                    Spacer()
                    Text(fw.buildDate.formatted(.dateTime.month().day().year()))
                        .font(.caption).foregroundColor(.secondary)
                }

                if !fw.releaseNotes.isEmpty {
                    Text(fw.releaseNotes)
                        .font(.caption).foregroundColor(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }

                VStack(alignment: .leading, spacing: 4) {
                    Text(String(localized: "OTA_FINGERPRINT_LABEL"))
                        .font(.caption2).foregroundColor(.secondary)
                    Text(fw.ed25519PublicKeyFingerprint)
                        .font(.system(.caption2, design: .monospaced))
                        .foregroundColor(.secondary)
                }
                .padding(8)
                .background(Color(.systemGray6))
                .clipShape(RoundedRectangle(cornerRadius: 6))

                Text(String(localized: "OTA_FINGERPRINT_VERIFY"))
                    .font(.caption2).foregroundColor(.orange)
                    .fixedSize(horizontal: false, vertical: true)

                if let error = ota.lastError {
                    Text(error.localizedDescription)
                        .font(.caption).foregroundColor(.red)
                        .fixedSize(horizontal: false, vertical: true)
                }
            }

            if fw.isSafetyMCUFirmware {
                Button("Update Safety MCU Firmware") {
                    showSafetyMCUConfirmation = true
                }
                .buttonStyle(.borderedProminent)
                .tint(.red)
                .disabled(!isReadyForUpdate)
            } else {
                Button("Install Update") {
                    Task { try? await ota.beginUpdate(image: fw) }
                }
                .buttonStyle(.borderedProminent)
                .disabled(!isReadyForUpdate)
            }
        } header: {
            Label("Update Available", systemImage: "arrow.down.circle.fill")
                .foregroundColor(.accentColor)
        }
    }

    // ISC-110: bytes/total bytes progress bar.
    // ISC-112: "Update complete — hub will restart" message.
    private func progressSection(_ session: OTASession) -> some View {
        Section("Update Progress") {
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    Text(ota.phase.description).font(.subheadline.bold())
                    Spacer()
                    Text("\(ota.progressPercent)%")
                        .font(.subheadline.monospacedDigit()).foregroundColor(.secondary)
                }

                ProgressView(value: Double(ota.progressPercent), total: 100)
                    .progressViewStyle(.linear)
                    .tint(ota.phase == .failed ? .red : .accentColor)

                // Byte-level progress during transfer (ISC-110).
                if ota.phase == .transferring && session.totalBytes > 0 {
                    Text("\(OTASession.formattedBytes(session.sentBytes)) / \(OTASession.formattedBytes(session.totalBytes))")
                        .font(.caption.monospacedDigit()).foregroundColor(.secondary)
                }

                // ISC-112: applying / complete messaging.
                if ota.phase == .applying {
                    Label("Hub is restarting — do not disconnect.", systemImage: "exclamationmark.triangle.fill")
                        .font(.caption).foregroundColor(.orange)
                } else if ota.phase == .complete {
                    Label("Update complete — hub restarted successfully.", systemImage: "checkmark.circle.fill")
                        .font(.caption).foregroundColor(.green)
                } else if ota.phase == .verifying || ota.phase == .verified {
                    Text(String(localized: "OTA_VERIFYING"))
                        .font(.caption).foregroundColor(.secondary)
                }

                if ota.phase.isBusy && ota.phase != .applying {
                    Button("Abort", role: .destructive) { ota.abort() }
                        .buttonStyle(.bordered).controlSize(.small)
                }
            }
        }
    }

    private var rollbackNotice: some View {
        Section("Safety") {
            Text("If the hub fails to boot after an update, it automatically rolls back"
                + " to the previous firmware version after 3 failed boot attempts."
                + " You can also force USB-C DFU recovery by holding the hub reset"
                + " button during USB-C connection.")
                .font(.caption).foregroundColor(.secondary)
        }
    }

    // MARK: - Helpers

    private var isReadyForUpdate: Bool {
        gatt.connectionState == .connected && !ota.phase.isBusy
    }
}
