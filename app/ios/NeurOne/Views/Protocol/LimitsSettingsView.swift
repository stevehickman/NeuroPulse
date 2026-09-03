import SwiftUI

// MARK: - LimitLevel (view-level enum mirrors NPLimitsSet.LimitLevel)

private enum LimitTab: String, CaseIterable {
    case global     = "Global"
    case helmet     = "Helmet"
    case individual = "Individual"
}

// MARK: - LimitsSettingsView

struct LimitsSettingsView: View {
    @EnvironmentObject private var limitsStore: NPLimitsStore
    @EnvironmentObject private var library: NPProtocolLibrary
    @Environment(\.dismiss) private var dismiss

    @State private var selectedTab: LimitTab = .global
    @State private var showEditor = false
    @State private var editingLimits: NPLimitsSet = .unlimited
    @State private var onSaveLimits: ((NPLimitsSet) -> Void) = { _ in }

    @State private var showAddHelmet = false
    @State private var newHelmetSerial = ""

    @State private var showAddProfile = false
    @State private var newProfileName = ""

    @State private var showScriptImport = false
    @State private var scriptText = ""
    @State private var scriptError: String? = nil
    @State private var showScriptError = false
    @State private var pendingScriptLevelSave: ((NPLimitsSet) -> Void)? = nil

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                Picker("LIMITS_LEVEL", selection: $selectedTab) {
                    ForEach(LimitTab.allCases, id: \.self) {
                        Text($0.rawValue).tag($0)
                    }
                }
                .pickerStyle(.segmented)
                .padding()

