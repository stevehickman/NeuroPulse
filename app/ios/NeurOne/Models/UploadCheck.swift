import CryptoKit
import Foundation

// UPLOAD_CHECK (SWR-UP-01, OI-UI-KMP-10 U1): what the helmet says it received. After every protocol upload the helmet
// hashes the buffer it received, before parsing, and reports that hash with an accept or refuse status. The app compares
// the hash with the SHA-256 of the exact bytes it sent. Wire layout: firmware/hub_control/include/np_upload_check.h,
// 34 bytes: status, reason, SHA-256. The hash is of protocol bytes the person authored and carries no user biology.

enum UploadCheckStatus: UInt8 {
    /// Nothing received since power-on.
    case none = 0
    case accepted = 1
    case refused = 2
    /// Hashed, verdict not yet reached: the app reads again, and never sends the protocol again (SWR-UP-03).
    case pending = 3
}

struct UploadCheck: Equatable {
    var status: UploadCheckStatus
    /// The refusal's `np_hub_status_t` code as a positive number; 0 unless `status` is `.refused`.
    var reason: Int
    /// SHA-256 of the received bytes, 64 lowercase hex digits; all zeros while `status` is `.none`.
    var receivedHash: String

    static let length = 34

    /// `NP_HUB_ERR_TIER_F4` (19) and `NP_HUB_ERR_TIER_UNVERIFIED` (20): the helmet refused a protocol for its tier. A refusal
    /// for the hardware version or an unmodified clone is not made by the hub (not decided), so no code names it.
    private static let tierRefusalCodes: Set<Int> = [19, 20]

    /// The `helmetRefusal` the library's upload rule takes for this refusal, or nil when it is none of the three it names.
    var refusalKind: String? {
        status == .refused && Self.tierRefusalCodes.contains(reason) ? "tier" : nil
    }

    /// Nil for a frame of the wrong length or an unknown status: no part of it is believed.
    init?(wire data: Data) {
        guard data.count == Self.length, let status = UploadCheckStatus(rawValue: data[data.startIndex]) else { return nil }
        self.status = status
        self.reason = Int(data[data.startIndex + 1])
        self.receivedHash = data.dropFirst(2).map { String(format: "%02x", $0) }.joined()
    }

    init(status: UploadCheckStatus, reason: Int, receivedHash: String) {
        self.status = status
        self.reason = reason
        self.receivedHash = receivedHash
    }

    /// The hash of the exact bytes the app sent, in the form `receivedHash` takes.
    static func hash(of sent: Data) -> String {
        SHA256.hash(data: sent).map { String(format: "%02x", $0) }.joined()
    }
}

/// What the library's upload rule is told (`SWR-UP-02`). Whether it was confirmed, unknown or failed is the core's decision
/// (`NPProtocolLibrary.recordUploadResult`); this only reads the helmet's answer.
struct UploadVerdict: Equatable {
    var helmetStatus: String?
    var helmetHash: String?
    var helmetRefusal: String?

    /// The verdict from the last report the helmet gave (nil if it never gave one). A final report (accepted or refused)
    /// is passed on with its hash; the library compares that hash with the one the app sent, so a final report for other
    /// bytes is a failed upload and not a guess here. No report, an empty one or one still pending is no answer, which the
    /// library treats as unknown (`SWR-UP-03`).
    static func of(_ check: UploadCheck?) -> UploadVerdict {
        guard let check, check.status == .accepted || check.status == .refused else {
            return UploadVerdict(helmetStatus: nil, helmetHash: nil, helmetRefusal: nil)
        }
        return UploadVerdict(helmetStatus: check.status == .accepted ? "accepted" : "refused",
                             helmetHash: check.receivedHash, helmetRefusal: check.refusalKind)
    }
}
