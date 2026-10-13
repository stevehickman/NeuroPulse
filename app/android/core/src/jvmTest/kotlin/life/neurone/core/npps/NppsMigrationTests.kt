package life.neurone.core.npps

import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonObject
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue

/**
 * The migration rules through the JNI binding (`SWR-MG`). The Rust tests hold the rules and the claim race; this holds
 * the layer between: the request builder, string marshalling and exception mapping.
 */
class NppsMigrationTests {
    private fun ask(op: String, build: kotlinx.serialization.json.JsonObjectBuilder.() -> Unit = {}): JsonObject =
        NppsCore.migration(buildJsonObject { put("op", op); build() })

    // @verifies SWR-MG-05
    @Test
    fun aNewerBundleIsRefusedWholeAndTheKindIsReadFirst() {
        val newer = ask("readHeader") { putJsonObject("header") { put("kind", "migrate"); put("formatVersion", 99) } }
        assertEquals(false, newer["ok"]!!.jsonPrimitive.boolean)
        assertEquals("newer_version", newer["reason"]!!.jsonPrimitive.content)
        val ok = ask("readHeader") { putJsonObject("header") { put("kind", "uhdr"); put("formatVersion", 1) } }
        assertEquals("uhdr", ok["kind"]!!.jsonPrimitive.content)
    }

    // @verifies SWR-MG-03
    @Test
    fun theNightlyKindNamesWhatItLeavesOut() {
        val inv = ask("inventory") { put("kind", "uhdr") }
        val omitted = inv["omitted"]!!.jsonArray.map { it.jsonPrimitive.content }
        assertTrue("protocols" in omitted && "limitsProfiles" in omitted)
        assertTrue("withdrawalState" in inv["carried"]!!.jsonArray.map { it.jsonPrimitive.content })
    }

    // @verifies SWR-MG-04
    @Test
    fun aWithdrawalSurvivesARestoreAndNeverAskedStaysNull() {
        val merged = ask("mergeWithdrawal") {
            putJsonObject("local") {}
            putJsonObject("incoming") { put("blanketConsentWithdrawnAt", "2026-03-01T00:00:00Z") }
        }
        assertEquals("2026-03-01T00:00:00Z", merged["blanketConsentWithdrawnAt"]!!.jsonPrimitive.content)
        val never = ask("mergeWithdrawal") { putJsonObject("local") {}; putJsonObject("incoming") {} }
        assertEquals(JsonNull, never["blanketConsentWithdrawnAt"])
    }

    // @verifies SWR-MG-08
    @Test
    fun theOldPhoneClearsOnlyOnAMatchingConfirmation() {
        val none = ask("clearDecision") { put("bundleHash", "h"); put("confirmation", JsonNull) }
        assertEquals(false, none["clear"]!!.jsonPrimitive.boolean)
        val ok = ask("clearDecision") { put("bundleHash", "h"); putJsonObject("confirmation") { put("bundleHash", "h") } }
        assertEquals(true, ok["clear"]!!.jsonPrimitive.boolean)
    }

    // @verifies-none a harness self-check of the exception mapping, not a requirement
    @Test
    fun aRequestTheCoreCannotReadIsAnIllegalArgumentException() {
        assertFailsWith<IllegalArgumentException> { ask("nope") }
    }
}
