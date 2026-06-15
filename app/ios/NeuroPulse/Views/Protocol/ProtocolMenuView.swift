import SwiftUI

// MARK: - Library Filter

enum LibraryFilter: String, CaseIterable {
    case all        = "All"
    case predefined = "Predefined"
    case mine       = "Mine"
}

// MARK: - ProtocolMenuView

struct ProtocolMenuView: View {
    @Environment(\.dismiss) private var dismiss
    @EnvironmentObject private var library: NPProtocolLibrary
    @EnvironmentObject private var uploader: SessionProtocolUploader
    @EnvironmentObject private var limitsStore: NPLimitsStore

    // When true, selecting a protocol programs it onto the hub for Mode 3
    // Autonomous use (via SessionProtocolUploader.programAutonomous) instead of
    // a standard Mode 2 upload. On success `onProgrammed` is invoked.
    var programMode: Bool = false
    var onProgrammed: (() -> Void)? = nil

    @State private var searchText = ""
    @State private var filter: LibraryFilter = .all
    @State private var showEditor = false
    @State private var editingEntry: NPProtocolEntry? = nil
    @State private var showComposer = false
    @State private var confirmDelete: NPProtocolEntry? = nil
    @State private var unavailableAlert: NPProtocolEntry? = nil
    @State private var uploadConfirm: NPProtocolEntry? = nil
    @State private var uploadError: String? = nil
    @State private var lastSelectedEntry: NPProtocolEntry? = nil
    @State private var showUploadSuccess = false
    @State private var showLimitsSettings = false
    @State private var validationDetailEntry: NPProtocolEntry? = nil

    var body: some View {
        NavigationStack {
            Group {
                if filteredProtocols.isEmpty {
                    emptyState
                } else {
                    protocolList
                }
            }
            .navigationTitle("Protocols")
            .navigationBarTitleDisplayMode(.large)
            .searchable(text: $searchText, prompt: "Search by name or tag")
            .toolbar { toolbarContent }
            .sheet(isPresented: $showEditor) {
                ProtocolEditorView(existing: editingEntry)
                    .environmentObject(library)
                    .environmentObject(limitsStore)
            }
            .sheet(isPresented: $showComposer) {
                ProtocolComposerView(existing: nil)
                    .environmentObject(library)
            }
            .sheet(isPresented: $showLimitsSettings) {
                LimitsSettingsView()
                    .environmentObject(limitsStore)
                    .environmentObject(library)
            }
            .sheet(item: $validationDetailEntry) { entry in
                ValidationDetailSheet(entry: entry, limitsStore: limitsStore, library: library)
            }
            .alert("Protocol Unavailable", isPresented: Binding(
                get: { unavailableAlert != nil },
                set: { if !$0 { unavailableAlert = nil } }
            )) {
                Button("OK", role: .cancel) { unavailableAlert = nil }
            } message: {
                if let entry = unavailableAlert {
                    let avail = library.availability(for: entry)
                    Text(avail.unavailableReason ?? "This protocol requires hardware not currently available.")
                }
            }
            .alert("Delete Protocol?", isPresented: Binding(
                get: { confirmDelete != nil },
                set: { if !$0 { confirmDelete = nil } }
            )) {
                Button("Delete", role: .destructive) {
                    if let entry = confirmDelete { library.delete(entry.id) }
                    confirmDelete = nil
                }
                Button("Cancel", role: .cancel) { confirmDelete = nil }
            } message: {
                if let entry = confirmDelete {
                    Text("Are you sure you want to delete \"\(entry.name)\"? This cannot be undone.")
                }
            }
            .overlay(alignment: .top) {
                if let errorMsg = uploadError {
                    HStack(spacing: 8) {
                        Image(systemName: "exclamationmark.triangle.fill")
                            .foregroundColor(.white)
                            .font(.caption)
                        Text(errorMsg)
                            .font(.caption)
                            .foregroundColor(.white)
                            .lineLimit(3)
                            .fixedSize(horizontal: false, vertical: true)
                        Spacer()
                        Button {
                            let retry = lastSelectedEntry
                            uploadError = nil
                            if let entry = retry { handleSelect(entry) }
                        } label: {
                            Text("Retry")
                                .font(.caption.bold())
                                .foregroundColor(.white)
                                .padding(.horizontal, 10)
                                .padding(.vertical, 4)
                                .background(Color.white.opacity(0.25))
                                .clipShape(Capsule())
                        }
                    }
                    .padding(.horizontal, 12)
                    .padding(.vertical, 10)
                    .background(Color.red)
                    .transition(.move(edge: .top).combined(with: .opacity))
                }
            }
            .overlay(alignment: .bottom) {
                if showUploadSuccess {
                    HStack(spacing: 8) {
                        Image(systemName: "checkmark.circle.fill")
                            .foregroundColor(.white)
                        Text("Protocol sent to hub")
                            .font(.subheadline.weight(.medium))
                            .foregroundColor(.white)
                    }
                    .padding(.horizontal, 16)
                    .padding(.vertical, 10)
                    .background(Color.green)
                    .clipShape(Capsule())
                    .padding(.bottom, 24)
                    .transition(.move(edge: .bottom).combined(with: .opacity))
                }
            }
            .animation(.easeInOut(duration: 0.3), value: uploadError != nil)
            .animation(.easeInOut(duration: 0.3), value: showUploadSuccess)
        }
    }

