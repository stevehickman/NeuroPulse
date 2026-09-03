import SwiftUI

// MARK: - ProtocolComposerView

struct ProtocolComposerView: View {
    @Environment(\.dismiss) private var dismiss
    @EnvironmentObject private var library: NPProtocolLibrary

    let existing: NPCompositeProtocol?

    @State private var draft: NPCompositeProtocol
    @State private var showLayerPicker = false
    @State private var expandedLayer: UUID? = nil
    @State private var showScript = false
    @State private var saveError: String? = nil
    @State private var showSaveError = false
    @State private var selectedLayerID: UUID? = nil

    init(existing: NPCompositeProtocol?) {
        self.existing = existing
        _draft = State(initialValue: existing ?? NPCompositeProtocol(name: ""))
    }

    var body: some View {
        NavigationStack {
            Form {
                identitySection
                timelineSection
                layersSection
                scriptPreviewSection
            }
            .navigationTitle(existing == nil ? "Compose Protocol" : "Edit Composition")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { toolbarContent }
            .sheet(isPresented: $showLayerPicker) {
                LayerPickerSheet(library: library) { protocolName in
                    let layer = NPCompositeLayer(
                        protocolName: protocolName,
                        startOffsetSeconds: nextStartOffset,
                        durationSeconds: nil,
                        intensityScale: 1.0
                    )
                    draft.layers.append(layer)
                    expandedLayer = layer.id
                }
            }
            .sheet(isPresented: $showScript) {
                scriptPreviewSheet
            }
            .alert("PROTOCOL_COMPOSER_CANNOT_SAVE", isPresented: $showSaveError) {
                Button("OK", role: .cancel) {}
            } message: {
                Text(saveError ?? "Please fill in required fields.")
            }
        }
    }

    // MARK: - Toolbar

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItem(placement: .cancellationAction) {
            Button(String(localized: "COMMON_CANCEL")) { dismiss() }
        }
        ToolbarItem(placement: .confirmationAction) {
            Button("PROTOCOL_COMPOSER_SAVE") { attemptSave() }
        }
    }

    // MARK: - Identity section

    private var identitySection: some View {
        Section("UI_SECTION_IDENTITY") {
            TextField("PROTOCOL_COMPOSER_COMPOSITION_NAME", text: $draft.name)
                .autocorrectionDisabled()
            TextField("UI_FIELD_DESCRIPTION", text: $draft.description, axis: .vertical)
                .lineLimit(2...4)
            VStack(alignment: .leading, spacing: 4) {
                Text(String(localized: "PROTOCOL_TAGS")).font(.caption).foregroundColor(.secondary)
                TagInputView { newTag in
                    let cleaned = newTag.lowercased().trimmingCharacters(in: .whitespaces)
                    if !cleaned.isEmpty && !draft.tags.contains(cleaned) {
                        draft.tags.append(cleaned)
                    }
                }
                FlowLayout(tags: draft.tags) { tag in
                    draft.tags.removeAll { $0 == tag }
                }
            }
            VStack(alignment: .leading, spacing: 4) {
                Text("PROTOCOL_CONFLICT_RESOLUTION").font(.caption).foregroundColor(.secondary)
                Picker("PROTOCOL_CONFLICT_RESOLUTION", selection: $draft.conflictResolution) {
                    ForEach(NPCompositeProtocol.ConflictResolution.allCases) { cr in
                        Text(cr.displayName).tag(cr)
                    }
                }
                .pickerStyle(.menu)
            }
        }
    }

    // MARK: - Timeline visualization section

    private var timelineSection: some View {
        Section("PROTOCOL_COMPOSER_TIMELINE") {
            if draft.layers.isEmpty {
                Text("PROTOCOL_ADD_LAYERS_HINT")
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .frame(maxWidth: .infinity, alignment: .center)
                    .padding(.vertical, 8)
            } else {
                TimelineView(
                    layers: draft.layers,
                    selectedLayerID: $selectedLayerID,
                    totalSeconds: totalDurationSeconds
                )
                .frame(height: 80)
                .padding(.vertical, 4)
            }
        }
    }

    // MARK: - Layers section

    private var layersSection: some View {
        Section {
            if draft.layers.isEmpty {
                Text("PROTOCOL_NO_LAYERS")
                    .foregroundColor(.secondary)
                    .frame(maxWidth: .infinity, alignment: .center)
                    .padding(.vertical, 8)
            } else {
                ForEach($draft.layers) { $layer in
                    LayerEditorRow(
                        layer: $layer,
                        isExpanded: expandedLayer == layer.id,
                        isSelected: selectedLayerID == layer.id
                    ) {
                        withAnimation {
                            expandedLayer = expandedLayer == layer.id ? nil : layer.id
                            selectedLayerID = layer.id
                        }
                    }
                }
                .onMove { source, dest in
                    draft.layers.move(fromOffsets: source, toOffset: dest)
                }
                .onDelete { offsets in
                    draft.layers.remove(atOffsets: offsets)
                }
            }

            Button {
                showLayerPicker = true
            } label: {
                Label("UI_ADD_LAYER_TITLE", systemImage: "plus.circle.fill")
            }
        } header: {
            HStack {
                Text("PROTOCOL_LAYERS")
                Spacer()
                EditButton().font(.caption)
            }
        }
    }

    // MARK: - Script preview section

    private var scriptPreviewSection: some View {
        Section {
            Button {
                showScript = true
            } label: {
                Label("PROTOCOL_COMPOSER_VIEW_SCRIPT", systemImage: "doc.text")
            }
        }
    }

    private var scriptPreviewSheet: some View {
        NavigationStack {
            let scriptText = NPPSSerializer().serialize(.composite(draft))
            VStack {
                ProtocolScriptEditorView(
                    text: .constant(scriptText),
                    error: .constant(nil),
                    onFormat: {}
                )
            }
            .navigationTitle("PROTOCOL_COMPOSER_COMPOSITION_SCRIPT")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button(String(localized: "CONSENT_DONE_BUTTON")) { showScript = false }
                }
            }
        }
    }

    // MARK: - Computed

    private var totalDurationSeconds: Int {
        draft.layers.reduce(0) { maxEnd, layer in
            let end = layer.startOffsetSeconds + (layer.durationSeconds ?? 1200)
            return max(maxEnd, end)
        }
    }

    private var nextStartOffset: Int {
        guard let last = draft.layers.last else { return 0 }
        return last.startOffsetSeconds + (last.durationSeconds ?? 1200)
    }

    // MARK: - Save

    private func attemptSave() {
        guard !draft.name.trimmingCharacters(in: .whitespaces).isEmpty else {
            saveError = "Composition name cannot be empty."
            showSaveError = true
            return
        }
        guard !draft.layers.isEmpty else {
            saveError = "Composition must have at least one layer."
            showSaveError = true
            return
        }
        draft.modifiedAt = Date()
        draft.isPredefined = false
        if existing == nil { draft.createdAt = Date() }
        library.save(.composite(draft))
        dismiss()
    }
}

