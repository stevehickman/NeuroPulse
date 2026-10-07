package life.neurone.shared

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import life.neurone.core.analytics.EngagementTier
import life.neurone.core.analytics.ResearchAnalyticsGate
import life.neurone.core.analytics.WarrantyAnalyticsGate
import life.neurone.core.common.KeyValueStore
import life.neurone.core.consent.ConsentStore
import life.neurone.core.consumable.ConsumableCountsProviding
import life.neurone.core.consumable.ConsumableTracker
import life.neurone.core.models.ActiveUserTag
import life.neurone.core.models.SessionState
import life.neurone.core.protocol.NPLimitsStore
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.research.ResearchSuggestionStore
import life.neurone.core.session.SessionHistoryStore
import life.neurone.shared.ble.NeurOneGattManager

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
    val keyValueStore: KeyValueStore = platform.keyValueStore

    val researchAnalyticsGate = ResearchAnalyticsGate(keyValueStore, platform.analyticsBackend)
    val warrantyAnalyticsGate = WarrantyAnalyticsGate(keyValueStore)
    val consentStore = ConsentStore(keyValueStore, researchAnalyticsGate)
    val sessionHistoryStore = SessionHistoryStore(keyValueStore)
    val researchSuggestionStore = ResearchSuggestionStore(keyValueStore)

    val gattManager = NeurOneGattManager(platform.bleCentral, scope, keyValueStore)

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
