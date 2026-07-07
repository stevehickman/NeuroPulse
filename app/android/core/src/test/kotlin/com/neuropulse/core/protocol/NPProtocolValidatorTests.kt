package com.neuropulse.core.protocol

import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * Port of iOS NPProtocolValidatorTests. Verifies the safety guards Android was missing:
 * hardware current ceilings (tDCS 2 mA, BES 1 mA), tDCS charge density (40 µC/cm²),
 * zero-duration rejection, PBM session-dose limit, and configured dosage limits.
 */
class NPProtocolValidatorTests {

    /** Validator with no configured dosage limits — only hardware ceilings apply. */
    private fun hardwareOnlyValidator() = NPProtocolValidator(NPLimitsSet(name = "unlimited"))

    private fun protocolWith(
        params: NPModalityParams,
        interval: NPIntervalConfig = NPIntervalConfig.CONTINUOUS,
        durationSeconds: Int = 20 * 60,
    ) = NPProtocolDefinition(
        name = "Test Protocol",
        timingMode = NPTimingMode.Duration(durationSeconds),
        modalities = listOf(NPProtocolModality(params = params, interval = interval, enabled = true)),
    )

    @Test
    fun validProtocolAccepted() {
        val pbm = NPPBMTranscranialParams(intensityPercent = 75.0, frequencyHz = 20.0, dutyCyclePercent = 25)
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.PbmTranscranial(pbm)))
        assertTrue(result.isValid, "A minimal in-bounds PBM protocol must validate as success.")
        assertTrue(result.errors.isEmpty(), "Valid protocol must produce no errors.")
    }

    @Test
    fun currentOverLimitRejected_tDCS() {
        val tdcs = NPTDCSParams(intensityMilliamps = 2.5, electrodePairs = listOf(listOf("Fp1", "P3")))
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.Tdcs(tdcs)))
        assertFalse(result.isValid, "tDCS current above 2 mA must be rejected.")
        assertTrue(
            result.errors.any { it.modality == NPModalityType.TDCS && it.parameterKey == "intensityMilliamps" },
            "Rejection must cite the tDCS intensity parameter.",
        )
    }

    @Test
    fun currentOverLimitRejected_BES() {
        val bes = NPBESTacsParams(frequencyHz = 20.0, intensityMilliamps = 1.5)
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.BesTacs(bes)))
        assertFalse(result.isValid, "BES current above 1 mA must be rejected.")
        assertTrue(
            result.errors.any { it.modality == NPModalityType.BES_TACS && it.parameterKey == "intensityMilliamps" },
            "Rejection must cite the BES intensity parameter.",
        )
    }

    @Test
    fun chargeDensityOverLimitRejected() {
        // 2.0 mA × 3600s / (35 cm² × 2 electrodes) = 102.9 µC/cm² — over 40.
        val tdcs = NPTDCSParams(intensityMilliamps = 2.0, electrodePairs = listOf(listOf("Fp1", "P3")))
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.Tdcs(tdcs), durationSeconds = 60 * 60))
        assertTrue(
            result.errors.any { it.parameterKey.lowercase().contains("charge") },
            "Charge density over 40 µC/cm² must be rejected with a charge-density error.",
        )
    }

    @Test
    fun chargeDensityBorderlineValid() {
        // 20-minute default: 2.0 mA × 1200s / 70 cm² = 34.3 µC/cm² — under 40, accepted.
        val tdcs = NPTDCSParams(intensityMilliamps = 2.0, electrodePairs = listOf(listOf("Fp1", "P3")))
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.Tdcs(tdcs)))
        assertTrue(result.isValid, "A tDCS protocol under the charge-density ceiling must be accepted.")
    }

    @Test
    fun zeroDurationRejected() {
        val pbm = NPPBMTranscranialParams(intensityPercent = 75.0, frequencyHz = 20.0, dutyCyclePercent = 25)
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.PbmTranscranial(pbm), durationSeconds = 0))
        assertFalse(result.isValid, "A protocol with 0-second duration must be rejected.")
        assertTrue(result.errors.any { it.parameterKey == "duration" }, "Zero-duration rejection must cite the duration parameter.")
    }

    @Test
    fun doseOverLimitRejected() {
        // 100% CW intensity = 200 mW/cm² × 3600s / 1000 = 720 J/cm² — over the 10 J/cm² limit.
        val limits = NPLimitsSet(name = "Dose-capped", level = NPLimitsSet.LimitLevel.GLOBAL,
            pbmTranscranial = NPPBMTranscranialLimits(maxSessionDoseJCm2 = 10.0))
        val validator = NPProtocolValidator(limits)
        val pbm = NPPBMTranscranialParams(intensityPercent = 100.0, frequencyHz = 0.0, dutyCyclePercent = 25)
        val result = validator.validate(protocolWith(NPModalityParams.PbmTranscranial(pbm), durationSeconds = 60 * 60))
        assertTrue(
            result.errors.any { it.modality == NPModalityType.PBM_TRANSCRANIAL && it.parameterKey.lowercase().contains("dose") },
            "PBM dose exceeding the configured J/cm² limit must be rejected.",
        )
    }

    @Test
    fun configuredIntensityLimitRejected() {
        val limits = NPLimitsSet(name = "Capped", level = NPLimitsSet.LimitLevel.GLOBAL,
            pbmTranscranial = NPPBMTranscranialLimits(maxIntensityPercent = 50.0))
        val validator = NPProtocolValidator(limits)
        val pbm = NPPBMTranscranialParams(intensityPercent = 80.0, frequencyHz = 20.0, dutyCyclePercent = 25)
        val result = validator.validate(protocolWith(NPModalityParams.PbmTranscranial(pbm)))
        assertFalse(result.isValid, "Intensity above the configured dosage limit must be rejected.")
        assertTrue(result.errors.any { it.parameterKey == "intensityPercent" }, "Rejection must cite the intensity parameter.")
    }

    @Test
    fun emptyModalitiesRejected() {
        val def = NPProtocolDefinition(name = "Empty", modalities = emptyList())
        val result = hardwareOnlyValidator().validate(def)
        assertFalse(result.isValid, "A protocol with no enabled modalities must be rejected.")
        assertTrue(result.errors.any { it.parameterKey == "modalities" })
    }

    @Test
    fun dutyCycleOverHardwareCeilingRejected() {
        val pbm = NPPBMTranscranialParams(intensityPercent = 75.0, frequencyHz = 20.0, dutyCyclePercent = 40)
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.PbmTranscranial(pbm)))
        assertFalse(result.isValid, "PBM duty cycle above 25% must be rejected.")
        assertTrue(result.errors.any { it.parameterKey == "dutyCyclePercent" })
    }
}
