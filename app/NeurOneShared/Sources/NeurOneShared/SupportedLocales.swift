import Foundation

public enum SupportedLocale: String, CaseIterable, Sendable {
    case en = "en"
    case enGB = "en-GB"
    case es419 = "es-419"
    case ca = "ca"
    case zhHans = "zh-Hans"
    case ar = "ar"
    case fr = "fr"
    case hi = "hi"
    case bn = "bn"
    case id = "id"
    case ru = "ru"

    public var bcp47Tag: String { rawValue }

    public var displayName: String {
        switch self {
        case .en: return "English (US)"
        case .enGB: return "English (UK)"
        case .es419: return "Español (Latinoamérica)"
        case .ca: return "Català"
        case .zhHans: return "简体中文"
        case .ar: return "العربية"
        case .fr: return "Français"
        case .hi: return "हिन्दी"
        case .bn: return "বাংলা"
        case .id: return "Bahasa Indonesia"
        case .ru: return "Русский"
        }
    }

    public var isRTL: Bool {
        self == .ar
    }

    public static func from(languageCode: String) -> SupportedLocale {
        if let exact = SupportedLocale(rawValue: languageCode) {
            return exact
        }
        let prefix = languageCode.components(separatedBy: "-").first ?? languageCode
        switch prefix {
        case "es": return .es419
        case "zh": return .zhHans
        default:
            return SupportedLocale.allCases.first { $0.rawValue.hasPrefix(prefix) } ?? .en
        }
    }
}
