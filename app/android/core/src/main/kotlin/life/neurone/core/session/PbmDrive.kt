package life.neurone.core.session

import life.neurone.core.protocol.NPPBMChannelElement
import kotlin.math.floor

// Absolute dose → drive register. Port of app/web/src/lib/pbmDrive.ts, which owns the
// figures: change them there and here together (register rows UC-064 to UC-066,
// docs/status/unjustified-choices.md). A request the hardware cannot reach is REFUSED, never
// clamped (CLAUDE.md §3).
object PbmDrive {
    /** Peak irradiance at the scalp each channel delivers at full drive (register 255), mW/cm². */
    val PBM_FULL_SCALE_MW_CM2: Map<NPPBMChannelElement, Double> = mapOf(
        NPPBMChannelElement.LED_660 to 403.0,
        NPPBMChannelElement.LED_808 to 403.0,
        NPPBMChannelElement.LED_1064 to 28.0,
    )

    /** PROVISIONAL PLACEHOLDER (UC-065): the probe has no owning specification (OI-ART-04). */
    const val INTRANASAL_FULL_SCALE_MW_CM2 = 100.0

    /** PROVISIONAL PLACEHOLDER (UC-066, OI-AUDIOHW-01). */
    const val AUDIO_DB_AT_ZERO = 40.0
    const val AUDIO_DB_PER_PERCENT = 0.5

    private const val EPS = 1e-9

    fun irradianceToRegister(irradianceMWcm2: Double, fullScaleMWcm2: Double, channel: String): Int {
        if (!irradianceMWcm2.isFinite() || irradianceMWcm2 <= 0) {
            throw IllegalArgumentException("PBM irradiance must be positive, got $irradianceMWcm2 mW/cm².")
        }
        if (irradianceMWcm2 > fullScaleMWcm2 * (1 + EPS)) {
            throw IllegalArgumentException(
                "PBM irradiance $irradianceMWcm2 mW/cm² exceeds what $channel delivers at full " +
                    "drive ($fullScaleMWcm2 mW/cm²). The protocol is refused, not reduced to fit.",
            )
        }
        return (floor(irradianceMWcm2 / fullScaleMWcm2 * 255 + 0.5).toInt()).coerceIn(1, 255)
    }

    fun dbToVolumePercent(db: Double): Int {
        val pct = (db - AUDIO_DB_AT_ZERO) / AUDIO_DB_PER_PERCENT
        if (!db.isFinite() || pct < -EPS || pct > 100 + EPS) {
            throw IllegalArgumentException(
                "Audio level $db dB SPL is outside the range the drive can deliver " +
                    "(${AUDIO_DB_AT_ZERO.toInt()}–${(AUDIO_DB_AT_ZERO + 100 * AUDIO_DB_PER_PERCENT).toInt()} dB SPL). " +
                    "The protocol is refused, not reduced to fit.",
            )
        }
        return floor(pct + 0.5).toInt().coerceIn(0, 100)
    }
}
