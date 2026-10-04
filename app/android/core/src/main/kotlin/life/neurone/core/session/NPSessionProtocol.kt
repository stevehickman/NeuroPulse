package life.neurone.core.session

import life.neurone.core.protocol.NPEEGNeurofeedbackParams
import life.neurone.core.protocol.NPModalityParams
import life.neurone.core.protocol.NPPBMChannelElement
import life.neurone.core.protocol.NPPBMTranscranialParams
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPTimingMode
import life.neurone.core.protocol.NPVNSHRVParams
import life.neurone.core.protocol.NPPbmChannelResolution
import life.neurone.core.protocol.NPWavelengthRulesEngine
import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.security.MessageDigest
import java.util.UUID

// Port of iOS NPSessionProtocol + NPSessionProtocol+FromDefinition + SessionProtocol signing.
// The hub wire descriptor for a single session: the rich NPProtocolDefinition (authoring
// model) is compiled down to this T1 wire form, serialized to JSON, and signed before upload.
//
// WIRE-FORMAT CONTRACT (OI-AND-WIRE-01): this canonical JSON must be frozen and shared with
// the hub firmware and iOS before any hub parses it. Android uses kotlinx polymorphic JSON
// with an explicit "type" discriminator (below); iOS currently uses Swift Codable's default
// enum encoding. The two must converge on ONE schema at hub-firmware time — flagged, not yet
// reconciled (the hub UUIDs/firmware are still placeholders).

