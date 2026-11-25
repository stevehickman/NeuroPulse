package life.neurone.core.analytics

import kotlinx.coroutines.launch
import life.neurone.core.common.KeyValueStore
import life.neurone.core.protocol.PersistedKeys

/**
 * Consent gate for research analytics and crash-reporting telemetry.
 * Port of iOS ResearchAnalyticsGate (NP-APP-TELEMETRY-001 Rev B).
 *
 * No analytics or crash-reporting SDK may initialize or receive any event until
 * the user has ACTIVELY completed the consent onboarding flow (tapped Done —
 * Skip does NOT open the gate). Withdrawing blanket research consent (L3)
 * clears the gate key and tears down the SDK.
 *
 * Entirely independent of [WarrantyAnalyticsGate] (SHDR fleet telemetry under
 * warranty-owner consent). The two consent subjects are never mixed and never
 * share a persistence key (CLAUDE.md §6.0).
 */
class ResearchAnalyticsGate(
    private val store: KeyValueStore,
    private val backend: AnalyticsBackend,
    /** Where a withdrawn person's old analytics ID waits to be erased from the vendor (`REQ-SW03-19`); null in a build with no vendor. */
    private val erasureQueue: AnalyticsErasureQueue? = null,
    /** Runs the erasure request after a withdrawal; null runs none (the queue is drained at the next launch). */
    private val scope: kotlinx.coroutines.CoroutineScope? = null,
) {
    /** Idempotency flag so configure() only ever starts the SDK once. */
    private var isConfigured = false

    /** True only when the user actively completed the research consent flow. */
    val isOpen: Boolean
        get() = store.getBoolean(PersistedKeys.RESEARCH_ANALYTICS_KEY)

    /**
     * Initialize the analytics SDK. Call exactly once, after the research
     * consent flow completes. No-ops if already configured or gate closed.
     */
    fun configure() {
        if (isConfigured) return
        if (!isOpen) return
        isConfigured = true
        backend.configure()
    }

    /**
     * Close the research analytics gate and tear down the SDK.
     * Tears down the backend BEFORE clearing the configured flag so a
     * concurrent configure() cannot re-initialize before shutdown completes
     * (order preserved from the iOS implementation).
     * Does NOT affect [WarrantyAnalyticsGate] or SHDR fleet uploads.
     *
     * It does not depend on [configure] having run in this process (`OI-PRIV-08` (1)): the backend keeps its ID across runs, so a
     * withdrawal made before the gate was configured still has an ID the vendor holds events under, and it is forgotten and
     * queued all the same. A backend that never sent anything has no ID and nothing is queued.
     */
    fun reset() {
        if (forgetAndQueue()) scope?.launch { erasureQueue?.drain() }
    }

    /**
     * [reset] for an erase of this device: the same teardown, and then the relay is asked now and the answer waited for, because
     * the erase deletes the queue with everything else and an ID still in it could never be erased from the vendor afterwards.
     * IDs queued by earlier withdrawals are asked about too. Returns how many IDs the relay has not accepted (it could not be
     * reached, or it refused): the erase goes on regardless, since the person's erase is never held back, and the caller can say so.
     * An ID the relay answered `202` counts as accepted: the relay holds it and deletes it in its next batch, though the wipe then
     * removes the phone's copy (`OI-PRIV-48`).
     */
    suspend fun resetForErase(): Int {
        forgetAndQueue()
        val queue = erasureQueue ?: return 0
        val queuedAtRelay = queue.drain()
        return queue.pending.count { it !in queuedAtRelay }
    }

    /**
     * The IDs the vendor holds events under on this install, for a device-replacement clear (`OI-ANALYTICS-01` (5c)): the current
     * one and any still queued by an earlier withdrawal. **Changes nothing**: the person has not withdrawn, and a clear the core
     * refuses must not rotate a consenting person's ID. The per-install keys never move to the new phone, so these IDs end here.
     */
    fun idsOnThisInstall(): List<String> = (listOfNotNull(backend.distinctId) + (erasureQueue?.pending ?: emptyList())).distinct()

    /**
     * Ask the relay to erase [ids], after the core has accepted the clear and the store is wiped. Returns how many it did not
     * take. Never throws and never queues (the store is empty; an ID written back would outlive the wipe).
     */
    suspend fun eraseAfterClear(ids: List<String>): Int {
        val queue = erasureQueue ?: return 0
        return ids.count { id -> !(try { queue.eraseNow(id) } catch (e: kotlinx.coroutines.CancellationException) { throw e } catch (e: Exception) { false }) }
    }

    /** True when an ID was queued. */
    private fun forgetAndQueue(): Boolean {
        // Read the pseudonymous ID BEFORE the backend resets it: `reset()` forgets it, and it is what the vendor holds this
        // person's events under. It is queued, durably, and erased from the vendor within 30 days
        // (NP-APP-TELEMETRY-001 §5, REQ-SW03-19).
        val withdrawnId = backend.distinctId
        backend.reset()
        isConfigured = false
        if (withdrawnId == null || erasureQueue == null) return false
        erasureQueue.enqueue(withdrawnId)
        return true
    }

    /**
     * Record a research analytics event. Silently no-ops when the gate is
     * closed. Drops the event entirely (does NOT transmit) unless [AnalyticsEventPolicy]
     * permits it: a property the telemetry policy does not name, or a value not of
     * its form, drops the whole event (an allowlist, NP-APP-TELEMETRY-001 §3), and
     * so does an event that cannot be checked.
     */
    fun track(event: String, properties: Map<String, String>) {
        if (!isOpen) return
        if (!AnalyticsEventPolicy.permits(event, properties)) return
        backend.track(event, properties)
    }
}
