import Foundation

// PAUSE_LIMIT (SWR-SS-05): the longest a session may stay paused before the helmet ends it, in seconds. It is the hardware
// owner's option and not the wearer's, and it defaults to NO LIMIT. Wire: a little-endian uint32, 0 is no limit
// (firmware/hub_control/include/np_pause_limit.h). Any value is accepted by the helmet, since the owner decides what is
// sensible for the clinic, so no figure is built in here either; the screen offers whole minutes.
enum PauseLimit {
    static let length = 4
    static let noLimitSeconds: UInt32 = 0

    /// Nil for any frame that is not exactly four bytes: no part of it is believed.
    static func seconds(fromWire data: Data) -> UInt32? {
        guard data.count == length else { return nil }
        let b = [UInt8](data)
        return (0..<4).reduce(UInt32(0)) { $0 | (UInt32(b[$1]) << UInt32(8 * $1)) }
    }

    /// The write for `seconds`.
    static func wire(seconds: UInt32) -> Data {
        Data((0..<4).map { UInt8((seconds >> UInt32(8 * $0)) & 0xFF) })
    }

    /// Whole minutes to the seconds the helmet takes; 0 is no limit. Nil for a number that would overflow the helmet's
    /// uint32.
    static func seconds(ofMinutes minutes: UInt32) -> UInt32? {
        let (product, overflow) = minutes.multipliedReportingOverflow(by: 60)
        return overflow ? nil : product
    }
}
