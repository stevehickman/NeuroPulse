import XCTest
@testable import NeuroPulseShared

final class SupportedLocalesTests: XCTestCase {

    func testAllCasesReturnsExactly11() {
        XCTAssertEqual(SupportedLocale.allCases.count, 11)
    }

    func testEveryLocaleHasNonEmptyBCP47Tag() {
        for locale in SupportedLocale.allCases {
            XCTAssertFalse(locale.bcp47Tag.isEmpty, "\(locale) has empty bcp47Tag")
        }
    }

    func testEveryLocaleHasNonEmptyDisplayName() {
        for locale in SupportedLocale.allCases {
            XCTAssertFalse(locale.displayName.isEmpty, "\(locale) has empty displayName")
        }
    }

    func testArabicIsRTL() {
        XCTAssertTrue(SupportedLocale.ar.isRTL)
    }

    func testNonArabicLocalesAreNotRTL() {
        for locale in SupportedLocale.allCases where locale != .ar {
            XCTAssertFalse(locale.isRTL, "\(locale) should not be RTL")
        }
    }

    func testBCP47TagsAreUnique() {
        let tags = SupportedLocale.allCases.map(\.bcp47Tag)
        XCTAssertEqual(tags.count, Set(tags).count, "Duplicate BCP 47 tags found")
    }

    func testFromLanguageCodeExactMatch() {
        XCTAssertEqual(SupportedLocale.from(languageCode: "fr"), .fr)
        XCTAssertEqual(SupportedLocale.from(languageCode: "ar"), .ar)
        XCTAssertEqual(SupportedLocale.from(languageCode: "en-GB"), .enGB)
    }

    func testFromLanguageCodeFallsBackToPrefix() {
        XCTAssertEqual(SupportedLocale.from(languageCode: "es-MX"), .es419)
        XCTAssertEqual(SupportedLocale.from(languageCode: "zh-TW"), .zhHans)
    }

    func testFromLanguageCodeFallsBackToEnglish() {
        XCTAssertEqual(SupportedLocale.from(languageCode: "ja"), .en)
    }

    func testRussianLocaleExists() {
        XCTAssertEqual(SupportedLocale.ru.bcp47Tag, "ru")
        XCTAssertEqual(SupportedLocale.ru.displayName, "Русский")
        XCTAssertFalse(SupportedLocale.ru.isRTL)
    }
}
