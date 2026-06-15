import SwiftUI

// MARK: - ProtocolScriptEditorView

struct ProtocolScriptEditorView: View {
    @Binding var text: String
    @Binding var error: String?
    let onFormat: () -> Void

    @State private var localText: String = ""
    @FocusState private var focused: Bool
    @State private var parseDebounceTask: Task<Void, Never>? = nil
    @State private var lineCount: Int = 1
    @State private var errorLine: Int? = nil

    var body: some View {
        VStack(spacing: 0) {
            // Toolbar
            HStack {
                Button(action: onFormat) {
                    Label("Format", systemImage: "text.alignleft")
                        .font(.caption)
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
                .disabled(localText.isEmpty)

                Spacer()

                Text("\(lineCount) line\(lineCount == 1 ? "" : "s")")
                    .font(.caption2)
                    .foregroundColor(.secondary)

                Button {
                    UIPasteboard.general.string = localText
                } label: {
                    Label("Copy", systemImage: "doc.on.doc")
                        .font(.caption)
                }
                .buttonStyle(.bordered)
                .controlSize(.small)
            }
            .padding(.horizontal)
            .padding(.vertical, 6)
            .background(Color(.systemGray6))

            Divider()

            // Editor with line numbers
            ScrollView {
                HStack(alignment: .top, spacing: 0) {
                    // Line numbers
                    VStack(alignment: .trailing, spacing: 0) {
                        ForEach(1...max(1, lineCount), id: \.self) { n in
                            HStack(spacing: 2) {
                                if n == errorLine {
                                    Image(systemName: "exclamationmark.circle.fill")
                                        .font(.system(size: 9))
                                        .foregroundColor(.red)
                                }
                                Text("\(n)")
                                    .font(.system(.caption2, design: .monospaced))
                                    .foregroundColor(n == errorLine ? .red : .secondary)
                            }
                            .frame(height: 20, alignment: .topTrailing)
                        }
                    }
                    .padding(.horizontal, 6)
                    .padding(.vertical, 8)
                    .frame(minWidth: 36, alignment: .trailing)
                    .background(Color(.systemGray6))

                    // Code area
                    TextEditor(text: $localText)
                        .font(.system(.body, design: .monospaced))
                        .autocorrectionDisabled()
                        .textInputAutocapitalization(.never)
                        .focused($focused)
                        .frame(minHeight: 400)
                        .padding(.vertical, 8)
                        .padding(.horizontal, 4)
                        .onChange(of: localText) { _, newValue in
                            text = newValue
                            lineCount = newValue.components(separatedBy: .newlines).count
                            scheduleParseCheck(newValue)
                        }
                }
            }

            // Error banner
            if let errorText = error {
                Divider()
                HStack(alignment: .top, spacing: 8) {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundColor(.red)
                        .font(.caption)
                    Text(errorText)
                        .font(.caption.monospacedDigit())
                        .foregroundColor(.red)
                    Spacer()
                }
                .padding(.horizontal)
                .padding(.vertical, 8)
                .background(Color.red.opacity(0.08))
            }
        }
        .onAppear {
            localText = text
            lineCount = text.components(separatedBy: .newlines).count
        }
        .onChange(of: text) { _, newValue in
            if localText != newValue {
                localText = newValue
                lineCount = newValue.components(separatedBy: .newlines).count
            }
        }
    }

    // MARK: - Parse debounce

    private func scheduleParseCheck(_ newText: String) {
        parseDebounceTask?.cancel()
        parseDebounceTask = Task { @MainActor in
            try? await Task.sleep(nanoseconds: 800_000_000) // 800ms
            guard !Task.isCancelled else { return }
            validateScript(newText)
        }
    }

    private func validateScript(_ source: String) {
        guard !source.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            error = nil
            errorLine = nil
            return
        }
        do {
            var lexer = NPPSLexer(source)
            let tokens = try lexer.tokenize()
            var parser = NPPSParser(tokens)
            _ = try parser.parse()
            error = nil
            errorLine = nil
        } catch let e as NPPSError {
            error = e.errorDescription
            errorLine = e.line
        } catch {
            self.error = error.localizedDescription
            errorLine = nil
        }
    }
}