    // MARK: - Toolbar

    @ToolbarContentBuilder
    private var toolbarContent: some ToolbarContent {
        ToolbarItem(placement: .cancellationAction) {
            Button("Cancel") { dismiss() }
        }
        ToolbarItem(placement: .navigationBarLeading) {
            Button {
                showLimitsSettings = true
            } label: {
                Label("Limits", systemImage: "slider.horizontal.3")
            }
        }
        ToolbarItem(placement: .primaryAction) {
            Menu {
                Button {
                    editingEntry = nil
                    showEditor = true
                } label: {
                    Label("New Protocol", systemImage: "plus.rectangle")
                }
                Button {
                    showComposer = true
                } label: {
                    Label("Compose", systemImage: "square.stack.3d.up")
                }
            } label: {
                Image(systemName: "plus")
            }
        }
    }

    // MARK: - Filtered data

    private var filteredProtocols: [NPProtocolEntry] {
        let base: [NPProtocolEntry]
        switch filter {
        case .all:
            base = library.allProtocols
        case .predefined:
            base = library.allProtocols.filter { $0.isPredefined }
        case .mine:
            base = library.userProtocols
        }

        if searchText.isEmpty { return base }
        let q = searchText.lowercased()
        return base.filter { entry in
            entry.name.lowercased().contains(q) ||
            entry.description.lowercased().contains(q) ||
            entry.tags.contains { $0.lowercased().contains(q) }
        }
    }

    private var predefinedFiltered: [NPProtocolEntry] {
        filteredProtocols.filter { $0.isPredefined }
    }

    private var userFiltered: [NPProtocolEntry] {
        filteredProtocols.filter { !$0.isPredefined }
    }

    // MARK: - List

    private var protocolList: some View {
        List {
            // Active profile banner
            if let profile = limitsStore.activeProfile {
                Section {
                    HStack(spacing: 8) {
                        Image(systemName: "person.circle.fill")
                            .foregroundColor(.accentColor)
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Profile: \(profile.name)")
                                .font(.subheadline.weight(.medium))
                            Text(limitsStore.resolutionChainDescription)
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                        Spacer()
                        Button {
                            limitsStore.setActiveProfile(nil)
                        } label: {
                            Image(systemName: "xmark.circle.fill")
                                .foregroundColor(.secondary)
                        }
                        .buttonStyle(.plain)
                    }
                    .padding(.vertical, 2)
                }
            }

            Picker("Filter", selection: $filter) {
                ForEach(LibraryFilter.allCases, id: \.self) {
                    Text($0.rawValue).tag($0)
                }
            }
            .pickerStyle(.segmented)
            .listRowBackground(Color.clear)
            .listRowInsets(EdgeInsets(top: 8, leading: 0, bottom: 8, trailing: 0))

            if !predefinedFiltered.isEmpty && (filter == .all || filter == .predefined) {
                Section("Predefined") {
                    ForEach(predefinedFiltered) { entry in
                        protocolRow(entry)
                            .swipeActions(edge: .trailing) {
                                duplicateButton(for: entry)
                            }
                            .contextMenu { contextMenu(for: entry) }
                    }
                }
            }

            if !userFiltered.isEmpty && (filter == .all || filter == .mine) {
                Section("My Protocols") {
                    ForEach(userFiltered) { entry in
                        protocolRow(entry)
                            .swipeActions(edge: .trailing) {
                                if library.canDelete(entry) {
                                    deleteButton(for: entry)
                                }
                                if library.canEdit(entry) {
                                    editButton(for: entry)
                                }
                                duplicateButton(for: entry)
                            }
                            .contextMenu { contextMenu(for: entry) }
                    }
                }
            }
        }
        .listStyle(.insetGrouped)
    }