                switch selectedTab {
                case .global:     globalTab
                case .helmet:     helmetTab
                case .individual: individualTab
                }
            }
            .navigationTitle("UI_LIMITS_TITLE")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button(String(localized: "CONSENT_DONE_BUTTON")) { dismiss() }
                }
            }
            .sheet(isPresented: $showEditor) {
                LimitsEditorView(limits: $editingLimits, onSave: onSaveLimits)
                    .environmentObject(limitsStore)
            }
            .sheet(isPresented: $showScriptImport) {
                ScriptImportView(text: $scriptText, error: scriptError) { imported in
                    pendingScriptLevelSave?(imported)
                    pendingScriptLevelSave = nil
                    showScriptImport = false
                }
                .environmentObject(limitsStore)
            }
            .alert("LIMITS_SCRIPT_ERROR", isPresented: $showScriptError) {
                Button("OK", role: .cancel) {}
            } message: {
                Text(scriptError ?? "Unknown error")
            }
            // Add Helmet
            .alert("LIMITS_ADD_HELMET", isPresented: $showAddHelmet) {
                TextField("LIMITS_SERIAL_NUMBER_E_G_NP_001", text: $newHelmetSerial)
                    .autocorrectionDisabled()
                Button("UI_ADD") {
                    guard !newHelmetSerial.trimmingCharacters(in: .whitespaces).isEmpty else { return }
                    let serial = newHelmetSerial.trimmingCharacters(in: .whitespaces)
                    var lim = NPLimitsSet(name: "Helmet \(serial)", level: .helmet)
                    lim.helmetId = serial
                    limitsStore.saveHelmetLimits(lim, forHelmet: serial)
                    newHelmetSerial = ""
                }
                Button("COMMON_CANCEL", role: .cancel) { newHelmetSerial = "" }
            } message: {
                Text("LIMITS_ENTER_THE_HELMET_SERIAL_NUMBER_TO_CONFIGURE")
            }
            // Add Profile
            .alert("LIMITS_ADD_PROFILE", isPresented: $showAddProfile) {
                TextField("LIMITS_PROFILE_NAME", text: $newProfileName)
                Button("UI_ADD") {
                    guard !newProfileName.trimmingCharacters(in: .whitespaces).isEmpty else { return }
                    let profile = NPIndividualProfile(name: newProfileName.trimmingCharacters(in: .whitespaces))
                    limitsStore.saveProfile(profile)
                    newProfileName = ""
                }
                Button("COMMON_CANCEL", role: .cancel) { newProfileName = "" }
            } message: {
                Text("LIMITS_ENTER_A_NAME_FOR_THIS_INDIVIDUAL_PROFILE")
            }
        }
    }

    // MARK: - Global Tab

    private var globalTab: some View {
        List {
            Section("LIMITS_GLOBAL_LIMITS") {
                if let gl = limitsStore.globalLimits {
                    limitsSummaryRow(gl)
                    Button("UI_EDIT_GLOBAL_LIMITS") {
                        editingLimits = gl
                        onSaveLimits = { limitsStore.saveGlobalLimits($0); revalidate() }
                        showEditor = true
                    }
                } else {
                    Text("LIMITS_NO_GLOBAL_LIMITS_CONFIGURED_ALL_HARDWARE_MAX")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                    Button("LIMITS_CONFIGURE_GLOBAL_LIMITS") {
                        editingLimits = NPLimitsSet(name: "Global", level: .global)
                        onSaveLimits = { limitsStore.saveGlobalLimits($0); revalidate() }
                        showEditor = true
                    }
                }
            }

            Section("UI_TAB_SCRIPT") {
                Button("LIMITS_IMPORT_FROM_SCRIPT") {
                    scriptText = ""
                    pendingScriptLevelSave = { limitsStore.saveGlobalLimits($0); revalidate() }
                    showScriptImport = true
                }
                if limitsStore.globalLimits != nil {
                    Button("LIMITS_EXPORT_AS_SCRIPT") {
                        if let gl = limitsStore.globalLimits {
                            scriptText = limitsStore.exportLimitsAsNPPS(gl)
                        }
                    }
                    .disabled(limitsStore.globalLimits == nil)
                }
            }

            resolutionFooter
        }
        .listStyle(.insetGrouped)
    }

    // MARK: - Helmet Tab

    private var helmetTab: some View {
        List {
            Section {
                Button {
                    showAddHelmet = true
                } label: {
                    Label("LIMITS_ADD_HELMET_SERIAL", systemImage: "plus")
                }
            }

            if limitsStore.helmetLimits.isEmpty {
                Section {
                    Text("LIMITS_NO_HELMET_SPECIFIC_LIMITS_CONFIGURED")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
            } else {
                Section("LIMITS_CONFIGURED_HELMETS") {
                    ForEach(Array(limitsStore.helmetLimits.keys).sorted(), id: \.self) { serial in
                        let lim = limitsStore.helmetLimits[serial]!
                        let isConnected = serial == limitsStore.activeHelmetSerial
                        VStack(alignment: .leading, spacing: 4) {
                            HStack {
                                VStack(alignment: .leading) {
                                    HStack(spacing: 6) {
                                        Text(serial)
                                            .font(.headline)
                                        if isConnected {
                                            Text("LIMITS_CONNECTED")
                                                .font(.caption2)
                                                .padding(.horizontal, 6).padding(.vertical, 2)
                                                .background(Color.green.opacity(0.15))
                                                .foregroundColor(.green)
                                                .clipShape(Capsule())
                                        }
                                    }
                                    Text(limitsSummaryText(lim))
                                        .font(.caption)
                                        .foregroundColor(.secondary)
                                }
                                Spacer()
                                Button("UI_EDIT") {
                                    editingLimits = lim
                                    onSaveLimits = { limitsStore.saveHelmetLimits($0, forHelmet: serial); revalidate() }
                                    showEditor = true
                                }
                                .buttonStyle(.borderless)
                                .foregroundColor(.accentColor)
                            }
                        }
                        .swipeActions {
                            Button(role: .destructive) {
                                limitsStore.helmetLimits.removeValue(forKey: serial)
                                revalidate()
                            } label: {
                                Label("UI_DELETE", systemImage: "trash")
                            }
                        }
                    }
                }
            }

            resolutionFooter
        }
        .listStyle(.insetGrouped)
    }

    // MARK: - Individual Tab

    private var individualTab: some View {
        List {
            Section {
                Button {
                    showAddProfile = true
                } label: {
                    Label("LIMITS_ADD_PROFILE", systemImage: "plus")
                }
            }

            if limitsStore.profiles.isEmpty {
                Section {
                    Text("LIMITS_NO_INDIVIDUAL_PROFILES_CONFIGURED")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
            } else {
                if let active = limitsStore.activeProfile {
                    Section("LIMITS_ACTIVE_PROFILE") {
                        profileRow(active, isActive: true)
                    }
                }

                let inactive = limitsStore.profiles.filter { $0.id != limitsStore.activeProfileId }
                if !inactive.isEmpty {
                    Section("LIMITS_PROFILES") {
                        ForEach(inactive) { profile in
                            profileRow(profile, isActive: false)
                        }
                    }
                }
            }

            resolutionFooter
        }
        .listStyle(.insetGrouped)
    }

    private func profileRow(_ profile: NPIndividualProfile, isActive: Bool) -> some View {
        let hasLimits = limitsStore.individualLimits[profile.id] != nil
        return VStack(alignment: .leading, spacing: 4) {
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    HStack(spacing: 6) {
                        Text(profile.name)
                            .font(.headline)
                        if isActive {
                            Text("LIMITS_ACTIVE")
                                .font(.caption2)
                                .padding(.horizontal, 6).padding(.vertical, 2)
                                .background(Color.accentColor.opacity(0.15))
                                .foregroundColor(.accentColor)
                                .clipShape(Capsule())
                        }
                    }
                    if !profile.notes.isEmpty {
                        Text(profile.notes)
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                    Text(hasLimits ? "Individual limits configured" : "No individual limits")
                        .font(.caption2)
                        .foregroundColor(.secondary)
                }
                Spacer()
                VStack(spacing: 6) {
                    Button(isActive ? "Deactivate" : "Set Active") {
                        limitsStore.setActiveProfile(isActive ? nil : profile.id)
                        revalidate()
                    }
                    .buttonStyle(.borderless)
                    .font(.caption)
                    .foregroundColor(isActive ? .secondary : .accentColor)

                    Button("UI_EDIT_LIMITS") {
                        editingLimits = limitsStore.individualLimits[profile.id]
                            ?? NPLimitsSet(name: profile.name, level: .individual)
                        onSaveLimits = { limitsStore.saveIndividualLimits($0, forProfile: profile.id); revalidate() }
                        showEditor = true
                    }
                    .buttonStyle(.borderless)
                    .font(.caption)
                    .foregroundColor(.accentColor)
                }
            }
        }
        .padding(.vertical, 2)
        .swipeActions(edge: .trailing) {
            Button(role: .destructive) {
                limitsStore.deleteProfile(profile.id)
                revalidate()
            } label: {
                Label("UI_DELETE", systemImage: "trash")
            }
        }
    }

    // MARK: - Helpers

    private var resolutionFooter: some View {
        Section {
            Text(limitsStore.resolutionChainDescription)
                .font(.caption)
                .foregroundColor(.secondary)
        }
    }

    private func limitsSummaryRow(_ limits: NPLimitsSet) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(limits.name)
                .font(.subheadline.weight(.medium))
            Text(limitsSummaryText(limits))
                .font(.caption)
                .foregroundColor(.secondary)
        }
    }

    private func limitsSummaryText(_ limits: NPLimitsSet) -> String {
        var parts: [String] = []
        if limits.pbmTranscranial != nil    { parts.append("PBM") }
        if limits.besTacs != nil            { parts.append("BES") }
        if limits.tdcs != nil               { parts.append("tDCS") }
        if limits.vnsHrv != nil             { parts.append("VNS") }
        if limits.visualStimulation != nil  { parts.append("Visual") }
        if limits.tms != nil                { parts.append("TMS") }
        if limits.hdTdcs != nil             { parts.append("HD-tDCS") }
        if limits.cervicalVns != nil        { parts.append("Cervical VNS") }
        if parts.isEmpty { return "No modality limits configured" }
        return "Limits: " + parts.joined(separator: ", ")
    }

    private func revalidate() {
        library.revalidateAll(using: limitsStore)
    }
}

