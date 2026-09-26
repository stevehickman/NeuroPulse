package life.neurone.core.models

// Port of iOS ZoneModuleFrame.swift (app/ios/NeurOne/BLE/ZoneModuleFrame.swift).
//
// Decoders and reassembler for the two zone-module characteristics. The wire contract lives in
// firmware/zone_announce/include/np_zone_notify.h and is pinned by
// firmware/zone_announce/tests/np_zone_notify_tests.c. The constants below mirror it; if one
// moves, firmware, iOS and Android move in the same commit.
//
// ── Two shapes over one header ───────────────────────────────────────────────
//
// SOCKET_MAP frames carry 8-byte descriptor records and are read once at link.
// ZONE_MODULE_STATUS frames carry 3-byte status records and arrive on every change. Both use
// the same 4-byte header; byte 1's MAP bit says which shape the body holds, so a decoder checks
// rather than infers — reading one as the other would misparse silently instead of failing.
//
// ── Why frames fragment ──────────────────────────────────────────────────────
//
// An ATT notification is only guaranteed to carry 20 bytes, so a full map or snapshot arrives
// as an ordered fragment run, the last one flagged. The assemblers emit a run only when the
// final fragment lands — a partial sequence is never handed on as a complete inventory.

/** Shared header constants and parsing for both frame kinds. */
object ZoneFrameFormat {

    /**
     * Mirrors `NP_ZN_FORMAT_VERSION`. Version 0 is the retired 5-byte one-byte-per-slot payload,
     * which had no version byte at all — so a hub still speaking it decodes as 0 and is rejected
     * rather than misread.
     */
    const val VERSION = 0x02
    const val HEADER_BYTES = 4
    const val STATUS_RECORD_BYTES = 3
    const val MAP_RECORD_BYTES = 8

    /** Mirrors `NP_ZN_MAX_SOCKET_ID` — the full 7-bit addressing domain, 1-based. */
    const val MAX_SOCKET_ID = 128

    const val FLAG_SNAPSHOT = 0x01
    const val FLAG_LAST = 0x02
    const val FLAG_MAP = 0x04

    const val REC_PRESENT = 0x01
    const val REC_FAULT = 0x02
    const val MAP_WIRED = 0x01

    data class Header(
        val isSnapshot: Boolean,
        val isLastFragment: Boolean,
        val fragmentIndex: Int,
        val recordCount: Int,
    )

    /**
     * Validate the header and confirm the body holds exactly the records it claims, at the
     * record size implied by [expectMap]. Returns null for anything not exactly well-formed —
     * presence gates safety-critical placement checks, so a malformed frame is discarded rather
     * than partially believed.
     */
    fun parseHeader(data: ByteArray, expectMap: Boolean): Header? {
        if (data.size < HEADER_BYTES) return null
        if (data.u8(0) != VERSION) return null
        val flags = data.u8(1)
        if (((flags and FLAG_MAP) != 0) != expectMap) return null
        val count = data.u8(3)
        val recordBytes = if (expectMap) MAP_RECORD_BYTES else STATUS_RECORD_BYTES
        if (data.size < HEADER_BYTES + count * recordBytes) return null
        return Header(
            isSnapshot = (flags and FLAG_SNAPSHOT) != 0,
            isLastFragment = (flags and FLAG_LAST) != 0,
            fragmentIndex = data.u8(2),
            recordCount = count,
        )
    }

    /** 0 is never emitted; ids above the domain would alias onto a real socket — wrong-site targeting. */
    fun isValidSocketId(id: Int): Boolean = id in 1..MAX_SOCKET_ID

    internal fun ByteArray.u8(offset: Int): Int = this[offset].toInt() and 0xFF

    internal fun ByteArray.leInt16(offset: Int): Int =
        ((u8(offset) or (u8(offset + 1) shl 8)).toShort()).toInt()
}

/** One SOCKET_MAP fragment (static, read once at link). */
data class SocketMapFrame(
    val isLastFragment: Boolean,
    val fragmentIndex: Int,
    val descriptors: List<SocketDescriptor>,
) {
    companion object {
        fun fromWire(data: ByteArray): SocketMapFrame? = with(ZoneFrameFormat) {
            val h = parseHeader(data, expectMap = true) ?: return null
            val decoded = ArrayList<SocketDescriptor>(h.recordCount)
            for (i in 0 until h.recordCount) {
                val r = HEADER_BYTES + i * MAP_RECORD_BYTES
                val socketId = data.u8(r)
                if (!isValidSocketId(socketId)) return null
                // Aircraft body axes, int16 little-endian: +x forward, +y right, +z down.
                decoded += SocketDescriptor(
                    socketId = socketId,
                    position = SocketPosition(
                        forwardMm = data.leInt16(r + 2),
                        rightMm = data.leInt16(r + 4),
                        downMm = data.leInt16(r + 6),
                    ),
                    isWiredInShell = (data.u8(r + 1) and MAP_WIRED) != 0,
                )
            }
            SocketMapFrame(h.isLastFragment, h.fragmentIndex, decoded)
        }
    }
}