    // MARK: - Row

    private func protocolRow(_ entry: NPProtocolEntry) -> some View {
        let avail = library.availability(for: entry)
        let counts = library.issueCount(for: entry)
        return ProtocolRowView(
            entry: entry,
            availability: avail,
            errorCount: counts.errors,
            warningCount: counts.warnings,
            onValidationBadgeTap: { validationDetailEntry = entry }
        )
        .contentShape(Rectangle())
        .onTapGesture {
            if avail.isAvailable {
                handleSelect(entry)
            } else {
                unavailableAlert = entry
            }
        }
        .onAppear {
            // Lazily compute validation on first display
            _ = library.validationResult(for: entry, limitsStore: limitsStore)
        }
    }

    // MARK: - Swipe buttons

    private func deleteButton(for entry: NPProtocolEntry) -> some View {
        Button(role: .destructive) {
            confirmDelete = entry
        } label: {
            Label("Delete", systemImage: "trash")
        }
        .tint(.red)
    }

    private func editButton(for entry: NPProtocolEntry) -> some View {
        Button {
            editingEntry = entry
            if entry.isComposite {
                showComposer = true
            } else {
                showEditor = true
            }
        } label: {
            Label("Edit", systemImage: "pencil")
        }
        .tint(.blue)
    }

    private func duplicateButton(for entry: NPProtocolEntry) -> some View {
        Button {
            let dup = entry.duplicated(newName: "Copy of \(entry.name)")
            library.save(dup)
        } label: {
            Label("Duplicate", systemImage: "plus.square.on.square")
        }
        .tint(.orange)
    }

    @ViewBuilder
    private func contextMenu(for entry: NPProtocolEntry) -> some View {
        Button {
            handleSelect(entry)
        } label: {
            Label("Select Protocol", systemImage: "checkmark.circle")
        }
        .disabled(!library.availability(for: entry).isAvailable)

        Divider()

        Button {
            let dup = entry.duplicated(newName: "Copy of \(entry.name)")
            library.save(dup)
        } label: {
            Label("Duplicate", systemImage: "plus.square.on.square")
        }

        if !entry.isPredefined {
            if library.canEdit(entry) {
                Button {
                    editingEntry = entry
                    if entry.isComposite { showComposer = true } else { showEditor = true }
                } label: {
                    Label("Edit", systemImage: "pencil")
                }
            }

            // Read-only toggle for user-created protocols
            Button {
                toggleReadOnly(entry)
            } label: {
                if entry.isReadOnly {
                    Label("Unlock Protocol", systemImage: "lock.open")
                } else {
                    Label("Lock Protocol", systemImage: "lock")
                }
            }

            if library.canDelete(entry) {
                Button(role: .destructive) {
                    confirmDelete = entry
                } label: {
                    Label("Delete", systemImage: "trash")
                }
            }
        }
    }

    private func toggleReadOnly(_ entry: NPProtocolEntry) {
        switch entry {
        case .single(var proto):
            proto.isReadOnly.toggle()
            library.save(.single(proto))
        case .composite(var comp):
            comp.isReadOnly.toggle()
            library.save(.composite(comp))
        case .limits:
            break
        }
    }

    // MARK: - Empty state

    private var emptyState: some View {
        VStack(spacing: 16) {
            Picker("Filter", selection: $filter) {
                ForEach(LibraryFilter.allCases, id: \.self) {
                    Text($0.rawValue).tag($0)
                }
            }
            .pickerStyle(.segmented)
            .padding(.horizontal)

            Spacer()
            Image(systemName: "magnifyingglass")
                .font(.system(size: 48))
                .foregroundColor(.secondary)
            Text("No Protocols Found")
                .font(.headline)
            Text(filter == .mine ? "Create your first protocol using the + button above." : "Try a different search term.")
                .font(.subheadline)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal)
            Spacer()
        }
        .padding()
    }

    // MARK: - Selection handling

    private func handleSelect(_ entry: NPProtocolEntry) {
        guard case .single(let proto) = entry else {
            // Composite protocols: select the first layer's source protocol.
            if case .composite(let comp) = entry, let firstLayer = comp.layers.first {
                let found = library.allProtocols.first { $0.name == firstLayer.protocolName }
                if let found = found { handleSelect(found) }
            }
            dismiss()
            return
        }

        lastSelectedEntry = entry
        withAnimation { uploadError = nil }

        Task {
            do {
                if programMode {
                    try await uploader.programAutonomous(proto)
                    dismiss()
                    onProgrammed?()
                } else {
                    try await uploader.upload(proto)
                    withAnimation { showUploadSuccess = true }
                    try? await Task.sleep(nanoseconds: 2_000_000_000)
                    dismiss()
                }
            } catch {
                withAnimation { uploadError = error.localizedDescription }
            }
        }
    }
}

