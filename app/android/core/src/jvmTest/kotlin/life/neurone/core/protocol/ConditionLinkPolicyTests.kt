package life.neurone.core.protocol

import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/**
 * Condition link policy — NP-COND-LINK-001 §3.
 *
 * Driven by `npps/fixtures/condition_links.json`, the same vector file the
 * TypeScript, Swift, and C# suites run. A verdict that differs here from the
 * other platforms is a parity break, not a local test failure.
 *
 * The vector file is read with a minimal hand-rolled reader so `core` keeps no
 * JSON dependency for tests.
 */
class ConditionLinkPolicyTests {

    private data class Vector(
        val name: String,
        val url: String,
        val verdict: String,
        val reason: String,
        val host: String?,
    )

    private companion object {

        /** Walk up from the module dir to the repo root holding `npps/`. */
        fun repoRoot(): File {
            var dir = File(System.getProperty("user.dir")).absoluteFile
            while (!File(dir, "npps/fixtures/condition_links.json").exists()) {
                dir = dir.parentFile
                    ?: error("could not locate npps/fixtures from ${System.getProperty("user.dir")}")
            }
            return dir
        }

        fun loadVectors(): List<Vector> {
            val text = File(repoRoot(), "npps/fixtures/condition_links.json").readText()
            val body = text.substringAfter("\"vectors\"").substringAfter('[')

            return splitObjects(body).mapNotNull { obj ->
                val name = stringField(obj, "name") ?: return@mapNotNull null
                val url = stringField(obj, "url") ?: return@mapNotNull null
                val verdict = stringField(obj, "verdict") ?: return@mapNotNull null
                val reason = stringField(obj, "reason") ?: return@mapNotNull null
                Vector(name, url, verdict, reason, stringField(obj, "host"))
            }
        }

        /** Split the vectors array into its top-level `{ ... }` objects. */
        fun splitObjects(body: String): List<String> {
            val objects = mutableListOf<String>()
            var depth = 0
            var start = -1
            var inString = false
            var escaped = false

            for ((i, c) in body.withIndex()) {
                when {
                    escaped -> escaped = false
                    c == '\\' && inString -> escaped = true
                    c == '"' -> inString = !inString
                    inString -> {}
                    c == '{' -> { if (depth == 0) start = i; depth++ }
                    c == '}' -> {
                        depth--
                        if (depth == 0 && start >= 0) objects.add(body.substring(start, i + 1))
                    }
                    c == ']' && depth == 0 -> return objects
                }
            }
            return objects
        }

        /** Read `"key": "value"` honouring \\n, \\t, \\" and \\\\ escapes. */
        fun stringField(obj: String, key: String): String? {
            val marker = "\"$key\""
            val at = obj.indexOf(marker)
            if (at == -1) return null
            var i = obj.indexOf('"', obj.indexOf(':', at + marker.length) + 1)
            if (i == -1) return null
            i++

            val sb = StringBuilder()
            while (i < obj.length) {
                val c = obj[i]
                if (c == '\\') {
                    when (val esc = obj[i + 1]) {
                        'n' -> sb.append('\n')
                        't' -> sb.append('\t')
                        'r' -> sb.append('\r')
                        'u' -> {
                            sb.append(obj.substring(i + 2, i + 6).toInt(16).toChar())
                            i += 4
                        }
                        else -> sb.append(esc)
                    }
                    i += 2
                } else if (c == '"') {
                    return sb.toString()
                } else {
                    sb.append(c)
                    i++
                }
            }
            return null
        }
    }

    // MARK: - Shared cross-platform vectors

    @Test
    fun sharedVectorsMatch() {
        val vectors = loadVectors()
        assertTrue(vectors.size > 25, "shared vector file looks truncated: ${vectors.size}")

        for (v in vectors) {
            val got = NPConditionLinkPolicy.evaluate(v.url)
            assertEquals(v.verdict == "allow", got.allowed, "verdict mismatch for ${v.name}")
            assertEquals(v.reason, got.reason.wireName, "reason mismatch for ${v.name}")
            if (v.host != null) assertEquals(v.host, got.host, "host mismatch for ${v.name}")
        }
    }

