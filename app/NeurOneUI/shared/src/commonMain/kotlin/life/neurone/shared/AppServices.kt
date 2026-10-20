package life.neurone.shared

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import life.neurone.core.analytics.EngagementTier
import life.neurone.core.analytics.FleetConfig
import life.neurone.core.analytics.FleetUploadWiring
import life.neurone.core.analytics.FleetUploader
import life.neurone.core.analytics.SealedDeviceTokenStore
import life.neurone.core.analytics.AnalyticsBackend
import life.neurone.core.analytics.AnalyticsErasureQueue
import life.neurone.core.analytics.PostHogAnalyticsBackend
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.analytics.WarrantyAnalyticsGate
import life.neurone.core.common.EncryptedKeyValueStore
import life.neurone.core.common.KeyValueStore
import life.neurone.core.common.ProtectedStorage
import life.neurone.core.common.StorageProtection
import life.neurone.core.migration.ClearActions
import life.neurone.core.consent.ConsentStore
import life.neurone.core.migration.LINK_ROUTE
import life.neurone.core.migration.MemoryObjectStore
import life.neurone.core.consumable.ConsumableCountsProviding
import life.neurone.core.consumable.ConsumableTracker
import life.neurone.core.models.ActiveUserTag
import life.neurone.core.models.SessionState
import life.neurone.core.protocol.NPLimitsStore
import life.neurone.core.protocol.NPWavelengthRulesStore
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.research.ResearchSuggestionStore
import life.neurone.core.session.SessionHistoryStore
import life.neurone.shared.ble.NeurOneGattManager
import life.neurone.shared.migration.SharedClearActions
import life.neurone.shared.session.ProtocolUploader

/**
 * The composition root, written once. It builds every store and gate the screens read from
 * `:core` and the platform's [PlatformServices], so the wiring that decides what may happen
 * (which gate guards which flow, what a replaced consumable tells the hub) cannot differ
 * between platforms. Before this existed, each app assembled its own and the Android wiring was
 * the only one that could be read in one place.
 *
 * **UHDR/SHDR boundary.** The two consent subjects stay structurally separate here, as in
 * CLAUDE.md §6.0: the research gate is built on the user's [consentStore], the warranty gate on
 * nothing but the key-value store, and neither holds a reference to the other.
 */