// MARK: - Protocol Row View

struct ProtocolRowView: View {
    let entry: NPProtocolEntry
    let availability: ProtocolAvailability
    var errorCount: Int = 0
    var warningCount: Int = 0
    var onValidationBadgeTap: (() -> Void)? = nil

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            // Header row
            HStack(alignment: .center, spacing: 6) {
                availabilityDot
                Text(entry.name)
                    .font(.headline)
                    .foregroundColor(availability.isAvailable ? .primary : .secondary)
                Spacer()
                validationBadges
                badges
            }

            // Description
            if !entry.description.isEmpty {
                Text(entry.description)
                    .font(.caption)
                    .foregroundColor(.secondary)
                    .lineLimit(2)
            }

            // Duration + unavailability note
            HStack(spacing: 6) {
                if let dur = durationString {
                    Text(dur)
                        .font(.caption2)
                        .foregroundColor(.secondary)
                        .padding(.horizontal, 6)
                        .padding(.vertical, 2)
                        .background(Color(.systemGray5))
                        .clipShape(Capsule())
                }
                if !availability.isAvailable, let reason = availability.unavailableReason {
                    HStack(spacing: 3) {
                        Image(systemName: "exclamationmark.triangle.fill")
                            .font(.caption2)
                            .foregroundColor(.red)
                        Text(reason)
                            .font(.caption2)
                            .foregroundColor(.red)
                            .lineLimit(1)
                    }
                }
            }

            // Modality icons
            if !modalityTypes.isEmpty {
                modalityIconRow
            }

