package life.neurone.core.analytics

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.runBlocking
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The vendor-side erasure of a withdrawn person's analytics ID, and the common PostHog backend (`REQ-SW03-19`, `REQ-SW03-20`). */
class AnalyticsErasureTests {
    private val id = "0b1c2d3e4f5061728394a5b6c7d8e9f0"

    private class Fake(var status: Int? = 200) : HttpTransport {
        val seen = mutableListOf<HttpRequest>()
        override suspend fun post(request: HttpRequest): HttpResponse? {
            seen += request
            return status?.let { HttpResponse(it, emptyMap(), ByteArray(0)) }
        }
    }

    private fun queue(store: InMemoryKeyValueStore, t: HttpTransport?, base: String? = "https://analytics.example.org") =
        AnalyticsErasureQueue(store, t) { base }

    // @verifies REQ-SW03-19
    @Test fun anIdIsKeptUntilTheRelayAcceptsItAndThenDropped() = runBlocking {
        val store = InMemoryKeyValueStore()
        val t = Fake(200)
        val q = queue(store, t)
        q.enqueue(id)
        assertEquals(listOf(id), q.pending)
        q.drain()
        assertEquals(emptyList(), q.pending)
        val sent = t.seen.single()
        assertEquals("https://analytics.example.org/np/erase", sent.url)
        assertEquals(setOf("distinct_id"), Json.parseToJsonElement(sent.body.decodeToString()).jsonObject.keys)
        assertEquals(id, Json.parseToJsonElement(sent.body.decodeToString()).jsonObject["distinct_id"]!!.jsonPrimitive.content)
    }

    // @verifies REQ-SW03-19
    @Test fun aTransientFailureKeepsTheIdAndTheRestWait() = runBlocking {
        val store = InMemoryKeyValueStore()
        val q = queue(store, Fake(503))
        q.enqueue(id)
        q.enqueue("ffffffffffffffffffffffffffffffff")
        q.drain()
        assertEquals(2, q.pending.size)
        for (status in listOf(null, 408, 429, 500)) {
            val again = queue(store, Fake(status))
            again.drain()
            assertEquals(2, again.pending.size, "status $status keeps it")
        }
    }

    // @verifies REQ-SW03-19
    @Test fun aRefusalForGoodDropsTheId() = runBlocking {
        val q = queue(InMemoryKeyValueStore(), Fake(400))
        q.enqueue(id)
        q.drain()
        assertEquals(emptyList(), q.pending)
    }

    // @verifies REQ-SW03-19
    @Test fun noHostOrNoTransportMakesNoCallAndKeepsTheId() = runBlocking {
        val store = InMemoryKeyValueStore()
        val t = Fake()
        queue(store, t, base = null).also { it.enqueue(id) }.drain()
        assertTrue(t.seen.isEmpty())
        queue(store, null).drain()
        assertEquals(listOf(id), queue(store, t).pending)
    }

    // @verifies REQ-SW03-19
    @Test fun anIdIsQueuedOnceAndOnlyIfItCouldBeOne() {
        val q = queue(InMemoryKeyValueStore(), Fake())
        q.enqueue(id); q.enqueue(id)
        q.enqueue(""); q.enqueue("short"); q.enqueue("a\nb-0123456789abcdef")
        assertEquals(listOf(id), q.pending)
    }

    private class Backend(override val distinctId: String?) : AnalyticsBackend {
        var resets = 0
        override fun configure() {}
        override fun reset() { resets++ }
        override fun track(event: String, properties: Map<String, String>) {}
    }

    // @verifies REQ-SW03-25#d
    @Test fun withdrawingResearchConsentQueuesTheOldIdBeforeTheBackendForgetsIt() {
        val store = InMemoryKeyValueStore()
        store.putBoolean(life.neurone.core.protocol.PersistedKeys.RESEARCH_ANALYTICS_KEY, true)
        val q = queue(store, null)
        val backend = Backend(id)
        val gate = ResearchAnalyticsGate(store, backend, q)
        gate.configure()
        gate.reset()
        assertEquals(1, backend.resets)
        assertEquals(listOf(id), q.pending)
    }

