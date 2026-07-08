import Foundation

// RegionHelper — retained as a namespace stub.
//
// The previous `isLikelyIllinois` property detected Illinois via locale, then
// gated the BIPA biometric-consent disclosure to IL users only. Locale is not
// a reliable proxy for legal jurisdiction (a Chicago user with a non-IL device
// locale would never see the disclosure — a BIPA violation).
//
// Resolution (OI-PA-03): the biometric-data consent disclosure is now shown to
// ALL users before their first EEG session. No locale check is needed. The
// disclosure content satisfies BIPA for IL users, GDPR Art. 9 for EU users,
// and WA MHMD for Washington users — and is sound privacy practice everywhere.
enum RegionHelper {}
