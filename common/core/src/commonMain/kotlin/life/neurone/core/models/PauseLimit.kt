package life.neurone.core.models

// PAUSE_LIMIT (SWR-SS-05): the longest a session may stay paused before the helmet ends it, in seconds. It is the hardware
// owner's option and not the wearer's, and it defaults to NO LIMIT. Wire: a little-endian uint32, 0 is no limit
// (firmware/hub_control/include/np_pause_limit.h). Any value is accepted by the helmet, since the owner decides what is
// sensible for the clinic, so no figure is built in here either; the screen offers whole minutes.
object PauseLimit {
    const val LENGTH = 4
    const val NO_LIMIT_SECONDS = 0L

    /** Null for any frame that is not exactly four bytes: no part of it is believed. */
    fun fromWire(data: ByteArray): Long? {
        if (data.size != LENGTH) return null
        return (data[0].toLong() and 0xFF) or ((data[1].toLong() and 0xFF) shl 8) or
            ((data[2].toLong() and 0xFF) shl 16) or ((data[3].toLong() and 0xFF) shl 24)
    }

    /** The write for [seconds]; null when it does not fit the helmet's uint32. */
    fun toWire(seconds: Long): ByteArray? {
        if (seconds < 0L || seconds > 0xFFFF_FFFFL) return null
        return ByteArray(LENGTH) { i -> ((seconds shr (8 * i)) and 0xFF).toByte() }
    }

    /**
     * Whole minutes to the seconds the helmet takes; 0 is no limit. Null for a negative number or one that would overflow
     * the helmet's uint32.
     */
    fun secondsOfMinutes(minutes: Long): Long? {
        if (minutes < 0L || minutes > 0xFFFF_FFFFL / 60L) return null
        return minutes * 60L
    }
}
