package life.neurone.shared

import life.neurone.shared.ui.ALL_SOCKET_REASONS
import life.neurone.shared.ui.CommandOutcome
import life.neurone.shared.ui.CutoffReason
import life.neurone.shared.ui.HelmetSocket
import life.neurone.shared.ui.HelmetState
import life.neurone.shared.ui.HubStatusReason
import life.neurone.shared.ui.LineVerdict
import life.neurone.shared.ui.SafetyStatusBits
import life.neurone.shared.ui.SocketDrawState
import life.neurone.shared.ui.WithheldReason
import life.neurone.shared.ui.cutoffReasonOf
import life.neurone.shared.ui.drawStateOf
import life.neurone.shared.ui.helmetReasonRows
import life.neurone.shared.ui.reasonOf
import life.neurone.shared.ui.withheldReasonOf
import java.io.File
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** The reason beside each helmet state (REQ-SIMFW-18#d): typed, one label per reason in every locale, and never a cause nothing reported. */
class HelmetReasonTest {
    private fun sock(
        id: Int = 1,
        commanded: Boolean = true,
        line: LineVerdict = LineVerdict.NOT_IN_LOOP,
        fault: Boolean = false,
        cutoff: Boolean = false,
        outcome: CommandOutcome = CommandOutcome.NONE,
        reason: life.neurone.shared.ui.SocketReason? = null,
    ) = HelmetSocket(id, true, commanded, line, fault, cutoff, outcome, reason)

    private fun repoRoot(): File {
        var d: File? = File(System.getProperty("user.dir")).absoluteFile
        while (d != null && !File(d, "locales/en.json").exists()) d = d.parentFile
        return assertNotNull(d, "the repository root (the directory holding locales/en.json) was not found above ${System.getProperty("user.dir")}")
    }

    // @verifies REQ-SIMFW-18#d
    @Test
    fun aRefusedWriteFailedOrFaultedSocketShowsTheHubStatusItWasGiven() {
        val refused = sock(outcome = CommandOutcome.REFUSED, reason = HubStatusReason.POWER_BUDGET)
        assertEquals(SocketDrawState.REFUSED, drawStateOf(refused))
        assertEquals(HubStatusReason.POWER_BUDGET, reasonOf(refused))
        val failed = sock(outcome = CommandOutcome.WRITE_FAILED, reason = HubStatusReason.TIMEOUT)
        assertEquals(SocketDrawState.WRITE_FAILED, drawStateOf(failed))
        assertEquals(HubStatusReason.TIMEOUT, reasonOf(failed))
        val faulted = sock(fault = true, reason = HubStatusReason.MODULE_FAULT)
        assertEquals(SocketDrawState.FAULT, drawStateOf(faulted))
        assertEquals(HubStatusReason.MODULE_FAULT, reasonOf(faulted))
    }

    // @verifies REQ-SIMFW-18#d
    @Test
    fun aWithheldAndACutOffSocketShowTheSafetyMcusReason() {
        val withheld = sock(line = LineVerdict.DISABLED, reason = WithheldReason.CONTACT_CHECK_FAILED)
        assertEquals(SocketDrawState.WITHHELD, drawStateOf(withheld))
        assertEquals(WithheldReason.CONTACT_CHECK_FAILED, reasonOf(withheld))
        val cut = sock(line = LineVerdict.DISABLED, cutoff = true, reason = CutoffReason.THERMAL)
        assertEquals(SocketDrawState.CUT_OFF, drawStateOf(cut))
        assertEquals(CutoffReason.THERMAL, reasonOf(cut))
    }

    // @verifies REQ-SIMFW-18#d
    @Test
    fun noReasonOrOneOfTheWrongKindIsShownAsNotReportedNeverAsACauseThatDoesNotApply() {
        // A thermal cutoff handed to a socket that is merely withheld is not a reason it can show.
        val withheld = sock(line = LineVerdict.DISABLED, reason = CutoffReason.THERMAL)
        assertEquals(WithheldReason.UNREPORTED, reasonOf(withheld))
        assertEquals(CutoffReason.UNREPORTED, reasonOf(sock(line = LineVerdict.DISABLED, cutoff = true)))
        assertEquals(HubStatusReason.UNREPORTED, reasonOf(sock(outcome = CommandOutcome.REFUSED, reason = WithheldReason.AWAITING_SESSION_SIGNATURE)))
        assertEquals(HubStatusReason.UNREPORTED, reasonOf(sock(fault = true)))
    }

    // @verifies REQ-SIMFW-18#d
    @Test
    fun theStatesWithNothingWrongShowNoReasonWhateverIsSupplied() {
        val any = HubStatusReason.POWER_BUDGET
        assertNull(reasonOf(HelmetSocket(1, fitted = false, reason = any)))
        assertNull(reasonOf(sock(commanded = false, reason = any)))
        assertNull(reasonOf(sock(line = LineVerdict.NOT_IN_LOOP, reason = any)))
        assertNull(reasonOf(sock(line = LineVerdict.ENABLED, reason = any)))
        assertNull(reasonOf(sock(line = LineVerdict.UNGATED, reason = any)))
    }

    // @verifies REQ-SIMFW-18#d
    @Test
    fun aHubStatusCodeBecomesOneReasonAndACodeTheTableDoesNotNameBecomesNotReported() {
        val named = HubStatusReason.entries.filter { it != HubStatusReason.UNREPORTED }
        assertEquals(named.size, named.map { it.code }.toSet().size, "two reasons share a code")
        for (r in named) assertEquals(r, HubStatusReason.fromCode(r.code))
        assertEquals(HubStatusReason.POWER_BUDGET, HubStatusReason.fromCode(-18))
        assertEquals(HubStatusReason.DRIVER_INTERLOCK, HubStatusReason.fromCode(-10))
        assertEquals(HubStatusReason.UNREPORTED, HubStatusReason.fromCode(-99))
        assertEquals(HubStatusReason.UNREPORTED, HubStatusReason.fromCode(0), "a success code is not a reason")
    }

    // @verifies REQ-SIMFW-18#d
    @Test
    fun theWithheldReasonFollowsTheStatusByteInTheOrderDocumented() {
        assertEquals(WithheldReason.AWAITING_SESSION_SIGNATURE, withheldReasonOf(SafetyStatusBits.SIG_PENDING))
        assertEquals(WithheldReason.CONTACT_CHECK_FAILED, withheldReasonOf(SafetyStatusBits.IMPEDANCE))
        assertEquals(WithheldReason.AWAITING_SESSION_SIGNATURE, withheldReasonOf(SafetyStatusBits.SIG_PENDING or SafetyStatusBits.IMPEDANCE))
        assertEquals(WithheldReason.UNREPORTED, withheldReasonOf(0), "status OK says nothing about why a line is withheld")
    }

    // @verifies REQ-SIMFW-18#d
    @Test
    fun theCutoffReasonFollowsTheStatusByteAndTheFaultSlotInTheOrderDocumented() {
        assertEquals(CutoffReason.INDEPENDENT_WATCHDOG, cutoffReasonOf(SafetyStatusBits.THERMAL, null, independentWatchdogReset = true))
        assertEquals(CutoffReason.SESSION_SIGNATURE,
            cutoffReasonOf(SafetyStatusBits.FAULT or SafetyStatusBits.CUTOFF, SafetyStatusBits.SLOT_BAD_SESSION_SIGNATURE, false))
        assertEquals(CutoffReason.MCU_FAULT, cutoffReasonOf(SafetyStatusBits.FAULT or SafetyStatusBits.CUTOFF, 0xFE, false))
        assertEquals(CutoffReason.THERMAL, cutoffReasonOf(SafetyStatusBits.THERMAL, 0xFF, false))
        assertEquals(CutoffReason.CHARGE, cutoffReasonOf(SafetyStatusBits.CHARGE, null, false))
        assertEquals(CutoffReason.CARDIAC, cutoffReasonOf(SafetyStatusBits.CARDIAC, null, false))
        assertEquals(CutoffReason.HEARTBEAT_LOST, cutoffReasonOf(SafetyStatusBits.WATCHDOG, null, false))
        assertEquals(CutoffReason.CUTOFF_ONLY, cutoffReasonOf(SafetyStatusBits.CUTOFF, null, false))
        assertEquals(CutoffReason.UNREPORTED, cutoffReasonOf(0, null, false), "a drop no reply has explained yet")
    }

    // @verifies REQ-SIMFW-18#d
    @Test
    fun everyReasonNamesOneLocaleKeyAndEveryLocaleHasIt() {
        val keys = ALL_SOCKET_REASONS.map { it.key }
        assertEquals(keys.size, keys.toSet().size, "two reasons share a key")
        assertTrue(keys.all { it.startsWith("HELMET_REASON_") })
        val locales = File(repoRoot(), "locales").listFiles { f -> f.name.endsWith(".json") && !f.name.startsWith("_") }!!.toList()
        assertEquals(11, locales.size, "the eleven locales")
        for (locale in locales) {
            val text = locale.readText()
            for (k in keys + listOf("HELMET_REASONS_TITLE", "HELMET_REASON_SOCKETS")) {
                assertTrue("\"$k\":" in text, "${locale.name} has no $k")
            }
        }
    }

    // @verifies REQ-SIMFW-18#d
    @Test
    fun theViewResolvesEachReasonsKeyToItsResource() {
        // `reasonLabel` in HelmetView.kt is an exhaustive `when`, so a reason with no branch does not compile. What the compiler cannot
        // see is that a branch names the resource of the key its own value declares: that is held here.
        val src = File(repoRoot(), "app/NeurOneUI/shared/src/commonMain/kotlin/life/neurone/shared/ui/HelmetView.kt").readText()
        for (r in ALL_SOCKET_REASONS) {
            assertTrue("Res.string.${r.key.lowercase()}" in src, "HelmetView.kt does not use the resource of ${r.key}")
        }
    }

    // @verifies REQ-SIMFW-18#d
    @Test
    fun theReasonsListGroupsSocketsByStateAndReasonAndCountsThem() {
        val sockets = mapOf(
            1 to sock(id = 1, outcome = CommandOutcome.REFUSED, reason = HubStatusReason.POWER_BUDGET),
            2 to sock(id = 2, outcome = CommandOutcome.REFUSED, reason = HubStatusReason.POWER_BUDGET),
            3 to sock(id = 3, outcome = CommandOutcome.REFUSED, reason = HubStatusReason.DRIVER_INTERLOCK),
            4 to sock(id = 4, line = LineVerdict.DISABLED, cutoff = true, reason = CutoffReason.CARDIAC),
            5 to sock(id = 5, line = LineVerdict.ENABLED),
            6 to HelmetSocket(6, fitted = false),
        )
        val rows = helmetReasonRows(HelmetState(sockets, safetyInLoop = true))
        assertEquals(3, rows.size, "driven and absent sockets add no row")
        assertEquals(2, rows.single { it.reason == HubStatusReason.POWER_BUDGET }.count)
        assertEquals(1, rows.single { it.reason == HubStatusReason.DRIVER_INTERLOCK }.count)
        assertEquals(SocketDrawState.CUT_OFF, rows.single { it.reason == CutoffReason.CARDIAC }.state)
        assertTrue(rows.map { it.state.ordinal } == rows.map { it.state.ordinal }.sorted(), "rows follow the order of the states")
        assertTrue(helmetReasonRows(HelmetState.empty()).isEmpty(), "an empty helmet has nothing to explain")
    }
}
