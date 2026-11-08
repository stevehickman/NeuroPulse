package life.neurone.core.models

// PPX_SENSITIVITY (REQ-FWHUB-68): how sensitive the photoparoxysmal detector is, as the helmet holds it
// (firmware/hub_control/include/np_ppx_config.h). Level 0 is the validated setting and there is no level below it: a clinician
// can make the detector more sensitive and never less than it was validated. Read: two bytes, the level and the highest level the
// helmet offers, so the app does not hard-code the range. Write: one byte, the level. A frame that is not exactly that, or a level
// above the highest, is not believed.

data class PpxSensitivity(
    /** 0 is the validated setting; each level above it halts on smaller signals (more sessions stopped, never fewer for real ones). */
    val level: Int,
    /** The highest level the helmet offers. */
    val maxLevel: Int,
) {
    val isStandard: Boolean get() = level == 0

    companion object {
        const val READ_LENGTH = 2
        const val WRITE_LENGTH = 1

        fun fromWire(data: ByteArray): PpxSensitivity? {
            if (data.size != READ_LENGTH) return null
            val level = data[0].toInt() and 0xFF
            val max = data[1].toInt() and 0xFF
            if (level > max) return null
            return PpxSensitivity(level, max)
        }

        /** The write for [level]; null for a negative level, which the helmet has none of. */
        fun toWire(level: Int): ByteArray? {
            if (level < 0 || level > 0xFF) return null
            return byteArrayOf(level.toByte())
        }
    }
}