// MARK: - LimitsEditorView

struct LimitsEditorView: View {
    @EnvironmentObject private var limitsStore: NPLimitsStore
    @Binding var limits: NPLimitsSet
    var onSave: (NPLimitsSet) -> Void
    @Environment(\.dismiss) private var dismiss

    enum EditorMode: String, CaseIterable {
        case visual = "Visual"
        case script = "Script"
    }
    @State private var viewMode: EditorMode = .visual
    @State private var scriptText: String = ""
    @State private var scriptError: String? = nil
    @State private var workingLimits: NPLimitsSet = .unlimited

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                Picker("VALIDATE_PARAM_MODE", selection: $viewMode) {
                    ForEach(EditorMode.allCases, id: \.self) {
                        Text($0.rawValue).tag($0)
                    }
                }
                .pickerStyle(.segmented)
                .padding()

                if viewMode == .visual {
                    visualEditor
                } else {
                    scriptEditor
                }
            }
            .navigationTitle("UI_EDIT_LIMITS")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(String(localized: "COMMON_CANCEL")) { dismiss() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("LIMITS_SAVE") {
                        onSave(workingLimits)
                        dismiss()
                    }
                }
            }
            .onAppear {
                workingLimits = limits
                scriptText = limitsStore.exportLimitsAsNPPS(limits)
            }
            .onChange(of: viewMode) { _, newMode in
                if newMode == .script {
                    scriptText = limitsStore.exportLimitsAsNPPS(workingLimits)
                    scriptError = nil
                }
            }
        }
    }

    // MARK: Visual editor

    private var visualEditor: some View {
        Form {
            Section("CLINICIAN_GRANT_NAME_PLACEHOLDER") {
                TextField("UI_LIMITS_NAME_PLACEHOLDER", text: $workingLimits.name)
                TextField("UI_LIMITS_DESC_PLACEHOLDER", text: $workingLimits.description)
            }

            pbmTranscranialSection
            pbmIntranasalSection
            eegSection
            besTacsSection
            tdcsSection
            vnsHrvSection
            audioSection
            visualSection
            tmsSection
            deepPBMSection
            clinicalTacsSection
            hdTdcsSection
            cervicalVnsSection
            vibrotactileSection
        }
    }

    // MARK: Script editor

    private var scriptEditor: some View {
        VStack(spacing: 0) {
            if let err = scriptError {
                HStack {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundColor(.red)
                    Text(err)
                        .font(.caption)
                        .foregroundColor(.red)
                }
                .padding(.horizontal)
                .padding(.vertical, 6)
                .background(Color.red.opacity(0.1))
            }
            TextEditor(text: $scriptText)
                .font(.system(.caption, design: .monospaced))
                .autocorrectionDisabled()
                .textInputAutocapitalization(.never)
                .onChange(of: scriptText) { _, newText in
                    do {
                        let imported = try limitsStore.importLimitsFromNPPS(newText)
                        workingLimits = imported
                        scriptError = nil
                    } catch {
                        scriptError = error.localizedDescription
                    }
                }
        }
    }

    // MARK: Modality sections

    private var pbmTranscranialSection: some View {
        ModalityLimitsSection(
            title: "Transcranial PBM",
            isEnabled: Binding(
                get: { workingLimits.pbmTranscranial != nil },
                set: { if $0 { workingLimits.pbmTranscranial = NPPBMTranscranialLimits() } else { workingLimits.pbmTranscranial = nil } }
            )
        ) {
            if workingLimits.pbmTranscranial != nil {
                OptionalDoubleField(label: "Max Intensity", unit: "%", value: Binding(
                    get: { workingLimits.pbmTranscranial?.maxIntensityPercent },
                    set: { workingLimits.pbmTranscranial?.maxIntensityPercent = $0 }
                ), range: 0...100)
                OptionalDoubleField(label: "Max Frequency", unit: "Hz", value: Binding(
                    get: { workingLimits.pbmTranscranial?.maxFrequencyHz },
                    set: { workingLimits.pbmTranscranial?.maxFrequencyHz = $0 }
                ), range: 0...100)
                OptionalIntField(label: "Max Duty Cycle", unit: "%", value: Binding(
                    get: { workingLimits.pbmTranscranial?.maxDutyCyclePercent },
                    set: { workingLimits.pbmTranscranial?.maxDutyCyclePercent = $0 }
                ), range: 0...25)
                OptionalDoubleField(label: "Max Session Dose", unit: "J/cm²", value: Binding(
                    get: { workingLimits.pbmTranscranial?.maxSessionDoseJCm2 },
                    set: { workingLimits.pbmTranscranial?.maxSessionDoseJCm2 = $0 }
                ), range: 0...1000)
                OptionalDoubleField(label: "Max Daily Dose", unit: "J/cm²", value: Binding(
                    get: { workingLimits.pbmTranscranial?.maxDailyDoseJCm2 },
                    set: { workingLimits.pbmTranscranial?.maxDailyDoseJCm2 = $0 }
                ), range: 0...5000)
            }
        }
    }

    private var pbmIntranasalSection: some View {
        ModalityLimitsSection(
            title: "Intranasal PBM",
            isEnabled: Binding(
                get: { workingLimits.pbmIntranasal != nil },
                set: { if $0 { workingLimits.pbmIntranasal = NPPBMIntranasalLimits() } else { workingLimits.pbmIntranasal = nil } }
            )
        ) {
            if workingLimits.pbmIntranasal != nil {
                OptionalDoubleField(label: "Max Intensity", unit: "%", value: Binding(
                    get: { workingLimits.pbmIntranasal?.maxIntensityPercent },
                    set: { workingLimits.pbmIntranasal?.maxIntensityPercent = $0 }
                ), range: 0...100)
                OptionalDoubleField(label: "Max Session Dose", unit: "J/cm²", value: Binding(
                    get: { workingLimits.pbmIntranasal?.maxSessionDoseJCm2 },
                    set: { workingLimits.pbmIntranasal?.maxSessionDoseJCm2 = $0 }
                ), range: 0...500)
                OptionalDurationField(label: "Max Session Duration", value: Binding(
                    get: { workingLimits.pbmIntranasal?.maxSessionDurationSeconds },
                    set: { workingLimits.pbmIntranasal?.maxSessionDurationSeconds = $0 }
                ))
            }
        }
    }

    private var eegSection: some View {
        ModalityLimitsSection(
            title: "EEG Neurofeedback",
            isEnabled: Binding(
                get: { workingLimits.eegNeurofeedback != nil },
                set: { if $0 { workingLimits.eegNeurofeedback = NPEEGNeurofeedbackLimits() } else { workingLimits.eegNeurofeedback = nil } }
            )
        ) {
            if workingLimits.eegNeurofeedback != nil {
                OptionalBoolField(label: "Require Closed Loop", value: Binding(
                    get: { workingLimits.eegNeurofeedback?.requireClosedLoop },
                    set: { workingLimits.eegNeurofeedback?.requireClosedLoop = $0 }
                ))
                AllowedBandsField(value: Binding(
                    get: { workingLimits.eegNeurofeedback?.allowedBands },
                    set: { workingLimits.eegNeurofeedback?.allowedBands = $0 }
                ))
            }
        }
    }

    private var besTacsSection: some View {
        ModalityLimitsSection(
            title: "Brainwave Entrainment (BES/tACS)",
            isEnabled: Binding(
                get: { workingLimits.besTacs != nil },
                set: { if $0 { workingLimits.besTacs = NPBESTacsLimits() } else { workingLimits.besTacs = nil } }
            )
        ) {
            if workingLimits.besTacs != nil {
                OptionalDoubleField(label: "Max Intensity", unit: "mA", value: Binding(
                    get: { workingLimits.besTacs?.maxIntensityMilliamps },
                    set: { workingLimits.besTacs?.maxIntensityMilliamps = $0 }
                ), range: 0...1.0)
                OptionalDoubleField(label: "Max Frequency", unit: "Hz", value: Binding(
                    get: { workingLimits.besTacs?.maxFrequencyHz },
                    set: { workingLimits.besTacs?.maxFrequencyHz = $0 }
                ), range: 0.5...40)
                OptionalDoubleField(label: "Min Frequency", unit: "Hz", value: Binding(
                    get: { workingLimits.besTacs?.minFrequencyHz },
                    set: { workingLimits.besTacs?.minFrequencyHz = $0 }
                ), range: 0.5...40)
                OptionalDurationField(label: "Max Session Duration", value: Binding(
                    get: { workingLimits.besTacs?.maxSessionDurationSeconds },
                    set: { workingLimits.besTacs?.maxSessionDurationSeconds = $0 }
                ))
                OptionalIntField(label: "Max Sessions/Day", unit: "", value: Binding(
                    get: { workingLimits.besTacs?.maxSessionsPerDay },
                    set: { workingLimits.besTacs?.maxSessionsPerDay = $0 }
                ), range: 1...10)
            }
        }
    }

    private var tdcsSection: some View {
        ModalityLimitsSection(
            title: "Cortical Priming (tDCS)",
            isEnabled: Binding(
                get: { workingLimits.tdcs != nil },
                set: { if $0 { workingLimits.tdcs = NPTDCSLimits() } else { workingLimits.tdcs = nil } }
            )
        ) {
            if workingLimits.tdcs != nil {
                OptionalDoubleField(label: "Max Intensity", unit: "mA", value: Binding(
                    get: { workingLimits.tdcs?.maxIntensityMilliamps },
                    set: { workingLimits.tdcs?.maxIntensityMilliamps = $0 }
                ), range: 0.1...2.0)
                OptionalDurationField(label: "Max Session Duration", value: Binding(
                    get: { workingLimits.tdcs?.maxSessionDurationSeconds },
                    set: { workingLimits.tdcs?.maxSessionDurationSeconds = $0 }
                ))
                OptionalIntField(label: "Max Sessions/Day", unit: "", value: Binding(
                    get: { workingLimits.tdcs?.maxSessionsPerDay },
                    set: { workingLimits.tdcs?.maxSessionsPerDay = $0 }
                ), range: 1...5)
            }
        }
    }

    private var vnsHrvSection: some View {
        ModalityLimitsSection(
            title: "VNS + HRV Biofeedback",
            isEnabled: Binding(
                get: { workingLimits.vnsHrv != nil },
                set: { if $0 { workingLimits.vnsHrv = NPVNSHRVLimits() } else { workingLimits.vnsHrv = nil } }
            )
        ) {
            if workingLimits.vnsHrv != nil {
                OptionalDoubleField(label: "Max Intensity", unit: "mA", value: Binding(
                    get: { workingLimits.vnsHrv?.maxIntensityMilliamps },
                    set: { workingLimits.vnsHrv?.maxIntensityMilliamps = $0 }
                ), range: 0...2.0)
                OptionalDoubleField(label: "Max Frequency", unit: "Hz", value: Binding(
                    get: { workingLimits.vnsHrv?.maxFrequencyHz },
                    set: { workingLimits.vnsHrv?.maxFrequencyHz = $0 }
                ), range: 1...25)
                OptionalDurationField(label: "Max Session Duration", value: Binding(
                    get: { workingLimits.vnsHrv?.maxSessionDurationSeconds },
                    set: { workingLimits.vnsHrv?.maxSessionDurationSeconds = $0 }
                ))
                AllowedVNSProtocolsField(value: Binding(
                    get: { workingLimits.vnsHrv?.allowedProtocols },
                    set: { workingLimits.vnsHrv?.allowedProtocols = $0 }
                ))
            }
        }
    }

    private var audioSection: some View {
        ModalityLimitsSection(
            title: "Neural Audio Entrainment",
            isEnabled: Binding(
                get: { workingLimits.audioEntrainment != nil },
                set: { if $0 { workingLimits.audioEntrainment = NPAudioEntrainmentLimits() } else { workingLimits.audioEntrainment = nil } }
            )
        ) {
            if workingLimits.audioEntrainment != nil {
                OptionalDoubleField(label: "Max Volume", unit: "%", value: Binding(
                    get: { workingLimits.audioEntrainment?.maxVolumePercent },
                    set: { workingLimits.audioEntrainment?.maxVolumePercent = $0 }
                ), range: 0...100)
                OptionalDoubleField(label: "Max Binaural Beats", unit: "Hz", value: Binding(
                    get: { workingLimits.audioEntrainment?.maxBinauralBeatsHz },
                    set: { workingLimits.audioEntrainment?.maxBinauralBeatsHz = $0 }
                ), range: 0...100)
                OptionalDoubleField(label: "Max Isochronic Tones", unit: "Hz", value: Binding(
                    get: { workingLimits.audioEntrainment?.maxIsochronicTonesHz },
                    set: { workingLimits.audioEntrainment?.maxIsochronicTonesHz = $0 }
                ), range: 0...100)
            }
        }
    }

    private var visualSection: some View {
        ModalityLimitsSection(
            title: "Visual Stimulation",
            isEnabled: Binding(
                get: { workingLimits.visualStimulation != nil },
                set: { if $0 { workingLimits.visualStimulation = NPVisualStimLimits() } else { workingLimits.visualStimulation = nil } }
            )
        ) {
            if workingLimits.visualStimulation != nil {
                OptionalDoubleField(label: "Max Frequency", unit: "Hz", value: Binding(
                    get: { workingLimits.visualStimulation?.maxFrequencyHz },
                    set: { workingLimits.visualStimulation?.maxFrequencyHz = $0 }
                ), range: 0...100)
                OptionalDoubleField(label: "Min Frequency", unit: "Hz", value: Binding(
                    get: { workingLimits.visualStimulation?.minFrequencyHz },
                    set: { workingLimits.visualStimulation?.minFrequencyHz = $0 }
                ), range: 0...100)
                OptionalBoolField(label: "Block 3–30 Hz Risk Range", value: Binding(
                    get: { workingLimits.visualStimulation?.blockHighRiskRange },
                    set: { workingLimits.visualStimulation?.blockHighRiskRange = $0 }
                ))
                AllowedVisualModesField(value: Binding(
                    get: { workingLimits.visualStimulation?.allowedModes },
                    set: { workingLimits.visualStimulation?.allowedModes = $0 }
                ))
            }
        }
    }

    private var tmsSection: some View {
        ModalityLimitsSection(
            title: "TMS (T2)",
            isEnabled: Binding(
                get: { workingLimits.tms != nil },
                set: { if $0 { workingLimits.tms = NPTMSLimits() } else { workingLimits.tms = nil } }
            )
        ) {
            if workingLimits.tms != nil {
                OptionalIntField(label: "Max Intensity (%MT)", unit: "%MT", value: Binding(
                    get: { workingLimits.tms?.maxIntensityPercentMT },
                    set: { workingLimits.tms?.maxIntensityPercentMT = $0 }
                ), range: 0...150)
                OptionalIntField(label: "Max Pulses/Session", unit: "", value: Binding(
                    get: { workingLimits.tms?.maxPulsesPerSession },
                    set: { workingLimits.tms?.maxPulsesPerSession = $0 }
                ), range: 0...5000)
                OptionalIntField(label: "Max Pulses/Day", unit: "", value: Binding(
                    get: { workingLimits.tms?.maxPulsesPerDay },
                    set: { workingLimits.tms?.maxPulsesPerDay = $0 }
                ), range: 0...10000)
                OptionalIntField(label: "Max Sessions/Week", unit: "", value: Binding(
                    get: { workingLimits.tms?.maxSessionsPerWeek },
                    set: { workingLimits.tms?.maxSessionsPerWeek = $0 }
                ), range: 0...7)
            }
        }
    }

    private var deepPBMSection: some View {
        ModalityLimitsSection(
            title: "Deep PBM 1170nm (T2)",
            isEnabled: Binding(
                get: { workingLimits.pbmDeep1170nm != nil },
                set: { if $0 { workingLimits.pbmDeep1170nm = NPDeepPBMLimits() } else { workingLimits.pbmDeep1170nm = nil } }
            )
        ) {
            if workingLimits.pbmDeep1170nm != nil {
                OptionalDoubleField(label: "Max Intensity", unit: "mW/cm²", value: Binding(
                    get: { workingLimits.pbmDeep1170nm?.maxIntensityMWcm2 },
                    set: { workingLimits.pbmDeep1170nm?.maxIntensityMWcm2 = $0 }
                ), range: 0...1000)
                OptionalDurationField(label: "Max Session Duration", value: Binding(
                    get: { workingLimits.pbmDeep1170nm?.maxSessionDurationSeconds },
                    set: { workingLimits.pbmDeep1170nm?.maxSessionDurationSeconds = $0 }
                ))
            }
        }
    }

    private var clinicalTacsSection: some View {
        ModalityLimitsSection(
            title: "Clinical tACS (T2)",
            isEnabled: Binding(
                get: { workingLimits.clinicalTacs != nil },
                set: { if $0 { workingLimits.clinicalTacs = NPClinicalTacsLimits() } else { workingLimits.clinicalTacs = nil } }
            )
        ) {
            if workingLimits.clinicalTacs != nil {
                OptionalDoubleField(label: "Max Intensity", unit: "mA", value: Binding(
                    get: { workingLimits.clinicalTacs?.maxIntensityMilliamps },
                    set: { workingLimits.clinicalTacs?.maxIntensityMilliamps = $0 }
                ), range: 0...4.0)
                OptionalDurationField(label: "Max Session Duration", value: Binding(
                    get: { workingLimits.clinicalTacs?.maxSessionDurationSeconds },
                    set: { workingLimits.clinicalTacs?.maxSessionDurationSeconds = $0 }
                ))
            }
        }
    }

    private var hdTdcsSection: some View {
        ModalityLimitsSection(
            title: "HD-tDCS (T2)",
            isEnabled: Binding(
                get: { workingLimits.hdTdcs != nil },
                set: { if $0 { workingLimits.hdTdcs = NPHDTdcsLimits() } else { workingLimits.hdTdcs = nil } }
            )
        ) {
            if workingLimits.hdTdcs != nil {
                OptionalDoubleField(label: "Max Intensity/Electrode", unit: "mA", value: Binding(
                    get: { workingLimits.hdTdcs?.maxIntensityMilliamps },
                    set: { workingLimits.hdTdcs?.maxIntensityMilliamps = $0 }
                ), range: 0...2.0)
                OptionalDurationField(label: "Max Session Duration", value: Binding(
                    get: { workingLimits.hdTdcs?.maxSessionDurationSeconds },
                    set: { workingLimits.hdTdcs?.maxSessionDurationSeconds = $0 }
                ))
                AllowedHDTdcsMontagesField(value: Binding(
                    get: { workingLimits.hdTdcs?.allowedMontages },
                    set: { workingLimits.hdTdcs?.allowedMontages = $0 }
                ))
            }
        }
    }

    private var cervicalVnsSection: some View {
        ModalityLimitsSection(
            title: "Cervical VNS (T2)",
            isEnabled: Binding(
                get: { workingLimits.cervicalVns != nil },
                set: { if $0 { workingLimits.cervicalVns = NPCervicalVnsLimits() } else { workingLimits.cervicalVns = nil } }
            )
        ) {
            if workingLimits.cervicalVns != nil {
                OptionalDoubleField(label: "Max Intensity", unit: "mA", value: Binding(
                    get: { workingLimits.cervicalVns?.maxIntensityMilliamps },
                    set: { workingLimits.cervicalVns?.maxIntensityMilliamps = $0 }
                ), range: 0...2.0)
                OptionalDurationField(label: "Max Session Duration", value: Binding(
                    get: { workingLimits.cervicalVns?.maxSessionDurationSeconds },
                    set: { workingLimits.cervicalVns?.maxSessionDurationSeconds = $0 }
                ))
                Text("LIMITS_CARDIAC_INTERLOCK_IS_ALWAYS_ENFORCED_BY_THE")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
        }
    }

    private var vibrotactileSection: some View {
        ModalityLimitsSection(
            title: "40Hz Vibrotactile (Accessory)",
            isEnabled: Binding(
                get: { workingLimits.vibrotactile40hz != nil },
                set: { if $0 { workingLimits.vibrotactile40hz = NPVibrotactileLimits() } else { workingLimits.vibrotactile40hz = nil } }
            )
        ) {
            if workingLimits.vibrotactile40hz != nil {
                OptionalDoubleField(label: "Max Intensity", unit: "G", value: Binding(
                    get: { workingLimits.vibrotactile40hz?.maxIntensityG },
                    set: { workingLimits.vibrotactile40hz?.maxIntensityG = $0 }
                ), range: 0.6...1.2)
                OptionalDurationField(label: "Max Session Duration", value: Binding(
                    get: { workingLimits.vibrotactile40hz?.maxSessionDurationSeconds },
                    set: { workingLimits.vibrotactile40hz?.maxSessionDurationSeconds = $0 }
                ))
            }
        }
    }
}

