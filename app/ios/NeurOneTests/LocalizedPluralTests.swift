import XCTest
@testable import NeurOne

/// Copy moved out of Swift literals into locale keys must still render as text.
///
/// The plural families (`PROTOCOL_TIMING_INTERVALS`, `MODALITY_SOCKETS_TARGETED`) are the first in the app resolved from Swift, via
/// `NSLocalizedString` + `String.localizedStringWithFormat` against the compiled
/// `.stringsdict`. A wrong lookup renders the key, or the `other` form for 1, and
/// a green build would not show either.
final class LocalizedPluralTests: XCTestCase {

    func testIntervalCountSelectsThePluralCategory() {
        XCTAssertEqual(NPProtocolDefinition.TimingMode.intervalCount(1).displayString, "1 interval")
        XCTAssertEqual(NPProtocolDefinition.TimingMode.intervalCount(3).displayString, "3 intervals")
    }

    func testSocketFamilyResolves() {
        func plural(_ key: String, _ n: Int) -> String {
            String.localizedStringWithFormat(NSLocalizedString(key, comment: ""), n)
        }
        XCTAssertEqual(plural("MODALITY_SOCKETS_TARGETED", 1), "1 socket targeted.")
        XCTAssertEqual(plural("MODALITY_SOCKETS_TARGETED", 7), "7 sockets targeted.")
    }

    /// The tier name was the persisted `rawValue`, formatted into on-screen text.
    func testClinicianTierRendersItsDisplayNameAndPrice() {
        XCTAssertEqual(ClinicianUseCaseTier.fullClinical.displayName, "Full Clinical")
        XCTAssertEqual(ClinicianUseCaseTier.fullClinical.monthlyPrice, "$299/month/patient")
        XCTAssertEqual(ClinicianUseCaseTier.research.monthlyPrice, "$599/month/study")
    }
}
