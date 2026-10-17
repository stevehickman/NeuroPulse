package life.neurone.shared

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import life.neurone.core.migration.Link
import life.neurone.shared.migration.LinkReceiveModel
import life.neurone.shared.migration.LinkReceiveStep
import life.neurone.shared.migration.LanLinkTransport
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.withTimeout
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.put
import life.neurone.core.common.InMemoryKeyValueStore
import life.neurone.core.data.BundleKeyDerivation
import life.neurone.core.data.BundleSecretResult
import life.neurone.core.migration.LINK_ROUTE
import life.neurone.core.migration.LinkReceiveSession
import life.neurone.core.migration.LinkSendSession
import life.neurone.core.migration.LinkSendState
import life.neurone.core.migration.MemoryObjectStore
import life.neurone.core.migration.MigrationFlow
import life.neurone.core.migration.StandardMigrationHost
import life.neurone.core.migration.toJson
import kotlin.test.assertIs
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

// Real sockets on the loopback address: the broadcast address is the network's to route and a sandbox may not, so the
// looking phone is pointed at 127.0.0.1 and a private discovery port, which exercises everything but the broadcast itself.
class LanLinkTransportTest {
    private val port = 48123

    private suspend fun readAll(link: Link, count: Int): ByteArray {
        var out = ByteArray(0)
        while (out.size < count) out += link.receive() ?: break
        return out
    }

    // @verifies SWR-MG-10
    @Test
    fun twoPhonesFindEachOtherAndCarryBytesBothWaysUntilOneHangsUp() = runBlocking {
        val waiting = LanLinkTransport(listOf("127.0.0.1"), port, lookSeconds = 10)
        val looking = LanLinkTransport(listOf("127.0.0.1"), port, lookSeconds = 10)
        val accepted = async(Dispatchers.Default) { waiting.accept() }
        delay(500)
        val client = assertNotNull(looking.connect(), "the looking phone found the waiting one")
        val server = assertNotNull(accepted.await())
        client.send(byteArrayOf(1, 2, 3))
        assertContentEquals(byteArrayOf(1, 2, 3), readAll(server, 3))
        val big = ByteArray(300_000) { (it * 31).toByte() }
        server.send(big)
        assertContentEquals(big, readAll(client, big.size))
        client.close()
        assertNull(server.receive(), "a hung-up link reads as closed")
        server.close()
    }

    // @verifies SWR-MG-10
    @Test
    fun aPhoneThatFindsNobodyGivesUpWithNoLink() = runBlocking {
        val looking = LanLinkTransport(listOf("127.0.0.1"), port + 1, lookSeconds = 1)
        assertNull(looking.connect())
    }

    // @verifies SWR-MG-10
    @Test
    fun theDiscoveryValuesAreTheCores() {
        val d = life.neurone.core.migration.MigrationFlow.linkDiscovery()
        assertEquals("47731", d["port"].toString())
    }

    // @verifies SWR-MG-10
    // @verifies SWR-MG-07
    @Test
    fun aMigrationRunsBetweenTwoSessionsOverRealSockets() = runBlocking<Unit> {
        val old = InMemoryKeyValueStore().apply { putString("np.session.history", "history-x") }
        val store = MemoryObjectStore()
        val host = StandardMigrationHost()
        val params = buildJsonObject {
            put("route", LINK_ROUTE.toJson())
            put("hostItems", JsonArray(emptyList()))
            put("kind", "migrate")
            put("method", "passphrase")
        }
        val secret = (BundleKeyDerivation.passphraseSecret("a long enough passphrase") as BundleSecretResult.Ok).secret
        assertEquals("true", MigrationFlow.run("backup", params, host, secret, old, store)["ok"].toString())

        val sending = LinkSendSession(store, LanLinkTransport(listOf("127.0.0.1"), port + 2, lookSeconds = 10))
        val sent = async(Dispatchers.Default) { sending.run() }
        val code = withTimeout(10_000) { sending.step.first { it is LinkSendState.Waiting } as LinkSendState.Waiting }.display
        delay(500)
        val fresh = InMemoryKeyValueStore()
        val done = LinkReceiveSession(LanLinkTransport(listOf("127.0.0.1"), port + 2, lookSeconds = 10), fresh)
            .run(code) { (BundleKeyDerivation.passphraseSecret("a long enough passphrase") as BundleSecretResult.Ok).secret }
        assertEquals("true", done["ok"].toString(), "$done")
        assertEquals("history-x", fresh.getString("np.session.history"))
        assertIs<LinkSendState.Sent>(withTimeout(10_000) { sent.await() })
    }

    // @verifies SWR-MG-10
    @Test
    fun theNewPhonesModelAsksForTheSecretOnlyOncePairedAndTellsWhatWasRestored() = runBlocking<Unit> {
        val old = InMemoryKeyValueStore().apply { putString("np.session.history", "history-y") }
        val store = MemoryObjectStore()
        val params = buildJsonObject {
            put("route", LINK_ROUTE.toJson())
            put("hostItems", JsonArray(emptyList()))
            put("kind", "migrate")
            put("method", "passphrase")
        }
        val secret = (BundleKeyDerivation.passphraseSecret("a long enough passphrase") as BundleSecretResult.Ok).secret
        MigrationFlow.run("backup", params, StandardMigrationHost(), secret, old, store)

        val sending = LinkSendSession(store, LanLinkTransport(listOf("127.0.0.1"), port + 3, lookSeconds = 10))
        val sent = async(Dispatchers.Default) { sending.run() }
        val code = withTimeout(10_000) { sending.step.first { it is LinkSendState.Waiting } as LinkSendState.Waiting }.display
        delay(500)
        val fresh = InMemoryKeyValueStore()
        val model = LinkReceiveModel(LanLinkTransport(listOf("127.0.0.1"), port + 3, lookSeconds = 10), fresh)
        val running = async(Dispatchers.Default) { model.submitCode(code) }
        val asked = withTimeout(20_000) { model.step.first { it is LinkReceiveStep.EnterSecret } as LinkReceiveStep.EnterSecret }
        assertEquals(life.neurone.core.data.BundleKeyMethod.PASSPHRASE, asked.method)
        model.submitSecret("short")
        val again = withTimeout(10_000) { model.step.first { it is LinkReceiveStep.EnterSecret && it.problem != null } as LinkReceiveStep.EnterSecret }
        assertNotNull(again.problem, "a secret the core refuses as too short is asked for again")
        model.submitSecret("a long enough passphrase")
        running.await()
        val done = assertIs<LinkReceiveStep.Done>(model.step.value)
        assertEquals(true, done.report.confirmed)
        assertEquals("history-y", fresh.getString("np.session.history"))
        assertIs<LinkSendState.Sent>(withTimeout(10_000) { sent.await() })
    }
}
