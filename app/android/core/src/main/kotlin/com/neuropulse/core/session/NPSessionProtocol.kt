package com.neuropulse.core.session

import com.neuropulse.core.protocol.NPEEGNeurofeedbackParams
import com.neuropulse.core.protocol.NPModalityParams
import com.neuropulse.core.protocol.NPPBMTranscranialParams
import com.neuropulse.core.protocol.NPProtocolDefinition
import com.neuropulse.core.protocol.NPTimingMode
import com.neuropulse.core.protocol.NPVNSHRVParams
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
                when (val p = m.params) {
                    is NPModalityParams.PbmTranscranial -> configs.add(
                        ModalityConfig.PbmTranscranial(
                            zones = p.params.resolvedZones,
                            frequencyHz = p.params.frequencyHz,
                            dutyCyclePercent = p.params.dutyCyclePercent,
                            durationSeconds = duration,
                            targetDoseJoules = duration.toDouble() * (p.params.intensityPercent / 100.0) * 0.4,
                        ),
                    )
                    is NPModalityParams.PbmIntranasal -> configs.add(
                        ModalityConfig.PbmIntranasal(
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
                            eegAdaptive = p.params.eegAdaptive,
                            useBoneConductionForPacer = p.params.boneConductionPacer,
                        ),
                    )
                    is NPModalityParams.VisualStimulation -> configs.add(
                        ModalityConfig.VisualStimulation(
                            frequencyHz = p.params.frequencyHz,
                            mode = p.params.mode.rawValue,
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
        val frequencyHz: Double,
        val dutyCyclePercent: Int,
        val durationSeconds: Int,
        val targetDoseJoules: Double,
    ) : ModalityConfig()

    @Serializable
    @SerialName("pbm_intranasal")
    data class PbmIntranasal(
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
        val electrodePairs: List<List<String>>,
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

// MARK: - resolved zone/channel helpers (port of NPProtocolDefinition computed props)

val NPPBMTranscranialParams.resolvedZones: List<Int>
    get() = if (zones == NPPBMTranscranialParams.ZoneSelection.CUSTOM && customZones != null) customZones!!
    else when (zones) {
        NPPBMTranscranialParams.ZoneSelection.ALL -> listOf(0, 1, 2, 3, 4)
        NPPBMTranscranialParams.ZoneSelection.FRONT -> listOf(0, 1, 2)
        NPPBMTranscranialParams.ZoneSelection.REAR -> listOf(2, 3, 4)
        NPPBMTranscranialParams.ZoneSelection.CUSTOM -> emptyList()
    }

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