@Serializable
data class NPSessionProtocol(
    val id: String = UUID.randomUUID().toString(),
    val schemaVersion: Int = 1,
    val name: String,
    val modalities: List<ModalityConfig>,
    val totalDurationSeconds: Int,
    val mode: OperatingMode = OperatingMode.MODE2_PROGRAMMING,
) {
    @Serializable
    enum class OperatingMode(val rawValue: Int) {
        @SerialName("mode1_connected") MODE1_CONNECTED(1),      // real-time streaming
        @SerialName("mode2_programming") MODE2_PROGRAMMING(2),  // pre-upload, run from hub
        @SerialName("mode3_autonomous") MODE3_AUTONOMOUS(3),    // offline, power bank
    }

    companion object {
        /** Compile an authoring definition to the T1 hub wire form (parity with iOS init(from:)). */
        fun fromDefinition(
            definition: NPProtocolDefinition,
            mode: OperatingMode = OperatingMode.MODE2_PROGRAMMING,
        ): NPSessionProtocol {
            val duration = when (val t = definition.timingMode) {
                is NPTimingMode.Duration -> t.seconds
                is NPTimingMode.IntervalCount -> 20 * 60
            }
            val configs = mutableListOf<ModalityConfig>()
            for (m in definition.modalities.filter { it.enabled }) {
                // The wire has no per-block timing: every config runs from 0. A block
                // with `start` would run at the wrong time, so it is refused, not
                // flattened (NP-NPPS-REF-001 §5; per-block timing lands with
                // OI-AND-WIRE-01's shared schema).
                if (m.interval.startOffsetSeconds > 0) {
                    throw IllegalArgumentException(
                        "This protocol times a block with `start`, which the session wire cannot " +
                            "express yet. Refused, not reshaped.",
                    )
                }
                (m.params as? NPModalityParams.PbmTranscranial)?.let {
                    requireDeliverable("PBM", it.params.wavelength.rawValue, NPPBMChannelElement.entries)
                }
                (m.params as? NPModalityParams.PbmIntranasal)?.let {
                    requireDeliverable(
                        "Intranasal PBM", it.params.wavelength.rawValue,
                        listOf(NPPBMChannelElement.LED_660, NPPBMChannelElement.LED_808),
                    )
                }
                when (val p = m.params) {
                    is NPModalityParams.PbmTranscranial -> configs.add(
                        ModalityConfig.PbmTranscranial(
                            zones = p.params.resolvedZones,
                            wavelength = p.params.wavelength.rawValue,
                            frequencyHz = p.params.frequencyHz,
                            dutyCyclePercent = p.params.dutyCyclePercent,
                            durationSeconds = duration,
                            irradianceMWcm2 = p.params.irradianceMWcm2,
                            targetDoseJCm2 = p.params.irradianceMWcm2 *
                                (if (p.params.frequencyHz == 0.0) 1.0 else p.params.dutyCyclePercent / 100.0) *
                                duration / 1000.0,
                        ),
                    )
                    is NPModalityParams.PbmIntranasal -> configs.add(
                        ModalityConfig.PbmIntranasal(
                            wavelength = p.params.wavelength.rawValue,
                            irradianceMWcm2 = p.params.irradianceMWcm2,
                            frequencyHz = p.params.frequencyHz,
                            dutyCyclePercent = p.params.dutyCyclePercent,
                            durationSeconds = duration,
                        ),
                    )
                    is NPModalityParams.EegNeurofeedback -> configs.add(
                        ModalityConfig.EegNeurofeedback(
                            enabledChannels = p.params.resolvedChannels,
                            neurofeedbackBand = p.params.band.rawValue,
                            closedLoopEnabled = p.params.closedLoopEnabled,
                        ),
                    )
                    is NPModalityParams.BesTacs -> configs.add(
                        ModalityConfig.Bes(
                            frequencyHz = p.params.frequencyHz,
                            amplitudeMilliamps = p.params.intensityMilliamps,
                            durationSeconds = if (m.interval.isContinuous) duration else m.interval.intervalOnSeconds,
                            waveform = p.params.waveform.rawValue,
                        ),
                    )
                    is NPModalityParams.Tdcs -> configs.add(
                        ModalityConfig.Tdcs(
                            amplitudeMilliamps = p.params.intensityMilliamps,
                            durationSeconds = if (m.interval.isContinuous) duration else m.interval.intervalOnSeconds,
                            rampSeconds = p.params.rampSeconds,
                            electrodePairs = p.params.electrodePairs,
                            electrodeAreaCm2 = p.params.electrodeAreaCm2,
                        ),
                    )
                    is NPModalityParams.VnsHRV -> configs.add(
                        ModalityConfig.VnsHRV(
                            frequencyHz = p.params.frequencyHz,
                            amplitudeMilliamps = p.params.intensityMilliamps,
                            resonanceBreathingRateDefault = p.params.resonanceBreathingRate,
                            hrvProtocol = p.params.hrvProtocol.wireValue(),
                        ),
                    )
                    is NPModalityParams.AudioEntrainment -> configs.add(
                        ModalityConfig.NeuralAudio(
                            binauralBeatHz = p.params.binauralBeatsHz,
                            isochronicToneHz = p.params.isochronicTonesHz,
                            noiseType = p.params.noiseType?.rawValue,
                            volumeDb = p.params.volumeDb,
                            eegAdaptive = p.params.eegAdaptive,
                            useBoneConductionForPacer = p.params.boneConductionPacer,
                        ),
                    )
                    is NPModalityParams.VisualStimulation -> configs.add(
                        ModalityConfig.VisualStimulation(
                            frequencyHz = p.params.frequencyHz,
                            mode = p.params.mode.sessionWireName,
                            enableModeFInvisibleNIR = p.params.enableModeF,
                            emdrCadenceHz = p.params.emdrCadenceHz,
                        ),
                    )
                    // T2 + accessory modalities are not part of the T1 hub wire format
                    // (require a T2 hub session) — silently dropped, as on iOS.
                    else -> Unit
                }
            }
            return NPSessionProtocol(
                name = definition.name,
                modalities = configs,
                totalDurationSeconds = duration,
                mode = mode,
            )
        }
    }
}

// MARK: - Modality wire configs (T1)

@Serializable
sealed class ModalityConfig {
    @Serializable
    @SerialName("pbm_transcranial")
    data class PbmTranscranial(
        val zones: List<Int>,
        /**
         * The NPPS `wavelength` token, carried verbatim. Without it a "1064nm"
         * protocol compiled to the same config as a "660_808nm" one
         * (OI-PBMCH-04, NP-FEAS-PBMCH-001 §7.3).
         */
        val wavelength: String,
        val frequencyHz: Double,
        val dutyCyclePercent: Int,
        val durationSeconds: Int,
        /** Peak irradiance at the scalp, mW/cm², this wavelength alone (NP-NPPS-REF-001 Rev 18 §4.1b). */
        val irradianceMWcm2: Double,
        /** Scalp dose this block states, J/cm²: irradiance × duty × time. Not the old 0.4 W placeholder. */
        val targetDoseJCm2: Double,
    ) : ModalityConfig()

