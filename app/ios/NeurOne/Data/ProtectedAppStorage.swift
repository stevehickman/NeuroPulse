import SwiftUI

// `@AppStorage` for the values that must not sit in the backup-eligible `UserDefaults` plist (desktop audit of the privacy review,
// `SWR-MG-16`): the age and biometric-privacy answers, the onboarding screens already shown, the backup folder. The same shape
// as `@AppStorage` (a key, a default, a `Binding` from `$`), kept in `ProtectedDefaults`, and a view that reads one redraws when
// it changes, from this view or from any other.

/// A value `ProtectedAppStorage` can keep: it is rebuilt from what the property-list file holds.
protocol ProtectedStorable {
    init?(protectedValue: Any)
}

extension Bool: ProtectedStorable {
    init?(protectedValue: Any) {
        guard let number = protectedValue as? NSNumber else { return nil }
        self = number.boolValue
    }
}

extension Int: ProtectedStorable {
    init?(protectedValue: Any) {
        guard let number = protectedValue as? NSNumber else { return nil }
        self = number.intValue
    }
}

extension Double: ProtectedStorable {
    init?(protectedValue: Any) {
        guard let number = protectedValue as? NSNumber else { return nil }
        self = number.doubleValue
    }
}

extension String: ProtectedStorable {
    init?(protectedValue: Any) {
        guard let text = protectedValue as? String else { return nil }
        self = text
    }
}

@propertyWrapper
struct ProtectedAppStorage<Value: ProtectedStorable>: DynamicProperty {
    @ObservedObject private var store: ProtectedDefaults
    private let key: String
    private let fallback: Value

    init(wrappedValue: Value, _ key: String, store: ProtectedDefaults = .standard) {
        _store = ObservedObject(wrappedValue: store)
        self.key = key
        self.fallback = wrappedValue
    }

    var wrappedValue: Value {
        get { store.object(forKey: key).flatMap { Value(protectedValue: $0) } ?? fallback }
        nonmutating set { store.set(newValue, forKey: key) }
    }

    var projectedValue: Binding<Value> {
        Binding(get: { wrappedValue }, set: { wrappedValue = $0 })
    }
}
