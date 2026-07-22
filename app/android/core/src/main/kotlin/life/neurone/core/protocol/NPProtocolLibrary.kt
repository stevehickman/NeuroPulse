package life.neurone.core.protocol

import life.neurone.core.common.KeyValueStore
import java.util.UUID

// Port of iOS NPProtocolLibrary (app/ios/NeurOne/Protocol/NPProtocolLibrary.swift).
// Pure-JVM: bundled + user protocol store, device-tier/EEG-consent availability, and
// validation integration. On iOS this is an @MainActor ObservableObject; on Android the
// UI layer observes it (a ViewModel wraps it). Persistence is via KeyValueStore using the
// NPPS text format (round-tripped through the existing serializer/parser) instead of iOS's
// JSON file — the on-device format is an implementation detail, behavior is identical.

// MARK: - Modality tier helpers (port of NPModalityType.tier / t1Set / t2Set) -----------

enum class NPModalityTier { T1, T2, ACCESSORY }

val NPModalityType.tier: NPModalityTier
    get() = when (this) {
        NPModalityType.PBM_TRANSCRANIAL, NPModalityType.PBM_INTRANASAL,
        NPModalityType.EEG_NEUROFEEDBACK, NPModalityType.BES_TACS, NPModalityType.TDCS,
        NPModalityType.VNS_HRV, NPModalityType.AUDIO_ENTRAINMENT, NPModalityType.VISUAL_STIMULATION ->
            NPModalityTier.T1
        NPModalityType.QEEG_21CH, NPModalityType.TMS, NPModalityType.PBM_DEEP_1170NM,
        NPModalityType.CLINICAL_TACS, NPModalityType.HD_TDCS, NPModalityType.CERVICAL_VNS ->
            NPModalityTier.T2
        NPModalityType.VIBROTACTILE_40HZ -> NPModalityTier.ACCESSORY
    }

val NP_T1_MODALITY_SET: Set<NPModalityType> = setOf(
    NPModalityType.PBM_TRANSCRANIAL, NPModalityType.PBM_INTRANASAL, NPModalityType.EEG_NEUROFEEDBACK,
    NPModalityType.BES_TACS, NPModalityType.TDCS, NPModalityType.VNS_HRV,
    NPModalityType.AUDIO_ENTRAINMENT, NPModalityType.VISUAL_STIMULATION,
)

val NP_T2_MODALITY_SET: Set<NPModalityType> = NP_T1_MODALITY_SET + setOf(
    NPModalityType.QEEG_21CH, NPModalityType.TMS, NPModalityType.PBM_DEEP_1170NM,
    NPModalityType.CLINICAL_TACS, NPModalityType.HD_TDCS, NPModalityType.CERVICAL_VNS,
)

// MARK: - NPProtocolEntry / NPProtocolDefinition computed helpers ------------------------

val NPProtocolEntry.id: UUID
    get() = when (this) {
        is NPProtocolEntry.Single -> protocol.id
        is NPProtocolEntry.Composite -> composite.id
        is NPProtocolEntry.Limits -> limits.id
    }

val NPProtocolEntry.name: String
    get() = when (this) {
        is NPProtocolEntry.Single -> protocol.name
        is NPProtocolEntry.Composite -> composite.name
        is NPProtocolEntry.Limits -> limits.name
    }

val NPProtocolEntry.isReadOnly: Boolean
    get() = when (this) {
        is NPProtocolEntry.Single -> protocol.isReadOnly
        is NPProtocolEntry.Composite -> composite.isReadOnly
        is NPProtocolEntry.Limits -> false
    }

val NPProtocolEntry.isComposite: Boolean get() = this is NPProtocolEntry.Composite

val NPProtocolDefinition.requiredModalityTypes: Set<NPModalityType>
    get() = modalities.filter { it.enabled }.map { it.modalityType }.toSet()