    @Serializable
    @SerialName("pbm_intranasal")
    data class PbmIntranasal(
        /** One wavelength per block; the probe carries 660 and 808 nm. */
        val wavelength: String,
        val irradianceMWcm2: Double,
        val frequencyHz: Double,
        val dutyCyclePercent: Int,
        val durationSeconds: Int,
    ) : ModalityConfig()

    @Serializable
    @SerialName("eeg_neurofeedback")
    data class EegNeurofeedback(
        val enabledChannels: List<String>,
        val sampleRateHz: Int = 500,
        val neurofeedbackBand: String,
        val closedLoopEnabled: Boolean = true,
    ) : ModalityConfig()

    @Serializable
    @SerialName("bes")
    data class Bes(
        val frequencyHz: Double,
        val amplitudeMilliamps: Double,
        val durationSeconds: Int,
        val waveform: String = "sinusoidal",
    ) : ModalityConfig()

    @Serializable
    @SerialName("tdcs")
    data class Tdcs(
        val amplitudeMilliamps: Double,
        val durationSeconds: Int,
        val rampSeconds: Int = 30,
        val electrodePairs: List<List<String>>,   // 10-20 SITES, not pad geometry
        /**
         * OI-CHARGE-04: per-electrode pad area, cm². The hub converts this to the
         * milli-cm² of `np_mod_tdcs_params_t.electrode_area_mcm2` and hands it to the
         * safety MCU, which derives its 40 µC/cm² charge limit from it. No default —
         * a missing area is the defect this field closes.
         */
        val electrodeAreaCm2: Double,
    ) : ModalityConfig()

    @Serializable
    @SerialName("vns_hrv")
    data class VnsHRV(
        val frequencyHz: Double,
        val amplitudeMilliamps: Double,
        val enableHRVBiofeedback: Boolean = true,
        val resonanceBreathingRateDefault: Double = 6.0,
        val hrvProtocol: String = "standalone",
    ) : ModalityConfig()

    @Serializable
    @SerialName("neural_audio")
    data class NeuralAudio(
        val binauralBeatHz: Double? = null,
        val isochronicToneHz: Double? = null,
        val noiseType: String? = null,
        /** Sound pressure level at the ear, dB SPL (NP-NPPS-REF-001 Rev 18 §4.7). */
        val volumeDb: Double,
        val eegAdaptive: Boolean = true,
        val useBoneConductionForPacer: Boolean = true,
    ) : ModalityConfig()

    @Serializable
    @SerialName("visual_stimulation")
    data class VisualStimulation(
        val frequencyHz: Double,
        val mode: String = "binocular",
        val enableModeFInvisibleNIR: Boolean = false,
        val emdrCadenceHz: Double = 1.0,
    ) : ModalityConfig()
}

/**
 * A PBM block must name ONE wavelength that a channel of this modality delivers under the
 * wavelength rules in force. Refused, never moved to the nearest channel (NP-NPPS-REF-001
 * §4.1a); a retired combined name says which blocks replace it.
 */
private fun requireDeliverable(what: String, wavelength: String, allowed: List<NPPBMChannelElement>) {
    when (val r = NPWavelengthRulesEngine.resolveChannels(wavelength)) {
        is NPPbmChannelResolution.Refused -> throw IllegalArgumentException(
            when (r.reason) {
                "retired" -> "$what: ${NPWavelengthRulesEngine.retiredMessage(r.value)}"
                "invalid" -> "$what wavelength '${r.value}' is not a wavelength: write one value such as \"810nm\"."
                else -> "No emitter channel delivers ${r.value} under the wavelength rules in force. " +
                    "Refused, not moved to the nearest channel."
            },
        )
        is NPPbmChannelResolution.Ok -> if (r.elements.none { it in allowed }) {
            throw IllegalArgumentException("$what cannot be delivered on ${r.elements.first().rawValue}: ${r.elements.first().rawValue} is a channel it does not carry.")
        }
    }
}

// MARK: - resolved zone/channel helpers (port of NPProtocolDefinition computed props)

/**
 * The 1-based socket ids this modality drives.
 *
 * These used to be five hardware slot indices (0..4) from the retired
 * zone-module design. A zone is now a named set of sockets, so the ids are real
 * socket (major) addresses resolved through [NPZoneRegistry], which reads the
 * loaded `.npps` files — the same source iOS resolves against.
 *
 * A clinician-selected target has no answer without the operator's choice, so
 * it resolves to nothing here; callers that can run a session must go through
 * [NPPBMTarget.resolveSockets] with the chosen sockets and handle its error.
 */
