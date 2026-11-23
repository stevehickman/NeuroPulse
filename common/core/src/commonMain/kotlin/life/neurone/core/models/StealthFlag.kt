package life.neurone.core.models

// STEALTH (0x0022; NP-FW-HUB-001 §5.5a, REQ-FWHUB-86): whether the helmet's status indicators are dark, as the hub holds it
// (firmware/hub_control/src/np_status_led.c). One byte both ways; bit 0 is stealth and every other bit is zero. The hub refuses a
// write with any other bit set, so the app never makes one, and a read with another bit set is not believed.
//
// Stealth darkens the idle and session indicators only. A fault always shows red whatever this says (CLAUDE.md §4.7), so nothing
// in the app may present stealth as hiding a fault.
//
// UHDR-class (docs/reference/data-architecture-detail.md §5.1, "Stealth mode preference"): that a wearer hides the indicators is a
// fact about the person. It travels only on an encrypted link (CLAUDE.md §23) and is never written to SHDR or analytics.

object StealthFlag {
    const val LENGTH = 1
    private const val ON = 0x01

    /** The hub's value, or null when the frame is not one byte or has a bit other than bit 0 set. */
    fun fromWire(data: ByteArray): Boolean? {
        if (data.size != LENGTH) return null
        val b = data[0].toInt() and 0xFF
        if (b and ON.inv() and 0xFF != 0) return null
        return b == ON
    }

    fun toWire(on: Boolean): ByteArray = byteArrayOf(if (on) ON.toByte() else 0)

    /**
     * What the app asks the helmet to show (SWR-IND-01, SWR-IND-02). The person's own choice, unless the warranty owner has
     * forbidden stealth on this helmet, when it is off whatever the person chose. Null when no one is named and the owner has not
     * forbidden it: the app then writes nothing and the helmet keeps what it holds.
     */
    fun desired(profileChoice: Boolean?, ownerForbids: Boolean): Boolean? = when {
        ownerForbids -> false
        else -> profileChoice
    }
}
