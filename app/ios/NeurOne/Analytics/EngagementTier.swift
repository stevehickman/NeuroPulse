import Foundation

// engagement_tier replaces raw session_count per NP-APP-TELEMETRY-001 Rev B §3.
// Counts app launches (not stimulation sessions). Resets on uninstall.
// Never transmitted as a raw integer — only the bucketed enum label is sent.
enum EngagementTier: String {
    case new         = "new"         // 1–5 app launches
    case active      = "active"      // 6–50 app launches
    case established = "established" // 51+ app launches

    static func current() -> EngagementTier {
        let count = UserDefaults.standard.integer(forKey: "np.analytics.launch-count")
        switch count {
        case ..<6:   return .new       // covers 0 (key absent) and 1–5
        case 6...50: return .active
        default:     return .established
        }
    }

    static func incrementLaunchCount() {
        let key = "np.analytics.launch-count"
        let current = UserDefaults.standard.integer(forKey: key)
        UserDefaults.standard.set(current + 1, forKey: key)
    }
}
