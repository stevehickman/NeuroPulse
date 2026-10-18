import Foundation

/// The host the app sends analytics and erasure requests to, from the build (REQ-SW03-20).
///
/// It is a build setting (`ANALYTICS_HOST` in `project.yml`, read from Info.plist as `AnalyticsHost`), not a literal in the source, so
/// a release build names its production domain and a test host never ships by accident. Only a bare host name (and an optional
/// port) is accepted: an `.xcconfig` reads `//` as a comment, so a value with `https://` could not be set there, and the app
/// always speaks HTTPS. A host that is missing, unexpanded (`$(ANALYTICS_HOST)`) or not a plain host name gives `nil`, and a
/// caller then sends nothing: no analytics event leaves, and an erasure request stays queued.
enum AnalyticsHost {
    /// `https://<host>` for the build's host, or nil if none is configured or the value is not a plain host name.
    static func baseURL(bundle: Bundle = .main) -> URL? {
        baseURL(from: bundle.object(forInfoDictionaryKey: "AnalyticsHost") as? String)
    }

    /// The same rule on a value, for tests. Letters, digits, dots and hyphens, at least one dot or `localhost`, and an optional
    /// `:port`; nothing else (no scheme, path, query, space, user or `$(…)`).
    static func baseURL(from value: String?) -> URL? {
        guard let value, !value.isEmpty else { return nil }
        let parts = value.split(separator: ":", omittingEmptySubsequences: false)
        guard parts.count <= 2, let host = parts.first.map(String.init), !host.isEmpty else { return nil }
        let allowed = Set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-")
        guard host.allSatisfy({ allowed.contains($0) }),
              !host.hasPrefix("."), !host.hasPrefix("-"), !host.hasSuffix("."), !host.contains(".."),
              host == "localhost" || host.contains(".")
        else { return nil }
        if parts.count == 2 {
            guard let port = parts.last, !port.isEmpty, port.count <= 5, port.allSatisfy(\.isNumber),
                  let n = Int(port), (1...65535).contains(n)
            else { return nil }
        }
        return URL(string: "https://\(value)")
    }
}