/** One ZONE_MODULE_STATUS fragment (dynamic, one per change). */
data class ZoneModuleFrame(
    val isSnapshot: Boolean,
    val isLastFragment: Boolean,
    val fragmentIndex: Int,
    val records: List<ZoneModuleStatus>,
) {
    companion object {
        fun fromWire(data: ByteArray): ZoneModuleFrame? = with(ZoneFrameFormat) {
            val h = parseHeader(data, expectMap = false) ?: return null
            val decoded = ArrayList<ZoneModuleStatus>(h.recordCount)
            for (i in 0 until h.recordCount) {
                val r = HEADER_BYTES + i * STATUS_RECORD_BYTES
                val socketId = data.u8(r)
                if (!isValidSocketId(socketId)) return null
                val recFlags = data.u8(r + 2)
                val fault = (recFlags and REC_FAULT) != 0
                // Firmware clears presence on fault; re-assert it here so a hub that ever sends
                // both cannot produce a "present" faulted module.
                val present = (recFlags and REC_PRESENT) != 0 && !fault
                decoded += ZoneModuleStatus(
                    socketId = socketId,
                    moduleType = ZoneModuleType.from(data.u8(r + 1)),
                    isPresent = present,
                    hasFault = fault,
                )
            }
            ZoneModuleFrame(h.isSnapshot, h.isLastFragment, h.fragmentIndex, decoded)
        }
    }
}

/**
 * Accumulates ordered fragment runs. Deliberately strict about ordering: a dropped or
 * out-of-order fragment abandons the run rather than committing with a hole in it — an
 * inventory missing a socket reads as "that module is not installed", which is exactly the
 * wrong answer for a placement gate.
 */
class FragmentRunAssembler<T> {

    /** Bounds the buffer so a hub that never terminates a run cannot grow it without limit. */
    private val maxRecords = ZoneFrameFormat.MAX_SOCKET_ID

    private var pending = mutableListOf<T>()
    private var expectedFragment = 0

    var isAccumulating: Boolean = false
        private set

    /** Feed one fragment. Returns the completed run, or null when more are needed (or the run was abandoned). */
    fun accept(fragmentIndex: Int, isLast: Boolean, elements: List<T>): List<T>? {
        if (fragmentIndex == 0) {
            pending = mutableListOf()
            expectedFragment = 0
            isAccumulating = true
        } else if (!isAccumulating || fragmentIndex != expectedFragment) {
            reset()
            return null
        }
        if (pending.size + elements.size > maxRecords) {
            reset()
            return null
        }
        pending += elements
        expectedFragment = (fragmentIndex + 1) and 0xFF
        if (!isLast) return null
        val complete = pending.toList()
        reset()
        return complete
    }

    fun reset() {
        pending = mutableListOf()
        expectedFragment = 0
        isAccumulating = false
    }
}

/** Status-frame assembler: snapshots accumulate, deltas pass through. */
class ZoneModuleFrameAssembler {

    private val run = FragmentRunAssembler<ZoneModuleStatus>()

    /**
     * Deltas that arrived mid-run. A snapshot describes the hardware as of when the hub started
     * sending it, so a change observed DURING that run is newer and must survive the commit —
     * otherwise pulling a tile mid-snapshot resurrects it as present.
     */
    private val deferredDeltas = mutableListOf<ZoneModuleStatus>()

    val isAccumulating: Boolean get() = run.isAccumulating

    fun accept(frame: ZoneModuleFrame): ZoneModuleFrame? {
        if (!frame.isSnapshot) {
            if (!frame.isLastFragment) return null
            if (run.isAccumulating) {
                deferredDeltas += frame.records
                return null
            }
            return frame
        }
        val records = run.accept(frame.fragmentIndex, frame.isLastFragment, frame.records)
        if (records == null) {
            if (!run.isAccumulating) deferredDeltas.clear()   // run abandoned
            return null
        }
        // Deltas last, so a change seen during the run wins over the snapshot's older view.
        val merged = records + deferredDeltas
        deferredDeltas.clear()
        return ZoneModuleFrame(isSnapshot = true, isLastFragment = true, fragmentIndex = 0, records = merged)
    }

    fun reset() {
        run.reset()
        deferredDeltas.clear()
    }
}

/** Socket-map assembler. A map is always a complete snapshot, so there are no deltas to reconcile. */
class SocketMapFrameAssembler {

    private val run = FragmentRunAssembler<SocketDescriptor>()

    val isAccumulating: Boolean get() = run.isAccumulating

    fun accept(frame: SocketMapFrame): SocketMapFrame? {
        val descriptors = run.accept(frame.fragmentIndex, frame.isLastFragment, frame.descriptors)
            ?: return null
        return SocketMapFrame(isLastFragment = true, fragmentIndex = 0, descriptors = descriptors)
    }

    fun reset() = run.reset()
}