// MARK: - ModalityLimitsSection

struct ModalityLimitsSection<Content: View>: View {
    let title: String
    @Binding var isEnabled: Bool
    @ViewBuilder let content: () -> Content

    var body: some View {
        Section {
            Toggle(title, isOn: $isEnabled)
                .font(.subheadline.weight(.medium))
            if isEnabled {
                content()
            }
        }
    }
}

// MARK: - Reusable field components

struct OptionalDoubleField: View {
    let label: String
    let unit: String
    @Binding var value: Double?
    var range: ClosedRange<Double> = 0...1000

    @State private var text: String = ""
    @FocusState private var focused: Bool

    var body: some View {
        HStack {
            Text(label)
            Spacer()
            if value == nil {
                Button("LIMITS_SET_LIMIT") { value = (range.lowerBound + range.upperBound) / 2; syncText() }
                    .font(.caption)
                    .foregroundColor(.accentColor)
            } else {
                TextField("LIMITS_NO_LIMIT", text: $text)
                    .keyboardType(.decimalPad)
                    .multilineTextAlignment(.trailing)
                    .frame(width: 80)
                    .focused($focused)
                    .onChange(of: text) { _, v in value = Double(v) }
                    .onChange(of: focused) { _, isFocused in
                        guard !isFocused else { return }
                        if let v = value {
                            value = max(range.lowerBound, min(v, range.upperBound))
                        }
                        syncText()
                    }
                    .onAppear { syncText() }
                Text(unit)
                    .foregroundColor(.secondary)
                    .font(.caption)
                Button {
                    value = nil; text = ""
                } label: {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundColor(.secondary)
                }
                .buttonStyle(.plain)
            }
        }
    }

