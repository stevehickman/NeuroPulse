import SwiftUI

// OTA firmware update UI.
// Shows current hub version (ISC-108) and available update from manifest (ISC-107).
// Displays Ed25519 fingerprint before confirmation (ISC-113).
// Progress section shows bytes/total bytes (ISC-110) and reconnect wait (ISC-112).
// Safety MCU update requires an explicit confirmation alert.

struct OTAView: View {

    @EnvironmentObject private var ota:  OTAManager
    @EnvironmentObject private var gatt: NeurOneGATTManager

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
            .navigationTitle("OTA_FIRMWARE")
            .onAppear { ota.checkForUpdates() }
            .alert("OTA_UPDATE_SAFETY_MCU", isPresented: $showSafetyMCUConfirmation) {
                Button("COMMON_CANCEL", role: .cancel) {}
                Button("OTA_UPDATE_SAFETY_MCU_2", role: .destructive) {
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
        Section("OTA_HUB_STATUS") {
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
        Section("PROTOCOL_VERSION") {
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
        Section("OTA_UPDATES") {
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
                Button("OTA_UPDATE_SAFETY_MCU_FIRMWARE") {
                    showSafetyMCUConfirmation = true
                }
                .buttonStyle(.borderedProminent)
                .tint(.red)
                .disabled(!isReadyForUpdate)
            } else {
                Button("OTA_INSTALL_UPDATE") {
                    Task { try? await ota.beginUpdate(image: fw) }
                }
                .buttonStyle(.borderedProminent)
                .disabled(!isReadyForUpdate)
            }
        } header: {
            Label("OTA_UPDATE_AVAILABLE", systemImage: "arrow.down.circle.fill")
                .foregroundColor(.accentColor)
        }
    }

    // ISC-110: bytes/total bytes progress bar.
    // ISC-112: "Update complete — hub will restart" message.
    private func progressSection(_ session: OTASession) -> some View {
        Section("OTA_UPDATE_PROGRESS") {
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
                    Label("OTA_HUB_IS_RESTARTING_DO_NOT_DISCONNECT", systemImage: "exclamationmark.triangle.fill")
                        .font(.caption).foregroundColor(.orange)
                } else if ota.phase == .complete {
                    Label("OTA_UPDATE_COMPLETE_HUB_RESTARTED_SUCCESSFULLY", systemImage: "checkmark.circle.fill")
                        .font(.caption).foregroundColor(.green)
                } else if ota.phase == .verifying || ota.phase == .verified {
                    Text(String(localized: "OTA_VERIFYING"))
                        .font(.caption).foregroundColor(.secondary)
                }

                if ota.phase.isBusy && ota.phase != .applying {
                    Button("OTA_ABORT", role: .destructive) { ota.abort() }
                        .buttonStyle(.bordered).controlSize(.small)
                }
            }
        }
    }

    private var rollbackNotice: some View {
        Section("OTA_SAFETY") {
            Text("OTA_IF_THE_HUB_FAILS_TO_BOOT_AFTER_AN_UPDATE_IT"
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
