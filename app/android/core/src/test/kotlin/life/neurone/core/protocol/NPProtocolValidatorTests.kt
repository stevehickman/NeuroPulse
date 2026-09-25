package life.neurone.core.protocol

import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * Port of iOS NPProtocolValidatorTests. Verifies the safety guards Android was missing:
 * hardware current ceilings (tDCS 2 mA, BES 1 mA), tDCS DC per-session charge density
 * (150 mC/cm², OI-CHARGE-05), per-phase charge density for the charge-balanced modalities
 * (40 µC/cm², OI-CHARGE-05 b), zero-duration rejection, PBM session-dose limit, and
 * configured dosage limits.
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
        val pbm = NPPBMTranscranialParams(irradianceMWcm2 = 302.0, frequencyHz = 20.0, dutyCyclePercent = 25)
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
        // 2.0 mA × 3600s / 35 cm² (ONE electrode) = 205.7 mC/cm² — over 150.
        val tdcs = NPTDCSParams(intensityMilliamps = 2.0, electrodePairs = listOf(listOf("Fp1", "P3")))
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.Tdcs(tdcs), durationSeconds = 60 * 60))
        assertTrue(
            result.errors.any { it.parameterKey.lowercase().contains("charge") },
            "Charge density over 150 mC/cm² must be rejected with a charge-density error.",
        )
    }

    @Test
    fun chargeDensityBorderlineValid() {
        // Just under the ceiling: 1.4 mA × 3749s / 35 cm² = 149.96 mC/cm².
        // This was 2.0 mA at the 20-minute default and passed only under the summed-area
        // model (34.3 mC/cm² across 70 cm²); per electrode that protocol is 68.6 mC/cm².
        // OI-CHARGE-05 moved the ceiling (40 → 150 mC/cm², now with a derivation behind
        // it) and made the comparison inclusive, because the safety MCU trips at >=.
        val tdcs = NPTDCSParams(
            intensityMilliamps = 1.4,
            electrodePairs = listOf(listOf("Fp1", "P3")),
            electrodeAreaCm2 = 35.0,
        )
        val result = hardwareOnlyValidator().validate(
            protocolWith(NPModalityParams.Tdcs(tdcs), durationSeconds = 3749))
        assertTrue(result.isValid, "A tDCS protocol just under the ceiling must be accepted.")
    }

    /** The enforcer trips AT the ceiling, so the pre-flight must too (OI-CHARGE-05). */
    @Test
    fun chargeDensityAtCeilingRejected() {
        // 1.4 mA × 3750s / 35 cm² = exactly 150.0 mC/cm².
        val tdcs = NPTDCSParams(
            intensityMilliamps = 1.4,
            electrodePairs = listOf(listOf("Fp1", "P3")),
            electrodeAreaCm2 = 35.0,
        )
        assertTrue(
            hardwareOnlyValidator().validate(
                protocolWith(NPModalityParams.Tdcs(tdcs), durationSeconds = 3750))
                .errors.any { it.parameterKey == "chargeDensityMCcm2" },
            "Exactly at the ceiling must be rejected — np_charge_monitor.c trips at >=.",
        )
    }

    /**
     * The clinical consequence OI-CHARGE-05 (d) weighed, pinned so a later change to the
     * ceiling cannot quietly take it away again. At the retired 40 the routine protocol
     * was unavailable and 13 of the 14 shipped predefined tDCS protocols exceeded it.
     */
    @Test
    fun routineTwoMilliampTwentyMinuteProtocolAccepted() {
        // 2 mA × 1200s / 35 cm² = 68.6 mC/cm² — the single most common protocol in the
        // literature and the median of docs/tdcs_database_full.csv.
        val tdcs = NPTDCSParams(
            intensityMilliamps = 2.0,
            electrodePairs = listOf(listOf("F3", "F4")),
            electrodeAreaCm2 = 35.0,
        )
        assertFalse(
            hardwareOnlyValidator().validate(
                protocolWith(NPModalityParams.Tdcs(tdcs), durationSeconds = 1200))
                .errors.any { it.parameterKey == "chargeDensityMCcm2" },
            "2 mA × 20 min on a 35 cm² pad must be available.",
        )
    }

    @Test
    fun chargeDensityDoesNotRelaxWithMoreElectrodePairs() {
        // 1.0 mA × 6000s / 35 cm² = 171.4 mC/cm² — over 150 whatever the montage. The
        // summed-area model divided the pair's two pads (70 cm²), got 85.7, and accepted it.
        val montages = listOf(
            listOf(listOf("F3", "F4")),
            listOf(listOf("F3", "F4"), listOf("P3", "P4")),
            listOf(listOf("F3", "F4"), listOf("P3", "P4"), listOf("Fz", "Pz")),
        )
        for (pairs in montages) {
            val tdcs = NPTDCSParams(intensityMilliamps = 1.0, electrodePairs = pairs, electrodeAreaCm2 = 35.0)
            val result = hardwareOnlyValidator().validate(
                protocolWith(NPModalityParams.Tdcs(tdcs), durationSeconds = 6000))
            assertTrue(
                result.errors.any { it.parameterKey == "chargeDensityMCcm2" },
                "${pairs.size} pair(s): over-ceiling density must be rejected regardless of montage size.",
            )
        }
    }

    @Test
    fun smallerDeclaredElectrodeAreaIsStricter() {
        // 1.0 mA × 4000s = 4000 mC. On 35 cm²: 114.3 mC/cm², accepted. On 25 cm²: 160.0, rejected.
        val big = NPTDCSParams(intensityMilliamps = 1.0, electrodePairs = listOf(listOf("F3", "F4")), electrodeAreaCm2 = 35.0)
        val small = NPTDCSParams(intensityMilliamps = 1.0, electrodePairs = listOf(listOf("F3", "F4")), electrodeAreaCm2 = 25.0)
        assertFalse(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.Tdcs(big), durationSeconds = 4000))
                .errors.any { it.parameterKey == "chargeDensityMCcm2" },
            "35 cm² pad at 114.3 mC/cm² must be accepted.",
        )
        assertTrue(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.Tdcs(small), durationSeconds = 4000))
                .errors.any { it.parameterKey == "chargeDensityMCcm2" },
            "25 cm² pad at 160.0 mC/cm² must be rejected.",
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
            result.errors.any { it.parameterKey == "chargeDensityMCcm2" },
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

    // ── OI-CHARGE-05 (b): per-phase charge density for charge-balanced modalities ──

    /**
     * The session-cumulative DC dose must NEVER be applied to a charge-balanced modality:
     * net delivered charge is ~zero by construction, so integrating |I| over a session
     * measures nothing physical. The safety MCU did exactly that until 2026-09-15, which
     * would have tripped every one of these in 0.4-1.0 s.
     */
    @Test
    fun chargeBalancedModalityHasNoSessionDose() {
        val bes = NPBESTacsParams(
            frequencyHz = 10.0, intensityMilliamps = 1.0,
            waveform = NPBESTacsParams.Waveform.SINUSOIDAL,
        )
        for (seconds in listOf(3600, 36000)) {
            assertFalse(
                hardwareOnlyValidator().validate(
                    protocolWith(NPModalityParams.BesTacs(bes), durationSeconds = seconds))
                    .errors.any { it.parameterKey == "chargeDensityMCcm2" },
                "${seconds}s of tACS must raise no session-dose error — the waveform is charge balanced.",
            )
        }
    }

    /** Each pulsed modality passes at its rated worst case. */
    @Test
    fun pulsedModalitiesPassAtRatedMaximum() {
        // VNS: 2 mA × 250 µs = 0.5 µC on a 0.5 cm² clip pad = 1.0 µC/cm², 2.5% of ceiling.
        val vns = NPVNSHRVParams(frequencyHz = 25.0, intensityMilliamps = 2.0)
        assertFalse(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.VnsHRV(vns)))
                .errors.any { it.parameterKey == "phaseChargeDensityUCcm2" },
            "VNS at its rated 2 mA / 250 µs must pass.",
        )

        // Cervical VNS: 0.5 µC on a 2 cm² collar pad = 0.25 µC/cm².
        val cvns = NPCervicalVnsParams(frequencyHz = 25.0, intensityMilliamps = 2.0)
        assertFalse(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.CervicalVns(cvns)))
                .errors.any { it.parameterKey == "phaseChargeDensityUCcm2" },
            "Cervical VNS at its rated 2 mA must pass.",
        )

        // tACS at the band bottom, where phase charge is LARGEST: 1 mA at 0.5 Hz is a 1 s
        // half-period. Sinusoidal, so 2/π × 1000 µC = 636.6 µC ÷ 25 cm² = 25.5 µC/cm².
        val tacs = NPBESTacsParams(
            frequencyHz = 0.5, intensityMilliamps = 1.0,
            waveform = NPBESTacsParams.Waveform.SINUSOIDAL,
        )
        assertFalse(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.BesTacs(tacs)))
                .errors.any { it.parameterKey == "phaseChargeDensityUCcm2" },
            "1 mA sinusoidal tACS at the 0.5 Hz band bottom must pass.",
        )
    }

    /**
     * The 2/π factor is load-bearing, not a refinement. Identical amplitude and phase
     * duration; only the waveform differs. A square wave delivers the full I × T
     * (1000 µC ÷ 25 cm² = 40.0 µC/cm², exactly the ceiling, refused); the sinusoid
     * delivers 2/π of it and passes. The safety MCU applies the same factor.
     */
    @Test
    fun sineFactorAppliesOnlyToSinusoids() {
        val square = NPBESTacsParams(
            frequencyHz = 0.5, intensityMilliamps = 1.0,
            waveform = NPBESTacsParams.Waveform.SQUARE,
        )
        assertTrue(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.BesTacs(square)))
                .errors.any { it.parameterKey == "phaseChargeDensityUCcm2" },
            "A 1 mA square wave at 0.5 Hz is 40.0 µC/cm² per phase and must be rejected.",
        )
        val sine = NPBESTacsParams(
            frequencyHz = 0.5, intensityMilliamps = 1.0,
            waveform = NPBESTacsParams.Waveform.SINUSOIDAL,
        )
        assertFalse(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.BesTacs(sine)))
                .errors.any { it.parameterKey == "phaseChargeDensityUCcm2" },
            "The same settings as a sinusoid are 25.5 µC/cm² and must be accepted.",
        )
    }

    /** Lower frequency means a longer phase means more charge per phase. */
    @Test
    fun phaseChargeGetsStricterAsFrequencyFalls() {
        val slow = NPBESTacsParams(
            frequencyHz = 0.4, intensityMilliamps = 1.0,
            waveform = NPBESTacsParams.Waveform.SQUARE,
        )
        assertTrue(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.BesTacs(slow)))
                .errors.any { it.parameterKey == "phaseChargeDensityUCcm2" },
            "0.4 Hz square at 1 mA is a 1.25 s phase = 50 µC/cm² and must be rejected.",
        )
        val fast = NPBESTacsParams(
            frequencyHz = 1.0, intensityMilliamps = 1.0,
            waveform = NPBESTacsParams.Waveform.SQUARE,
        )
        assertFalse(
            hardwareOnlyValidator().validate(protocolWith(NPModalityParams.BesTacs(fast)))
                .errors.any { it.parameterKey == "phaseChargeDensityUCcm2" },
            "1 Hz halves the phase and must be accepted.",
        )
    }

    @Test
    fun zeroDurationRejected() {
        val pbm = NPPBMTranscranialParams(irradianceMWcm2 = 302.0, frequencyHz = 20.0, dutyCyclePercent = 25)
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.PbmTranscranial(pbm), durationSeconds = 0))
        assertFalse(result.isValid, "A protocol with 0-second duration must be rejected.")
        assertTrue(result.errors.any { it.parameterKey == "duration" }, "Zero-duration rejection must cite the duration parameter.")
    }

    @Test
    fun doseOverLimitRejected() {
        // 200 mW/cm² CW (continuous) × 3600 s / 1000 = 720 J/cm² — over the 10 J/cm² limit.
        val limits = NPLimitsSet(name = "Dose-capped", level = NPLimitsSet.LimitLevel.GLOBAL,
            pbmTranscranial = NPPBMTranscranialLimits(maxSessionDoseJCm2 = 10.0))
        val validator = NPProtocolValidator(limits)
        val pbm = NPPBMTranscranialParams(irradianceMWcm2 = 200.0, frequencyHz = 0.0, dutyCyclePercent = 100)
        val result = validator.validate(protocolWith(NPModalityParams.PbmTranscranial(pbm), durationSeconds = 60 * 60))
        assertTrue(
            result.errors.any { it.modality == NPModalityType.PBM_TRANSCRANIAL && it.parameterKey.lowercase().contains("dose") },
            "PBM dose exceeding the configured J/cm² limit must be rejected.",
        )
    }

    @Test
    fun configuredIntensityLimitRejected() {
        val limits = NPLimitsSet(name = "Capped", level = NPLimitsSet.LimitLevel.GLOBAL,
            pbmTranscranial = NPPBMTranscranialLimits(maxIrradianceMWcm2 = 200.0))
        val validator = NPProtocolValidator(limits)
        val pbm = NPPBMTranscranialParams(irradianceMWcm2 = 322.0, frequencyHz = 20.0, dutyCyclePercent = 25)
        val result = validator.validate(protocolWith(NPModalityParams.PbmTranscranial(pbm)))
        assertFalse(result.isValid, "Irradiance above the configured dosage limit must be rejected.")
        assertTrue(result.errors.any { it.parameterKey == "irradianceMWcm2" }, "Rejection must cite the irradiance parameter.")
    }

    @Test
    fun emptyModalitiesRejected() {
        val def = NPProtocolDefinition(name = "Empty", modalities = emptyList())
        val result = hardwareOnlyValidator().validate(def)
        assertFalse(result.isValid, "A protocol with no enabled modalities must be rejected.")
        assertTrue(result.errors.any { it.parameterKey == "modalities" })
    }

    // OI-HEXTILE-25: R-4 is two ceilings on the on-state irradiance.
    @Test
    fun pulsedIrradianceOverPeakCeilingRejected() {
        val ok = NPPBMTranscranialParams(irradianceMWcm2 = 400.0, frequencyHz = 40.0, dutyCyclePercent = 25)
        assertFalse(hardwareOnlyValidator().validate(protocolWith(NPModalityParams.PbmTranscranial(ok)))
            .errors.any { it.parameterKey == "irradianceMWcm2" }, "400 mW/cm² pulsed is at the peak ceiling.")
        val over = ok.copy(irradianceMWcm2 = 401.0)
        assertTrue(hardwareOnlyValidator().validate(protocolWith(NPModalityParams.PbmTranscranial(over)))
            .errors.any { it.parameterKey == "irradianceMWcm2" }, "401 mW/cm² pulsed must be rejected.")
    }

    @Test
    fun cwIrradianceOverContinuousCeilingRejectedAndDutyIgnored() {
        val over = NPPBMTranscranialParams(irradianceMWcm2 = 322.0, frequencyHz = 0.0, dutyCyclePercent = 100)
        val r = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.PbmTranscranial(over)))
        assertTrue(r.errors.any { it.parameterKey == "irradianceMWcm2" }, "322 mW/cm² CW exceeds 200.")
        assertFalse(r.errors.any { it.parameterKey == "dutyCyclePercent" }, "CW is continuous: 100 % is not a duty error.")
    }

    @Test
    fun dutyCycleOverHardwareCeilingRejected() {
        val pbm = NPPBMTranscranialParams(irradianceMWcm2 = 302.0, frequencyHz = 20.0, dutyCyclePercent = 40)
        val result = hardwareOnlyValidator().validate(protocolWith(NPModalityParams.PbmTranscranial(pbm)))
        assertFalse(result.isValid, "PBM duty cycle above 25% must be rejected.")
        assertTrue(result.errors.any { it.parameterKey == "dutyCyclePercent" })
    }
}