// MARK: - Timeline Canvas

struct TimelineView: View {
    let layers: [NPCompositeLayer]
    @Binding var selectedLayerID: UUID?
    let totalSeconds: Int

    private let colors: [Color] = [.blue, .green, .orange, .purple, .pink, .teal, .indigo, .cyan]

    var body: some View {
        GeometryReader { geo in
            ZStack(alignment: .bottomLeading) {
                // Background
                RoundedRectangle(cornerRadius: 8)
                    .fill(Color(.systemGray6))

                // Layer segments
                ForEach(Array(layers.enumerated()), id: \.element.id) { idx, layer in
                    let color = colors[idx % colors.count]
                    let total = max(1, totalSeconds)
                    let x = CGFloat(layer.startOffsetSeconds) / CGFloat(total) * geo.size.width
                    let w = CGFloat(layer.durationSeconds ?? (total - layer.startOffsetSeconds)) / CGFloat(total) * geo.size.width

                    ZStack(alignment: .center) {
                        RoundedRectangle(cornerRadius: 6)
                            .fill(color.opacity(selectedLayerID == layer.id ? 0.85 : 0.55))
                            .frame(width: max(4, w - 2), height: 52)
                            .overlay {
                                RoundedRectangle(cornerRadius: 6)
                                    .stroke(color, lineWidth: selectedLayerID == layer.id ? 2 : 0)
                            }

                        if w > 50 {
                            Text(layer.protocolName)
                                .font(.system(.caption2, design: .rounded))
                                .foregroundColor(.white)
                                .lineLimit(1)
                                .padding(.horizontal, 4)
                        }
                    }
                    .position(x: x + max(4, w - 2) / 2, y: geo.size.height / 2 - 10)
                    .onTapGesture {
                        selectedLayerID = selectedLayerID == layer.id ? nil : layer.id
                    }
                }

                // Time axis labels
                HStack {
                    Text("0m")
                        .font(.caption2)
                        .foregroundColor(.secondary)
                    Spacer()
                    Text(formatTime(totalSeconds))
                        .font(.caption2)
                        .foregroundColor(.secondary)
                }
                .padding(.horizontal, 4)
                .frame(height: 16)
                .position(x: geo.size.width / 2, y: geo.size.height - 8)
            }
        }
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }

