import Foundation

/// Whether an analytics event may leave the phone (REQ-SW03-07; NP-APP-TELEMETRY-001 §3 and §7).
///
/// The rule is the shared core's, once (`common/npps-core/src/analytics.rs`, reading
/// `common/npps/analytics-properties.json`), so iOS and Android cannot disagree about it. It is an allowlist: the event's name
/// must have the allowed shape and contain no forbidden term, and every property must be one the policy names, with a value of
/// its form. Anything else drops the whole event.
///
/// **It fails closed.** If the core cannot be asked, or answers with anything but `ok: true`, the event is not permitted: an
/// analytics event that could not be checked is not one to send.
enum AnalyticsEventPolicy {
    static func permits(event: String, properties: [String: String]) -> Bool {
        guard
            let answer = try? NppsCore.migration([
                "op": "checkAnalyticsEvent",
                "event": event,
                "properties": properties,
            ]),
            let ok = answer["ok"] as? Bool
        else { return false }
        return ok
    }
}
