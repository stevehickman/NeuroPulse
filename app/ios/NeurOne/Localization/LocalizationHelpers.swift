import Foundation

// Locale-aware number formatting utilities.
// Replace String(format: "%.Nf", value) with these helpers so decimal
// separators adapt to the user's locale (e.g. "," in German, "." in English).

enum NPNumberFormatter {

    private static let decimal1: NumberFormatter = {
        let f = NumberFormatter()
        f.numberStyle = .decimal
        f.minimumFractionDigits = 1
        f.maximumFractionDigits = 1
        return f
    }()

    private static let decimal0: NumberFormatter = {
        let f = NumberFormatter()
        f.numberStyle = .decimal
        f.minimumFractionDigits = 0
        f.maximumFractionDigits = 0
        return f
    }()

    /// Format a Float to one decimal place using the current locale.
    static func decimal1(_ value: Float) -> String {
        decimal1.string(from: NSNumber(value: value)) ?? String(format: "%.1f", value)
    }

    /// Format a Double to zero decimal places using the current locale.
    static func decimal0(_ value: Double) -> String {
        decimal0.string(from: NSNumber(value: value)) ?? String(format: "%.0f", value)
    }
}

// Temperature display — converts Celsius to the user's preferred unit.
// iOS respects the system Locale for MeasurementFormatter automatically.
enum NPTemperatureFormatter {

    /// Display a Celsius value in the user's preferred temperature unit.
    static func localized(_ celsius: Double) -> String {
        let measurement = Measurement(value: celsius, unit: UnitTemperature.celsius)
        let formatter = MeasurementFormatter()
        formatter.unitStyle = .short
        formatter.numberFormatter.maximumFractionDigits = 0
        return formatter.string(from: measurement)
    }
}
