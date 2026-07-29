import Foundation

// Decoder and reassembler for the ZONE_MODULE_STATUS characteristic.
//
// The wire contract lives in `firmware/zone_announce/include/np_zone_notify.h`
// and is pinned by `firmware/zone_announce/tests/np_zone_notify_tests.c`. The
// constants below mirror it; if one moves, both sides move in the same commit.
//
// ── Why frames fragment ──────────────────────────────────────────────────────
//
// A full inventory of ~80 occupied sockets is ~324 bytes. An ATT notification is
// only guaranteed to carry 20 bytes, so a snapshot arrives as an ordered run of
// fragments, the last one flagged. `ZoneModuleFrameAssembler` accumulates the
// run and emits the snapshot only when the final fragment lands — a partial
// sequence is never handed on as a complete inventory.

/// One decoded fragment.
struct ZoneModuleFrame: Equatable {

    /// Mirrors `NP_ZN_FORMAT_VERSION`. Version 1 is the first socket-keyed
    /// format. The retired 5-byte one-byte-per-slot payload carried no version
    /// byte, so a hub still speaking it decodes as version 0 and is rejected
    /// rather than misread as socket data.
    static let formatVersion: UInt8 = 0x01
    static let headerBytes = 4
    static let recordBytes = 4

    /// Mirrors `NP_ZN_MAX_SOCKET_ID` — the full 7-bit major addressing domain.
    static let maxSocketID: UInt8 = 128

    private static let flagSnapshot: UInt8 = 0x01
    private static let flagLast: UInt8     = 0x02
    private static let recPresent: UInt8   = 0x01
    private static let recFault: UInt8     = 0x02

    let isSnapshot: Bool
    let isLastFragment: Bool
    let fragmentIndex: UInt8
    let records: [ZoneModuleStatus]

    init?(_ data: Data) {
        guard data.count >= Self.headerBytes else { return nil }

        // Index off startIndex — a Data sliced from a larger buffer does not
        // start at 0, and indexing it as if it did is a classic silent misparse.
        let base = data.startIndex
        guard data[base] == Self.formatVersion else { return nil }

        let flags = data[base + 1]
        let fragment = data[base + 2]
        let count = Int(data[base + 3])

        // The body must hold exactly the records the header claims. A header
        // promising more records than the payload carries is truncation.
        let needed = Self.headerBytes + count * Self.recordBytes
        guard data.count >= needed else { return nil }

        var decoded: [ZoneModuleStatus] = []
        decoded.reserveCapacity(count)

        for i in 0..<count {
            let r = base + Self.headerBytes + i * Self.recordBytes
            let socketID = data[r]
            let typeRaw  = data[r + 1]
            let anatomy  = data[r + 2]
            let recFlags = data[r + 3]

            // 0 is never emitted; ids above the domain would alias onto a real
            // socket, which is a wrong-site targeting path, not a display bug.
            guard socketID >= 1, socketID <= Self.maxSocketID else { return nil }

            let fault   = (recFlags & Self.recFault) != 0
            // Firmware clears presence on fault; re-assert it here so a hub that
            // ever sends both cannot produce a "present" faulted module.
            let present = (recFlags & Self.recPresent) != 0 && !fault

            decoded.append(ZoneModuleStatus(
                socketID: socketID,
                moduleType: ZoneModuleType(rawValue: typeRaw) ?? .unknown,
                lobe: ZoneLobe(rawValue: anatomy & 0x07) ?? .unspecified,
                side: ZoneSide(rawValue: (anatomy >> 3) & 0x03) ?? .midline,
                isPresent: present,
                hasFault: fault))
        }

        self.isSnapshot     = (flags & Self.flagSnapshot) != 0
        self.isLastFragment = (flags & Self.flagLast) != 0
        self.fragmentIndex  = fragment
        self.records        = decoded
    }

    /// Direct initialiser for tests and for synthesising a cleared state.
    init(isSnapshot: Bool, isLastFragment: Bool, fragmentIndex: UInt8,
         records: [ZoneModuleStatus]) {
        self.isSnapshot = isSnapshot
        self.isLastFragment = isLastFragment
        self.fragmentIndex = fragmentIndex
        self.records = records
    }
}

/// Accumulates snapshot fragment runs; passes deltas straight through.
///
/// Deliberately strict about ordering. A dropped or out-of-order fragment
/// abandons the run rather than committing a snapshot with a hole in it — an
/// inventory missing a socket reads as "that module is not installed", which is
/// exactly the wrong answer for a placement gate.
struct ZoneModuleFrameAssembler {

    /// A snapshot cannot legitimately describe more sockets than the addressing
    /// domain holds. Bounds the accumulation buffer so a hub that never sends a
    /// terminating fragment cannot grow it without limit.
    private static let maxPendingRecords = Int(ZoneModuleFrame.maxSocketID)

    private var pending: [ZoneModuleStatus] = []
    /// Deltas that arrived mid-run. A snapshot describes the hardware as of when
    /// the hub started sending it, so a change observed DURING that run is newer
    /// and must survive the commit — otherwise pulling a tile mid-snapshot
    /// resurrects it as present, and presence gates safety-critical placement
    /// checks.
    private var deferredDeltas: [ZoneModuleStatus] = []
    private var expectedFragment: UInt8 = 0
    private var accumulating = false

    /// True while a snapshot run is part-received. Exposed for tests and
    /// diagnostics; callers should not treat a part-received run as state.
    var isAccumulating: Bool { accumulating }

    /// Feed one decoded fragment.
    ///
    /// Returns a frame ready to apply, or nil when more fragments are needed
    /// (or the run was abandoned).
    mutating func accept(_ frame: ZoneModuleFrame) -> ZoneModuleFrame? {

        // A delta names only the sockets that changed and is self-contained.
        guard frame.isSnapshot else {
            guard frame.isLastFragment else { return nil }
            // Mid-run, hold it back and re-apply after the snapshot commits so
            // the older snapshot cannot overwrite the newer observation.
            if accumulating {
                deferredDeltas.append(contentsOf: frame.records)
                return nil
            }
            return frame
        }

        if frame.fragmentIndex == 0 {
            // A fresh run supersedes anything part-received — but NOT the
            // deferred deltas, which are newer than any snapshot in flight.
            pending = []
            expectedFragment = 0
            accumulating = true
        } else if !accumulating || frame.fragmentIndex != expectedFragment {
            // Gap, duplicate, or a continuation with no run in progress.
            // Drop the whole run; the hub re-sends a snapshot on the next event.
            reset()
            return nil
        }

        guard pending.count + frame.records.count <= Self.maxPendingRecords else {
            // More records than the socket domain can hold: the run is malformed.
            reset()
            return nil
        }

        pending.append(contentsOf: frame.records)
        expectedFragment = frame.fragmentIndex &+ 1

        guard frame.isLastFragment else { return nil }

        // Deltas last, so a change seen during the run wins over the snapshot's
        // older view of the same socket.
        let complete = ZoneModuleFrame(isSnapshot: true,
                                       isLastFragment: true,
                                       fragmentIndex: 0,
                                       records: pending + deferredDeltas)
        reset()
        return complete
    }

    mutating func reset() {
        pending = []
        deferredDeltas = []
        expectedFragment = 0
        accumulating = false
    }
}
