import SwiftUI

// The dialogs the library's refusals put in front of a person (SWR-PL-02/04/06, SWR-FM-05, SWR-HV-04). Each states
// what happened, that nothing was lost or sent, and the way forward. The rules that raise them are the core's
// (NPProtocolFamilies.swift); this file only shows the answers.

/// The copy-name callback the library takes: the localized " (copy)" form, also used for a protocol that is cloned.
func protocolCopyName(_ base: String) -> String {
    String(format: String(localized: "PROTOCOL_MENU_COPY_OF"), base)
}

extension View {

    /// A save into a used protocol was refused (SWR-PL-02, SWR-FM-03): say so, and offer a copy of the whole family.
    func usedProtocolAlert(isPresented: Binding<Bool>, onClone: @escaping () -> Void) -> some View {
        alert("PROTOCOL_LIBRARY_USED_TITLE", isPresented: isPresented) {
            Button("PROTOCOL_LIBRARY_CLONE_ACTION", action: onClone)
            Button("COMMON_CANCEL", role: .cancel) {}
        } message: {
            Text("PROTOCOL_LIBRARY_USED_BODY")
        }
    }

    /// A delete of a used protocol was refused (SWR-FM-05): offer to archive the family instead.
    func deleteUsedAlert(isPresented: Binding<Bool>, onArchive: @escaping () -> Void) -> some View {
        alert("PROTOCOL_LIBRARY_DELETE_USED_TITLE", isPresented: isPresented) {
            Button("PROTOCOL_LIBRARY_ARCHIVE_ACTION", action: onArchive)
            Button("COMMON_CANCEL", role: .cancel) {}
        } message: {
            Text("PROTOCOL_LIBRARY_DELETE_USED_BODY")
        }
    }

    /// The upload gate's refusals (SWR-PL-06, SWR-HV-04). Nothing was sent. An unmodified clone offers three ways on:
    /// send the original, go back and edit the clone, or cancel. A hardware mismatch offers the variant for the helmet
    /// when the family has one.
    func uploadGateAlert(
        gate: Binding<UploadGate?>,
        variantAvailable: Bool,
        onUploadSource: @escaping (UUID) -> Void,
        onEditClone: @escaping () -> Void,
        onUseVariant: @escaping () -> Void
    ) -> some View {
        alert(uploadGateTitle(gate.wrappedValue), isPresented: Binding(
            get: { gate.wrappedValue != nil },
            set: { if !$0 { gate.wrappedValue = nil } }
        )) {
            switch gate.wrappedValue {
            case .unmodifiedClone(let sourceId):
                Button("PROTOCOL_LIBRARY_UNMODIFIED_UPLOAD_SOURCE") { onUploadSource(sourceId) }
                Button("PROTOCOL_LIBRARY_UNMODIFIED_EDIT", action: onEditClone)
                Button("COMMON_CANCEL", role: .cancel) {}
            case .hardwareMismatch:
                if variantAvailable { Button("PROTOCOL_LIBRARY_MISMATCH_USE_VARIANT", action: onUseVariant) }
                Button("COMMON_CANCEL", role: .cancel) {}
            default:
                Button("OK", role: .cancel) {}
            }
        } message: {
            Text(uploadGateMessage(gate.wrappedValue, variantAvailable: variantAvailable))
        }
    }
}

private func uploadGateTitle(_ gate: UploadGate?) -> LocalizedStringKey {
    switch gate {
    case .unmodifiedClone: return "PROTOCOL_LIBRARY_UNMODIFIED_TITLE"
    case .hardwareMismatch: return "PROTOCOL_LIBRARY_MISMATCH_TITLE"
    default: return ""
    }
}

private func uploadGateMessage(_ gate: UploadGate?, variantAvailable: Bool) -> String {
    switch gate {
    case .unmodifiedClone:
        return String(localized: "PROTOCOL_LIBRARY_UNMODIFIED_BODY")
    case .hardwareMismatch(let helmetVersion, let protocolVersions, let shared, let onlyProtocol, let onlyHelmet):
        var text = String(format: String(localized: "PROTOCOL_LIBRARY_MISMATCH_BODY"),
                          protocolVersions.map(String.init).joined(separator: ", "), String(helmetVersion))
        // The overlap is drawn only when the helmet's layout is in the table; otherwise no overlap is claimed.
        if let shared, let onlyProtocol, let onlyHelmet {
            text += "\n" + String(format: String(localized: "PROTOCOL_LIBRARY_MISMATCH_DIFF"),
                                  String(shared), String(onlyProtocol), String(onlyHelmet))
        }
        if !variantAvailable { text += "\n" + String(localized: "PROTOCOL_LIBRARY_MISMATCH_NO_VARIANT") }
        return text
    default:
        return ""
    }
}