    private func syncText() {
        if let v = value { text = String(format: "%g", v) } else { text = "" }
    }
}

struct OptionalIntField: View {
    let label: String
    let unit: String
    @Binding var value: Int?
    var range: ClosedRange<Int> = 0...1000

    @State private var text: String = ""
    @FocusState private var focused: Bool

    var body: some View {
        HStack {
            Text(label)
            Spacer()
            if value == nil {
                Button("LIMITS_SET_LIMIT") { value = (range.lowerBound + range.upperBound) / 2; syncText() }
                    .font(.caption)
                    .foregroundColor(.accentColor)
            } else {
                TextField("LIMITS_NO_LIMIT", text: $text)
                    .keyboardType(.numberPad)
                    .multilineTextAlignment(.trailing)
                    .frame(width: 70)
                    .focused($focused)
                    .onChange(of: text) { _, v in value = Int(v) }
                    .onChange(of: focused) { _, isFocused in
                        guard !isFocused else { return }
                        if let v = value {
                            value = max(range.lowerBound, min(v, range.upperBound))
                        }
                        syncText()
                    }
                    .onAppear { syncText() }
                if !unit.isEmpty {
                    Text(unit)
                        .foregroundColor(.secondary)
                        .font(.caption)
                }
                Button {
                    value = nil; text = ""
                } label: {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundColor(.secondary)
                }
                .buttonStyle(.plain)
            }
        }
    }

