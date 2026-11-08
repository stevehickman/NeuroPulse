package life.neurone.core.models

// CARDIAC_SENSITIVITY (REQ-FWHUB-74, REQ-CVNS-12): how sensitive the cervical VNS cardiac cutoff is, as the helmet holds it
// (firmware/hub_control/include/np_cardiac_sens.h). Level 0 is the cutoff as validated (a change of more than 15 BPM) and there is
// no level below it: the owner can make the cutoff fire on a smaller change, never a larger one. Read: two bytes, the level and the
// highest level the helmet offers, so the app does not hard-code the range. Write: one byte, the level. A frame that is not exactly
// that, or a level above the highest, is not believed.

data class CardiacSensitivity(
    /** 0 is the validated cutoff; each level above it fires on a smaller heart-rate change (more cutoffs, fewer missed). */
    val level: Int,
    /** The highest level the helmet offers. */
    val maxLevel: Int,
) {
    val isStandard: Boolean get() = level == 0

    companion object {
        const val READ_LENGTH = 2
        const val WRITE_LENGTH = 1

        fun fromWire(data: ByteArray): CardiacSensitivity? {
            if (data.size != READ_LENGTH) return null
            val level = data[0].toInt() and 0xFF
            val max = data[1].toInt() and 0xFF
            if (level > max) return null
            return CardiacSensitivity(level, max)
        }

        /** The write for [level]; null for a negative level, which the helmet has none of. */
        fun toWire(level: Int): ByteArray? {
            if (level < 0 || level > 0xFF) return null
            return byteArrayOf(level.toByte())
        }
    }
}