class AppServices(
    val platform: PlatformServices,
    /** Long-lived scope for the GATT manager and the consumable subscription. */
    val scope: CoroutineScope,
) {
    /**
     * The UHDR-class state is sealed at rest when the host has a keystore to keep the key in (SWR-MG-06); every store and
     * gate below reads and writes through it, so none of them knows. Without a keystore, or without the NPPS core binding that
     * seals, the data stays as the host stores it and [clearActions] is null.
     *
     * **A run that cannot open what an earlier run sealed does not fall back to the host's plain store** ([ProtectedStorage],
     * SWR-MG-13): it holds those keys in memory for the run and writes nothing to them on disk, so the sealed data is neither
     * read as ciphertext nor overwritten with plaintext. [storageProtection] says which of the three states this run is in.
     */
    private val protectedStorage: ProtectedStorage = ProtectedStorage.open(platform.keyValueStore, platform.secretKeyStore) {
        EncryptedKeyValueStore.uhdrKeys()
    }

    /** How the protected keys are held on this run; the screens tell the person when it is not [StorageProtection.SEALED]. */
    val storageProtection: StorageProtection = protectedStorage.state

    private val encrypted: EncryptedKeyValueStore? = protectedStorage.sealing

    val keyValueStore: KeyValueStore = protectedStorage.store

    // The remembered hub's id is a sealed key (SWR-MG-16): a central that read the host's raw store would see it as ciphertext.
    init { platform.bleCentral.useIdentityStore(keyValueStore) }

    /** Withdrawn people's old analytics IDs, kept until the erasure relay has answered for each (`REQ-SW03-19`). */
    val analyticsErasureQueue = AnalyticsErasureQueue(keyValueStore, platform.httpTransport) { platform.analyticsConfig?.baseUrl }

    /** The common PostHog backend where the build has a usable host, token and transport; otherwise the host's own (none). */
    private val analyticsBackend: AnalyticsBackend =
        platform.analyticsConfig?.takeIf { it.usable }?.let { config ->
            platform.httpTransport?.let { PostHogAnalyticsBackend(keyValueStore, it, config, scope) }
        } ?: platform.analyticsBackend

    val researchAnalyticsGate = ResearchAnalyticsGate(keyValueStore, analyticsBackend, analyticsErasureQueue, scope)
    val warrantyAnalyticsGate = WarrantyAnalyticsGate(keyValueStore)

    /**
     * The SHDR fleet uploader, gated only by the warranty owner's consent (`REQ-SHDR-01`), on a host that can pin the relay's
     * certificate (`REQ-SHDR-04`); null elsewhere. Its token store is the sealed one, so a helmet's token never leaves it in the clear.
     */
    val fleetUploader: FleetUploader? =
        platform.fleetTransport(FleetConfig.RELAY_HOST, FleetConfig.PINNED_SPKI_HASHES)?.let {
            FleetUploader(SealedDeviceTokenStore(keyValueStore), warrantyAnalyticsGate, it)
        }
    val consentStore = ConsentStore(keyValueStore, researchAnalyticsGate)

    /**
     * The folder the person has chosen to keep a backup in, for a host that offers the choice ([PlatformServices.folderPlaces]);
     * null for a host with a fixed place or none. Common to every host that has folders.
     */
    val backupFolder: life.neurone.shared.migration.BackupFolderModel? =
        platform.folderPlaces?.let { life.neurone.shared.migration.BackupFolderModel(it, keyValueStore) }

    /** The place the backup, restore and clear screens use: the chosen folder, or the host's own fixed place. */
    val backupPlace: StateFlow<MigrationPlace?> =
        backupFolder?.place ?: kotlinx.coroutines.flow.MutableStateFlow(platform.migrationPlace)

    /**
     * Where a bundle waits while it goes straight to another phone (SWR-MG-10): in memory, behind a claim only this process
     * can take, so nothing of it is left on disk to find. Null when the host cannot reach another phone. The send screen and
     * the clear screen share it, because the clear reads the new phone's confirmation from the same store.
     */
    val linkPlace: MigrationPlace? = platform.linkTransport?.let { MigrationPlace("", MemoryObjectStore(), LINK_ROUTE) }

    /** The items a migration carries that are not plain keys: the withdrawal records (`SWR-MG-04`). */
    val migrationItems: List<life.neurone.core.migration.MigrationItemStore> =
        listOf(life.neurone.core.migration.StoreConsentMigrationAdapter(consentStore))

    /**
     * The automatic health-data backup (SWR-MG-03): off until the person turns it on while making a backup, then made again at launch
     * and hourly while the app stays open, whenever the last is over 20 hours old. Only the host's folder place can run unattended.
     */
    val scheduledBackup = life.neurone.shared.migration.ScheduledBackup({ backupPlace.value }, keyValueStore, migrationItems)
    val sessionHistoryStore = SessionHistoryStore(keyValueStore)
    val researchSuggestionStore = ResearchSuggestionStore(keyValueStore)

    val gattManager = NeurOneGattManager(platform.bleCentral, scope, keyValueStore)

    /**
     * The clear's two platform steps (SWR-MG-06, SWR-MG-09), written once: destroy the data key in the keystore, and drop the
     * link to the helmet and forget it. Null when the data is not sealed, so there is no key to destroy.
     */
    val clearActions: ClearActions? = encrypted?.let { SharedClearActions(it, gattManager) }

    /**
     * The person's "erase this device" (SWR-MG-17), or null on a host that does not offer it ([PlatformServices.endRunAfterErase]).
     * It works against the host's raw store and the keystore as they are, not through [keyValueStore], so it also works on a run
     * that is locked or unsealed.
     */
    val eraseDevice: life.neurone.shared.migration.EraseDeviceModel? =
        platform.endRunAfterErase?.let {
            life.neurone.shared.migration.EraseDeviceModel(platform.keyValueStore, platform.migrationFiles, platform.secretKeyStore, gattManager)
        }

    /** Mode-2 upload: compile, sign with the platform's signer, chunk, write to the hub. */
    /** The user's PBM wavelength rules; the descriptor compiler reads the resolved set at each upload (NP-NPPS-REF-001 §7a). */
    val wavelengthRulesStore = NPWavelengthRulesStore(keyValueStore)

    val protocolUploader = ProtocolUploader(gattManager, { wavelengthRulesStore.resolved }, platform.protocolSigner)

    /** Consumable reminder engine, fed by the hub's CONSUMABLE_STATUS counts (SHDR-class). */
    val consumableTracker = ConsumableTracker(
        GattConsumableCountsProvider(gattManager.session, scope),
        keyValueStore,
        // OI-ACC-08: the hub owns the count; Mark replaced tells it to zero it.
        onReplaced = { gattManager.requestConsumableReset(it) },
    )

    val limitsStore = NPLimitsStore(keyValueStore).also { limits ->
        // Per-user cardiac scope (NP-SW-FAULTMSG-001): the active individual profile names the person on
        // the device. Until a profile is chosen the device keeps assuming whoever it last knew.
        gattManager.activeUserTag = ActiveUserTag.from(limits.activeProfileId)
        limits.onActiveProfileChanged = { gattManager.activeUserTag = ActiveUserTag.from(it) }
    }

    /**
     * Lazy because building it reads the bundled `.npps` library through the shared NPPS core, which a
     * target without that binding wired yet (OI-UI-KMP-01) cannot do. Screens that do not list
     * protocols never pay for it, or fail on it.
     */
    val protocolLibrary: NPProtocolLibrary by lazy { NPProtocolLibrary(keyValueStore) }

    init {
        EngagementTier.incrementLaunchCount(keyValueStore)
        // SDK initialization gate (NP-APP-TELEMETRY-001 Rev B §5): configure() no-ops unless the user
        // actively completed the consent flow.
        researchAnalyticsGate.configure()
        // A withdrawal made offline, or one the relay could not take, is retried at launch (REQ-SW03-19).
        scope.launch { analyticsErasureQueue.drain() }
        scope.launch {
            while (true) {
                scheduledBackup.runIfDue()
                kotlinx.coroutines.delay(BACKUP_CHECK_MILLIS)
            }
        }
        // The connected hub is the helmet whose limits tier applies; none connected means no helmet tier.
        scope.launch { gattManager.hubIdentifier.collect { limitsStore.activeHelmetSerial = it } }
        composeFleetUpload()
    }

    /**
     * The SHDR upload pipeline, the same on every host that has it (`REQ-SHDR-01`, `-02`, `-05`): the helmet the link is to names
     * the token, a hub-delivered token replaces the interim one (a disconnect never downgrades it), and a pending upload with a
     * staged record and an open warranty gate is sent, the staged file being removed only once the fleet service took it.
     */
    private fun composeFleetUpload() {
        val uploader = fleetUploader ?: return
        scope.launch { gattManager.hubIdentifier.collect { FleetUploadWiring.applyHubIdentifier(it, uploader) } }
        scope.launch { gattManager.warrantyToken.collect { FleetUploadWiring.applyWarrantyToken(it, uploader) } }
        val staging = platform.shdrStaging ?: return
        scope.launch {
            gattManager.shdrUploadPending.collect { pending ->
                val gateOpen = warrantyAnalyticsGate.isOpen
                val payload = if (pending && gateOpen) kotlinx.coroutines.withContext(Dispatchers.Default) { staging.read() } else null
                FleetUploadWiring.applyUploadPending(pending, gateOpen, payload, uploader) { if (it) staging.clear() }
            }
        }
    }
}

/**
 * Adapts the GATT manager's session flow into the core ConsumableCountsProviding contract.
 * `SessionState.consumableSessionCounts` (SHDR-class device counts, not user biology) is the source.
 * A StateFlow emits its current value on subscription, which satisfies the "current value
 * synchronously on subscription" contract iOS's CurrentValueSubject has.
 */
private class GattConsumableCountsProvider(
    private val session: StateFlow<SessionState>,
    private val scope: CoroutineScope,
) : ConsumableCountsProviding {
    override fun observe(listener: (List<Int>) -> Unit): AutoCloseable {
        val job = scope.launch {
            session.map { it.consumableSessionCounts }.distinctUntilChanged().collect { listener(it) }
        }
        return AutoCloseable { job.cancel() }
    }
}

/** The scope hosts give [AppServices]: lives as long as the process, and one failed child does not cancel the rest. */
fun createAppScope(): CoroutineScope = CoroutineScope(SupervisorJob() + Dispatchers.Main)

/** How often an app that stays open looks for an automatic backup that is due. */
private const val BACKUP_CHECK_MILLIS = 3_600_000L