    private func syncText() {
        if let v = value { text = "\(v)" } else { text = "" }
    }
}

struct OptionalDurationField: View {
    let label: String
    @Binding var value: Int?   // seconds

    @State private var text: String = ""

    var body: some View {
        HStack {
            Text(label)
            Spacer()
            if value == nil {
                Button("LIMITS_SET_LIMIT") { value = 1200; syncText() }
                    .font(.caption)
                    .foregroundColor(.accentColor)
            } else {
                TextField("LIMITS_NO_LIMIT", text: $text)
                    .keyboardType(.default)
                    .multilineTextAlignment(.trailing)
                    .frame(width: 90)
                    .onChange(of: text) { _, v in value = parseDuration(v) }
                    .onAppear { syncText() }
                Text("(e.g. 20m)")
                    .foregroundColor(.secondary)
                    .font(.caption2)
                Button {
                    value = nil; text = ""
                } label: {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundColor(.secondary)
                }
                .buttonStyle(.plain)
            }
        }
    }

    private func syncText() {
        guard let v = value else { text = ""; return }
        if v % 3600 == 0 { text = "\(v/3600)h" }
        else if v % 60 == 0 { text = "\(v/60)m" }
        else { text = "\(v)s" }
    }

    private func parseDuration(_ s: String) -> Int? {
        let trimmed = s.trimmingCharacters(in: .whitespaces)
        if trimmed.hasSuffix("h"), let n = Double(trimmed.dropLast()) { return Int(n * 3600) }
        if trimmed.hasSuffix("m"), let n = Double(trimmed.dropLast()) { return Int(n * 60) }
        if trimmed.hasSuffix("s"), let n = Double(trimmed.dropLast()) { return Int(n) }
        if let n = Double(trimmed) { return Int(n) }
        return nil
    }
}