    @Test
    fun blockedVerdictsCarryNoUrlOrHost() {
        for (v in loadVectors().filter { it.verdict == "block" }) {
            val got = NPConditionLinkPolicy.evaluate(v.url)
            assertNull(got.url, "blocked link must not expose a URL: ${v.name}")
            assertNull(got.host, "blocked link must not expose a host: ${v.name}")
        }
    }

    @Test
    fun allowedVerdictsCarryTrimmedUrl() {
        for (v in loadVectors().filter { it.verdict == "allow" }) {
            assertEquals(v.url.trim(), NPConditionLinkPolicy.evaluate(v.url).url, v.name)
        }
    }

    @Test
    fun everyBlockingReasonHasAMessage() {
        val reasons = loadVectors().filter { it.verdict == "block" }.map { it.reason }.toSet()
        for (raw in reasons) {
            val reason = NPLinkReason.fromWireName(raw)
            assertNotNull(reason, "unknown reason in vectors: $raw")
            assertTrue(reason.blockedMessage.isNotEmpty(), "no message for $raw")
        }
        assertTrue(NPLinkReason.OK.blockedMessage.isEmpty())
    }

    // MARK: - The shipped registry

    @Test
    fun bundledRegistryIsPopulatedAndUnique() {
        assertTrue(NPBundledConditions.all.size > 20)
        assertEquals(
            NPBundledConditions.all.size,
            NPBundledConditions.byName.size,
            "duplicate condition names in the generated registry",
        )
    }

    @Test
    fun everyBundledLinkPassesPolicy() {
        val rejected = NPBundledConditions.all
            .filter { !NPConditionLinkPolicy.evaluate(it.link).allowed }
            .map { it.name }

        assertEquals(emptyList(), rejected, "bundled conditions must all have openable links")
    }

    /**
     * The generated table must not drift from the .npps registry. Guards the
     * case where someone edits 00-conditions.npps without re-running
     * scripts/sync-conditions.ts.
     */
    @Test
    fun generatedTableMatchesRegistrySource() {
        val source = File(repoRoot(), "protocols/predefined/00-conditions.npps").readText()
        val blockCount = source.split("\ncondition ").size - 1

        assertEquals(
            blockCount,
            NPBundledConditions.all.size,
            "generated table is stale — run: bun scripts/sync-conditions.ts",
        )
        for (c in NPBundledConditions.all) {
            assertTrue(source.contains(c.link), "${c.name} link missing from registry — table is stale")
        }
    }

    // MARK: - Resolution

    @Test
    fun resolvePreservesOrderAndArity() {
        val names = listOf("Insomnia", "Not In Registry", "Migraine")
        val resolved = NPBundledConditions.resolve(names)

        assertEquals(names, resolved.map { it.name })
        assertNotNull(resolved[0].definition)
        assertTrue(resolved[0].verdict.allowed)
        assertNull(resolved[1].definition)
        assertTrue(!resolved[1].verdict.allowed)
        assertNotNull(resolved[2].definition)
    }

    @Test
    fun resolveKnownConditionExposesCodeAndHost() {
        val resolved = NPBundledConditions.resolve(listOf("Major Depressive Disorder")).single()
        assertEquals("6A70", resolved.definition?.code)
        assertEquals("en.wikipedia.org", resolved.verdict.host)
    }

    // MARK: - Spot checks independent of the vector file

    @Test
    fun javaScriptSchemeIsRefused() {
        assertEquals(
            NPLinkReason.SCHEME_NOT_HTTPS,
            NPConditionLinkPolicy.evaluate("javascript:alert(1)").reason,
        )
    }

    @Test
    fun hostSpoofViaUserinfoIsRefused() {
        val v = NPConditionLinkPolicy.evaluate("https://en.wikipedia.org@evil.example.com/x")
        assertEquals(NPLinkReason.EMBEDDED_CREDENTIALS, v.reason)
        assertNull(v.host, "must never surface a spoofable host")
    }
}
