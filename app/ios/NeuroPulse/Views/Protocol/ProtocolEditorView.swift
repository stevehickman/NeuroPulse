import SwiftUI

// MARK: - Editor View Mode

enum EditorViewMode: String, CaseIterable {
    case visual = "Visual"
    case script = "Script"
}

// MARK: - ProtocolEditorView

struct ProtocolEditorView: View {
    @Environment(\.dismiss) private var dismiss
    @EnvironmentObject private var library: NPProtocolLibrary
    @EnvironmentObject private var limitsStore: NPLimitsStore

    let existing: NPProtocolEntry?

    @State private var draft: NPProtocolDefinition
    @State private var viewMode: EditorViewMode = .visual
    @State private var scriptText: String = ""
    @State private var scriptError: String? = nil
    @State private var showAddModality = false
    @State private var saveError: String? = nil
    @State private var showSaveError = false
    @State private var expandedModality: UUID? = nil

    init(existing: NPProtocolEntry?) {
        self.existing = existing
        if let existing = existing, case .single(let proto) = existing {
            _draft = State(initialValue: proto)
        } else {
            _draft = State(initialValue: NPProtocolDefinition(name: ""))
        }
    }

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                Picker("View Mode", selection: $viewMode) {
                    ForEach(EditorViewMode.allCases, id: \.self) {
                        Text($0.rawValue).tag($0)
                    }
                }
                .pickerStyle(.segmented)
                .padding(.horizontal)
                .padding(.top, 8)

                Divider().padding(.top, 8)

                switch viewMode {
                case .visual:
                    visualEditor
                case .script:
                    scriptEditor
                }
            }
            .navigationTitle(existing == nil ? "New Protocol" : "Edit Protocol")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { toolbarContent }
            .sheet(isPresented: $showAddModality) {
                ModalityPickerSheet(existingTypes: Set(draft.modalities.map { $0.modalityType })) { type in
                    let modality = NPProtocolModality(
                        params: .defaultParams(for: type),
                        interval: .continuous,
                        enabled: true
                    )
                    draft.modalities.append(modality)
                    expandedModality = modality.id
                }
            }
            .alert("Cannot Save", isPresented: $showSaveError) {
                Button("OK", role: .cancel) {}
            } message: {
                Text(saveError ?? "Please fill in required fields.")
            }
            .onChange(of: viewMode) { _, newMode in
                if newMode == .script {
                    // Sync draft → script
                    scriptText = NPPSSerializer().serialize(.single(draft))
                    scriptError = nil
                } else {
                    // Sync script → draft (if valid)
                    syncScriptToDraft()
                }
            }
        }
    }

    // MARK: - Toolbar

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItem(placement: .cancellationAction) {
            Button("Cancel") { dismiss() }
        }
        ToolbarItem(placement: .confirmationAction) {
            Button("Save") { attemptSave() }
        }
    }

    // MARK: - Visual Editor

    private var visualEditor: some View {
        Form {
            identitySection
            timingSection
            modalitiesSection
        }
    }

    // MARK: Identity section

    private var identitySection: some View {
        Section("Identity") {
            TextField("Protocol Name", text: $draft.name)
                .autocorrectionDisabled()

            TextField("Description", text: $draft.description, axis: .vertical)
                .lineLimit(2...4)

            HStack {
                Text("Author")
                    .foregroundColor(.secondary)
                Spacer()
                TextField("NeuroPulse", text: $draft.author)
                    .multilineTextAlignment(.trailing)
            }

            HStack {
                Text("Version")
                    .foregroundColor(.secondary)
                Spacer()
                TextField("1.0", text: $draft.version)
                    .multilineTextAlignment(.trailing)
                    .frame(width: 60)
            }

            tagEditor
        }
    }

    private var tagEditor: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Tags").font(.subheadline).foregroundColor(.secondary)
            FlowLayout(tags: draft.tags) { tag in
                draft.tags.removeAll { $0 == tag }
            }
            TagInputView { newTag in
                let cleaned = newTag.lowercased().trimmingCharacters(in: .whitespaces)
                if !cleaned.isEmpty && !draft.tags.contains(cleaned) {
                    draft.tags.append(cleaned)
                }
            }
        }
    }

    // MARK: Timing section

    private var timingSection: some View {
        Section("Timing") {
            Picker("Mode", selection: timingModeBinding) {
                Text("Duration").tag("duration")
                Text("Interval Count").tag("interval_count")
            }
            .pickerStyle(.segmented)

            switch draft.timingMode {
            case .duration(let seconds):
                Stepper(
                    value: Binding(
                        get: { seconds / 60 },
                        set: { draft.timingMode = .duration($0 * 60) }
                    ),
                    in: 1...240
                ) {
                    HStack {
                        Text("Duration")
                        Spacer()
                        Text(formatDuration(seconds))
                            .foregroundColor(.secondary)
                    }
                }
            case .intervalCount(let n):
                Stepper(
                    value: Binding(
                        get: { n },
                        set: { draft.timingMode = .intervalCount($0) }
                    ),
                    in: 1...1000
                ) {
                    HStack {
                        Text("Interval Count")
                        Spacer()
                        Text("\(n)")
                            .foregroundColor(.secondary)
                    }
                }
            }
        }
    }

    private var timingModeBinding: Binding<String> {
        Binding(
            get: {
                switch draft.timingMode {
                case .duration: return "duration"
                case .intervalCount: return "interval_count"
                }
            },
            set: { val in
                switch val {
                case "duration":
                    if case .intervalCount = draft.timingMode {
                        draft.timingMode = .duration(20 * 60)
                    }
                case "interval_count":
                    if case .duration = draft.timingMode {
                        draft.timingMode = .intervalCount(10)
                    }
                default: break
                }
            }
        )
    }

    // MARK: Modalities section

    private var modalitiesSection: some View {
        Section {
            if draft.modalities.isEmpty {
                Text("No modalities added yet.")
                    .foregroundColor(.secondary)
                    .frame(maxWidth: .infinity, alignment: .center)
                    .padding(.vertical, 8)
            } else {
                ForEach($draft.modalities) { $mod in
                    ModalityEditorRow(
                        modality: $mod,
                        isExpanded: expandedModality == mod.id
                    ) {
                        expandedModality = expandedModality == mod.id ? nil : mod.id
                    }
                }
                .onMove { source, dest in
                    draft.modalities.move(fromOffsets: source, toOffset: dest)
                }
                .onDelete { offsets in
                    draft.modalities.remove(atOffsets: offsets)
                }
            }

            Button {
                showAddModality = true
            } label: {
                Label("Add Modality", systemImage: "plus.circle.fill")
            }
        } header: {
            HStack {
                Text("Modalities")
                Spacer()
                EditButton()
                    .font(.caption)
            }
        }
    }

    // MARK: - Script Editor

    private var scriptEditor: some View {
        VStack(spacing: 0) {
            ProtocolScriptEditorView(
                text: $scriptText,
                error: $scriptError,
                onFormat: {
                    if let formatted = try? nppsRoundTrip(scriptText) {
                        scriptText = formatted
                    }
                }
            )
        }
        .onChange(of: scriptText) { _, newText in
            // Debounced parse feedback
            do {
                var lexer = NPPSLexer(newText)
                let tokens = try lexer.tokenize()
                var parser = NPPSParser(tokens)
                let entries = try parser.parse()
                if let first = entries.first, case .single(let parsed) = first {
                    scriptError = nil
                    // Live update name so the title reflects changes
                    if !parsed.name.isEmpty {
                        draft.name = parsed.name
                    }
                }
            } catch let e as NPPSError {
                scriptError = e.errorDescription
            } catch {
                scriptError = error.localizedDescription
            }
        }
    }

    // MARK: - Sync

    private func syncScriptToDraft() {
        guard !scriptText.isEmpty else { return }
        do {
            var lexer = NPPSLexer(scriptText)
            let tokens = try lexer.tokenize()
            var parser = NPPSParser(tokens)
            let entries = try parser.parse()
            if let first = entries.first, case .single(let parsed) = first {
                var updated = parsed
                updated.id = draft.id   // preserve identity
                updated.isPredefined = false
                draft = updated
                scriptError = nil
            }
        } catch let e as NPPSError {
            scriptError = e.errorDescription
        } catch {
            scriptError = error.localizedDescription
        }
    }

    // MARK: - Save

    private func attemptSave() {
        // If in script mode, sync first
        if viewMode == .script {
            syncScriptToDraft()
        }

        guard !draft.name.trimmingCharacters(in: .whitespaces).isEmpty else {
            saveError = "Protocol name cannot be empty."
            showSaveError = true
            return
        }

        guard !draft.modalities.filter({ $0.enabled }).isEmpty else {
            saveError = "Protocol must have at least one enabled modality."
            showSaveError = true
            return
        }

        let validation = limitsStore.makeValidator().validate(draft)
        if !validation.errors.isEmpty {
            let descriptions = validation.errors.map { "• \($0.message)" }.joined(separator: "\n")
            saveError = "Fix the following errors before saving:\n\n\(descriptions)"
            showSaveError = true
            return
        }

        draft.modifiedAt = Date()
        draft.isPredefined = false

        if draft.createdAt == Date(timeIntervalSince1970: 0) || existing == nil {
            draft.createdAt = Date()
        }

        library.save(.single(draft))
        dismiss()
    }

    // MARK: - Helpers

    private func formatDuration(_ seconds: Int) -> String {
        let h = seconds / 3600
        let m = (seconds % 3600) / 60
        let s = seconds % 60
        if h > 0 { return "\(h)h \(m)m" }
        if s > 0 { return "\(m)m \(s)s" }
        return "\(m)m"
    }
}