    private func formatTime(_ s: Int) -> String {
        if s < 60 { return "\(s)s" }
        let m = s / 60
        if m < 60 { return "\(m)m" }
        return "\(m / 60)h \(m % 60)m"
    }
}

// MARK: - Layer Editor Row

struct LayerEditorRow: View {
    @Binding var layer: NPCompositeLayer
    let isExpanded: Bool
    let isSelected: Bool
    let onToggle: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Button(action: onToggle) {
                HStack {
                    Image(systemName: "square.stack.3d.up")
                        .foregroundColor(isSelected ? .accentColor : .secondary)
                    VStack(alignment: .leading, spacing: 1) {
                        Text(layer.protocolName)
                            .font(.subheadline.weight(.medium))
                            .foregroundColor(.primary)
                        HStack(spacing: 8) {
                            Text(String(format: String(localized: "PROTOCOL_COMPOSER_LAYER_START"),
                                        formatTime(layer.startOffsetSeconds)))
                            if let dur = layer.durationSeconds {
                                Text(String(format: String(localized: "PROTOCOL_COMPOSER_LAYER_DURATION"),
                                            formatTime(dur)))
                            } else {
                                Text("PROTOCOL_DURATION_FULL")
                            }
                            if abs(layer.intensityScale - 1.0) > 0.001 {
                                Text("\(Int(layer.intensityScale * 100))%")
                            }
                        }
                        .font(.caption2)
                        .foregroundColor(.secondary)
                    }
                    Spacer()
                    Image(systemName: isExpanded ? "chevron.up" : "chevron.down")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .padding(.vertical, 4)

            if isExpanded {
                VStack(alignment: .leading, spacing: 12) {
                    Divider()

                    // Start offset
                    Stepper(
                        value: Binding(
                            get: { layer.startOffsetSeconds / 60 },
                            set: { layer.startOffsetSeconds = $0 * 60 }
                        ),
                        in: 0...240
                    ) {
                        HStack {
                            Text("PROTOCOL_START_OFFSET").font(.caption).foregroundColor(.secondary)
                            Spacer()
                            Text(formatTime(layer.startOffsetSeconds)).font(.caption)
                        }
                    }

                    // Duration
                    Toggle("PROTOCOL_COMPOSER_CUSTOM_DURATION", isOn: Binding(
                        get: { layer.durationSeconds != nil },
                        set: { on in layer.durationSeconds = on ? 20 * 60 : nil }
                    ))
                    .font(.caption)

                    if layer.durationSeconds != nil {
                        Stepper(
                            value: Binding(
                                get: { (layer.durationSeconds ?? 1200) / 60 },
                                set: { layer.durationSeconds = $0 * 60 }
                            ),
                            in: 1...240
                        ) {
                            HStack {
                                Text(String(localized: "PROTOCOL_DURATION")).font(.caption).foregroundColor(.secondary)
                                Spacer()
                                Text(formatTime(layer.durationSeconds ?? 1200)).font(.caption)
                            }
                        }
                    }

                    // Intensity scale
                    VStack(alignment: .leading, spacing: 2) {
                        HStack {
                            Text("PROTOCOL_INTENSITY_SCALE").font(.caption).foregroundColor(.secondary)
                            Spacer()
                            Text("\(Int(layer.intensityScale * 100))%").font(.caption.monospacedDigit())
                        }
                        Slider(value: $layer.intensityScale, in: 0...1, step: 0.05)
                    }
                }
                .padding(.leading, 32)
            }
        }
    }

    private func formatTime(_ s: Int) -> String {
        if s == 0 { return "0m" }
        if s < 60 { return "\(s)s" }
        let m = s / 60
        let sec = s % 60
        if sec == 0 { return "\(m)m" }
        return "\(m)m \(sec)s"
    }
}

// MARK: - Layer Picker Sheet

struct LayerPickerSheet: View {
    @Environment(\.dismiss) private var dismiss
    let library: NPProtocolLibrary
    let onSelect: (String) -> Void

    var body: some View {
        NavigationStack {
            List {
                let singles = library.allProtocols.filter { !$0.isComposite }
                if singles.isEmpty {
                    Text("PROTOCOL_NO_SINGLES")
                        .foregroundColor(.secondary)
                        .font(.subheadline)
                } else {
                    ForEach(singles) { entry in
                        Button {
                            onSelect(entry.name)
                            dismiss()
                        } label: {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(entry.name).font(.subheadline)
                                Text(entry.description)
                                    .font(.caption2)
                                    .foregroundColor(.secondary)
                                    .lineLimit(2)
                            }
                        }
                    }
                }
            }
            .navigationTitle("PROTOCOL_COMPOSER_SELECT_PROTOCOL")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(String(localized: "COMMON_CANCEL")) { dismiss() }
                }
            }
        }
    }
}