struct OptionalBoolField: View {
    let label: String
    @Binding var value: Bool?

    var body: some View {
        HStack {
            Text(label)
            Spacer()
            if let v = value {
                Toggle("", isOn: Binding(get: { v }, set: { value = $0 }))
                    .labelsHidden()
                Button {
                    value = nil
                } label: {
                    Image(systemName: "xmark.circle.fill")
                        .foregroundColor(.secondary)
                }
                .buttonStyle(.plain)
            } else {
                Button("LIMITS_SET_LIMIT") { value = true }
                    .font(.caption)
                    .foregroundColor(.accentColor)
            }
        }
    }
}

// MARK: - Whitelist multi-select fields

struct AllowedBandsField: View {
    @Binding var value: [String]?
    private let allBands = NPEEGNeurofeedbackParams.EEGBand.allCases

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text("LIMITS_ALLOWED_EEG_BANDS")
                Spacer()
                if value == nil {
                    Button("LIMITS_RESTRICT") { value = allBands.map { $0.rawValue } }
                        .font(.caption).foregroundColor(.accentColor)
                } else {
                    Button("LIMITS_ALLOW_ALL") { value = nil }
                        .font(.caption).foregroundColor(.secondary)
                }
            }
            if let allowed = value {
                ForEach(allBands, id: \.self) { band in
                    Toggle(band.displayName, isOn: Binding(
                        get: { allowed.contains(band.rawValue) },
                        set: { on in
                            if on { value?.append(band.rawValue) }
                            else  { value?.removeAll { $0 == band.rawValue } }
                        }
                    ))
                    .font(.caption)
                }
            }
        }
    }
}