fun NPProtocolDefinition.requires1064SmartModule(): Boolean =
    modalities.filter { it.enabled }.any {
        val p = it.params
        p is NPModalityParams.PbmTranscranial && p.params.wavelength.requiresSmartModule
    }

// MARK: - Availability ------------------------------------------------------------------

enum class DeviceTier { NONE, T1, T2 }

/** Port of iOS ProtocolAvailability, incl. the ISC-90 `.eegConsentRequired` case. */
sealed class ProtocolAvailability {
    object Available : ProtocolAvailability()
    data class Unavailable(val missingModalities: List<NPModalityType>) : ProtocolAvailability()
    object EegConsentRequired : ProtocolAvailability()

    val isAvailable: Boolean get() = this is Available
}

// MARK: - NPProtocolLibrary -------------------------------------------------------------

class NPProtocolLibrary(
    private val kv: KeyValueStore,
) {
    companion object {
        const val USER_PROTOCOLS_KEY = "np.user-protocols.npps"
        const val BIPA_ACCEPTED_KEY = "np.onboarding.bipa-accepted"
    }

    /** BIPA biometric consent (ISC-90). Seeded from the shared onboarding key. */
    var eegConsentGranted: Boolean = kv.getBoolean(BIPA_ACCEPTED_KEY)
        private set

    var connectedDeviceTier: DeviceTier = DeviceTier.NONE
        private set
    var has1064SmartModules: Boolean = false
        private set
    var availableModalities: Set<NPModalityType> = NP_T1_MODALITY_SET
        private set

    val bundledProtocols: List<NPProtocolEntry> = loadBundledProtocols()

    private val _userProtocols: MutableList<NPProtocolEntry> = mutableListOf()
    val userProtocols: List<NPProtocolEntry> get() = _userProtocols

    val allProtocols: List<NPProtocolEntry> get() = bundledProtocols + _userProtocols

    private val validationCache: MutableMap<UUID, NPValidationResult> = mutableMapOf()

    init {
        loadFromStore()
    }

    // MARK: Availability

    fun availability(entry: NPProtocolEntry): ProtocolAvailability {
        // BIPA gate (ISC-90) runs before hardware/tier — parity with iOS.
        if (!eegConsentGranted && ProtocolConsentGate.isEEGDependent(entry, ::findSingle)) {
            return ProtocolAvailability.EegConsentRequired
        }
        if (connectedDeviceTier == DeviceTier.NONE) {
            return ProtocolAvailability.Unavailable(emptyList())
        }
        return when (entry) {
            is NPProtocolEntry.Composite -> {
                val missing = linkedSetOf<NPModalityType>()
                for (layer in entry.composite.layers) {
                    val def = findSingle(layer.protocolName) ?: continue
                    val sub = availability(NPProtocolEntry.Single(def))
                    if (sub is ProtocolAvailability.Unavailable) missing.addAll(sub.missingModalities)
                }
                if (missing.isEmpty()) ProtocolAvailability.Available
                else ProtocolAvailability.Unavailable(missing.toList())
            }
            is NPProtocolEntry.Single -> {
                val missing = mutableListOf<NPModalityType>()
                val def = entry.protocol
                if (def.requires1064SmartModule() && !has1064SmartModules) {
                    missing.add(NPModalityType.PBM_TRANSCRANIAL)
                }
                for (modality in def.requiredModalityTypes) {
                    if (!isModalityAvailable(modality) && !missing.contains(modality)) missing.add(modality)
                }
                if (missing.isEmpty()) ProtocolAvailability.Available
                else ProtocolAvailability.Unavailable(missing)
            }
            is NPProtocolEntry.Limits -> ProtocolAvailability.Available
        }
    }

    private fun isModalityAvailable(modality: NPModalityType): Boolean = when (modality.tier) {
        NPModalityTier.T1 -> connectedDeviceTier == DeviceTier.T1 || connectedDeviceTier == DeviceTier.T2
        NPModalityTier.T2 -> connectedDeviceTier == DeviceTier.T2
        NPModalityTier.ACCESSORY -> true
    }

    fun updateAvailability(tier: DeviceTier, smartModules: Boolean) {
        connectedDeviceTier = tier
        has1064SmartModules = smartModules
        availableModalities = when (tier) {
            DeviceTier.NONE -> emptySet()
            DeviceTier.T1 -> NP_T1_MODALITY_SET
            DeviceTier.T2 -> NP_T2_MODALITY_SET
        }
    }

    /** Push the BIPA consent state from onboarding (ISC-90), parity with iOS updateEEGConsent. */
    fun updateEEGConsent(granted: Boolean) {
        eegConsentGranted = granted
    }

    // MARK: Validation integration

    fun validationResult(
        entry: NPProtocolEntry,
        limits: NPLimitsSet = NPLimitsSet(name = "unlimited"),
    ): NPValidationResult {
        validationCache[entry.id]?.let { return it }
        val result = NPProtocolValidator(limits).validate(entry, ::findSingle)
        validationCache[entry.id] = result
        return result
    }

    fun issueCount(entry: NPProtocolEntry): Pair<Int, Int> {
        val result = validationCache[entry.id] ?: return 0 to 0
        return result.errors.size to result.warnings.size
    }

    fun revalidateAll(limits: NPLimitsSet = NPLimitsSet(name = "unlimited")) {
        validationCache.clear()
        val validator = NPProtocolValidator(limits)
        for (entry in allProtocols) validationCache[entry.id] = validator.validate(entry, ::findSingle)
    }

    // MARK: Read-only guards

    fun canEdit(entry: NPProtocolEntry): Boolean = !entry.isReadOnly
    fun canDelete(entry: NPProtocolEntry): Boolean = !entry.isReadOnly

    // MARK: Mutations

    fun save(entry: NPProtocolEntry) {
        if (bundledProtocols.any { it.id == entry.id }) return // never overwrite a bundled entry
        val idx = _userProtocols.indexOfFirst { it.id == entry.id }
        if (idx >= 0) _userProtocols[idx] = entry else _userProtocols.add(entry)
        saveToStore()
    }

    fun delete(id: UUID) {
        if (bundledProtocols.any { it.id == id }) return
        _userProtocols.removeAll { it.id == id }
        saveToStore()
    }

    // MARK: Script export / import

    fun exportScript(entry: NPProtocolEntry): String = NPPSSerializer().serialize(entry)

    fun importScript(text: String): NPProtocolEntry {
        val entries = NPPSParser(NPPSLexer(text).tokenize()).parse()
        return entries.firstOrNull() ?: throw NPPSError("No protocol found in script", 0)
    }

    // MARK: Lookup

    private fun findSingle(name: String): NPProtocolDefinition? =
        allProtocols.firstNotNullOfOrNull { e ->
            (e as? NPProtocolEntry.Single)?.protocol?.takeIf { it.name == name }
        }

    // MARK: Bundled loading

    private fun loadBundledProtocols(): List<NPProtocolEntry> =
        NPBundledProtocols.allContents.flatMap { content ->
            runCatching { NPPSParser(NPPSLexer(content).tokenize()).parse() }.getOrDefault(emptyList())
        }

    // MARK: Persistence (NPPS text via KeyValueStore)

    private fun loadFromStore() {
        val text = kv.getString(USER_PROTOCOLS_KEY) ?: return
        if (text.isBlank()) return
        val parsed = runCatching { NPPSParser(NPPSLexer(text).tokenize()).parse() }.getOrDefault(emptyList())
        _userProtocols.clear()
        _userProtocols.addAll(parsed)
    }

    private fun saveToStore() {
        val serializer = NPPSSerializer()
        val text = _userProtocols.joinToString("\n\n") { serializer.serialize(it) }
        kv.putString(USER_PROTOCOLS_KEY, text)
    }
}
