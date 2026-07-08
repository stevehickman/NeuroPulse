import Foundation
import os

/// Consent gate for SHDR fleet analytics — warranty analytics (CLAUDE.md §5.1,
/// NP-FW-EMMC-002 Rev A §A).
///
/// SHDR (System Health Data Record) is device-condition telemetry only — it never
/// contains user biology. It is linked exclusively to an opaque warranty token;
/// NeurOne cannot cross-reference SHDR records to any user identity.
///
/// Consent subject: the **warranty owner** (the entity that registered the device —
/// typically the purchaser, which may be a clinic or institution, NOT the device user).
///
/// This gate is entirely independent of `ResearchAnalyticsGate`. The two consent
/// subjects are never mixed:
///   • Warranty consent belongs to the warranty owner — governs SHDR uploads via
///     `SHDRUploader`. Granted once at device registration.
///   • Research consent belongs to the device user — governs PostHog SDK via
///     `ResearchAnalyticsGate`. `ConsentStore` manages this; it never touches
///     `WarrantyAnalyticsGate`.
///
/// Revoking warranty consent disables SHDR fleet uploads. It has no effect on
/// `ResearchAnalyticsGate`, `ConsentStore`, or any UHDR data flow.
@MainActor
enum WarrantyAnalyticsGate {

    private static let log = Logger(subsystem: "com.neurone.analytics", category: "warranty-gate")

    /// UserDefaults key set when the warranty owner grants SHDR fleet telemetry
    /// at device registration. Never read or written by `ConsentStore` or any
    /// research-consent path — warranty consent and research consent are independent.
    static let warrantyConsentKey = "np.warranty.consent.granted"

    /// True when the warranty owner has consented to SHDR fleet telemetry.
    /// Checked by `SHDRUploader.uploadIfConsented()` before each upload attempt.
    static var isOpen: Bool {
        UserDefaults.standard.bool(forKey: warrantyConsentKey)
    }

    /// Record warranty consent at device registration.
    ///
    /// Called once when the warranty owner completes device registration. After
    /// this, `SHDRUploader` may upload fleet telemetry on USB-C connections.
    ///
    /// Does NOT affect `ResearchAnalyticsGate`, `ConsentStore`, or any UHDR flows.
    static func grant() {
        UserDefaults.standard.set(true, forKey: warrantyConsentKey)
        log.debug("WarrantyAnalyticsGate.grant() — warranty consent recorded; SHDR uploads enabled.")
    }

    /// Revoke warranty consent. Disables future SHDR fleet uploads.
    ///
    /// Does NOT affect `ResearchAnalyticsGate`, `ConsentStore`, or any UHDR flows.
    /// Does NOT delete already-uploaded SHDR data from the fleet database.
    static func revoke() {
        UserDefaults.standard.removeObject(forKey: warrantyConsentKey)
        log.debug("WarrantyAnalyticsGate.revoke() — warranty consent revoked; SHDR uploads disabled.")
    }
}
