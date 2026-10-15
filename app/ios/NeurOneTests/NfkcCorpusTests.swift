//
//  NfkcCorpusTests.swift
//  NeurOneTests
//
//  The passphrase normalisation through the C ABI binding: the corpus of known NFKC answers
//  (app/NeurOneShared/TestData/nfkc-corpus.json) that the Rust core, Kotlin, C# and the web run too. Swift strings are
//  grapheme-aware UTF-8 and the C ABI takes bytes, so the cases outside the BMP and the combining sequences are the ones this
//  proves (SWR-MG-02, NP-SOUP-RUST-001).
//
//  Subject under test: NppsCore.migration "passphraseSecret" (app/ios/NeurOne/Session/NppsCore.swift)

import XCTest
@testable import NeurOne

final class NfkcCorpusTests: XCTestCase {

    private func corpus() throws -> [String: Any] {
        // .../app/ios/NeurOneTests/<this file> → .../app/NeurOneShared/TestData/
        var url = URL(fileURLWithPath: #filePath)
        url.deleteLastPathComponent()
        url.deleteLastPathComponent()
        url.deleteLastPathComponent()
        url.appendPathComponent("NeurOneShared/TestData/nfkc-corpus.json")
        let object = try JSONSerialization.jsonObject(with: Data(contentsOf: url))
        return try XCTUnwrap(object as? [String: Any])
    }

    private func secret(_ passphrase: String) throws -> [String: Any] {
        try NppsCore.migration(["op": "passphraseSecret", "passphrase": passphrase])
    }

    private func hexOf(_ text: String) -> String { text.utf8.map { String(format: "%02x", $0) }.joined() }

    // @verifies SWR-MG-02
    func testEveryKnownNfkcAnswerHoldsThroughTheBinding() throws {
        let corpus = try corpus()
        let pad = try XCTUnwrap(corpus["padding"] as? String)
        var wrong: [String] = []
        for c in try XCTUnwrap(corpus["cases"] as? [[String: Any]]) {
            let name = c["name"] as? String ?? ""
            let got = try secret(pad + (c["input"] as? String ?? ""))["secret"] as? String
            let want = hexOf(pad + (c["expected"] as? String ?? ""))
            if got != want { wrong.append("\(name): \(got ?? "nil") != \(want)") }
        }
        for c in try XCTUnwrap(corpus["exact"] as? [[String: Any]]) {
            let got = try secret(c["input"] as? String ?? "")["secret"] as? String
            if got != hexOf(c["expected"] as? String ?? "") { wrong.append("\(c["name"] as? String ?? ""): the floor is counted wrongly") }
        }
        for c in try XCTUnwrap(corpus["tooShort"] as? [[String: Any]]) {
            let got = try secret(c["input"] as? String ?? "")["reason"] as? String
            if got != "too_short" { wrong.append("\(c["name"] as? String ?? ""): accepted under the floor") }
        }
        XCTAssertTrue(wrong.isEmpty, "\(wrong.count) answer(s) differ:\n" + wrong.joined(separator: "\n"))
        XCTAssertEqual(corpus["minChars"] as? Int, 12)
    }
}
