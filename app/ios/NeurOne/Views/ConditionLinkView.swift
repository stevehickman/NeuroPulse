import SwiftUI

/// Condition chips + the "leaving the app" confirmation — NP-COND-LINK-001 §4.
///
/// One view for both iOS and macOS. The only platform-specific part is the
/// presentation detent, and the open call itself is `@Environment(\.openURL)`,
/// which resolves to UIApplication on iOS and NSWorkspace on macOS without any
/// conditional code here.
///
/// Nothing in this file logs. Which conditions a user looks up is health-
/// adjacent and therefore UHDR-class under the §5.1 boundary rule ("when in
/// doubt → UHDR"), so it is not recorded anywhere — no analytics event, no
/// SHDR field. See NP-COND-LINK-001 §2.

// MARK: - Chips

struct ConditionChipsView: View {
    let conditions: [String]

    @State private var selected: NPResolvedCondition?

    var body: some View {
        if conditions.isEmpty {
            EmptyView()
        } else {
            ConditionFlowLayout(spacing: 6) {
                ForEach(NPBundledConditions.resolve(conditions)) { condition in
                    ConditionChip(condition: condition) { selected = condition }
                }
            }
            .sheet(item: $selected) { condition in
                ConditionLinkConfirmation(condition: condition)
            }
        }
    }
}

private struct ConditionChip: View {
    let condition: NPResolvedCondition
    let action: () -> Void

    private var isAllowed: Bool { condition.verdict.allowed }

    var body: some View {
        Button(action: action) {
            HStack(spacing: 4) {
                Text(condition.name)
                    .font(.caption.weight(.semibold))
                    .strikethrough(!isAllowed)
                if isAllowed {
                    Image(systemName: "arrow.up.forward")
                        .font(.system(size: 8, weight: .semibold))
                        .opacity(0.7)
                }
            }
            .padding(.horizontal, 9)
            .padding(.vertical, 4)
            .background(
                Capsule().fill(Color.secondary.opacity(0.15))
            )
            .overlay(
                Capsule().strokeBorder(Color.secondary.opacity(0.3), lineWidth: 1)
            )
            .foregroundStyle(isAllowed ? Color.primary : Color.secondary)
            .opacity(isAllowed ? 1.0 : 0.55)
        }
        .buttonStyle(.plain)
        .accessibilityLabel(
            isAllowed
                ? "\(condition.name). Opens a definition on \(condition.verdict.host ?? "")."
                : "\(condition.name). \(condition.verdict.reason.blockedMessage)"
        )
    }
}

// MARK: - Confirmation

struct ConditionLinkConfirmation: View {
    let condition: NPResolvedCondition

    @Environment(\.dismiss) private var dismiss
    @Environment(\.openURL) private var openURL

    private var verdict: NPLinkVerdict { condition.verdict }

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            header

            if let description = condition.definition?.description {
                Text(description)
                    .font(.callout)
            }

            if verdict.allowed {
                Text("This opens an external site in your browser. NeurOne does not record which conditions you look up.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)

                destination
            } else {
                Label(blockedText, systemImage: "exclamationmark.triangle")
                    .font(.callout)
                    .foregroundStyle(.orange)
            }

            Spacer(minLength: 0)
            actions
        }
        .padding(20)
        .frame(minWidth: 320, idealWidth: 380)
        .presentationDetents([.height(verdict.allowed ? 300 : 240)])
    }

    private var header: some View {
        HStack(alignment: .firstTextBaseline) {
            Text(condition.name)
                .font(.headline)
            Spacer()
            if let code = condition.definition?.code {
                Text("ICD-11 \(code)")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    /// The host, not the full URL. The host is the security-relevant part, and a
    /// full Wikipedia URL is long enough to push the buttons off a small sheet.
    private var destination: some View {
        VStack(alignment: .leading, spacing: 3) {
            Text("DESTINATION")
                .font(.system(size: 10, weight: .semibold))
                .tracking(0.6)
                .foregroundStyle(.secondary)
            Text(verdict.host ?? "")
                .font(.body.weight(.bold))
                .textSelection(.enabled)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 8).fill(Color.secondary.opacity(0.12))
        )
    }

    private var blockedText: String {
        condition.definition == nil
            ? verdict.reason.blockedMessage + " This condition is not in the registry."
            : verdict.reason.blockedMessage
    }

    private var actions: some View {
        HStack {
            Spacer()
            Button(verdict.allowed ? "Cancel" : "Close") { dismiss() }
                .keyboardShortcut(.cancelAction)

            if verdict.allowed {
                Button("Open in browser") {
                    // `url` is non-nil exactly when the verdict is allowed, and it
                    // has already passed NPConditionLinkPolicy — this is the only
                    // place a condition link reaches the OS.
                    if let raw = verdict.url, let url = URL(string: raw) {
                        openURL(url)
                    }
                    dismiss()
                }
                .keyboardShortcut(.defaultAction)
                .buttonStyle(.borderedProminent)
            }
        }
    }
}

// MARK: - Layout

/// Wrapping row layout for the chips. `Layout` is available on the deployment
/// targets (iOS 17 / macOS 14), so this avoids pulling in a grid that would
/// force uniform column widths on variable-length condition names.
private struct ConditionFlowLayout: Layout {
    var spacing: CGFloat = 6

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let maxWidth = proposal.width ?? .infinity
        var rowWidth: CGFloat = 0
        var rowHeight: CGFloat = 0
        var totalHeight: CGFloat = 0
        var maxRowWidth: CGFloat = 0

        for subview in subviews {
            let size = subview.sizeThatFits(.unspecified)
            if rowWidth > 0 && rowWidth + spacing + size.width > maxWidth {
                totalHeight += rowHeight + spacing
                maxRowWidth = max(maxRowWidth, rowWidth)
                rowWidth = size.width
                rowHeight = size.height
            } else {
                rowWidth += rowWidth > 0 ? spacing + size.width : size.width
                rowHeight = max(rowHeight, size.height)
            }
        }
        maxRowWidth = max(maxRowWidth, rowWidth)
        return CGSize(width: maxRowWidth, height: totalHeight + rowHeight)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var x = bounds.minX
        var y = bounds.minY
        var rowHeight: CGFloat = 0

        for subview in subviews {
            let size = subview.sizeThatFits(.unspecified)
            if x > bounds.minX && x + size.width > bounds.maxX {
                x = bounds.minX
                y += rowHeight + spacing
                rowHeight = 0
            }
            subview.place(at: CGPoint(x: x, y: y), proposal: ProposedViewSize(size))
            x += size.width + spacing
            rowHeight = max(rowHeight, size.height)
        }
    }
}

// MARK: - Preview

#Preview("Allowed") {
    ConditionLinkConfirmation(
        condition: NPBundledConditions.resolve(["Major Depressive Disorder"])[0]
    )
}

#Preview("Blocked") {
    ConditionLinkConfirmation(
        condition: NPResolvedCondition(
            name: "Unlisted Condition",
            definition: nil,
            verdict: .blocked(.notAbsolute)
        )
    )
}

#Preview("Chips") {
    ConditionChipsView(conditions: [
        "Major Depressive Disorder", "Insomnia", "Migraine", "Unlisted Condition",
    ])
    .padding()
}
