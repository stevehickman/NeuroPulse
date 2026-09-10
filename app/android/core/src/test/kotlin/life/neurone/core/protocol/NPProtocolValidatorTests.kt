package life.neurone.core.protocol

import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * Port of iOS NPProtocolValidatorTests. Verifies the safety guards Android was missing:
 * hardware current ceilings (tDCS 2 mA, BES 1 mA), tDCS charge density (40 mC/cm²),
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

    // ─── tDCS charge density (OI-CHARGE-04) ──────────────────────────────────
    // Density is I(mA) × t(s) / A(cm²) for ONE electrode, where A is declared by the
    // protocol and carried to the safety MCU in the signed descriptor. Until 2026-09-09
    // this divided by TDCS_DEFAULT_ELECTRODE_AREA_CM2 × the count of every electrode in
    // the montage — a 35 cm² assumption the enforcer never saw, summed into 70 cm² for a
    // single pair against the MCU's 25. Two errors compounding to 2.8× permissive, so a
    // protocol this validator accepted was cut short mid-session by the MCU.

    @Test
    fun chargeDensityOverLimitRejected() {
        // 2.0 mA × 3600s / 35 cm² (ONE electrode) = 205.7 mC/cm² — over 40.
        val tdcs = NPTDCSParams(intensityMilliamps = 2.0, electrodePairs = listOf(listOf("Fp1", "P3")))
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.Tdcs(tdcs), durationSeconds = 60 * 60))
        assertTrue(
            result.errors.any { it.parameterKey.lowercase().contains("charge") },
            "Charge density over 40 mC/cm² must be rejected with a charge-density error.",
        )
    }

    @Test
    fun chargeDensityBorderlineValid() {
        // The ceiling is inclusive: 1.4 mA × 1000s / 35 cm² = exactly 40.0 mC/cm².
        // This was 2.0 mA at the 20-minute default and passed only under the summed-area
        // model (34.3 mC/cm² across 70 cm²); per electrode that protocol is 68.6 mC/cm².
        val tdcs = NPTDCSParams(
            intensityMilliamps = 1.4,
            electrodePairs = listOf(listOf("Fp1", "P3")),
            electrodeAreaCm2 = 35.0,
        )
        val result = hardwareOnlyValidator().validate(
            protocolWith(NPModalityParams.Tdcs(tdcs), durationSeconds = 1000))
        assertTrue(result.isValid, "A tDCS protocol exactly at the inclusive ceiling must be accepted.")
    }

    @Test
    fun chargeDensityDoesNotRelaxWithMoreElectrodePairs() {
        // 1.0 mA × 1500s / 35 cm² = 42.9 mC/cm² — over 40 whatever the montage. The
        // summed-area model got 2× more permissive with every pair added.
        val montages = listOf(
            listOf(listOf("F3", "F4")),
            listOf(listOf("F3", "F4"), listOf("P3", "P4")),
            listOf(listOf("F3", "F4"), listOf("P3", "P4"), listOf("Fz", "Pz")),
        )
        for (pairs in montages) {
            val tdcs = NPTDCSParams(intensityMilliamps = 1.0, electrodePairs = pairs, electrodeAreaCm2 = 35.0)
            val result = hardwareOnlyValidator().validate(
                protocolWith(NPModalityParams.Tdcs(tdcs), durationSeconds = 1500))
            assertTrue(
                result.errors.any { it.parameterKey == "chargeDensityUCcm2" },
                "${pairs.size} pair(s): over-ceiling density must be rejected regardless of montage size.",
            )
        }
    }

    @Test
    fun smallerDeclaredElectrodeAreaIsStricter() {
        // 1.0 mA × 1100s = 1100 mC. On 35 cm²: 31.4 mC/cm², accepted. On 25 cm²: 44.0, rejected.
        val big = NPTDCSParams(intensityMilliamps = 1.0, electrodePairs = listOf(listOf("F3", "F4")), electrodeAreaCm2 = 35.0)
        val small = NPTDCSParams(intensityMilliamps = 1.0, electrodePairs = listOf(listOf("F3", "F4")), electrodeAreaCm2 = 25.0)
        assertFalse(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.Tdcs(big), durationSeconds = 1100))
                .errors.any { it.parameterKey == "chargeDensityUCcm2" },
            "35 cm² pad at 31.4 mC/cm² must be accepted.",
        )
        assertTrue(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.Tdcs(small), durationSeconds = 1100))
                .errors.any { it.parameterKey == "chargeDensityUCcm2" },
            "25 cm² pad at 44.0 mC/cm² must be rejected.",
        )
    }

    @Test
    fun undeclaredElectrodeAreaRejected() {
        // Not a fallback: the hub refuses a zero area and the safety MCU's geometry gate
        // holds tDCS off, so accepting this hands the user a session that never stimulates.
        val tdcs = NPTDCSParams(intensityMilliamps = 1.0, electrodePairs = listOf(listOf("F3", "F4")), electrodeAreaCm2 = 0.0)
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.Tdcs(tdcs)))
        assertTrue(
            result.errors.any { it.parameterKey == "electrodeAreaCm2" },
            "A zero electrode area must be rejected as a geometry error.",
        )
        assertFalse(
            result.errors.any { it.parameterKey == "chargeDensityUCcm2" },
            "A zero area must not also report an infinite charge density — one defect, one message.",
        )
    }

    @Test
    fun unencodableElectrodeAreaRejected() {
        // Above what the uint16 milli-cm² wire field can carry.
        val tdcs = NPTDCSParams(intensityMilliamps = 1.0, electrodePairs = listOf(listOf("F3", "F4")), electrodeAreaCm2 = 70.0)
        assertTrue(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.Tdcs(tdcs)))
                .errors.any { it.parameterKey == "electrodeAreaCm2" },
            "An area beyond the uint16 milli-cm² wire field must be rejected.",
        )
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