// MARK: - Flow layout for tags

struct FlowLayout: View {
    let tags: [String]
    let onRemove: (String) -> Void

    var body: some View {
        let chunked = layoutChunks(tags: tags)
        VStack(alignment: .leading, spacing: 4) {
            ForEach(Array(chunked.enumerated()), id: \.offset) { _, row in
                HStack(spacing: 4) {
                    ForEach(row, id: \.self) { tag in
                        TagChip(tag: tag, onRemove: { onRemove(tag) })
                    }
                }
            }
        }
    }

    private func layoutChunks(tags: [String]) -> [[String]] {
        // Simple chunking: 4 per row
        stride(from: 0, to: tags.count, by: 4).map {
            Array(tags[$0..<min($0+4, tags.count)])
        }
    }
}

struct TagChip: View {
    let tag: String
    let onRemove: () -> Void

    var body: some View {
        HStack(spacing: 2) {
            Text(tag)
                .font(.caption2)
            Button {
                onRemove()
            } label: {
                Image(systemName: "xmark.circle.fill")
                    .font(.caption2)
            }
            .buttonStyle(.plain)
            .foregroundColor(.secondary)
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 4)
        .background(Color(.systemGray5))
        .clipShape(Capsule())
        .font(.caption2)
    }
}

struct TagInputView: View {
    let onAdd: (String) -> Void
    @State private var text = ""

    var body: some View {
        HStack {
            TextField("Add tag…", text: $text)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .onSubmit {
                    onAdd(text)
                    text = ""
                }
            if !text.isEmpty {
                Button("Add") {
                    onAdd(text)
                    text = ""
                }
                .font(.caption)
            }
        }
    }
}