struct AllowedVNSProtocolsField: View {
    @Binding var value: [String]?
    private let allProtos = NPVNSHRVParams.HRVProtocol.allCases

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text("UI_LIM_ALLOWED_HRV")
                Spacer()
                if value == nil {
                    Button("LIMITS_RESTRICT") { value = allProtos.map { $0.rawValue } }
                        .font(.caption).foregroundColor(.accentColor)
                } else {
                    Button("LIMITS_ALLOW_ALL") { value = nil }
                        .font(.caption).foregroundColor(.secondary)
                }
            }
            if let allowed = value {
                ForEach(allProtos, id: \.self) { proto in
                    Toggle(proto.displayName, isOn: Binding(
                        get: { allowed.contains(proto.rawValue) },
                        set: { on in
                            if on { value?.append(proto.rawValue) }
                            else  { value?.removeAll { $0 == proto.rawValue } }
                        }
                    ))
                    .font(.caption)
                }
            }
        }
    }
}

struct AllowedVisualModesField: View {
    @Binding var value: [String]?
    private let allModes = NPVisualStimParams.VisualMode.allCases

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text("LIMITS_ALLOWED_VISUAL_MODES")
                Spacer()
                if value == nil {
                    Button("LIMITS_RESTRICT") { value = allModes.map { $0.rawValue } }
                        .font(.caption).foregroundColor(.accentColor)
                } else {
                    Button("LIMITS_ALLOW_ALL") { value = nil }
                        .font(.caption).foregroundColor(.secondary)
                }
            }
            if let allowed = value {
                ForEach(allModes, id: \.self) { mode in
                    Toggle(mode.displayName, isOn: Binding(
                        get: { allowed.contains(mode.rawValue) },
                        set: { on in
                            if on { value?.append(mode.rawValue) }
                            else  { value?.removeAll { $0 == mode.rawValue } }
                        }
                    ))
                    .font(.caption)
                }
            }
        }
    }
}

struct AllowedHDTdcsMontagesField: View {
    @Binding var value: [String]?
    private let allMontages = NPHDTdcsParams.Montage.allCases

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack {
                Text("UI_LIM_ALLOWED_MONTAGES")
                Spacer()
                if value == nil {
                    Button("LIMITS_RESTRICT") { value = allMontages.map { $0.rawValue } }
                        .font(.caption).foregroundColor(.accentColor)
                } else {
                    Button("LIMITS_ALLOW_ALL") { value = nil }
                        .font(.caption).foregroundColor(.secondary)
                }
            }
            if let allowed = value {
                ForEach(allMontages, id: \.self) { montage in
                    Toggle(montage.displayName, isOn: Binding(
                        get: { allowed.contains(montage.rawValue) },
                        set: { on in
                            if on { value?.append(montage.rawValue) }
                            else  { value?.removeAll { $0 == montage.rawValue } }
                        }
                    ))
                    .font(.caption)
                }
            }
        }
    }
}

// MARK: - Script Import View

struct ScriptImportView: View {
    @EnvironmentObject private var limitsStore: NPLimitsStore
    @Binding var text: String
    var error: String?
    var onImport: (NPLimitsSet) -> Void
    @Environment(\.dismiss) private var dismiss

    @State private var parseError: String? = nil
    @State private var parsedLimits: NPLimitsSet? = nil

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                if let err = parseError {
                    HStack {
                        Image(systemName: "exclamationmark.triangle.fill").foregroundColor(.red)
                        Text(err).font(.caption).foregroundColor(.red)
                    }
                    .padding(.horizontal).padding(.vertical, 6)
                    .background(Color.red.opacity(0.1))
                }
                if parsedLimits != nil {
                    HStack {
                        Image(systemName: "checkmark.circle.fill").foregroundColor(.green)
                        Text("LIMITS_LIMITS_PARSED_SUCCESSFULLY").font(.caption).foregroundColor(.green)
                    }
                    .padding(.horizontal).padding(.vertical, 6)
                    .background(Color.green.opacity(0.1))
                }
                TextEditor(text: $text)
                    .font(.system(.caption, design: .monospaced))
                    .autocorrectionDisabled()
                    .textInputAutocapitalization(.never)
                    .onChange(of: text) { _, newText in
                        do {
                            parsedLimits = try limitsStore.importLimitsFromNPPS(newText)
                            parseError = nil
                        } catch {
                            parsedLimits = nil
                            parseError = error.localizedDescription
                        }
                    }
            }
            .navigationTitle("LIMITS_IMPORT_LIMITS_SCRIPT")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(String(localized: "COMMON_CANCEL")) { dismiss() }
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("LIMITS_IMPORT") {
                        if let l = parsedLimits { onImport(l) }
                        dismiss()
                    }
                    .disabled(parsedLimits == nil)
                }
            }
        }
    }
}
