package life.neurone.core.protocol

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/**
 * `zone` and `condition` top-level blocks, and the single namespace they
 * populate (NP-NPPS-REF-001 §1.6, §8, §9). Closes OI-NPPS-MOBILE-01: this
 * parser raised "Unexpected keyword" on both blocks until Rev 10, so the
 * shipped definition files could not be loaded at all.
 */
class NPNamespaceTests {

    private fun parse(text: String) = NPPSParser(NPPSLexer(text).tokenize()).parse()

    // MARK: the shipped library

    @Test
    fun shippedDefinitionFilesLoadIntoOneNamespace() {
        val ns = NPBundledProtocols.namespace
        assertTrue(ns.zones.isNotEmpty(), "00-zones.npps contributed no zones")
        assertTrue(ns.conditions.isNotEmpty(), "00-conditions.npps contributed no conditions")
        assertNotNull(ns.zones["Frontal Left"], "the predefined lobe zones must be present")
    }

    @Test
    fun everyShippedReferenceResolves() {
        // The point of the namespace: a protocol naming a zone or condition must
        // find it, whichever file defined it and in whatever order they loaded.
        assertEquals(
            emptyList(),
            validateNamespaceReferences(NPBundledProtocols.namespace),
            "every shipped protocol's zone and condition references must resolve",
        )
    }

    @Test
    fun definitionsAreNotLibraryItems() {
        val ns = NPBundledProtocols.namespace
        assertEquals(NPBundledProtocols.protocolFiles.size, ns.runnableEntries.size)
        assertTrue(ns.runnableEntries.none { it is NPProtocolEntry.Zone })
        assertTrue(ns.runnableEntries.none { it is NPProtocolEntry.Condition })
    }

    // MARK: zone blocks

    @Test
    fun zoneParsesSocketsAsASet() {
        val z = (parse("""zone "Z" { sockets: [5, 1, 5, 3] }""").single() as NPProtocolEntry.Zone).zone
        // Deduplicated and sorted, so two zones sharing a midline socket dose it once.
        assertEquals(listOf(1, 3, 5), z.sockets)
        assertTrue(!z.isPredefined, "a zone without an id is user-authored")
    }

    @Test
    fun zoneWithIdIsPredefined() {
        val z = (parse("""zone "Z" { id: "40000001-0000-0000-0000-000000000000" sockets: [1] }""")
            .single() as NPProtocolEntry.Zone).zone
        assertTrue(z.isPredefined)
    }

    @Test
    fun zoneRejectsIdsThatNameNoSocket() {
        for (bad in listOf("[0]", "[${SocketZones.MAX + 1}]", "[1.5]", "[true]", "[\"x\"]")) {
            val e = assertFailsWith<NPPSError>("sockets: $bad must not parse") {
                parse("""zone "Z" { sockets: $bad }""")
            }
            assertTrue(
                e.message.orEmpty().contains("not a socket"),
                "error should name the problem, got: ${e.message}",
            )
        }
    }

    @Test
    fun zoneTypeFilterParses() {
        val z = (parse("""zone "Z" { sockets: [1] types: [led_660, led_808] exclude_types: true }""")
            .single() as NPProtocolEntry.Zone).zone
        assertEquals(listOf("led_660", "led_808"), z.types)
        assertTrue(z.excludeTypes)
    }

    // MARK: condition blocks

    @Test
    fun conditionParses() {
        val c = (parse(
            """condition "MDD" { id: "41000004-0000-0000-0000-000000000000" """ +
                """link: "https://example.org/mdd" code: "6A70" }"""
        ).single() as NPProtocolEntry.Condition).condition
        assertEquals("MDD", c.name)
        assertEquals("https://example.org/mdd", c.link)
        assertEquals("6A70", c.code)
    }

    @Test
    fun conditionRequiresALink() {
        // Without a link the entry cannot do the one job it exists for.
        assertFailsWith<NPPSError> { parse("""condition "No Link" { code: "6A70" }""") }
    }

    // MARK: cross-file resolution

    @Test
    fun referencesResolveAcrossFilesInEitherOrder() {
        val protocolFirst = parse(
            """
            protocol "P" {
                conditions: ["C"]
                pbm_transcranial { zones: ["Z"] }
            }
            """.trimIndent()
        )
        val definitionsSecond = parse(
            """
            zone "Z" { sockets: [1, 2] }
            condition "C" { link: "https://example.org/c" }
            """.trimIndent()
        )
        // Definition order and file boundaries must not matter: the whole tree
        // loads before anything is resolved.
        val ns = buildNamespace(protocolFirst + definitionsSecond).namespace
        assertEquals(emptyList(), validateNamespaceReferences(ns))
    }

    @Test
    fun unresolvedReferencesAreReportedNotThrown() {
        val ns = buildNamespace(
            parse(
                """
                protocol "P" {
                    conditions: ["Missing Condition"]
                    pbm_transcranial { zones: ["Missing Zone"] }
                }
                """.trimIndent()
            )
        ).namespace
        val errors = validateNamespaceReferences(ns)
        assertEquals(2, errors.size, "both references are unresolved: $errors")
        assertTrue(errors.any { it.contains("Missing Condition") })
        assertTrue(errors.any { it.contains("Missing Zone") })
    }

    @Test
    fun duplicateNamesWarnAndLastWins() {
        val build = buildNamespace(
            parse(
                """
                zone "Z" { sockets: [1] }
                zone "Z" { sockets: [2] }
                """.trimIndent()
            )
        )
        assertEquals(listOf(2), build.namespace.zones["Z"]?.sockets)
        assertTrue(build.warnings.any { it.contains("Duplicate zone name 'Z'") })
    }

    // MARK: protocol conditions / references

    @Test
    fun protocolConditionsAndReferencesParseAndRoundTrip() {
        val script = """
            protocol "P" {
                version: "1.0"
                conditions: ["Major Depressive Disorder"]
                references: ["https://doi.org/10.1000/x", ["Cassano 2018", "https://doi.org/10.1089/y"]]
            }
        """.trimIndent()
        val p = (parse(script).single() as NPProtocolEntry.Single).protocol
        assertEquals(listOf("Major Depressive Disorder"), p.conditions)
        assertEquals(2, p.references.size)
        assertEquals("Cassano 2018", p.references[1].label)
        assertEquals("https://doi.org/10.1089/y", p.references[1].url)

        val out = NPPSSerializer().serialize(NPProtocolEntry.Single(p))
        val reparsed = (parse(out).single() as NPProtocolEntry.Single).protocol
        assertEquals(p.conditions, reparsed.conditions)
        assertEquals(p.references, reparsed.references)
    }

    // MARK: serialization

    @Test
    fun zoneAndConditionRoundTrip() {
        val text = """
            zone "Crown" { sockets: [11, 16, 21] types: [led_660] }
            condition "C" { link: "https://example.org/c" code: "6A70" }
        """.trimIndent()
        val entries = parse(text)
        val s = NPPSSerializer()
        val out = entries.joinToString("\n\n") { s.serialize(it) }
        val again = parse(out)
        assertEquals(entries, again, "zone/condition blocks must survive serialize -> parse")
    }
}