val NPPBMTranscranialParams.resolvedZones: List<Int>
    get() = runCatching { target.resolveSockets() }.getOrDefault(emptyList())

val NPEEGNeurofeedbackParams.resolvedChannels: List<String>
    get() = if (channels == NPEEGNeurofeedbackParams.ChannelSelection.CUSTOM && customChannels != null) customChannels!!
    else when (channels) {
        NPEEGNeurofeedbackParams.ChannelSelection.ALL -> listOf("Fp1", "Fp2", "F3", "F4", "C3", "C4", "P3", "P4")
        NPEEGNeurofeedbackParams.ChannelSelection.FRONT -> listOf("Fp1", "Fp2", "F3", "F4")
        NPEEGNeurofeedbackParams.ChannelSelection.CENTRAL -> listOf("C3", "C4", "P3", "P4")
        NPEEGNeurofeedbackParams.ChannelSelection.CUSTOM -> emptyList()
    }

private fun NPVNSHRVParams.HRVProtocol.wireValue(): String = when (this) {
    NPVNSHRVParams.HRVProtocol.STANDALONE -> "standalone"
    NPVNSHRVParams.HRVProtocol.TAVNS_SYNC -> "hrv_tavns_sync"
    NPVNSHRVParams.HRVProtocol.EEG_BIOFEEDBACK -> "hrv_eeg_biofeedback"
    NPVNSHRVParams.HRVProtocol.COMBINED_PBM -> "hrv_pbm"
}

// MARK: - Signed blob + signer

/**
 * Signed session descriptor ready for the wire. `payload` is the canonical JSON;
 * `signature` is Ed25519 over SHA-256(payload). Wire: 4-byte magic "NPPR" + 4-byte
 * little-endian payload length + payload + 64-byte signature (parity with iOS).
 */
data class SignedProtocolBlob(
    val payload: ByteArray,
    val signature: ByteArray,
    val publicKeyFingerprint: String,
) {
    companion object {
        val MAGIC = byteArrayOf(0x4E, 0x50, 0x50, 0x52) // "NPPR"
    }

    val wireFormat: ByteArray
        get() {
            val len = payload.size
            val lenLe = byteArrayOf(
                (len and 0xFF).toByte(),
                ((len ushr 8) and 0xFF).toByte(),
                ((len ushr 16) and 0xFF).toByte(),
                ((len ushr 24) and 0xFF).toByte(),
            )
            return MAGIC + lenLe + payload + signature
        }
}

/** Result of an Ed25519 signing operation over the 32-byte SHA-256 digest. */
data class SignatureResult(val signature: ByteArray, val publicKeyFingerprint: String)

/**
 * Platform-provided Ed25519 signer. Signs the 32-byte SHA-256 digest of the payload.
 * `:core` stays pure-JVM; the `:app` module provides the key-managed implementation
 * (parity with iOS SessionProtocolSigner, whose key lives in the Keychain).
 */
fun interface ProtocolSigner {
    fun sign(digest: ByteArray): SignatureResult
}

/**
 * Compiles a definition or session protocol into a signed, chunk-ready blob. Payload
 * construction, SHA-256 digest, and blob assembly are pure-JVM and unit-testable with a
 * fake signer; only the Ed25519 primitive is delegated to [ProtocolSigner].
 */
class SessionProtocolCompiler(
    private val signer: ProtocolSigner,
    private val json: Json = Json { encodeDefaults = true; classDiscriminator = "type" },
) {
    fun compile(proto: NPSessionProtocol): SignedProtocolBlob {
        val payload = json.encodeToString(proto).toByteArray(Charsets.UTF_8)
        val digest = MessageDigest.getInstance("SHA-256").digest(payload)
        val sig = signer.sign(digest)
        return SignedProtocolBlob(payload, sig.signature, sig.publicKeyFingerprint)
    }

    fun compile(
        definition: NPProtocolDefinition,
        mode: NPSessionProtocol.OperatingMode = NPSessionProtocol.OperatingMode.MODE2_PROGRAMMING,
    ): SignedProtocolBlob = compile(NPSessionProtocol.fromDefinition(definition, mode))
}