            // Tags
            if !entry.tags.isEmpty {
                tagChips
            }
        }
        .padding(.vertical, 4)
        .opacity(availability.isAvailable ? 1.0 : 0.7)
    }

    private var durationString: String? {
        switch entry {
        case .single(let proto):
            return proto.timingMode.displayString
        case .composite(let comp):
            guard !comp.layers.isEmpty else { return nil }
            let totalSec = comp.layers.reduce(0) { maxEnd, layer in
                max(maxEnd, layer.startOffsetSeconds + (layer.durationSeconds ?? 1200))
            }
            if totalSec < 60 { return "\(totalSec)s" }
            let m = totalSec / 60
            if m < 60 { return "\(m)m" }
            return "\(m / 60)h \(m % 60)m"
        case .limits:
            return nil
        }
    }

    private var availabilityDot: some View {
        Circle()
            .fill(availability.isAvailable ? Color.green : Color.red)
            .frame(width: 8, height: 8)
    }

    @ViewBuilder
    private var validationBadges: some View {
        if errorCount > 0 {
            Button {
                onValidationBadgeTap?()
            } label: {
                HStack(spacing: 3) {
                    Image(systemName: "exclamationmark.circle.fill")
                    Text("\(errorCount)")
                }
                .font(.caption2.bold())
                .foregroundColor(.white)
                .padding(.horizontal, 6).padding(.vertical, 2)
                .background(Color.red)
                .clipShape(Capsule())
            }
            .buttonStyle(.plain)
        } else if warningCount > 0 {
            Button {
                onValidationBadgeTap?()
            } label: {
                HStack(spacing: 3) {
                    Image(systemName: "exclamationmark.triangle.fill")
                    Text("\(warningCount)")
                }
                .font(.caption2.bold())
                .foregroundColor(.white)
                .padding(.horizontal, 6).padding(.vertical, 2)
                .background(Color.orange)
                .clipShape(Capsule())
            }
            .buttonStyle(.plain)
        }
    }

    @ViewBuilder
    private var badges: some View {
        if entry.isReadOnly {
            Image(systemName: "lock.fill")
                .font(.caption2)
                .foregroundColor(.secondary)
        }

        if entry.isComposite {
            Label("Composite", systemImage: "square.stack.3d.up.fill")
                .font(.caption2)
                .padding(.horizontal, 6).padding(.vertical, 2)
                .background(Color.purple.opacity(0.15))
                .foregroundColor(.purple)
                .clipShape(Capsule())
        }

        let tier = highestTier
        if tier == .t2 {
            Text("T2")
                .font(.caption2.bold())
                .padding(.horizontal, 6).padding(.vertical, 2)
                .background(Color.blue.opacity(0.15))
                .foregroundColor(.blue)
                .clipShape(Capsule())
        } else if tier == .accessory {
            Text("Accessory")
                .font(.caption2)
                .padding(.horizontal, 6).padding(.vertical, 2)
                .background(Color.orange.opacity(0.15))
                .foregroundColor(.orange)
                .clipShape(Capsule())
        }
    }

    private var highestTier: NPModalityTier {
        let types = entry.requiredModalityTypes
        if types.contains(where: { $0.tier == .t2 }) { return .t2 }
        if types.contains(where: { $0.tier == .accessory }) { return .accessory }
        return .t1
    }

    private var modalityTypes: [NPModalityType] {
        Array(entry.requiredModalityTypes).sorted { $0.rawValue < $1.rawValue }
    }

    private var modalityIconRow: some View {
        HStack(spacing: 6) {
            let shown = Array(modalityTypes.prefix(6))
            ForEach(shown) { type in
                Image(systemName: type.systemImage)
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
            if modalityTypes.count > 6 {
                Text("+\(modalityTypes.count - 6)")
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
        }
    }

    private var tagChips: some View {
        let shown = Array(entry.tags.prefix(3))
        let extra = entry.tags.count - shown.count
        return HStack(spacing: 4) {
            ForEach(shown, id: \.self) { tag in
                Text(tag)
                    .font(.caption2)
                    .padding(.horizontal, 6).padding(.vertical, 2)
                    .background(Color(.systemGray5))
                    .clipShape(Capsule())
            }
            if extra > 0 {
                Text("+\(extra) more")
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
        }
    }
}

// MARK: - Validation Detail Sheet

struct ValidationDetailSheet: View {
    let entry: NPProtocolEntry
    let limitsStore: NPLimitsStore
    let library: NPProtocolLibrary
    @Environment(\.dismiss) private var dismiss

    private var result: NPValidationResult {
        let validator = limitsStore.makeValidator()
        return validator.validate(entry, resolving: library)
    }

    var body: some View {
        NavigationStack {
            List {
                if result.isValid && !result.hasWarnings {
                    Section {
                        Label("No issues found", systemImage: "checkmark.circle.fill")
                            .foregroundColor(.green)
                    }
                }

                if !result.errors.isEmpty {
                    Section("Errors (\(result.errors.count))") {
                        ForEach(result.errors) { issue in
                            ValidationIssueRow(issue: issue)
                        }
                    }
                }

                if !result.warnings.isEmpty {
                    Section("Warnings (\(result.warnings.count))") {
                        ForEach(result.warnings) { issue in
                            ValidationIssueRow(issue: issue)
                        }
                    }
                }

                Section("Limit Resolution") {
                    Text(limitsStore.resolutionChainDescription)
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            }
            .listStyle(.insetGrouped)
            .navigationTitle("Validation: \(entry.name)")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done") { dismiss() }
                }
            }
        }
    }
}

// MARK: - Validation Issue Row

struct ValidationIssueRow: View {
    let issue: NPValidationIssue

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 6) {
                Image(systemName: issue.severity == .error ? "exclamationmark.circle.fill" : "exclamationmark.triangle.fill")
                    .foregroundColor(issue.severity == .error ? .red : .orange)
                    .font(.caption)
                if let modality = issue.modality {
                    Text(modality.displayName)
                        .font(.caption.bold())
                        .foregroundColor(.secondary)
                }
                Spacer()
                Text(issue.limitSource.description)
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
            Text(issue.message)
                .font(.subheadline)
            HStack(spacing: 4) {
                Text("Value: \(issue.actualValueDescription)")
                    .font(.caption)
                    .foregroundColor(.secondary)
                Text("·")
                    .font(.caption)
                    .foregroundColor(.secondary)
                Text("Limit: \(issue.limitValueDescription)")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
        }
        .padding(.vertical, 4)
    }
}