    // ── The common PostHog backend ───────────────────────────────────────────────────────────────────────────────────

    private val config = AnalyticsConfig("analytics.example.org", "phc_testtoken")

    private fun scope() = CoroutineScope(Job() + Dispatchers.Unconfined)

    // @verifies REQ-SW03-25#a
    @Test fun aBuildWithoutAUsableHostAndTokenNeverSends() {
        for (bad in listOf(AnalyticsConfig(null, "phc_x"), AnalyticsConfig("analytics.example.org", null), AnalyticsConfig("https://x.org", "phc_x"),
            AnalyticsConfig("analytics.example.org", "\$(POSTHOG_PROJECT_TOKEN)"), AnalyticsConfig("localhost", ""))) {
            val t = Fake()
            val b = PostHogAnalyticsBackend(InMemoryKeyValueStore(), t, bad, scope())
            b.configure()
            b.track("e", emptyMap())
            assertTrue(t.seen.isEmpty(), bad.host.toString())
            assertNull(b.distinctId)
        }
    }

    // @verifies REQ-SW03-25#a,b
    @Test fun anEventCarriesTheAllowedPropertiesAndTheTwoSwitchesAndNothingElse() {
        val t = Fake()
        val b = PostHogAnalyticsBackend(InMemoryKeyValueStore(), t, config, scope())
        b.track("early", emptyMap())
        assertTrue(t.seen.isEmpty(), "nothing before configure()")
        b.configure()
        b.track("onboarding_step", mapOf("step_name" to "age_gate"))
        val sent = t.seen.single()
        assertEquals("https://analytics.example.org/capture/", sent.url)
        val json = Json.parseToJsonElement(sent.body.decodeToString()).jsonObject
        assertEquals(setOf("api_key", "event", "distinct_id", "properties"), json.keys)
        assertEquals("phc_testtoken", json["api_key"]!!.jsonPrimitive.content)
        val props = json["properties"]!!.jsonObject
        assertEquals(setOf("step_name", "\$process_person_profile", "\$geoip_disable"), props.keys)
        assertEquals("false", props["\$process_person_profile"]!!.jsonPrimitive.content)
        assertEquals("true", props["\$geoip_disable"]!!.jsonPrimitive.content)
    }

    // @verifies REQ-SW03-25#c
    @Test fun theIdIsKeptAcrossRestartsAndRemadeOnReset() {
        val store = InMemoryKeyValueStore()
        val a = PostHogAnalyticsBackend(store, Fake(), config, scope()).also { it.configure() }
        val first = a.distinctId!!
        assertEquals(first, PostHogAnalyticsBackend(store, Fake(), config, scope()).also { it.configure() }.distinctId)
        a.reset()
        assertNull(a.distinctId, "no id to erase when nothing is active")
        val b = PostHogAnalyticsBackend(store, Fake(), config, scope()).also { it.configure() }
        assertNotEquals(first, b.distinctId)
        assertFalse(store.getString(PostHogAnalyticsBackend.ID_KEY).isNullOrEmpty())
    }

    // @verifies REQ-SW03-20
    @Test fun theHostRuleAcceptsAPlainHostAndRefusesEverythingElse() {
        assertEquals("https://analytics.example.org", AnalyticsHost.baseUrl("analytics.example.org"))
        assertEquals("https://localhost:8000", AnalyticsHost.baseUrl("localhost:8000"))
        for (bad in listOf(null, "", "http://x.org", "https://x.org", "x.org/path", "x", ".x.org", "x.org.", "a..b.org", "x.org:0", "x.org:99999", "x.org:80:90", "\$(ANALYTICS_HOST)", "x y.org")) {
            assertNull(AnalyticsHost.baseUrl(bad), bad.toString())
        }
    }
}
