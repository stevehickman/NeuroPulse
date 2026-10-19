import Foundation
import Security

// The fleet upload as Oblivious HTTP (RFC 9458; privacy review Finding 6, OI-FLEET-01 (1), REQ-SW03-24).
//
// The record is sealed to the fleet *gateway's* key and posted to the *relay*, so the server that reads it never sees the
// connection's network address and the relay never sees the record. The sealing is the shared Rust core's (`ohttp.rs`,
// CLAUDE.md §22); this only moves bytes and supplies entropy, as Kotlin's `FleetOhttp` does for Android. The protection holds
// only while the relay and the gateway are run by different parties and the relay keeps no log (NP-INFRA-001 §3.3b).
enum FleetOhttp {
    static let requestMediaType = "message/ohttp-req"
    static let responseMediaType = "message/ohttp-res"

    /// A sealed request. `enc` and `secret` are needed once, for the answer; keep them in memory and drop them.
    struct Sealed {
        let request: Data
        fileprivate let enc: String
        fileprivate let secret: String
    }

    /// Seal `POST <path>` to `authority` (the fleet service as the gateway knows it) with the device token header and
    /// `payload`. A key configuration that is not this build's suite is refused with the core's message.
    static func seal(
        keyConfigHex: String,
        authority: String,
        path: String,
        tokenHeader: String,
        token: String,
        payload: Data,
        entropy: Data? = nil
    ) throws -> Sealed {
        let r = try NppsCore.migration([
            "op": "ohttpSeal",
            "keyConfig": keyConfigHex,
            "method": "POST",
            "scheme": "https",
            "authority": authority,
            "path": path,
            "headers": [[tokenHeader, token], ["content-type", "application/octet-stream"]],
            "body": hex(payload),
            "entropy": hex(entropy ?? randomBytes(32)),
        ])
        guard let request = r["request"] as? String, let enc = r["enc"] as? String, let secret = r["secret"] as? String,
              let bytes = unhex(request) else {
            throw NppsCore.Refusal(message: "the core did not seal the request")
        }
        return Sealed(request: bytes, enc: enc, secret: secret)
    }

    /// Whether the fleet service took the record, given what came back from the relay. The relay's own status says only that
    /// it passed the message on, so an answer that is not `message/ohttp-res`, or does not authenticate, is not an acceptance,
    /// and neither is the fleet service's refusal.
    static func accepted(_ sealed: Sealed, relayOK: Bool, contentType: String?, body: Data?) -> Bool {
        guard relayOK, contentType == responseMediaType, let body else { return false }
        guard let r = try? NppsCore.migration([
            "op": "ohttpOpen", "enc": sealed.enc, "secret": sealed.secret, "response": hex(body),
        ]), let status = r["status"] as? Int else { return false }
        return (200...299).contains(status)
    }

    private static func randomBytes(_ count: Int) -> Data {
        var bytes = [UInt8](repeating: 0, count: count)
        let status = SecRandomCopyBytes(kSecRandomDefault, count, &bytes)
        precondition(status == errSecSuccess, "the system random source failed")
        return Data(bytes)
    }

    private static func hex(_ data: Data) -> String { data.map { String(format: "%02x", $0) }.joined() }

    private static func unhex(_ s: String) -> Data? {
        guard s.count % 2 == 0 else { return nil }
        var out = Data(capacity: s.count / 2)
        var index = s.startIndex
        while index < s.endIndex {
            let next = s.index(index, offsetBy: 2)
            guard let byte = UInt8(s[index..<next], radix: 16) else { return nil }
            out.append(byte)
            index = next
        }
        return out
    }
}
