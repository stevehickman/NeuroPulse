import XCTest
@testable import NeurOneShared

final class SupportedLocalesTests: XCTestCase {

    // @verifies REQ-SW03-09
    func testAllCasesReturnsExactly11() {
        XCTAssertEqual(SupportedLocale.allCases.count, 11)
    }

    // @verifies REQ-SW03-09
    func testEveryLocaleHasNonEmptyBCP47Tag() {
        for locale in SupportedLocale.allCases {
            XCTAssertFalse(locale.bcp47Tag.isEmpty, "\(locale) has empty bcp47Tag")
        }
    }

    // @verifies REQ-SW03-09
    func testEveryLocaleHasNonEmptyDisplayName() {
        for locale in SupportedLocale.allCases {
            XCTAssertFalse(locale.displayName.isEmpty, "\(locale) has empty displayName")
        }
    }

    // @verifies REQ-SW03-09
    func testArabicIsRTL() {
        XCTAssertTrue(SupportedLocale.ar.isRTL)
    }

    // @verifies REQ-SW03-09
    func testNonArabicLocalesAreNotRTL() {
        for locale in SupportedLocale.allCases where locale != .ar {
            XCTAssertFalse(locale.isRTL, "\(locale) should not be RTL")
        }
    }

    // @verifies REQ-SW03-09
    func testBCP47TagsAreUnique() {
        let tags = SupportedLocale.allCases.map(\.bcp47Tag)
        XCTAssertEqual(tags.count, Set(tags).count, "Duplicate BCP 47 tags found")
    }

    // @verifies REQ-SW03-09
    func testFromLanguageCodeExactMatch() {
        XCTAssertEqual(SupportedLocale.from(languageCode: "fr"), .fr)
        XCTAssertEqual(SupportedLocale.from(languageCode: "ar"), .ar)
        XCTAssertEqual(SupportedLocale.from(languageCode: "en-GB"), .enGB)
    }

    // @verifies REQ-SW03-09
    func testFromLanguageCodeFallsBackToPrefix() {
        XCTAssertEqual(SupportedLocale.from(languageCode: "es-MX"), .es419)
        XCTAssertEqual(SupportedLocale.from(languageCode: "zh-TW"), .zhHans)
    }

    // @verifies REQ-SW03-09
    func testFromLanguageCodeFallsBackToEnglish() {
        XCTAssertEqual(SupportedLocale.from(languageCode: "ja"), .en)
    }

    // @verifies REQ-SW03-09
    func testRussianLocaleExists() {
        XCTAssertEqual(SupportedLocale.ru.bcp47Tag, "ru")
        XCTAssertEqual(SupportedLocale.ru.displayName, "Русский")
        XCTAssertFalse(SupportedLocale.ru.isRTL)
    }
}
