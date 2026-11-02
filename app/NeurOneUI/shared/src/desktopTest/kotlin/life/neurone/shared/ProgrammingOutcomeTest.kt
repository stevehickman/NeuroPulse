package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.core.models.HelmetRefusalCause
import life.neurone.core.models.UploadCheck
import life.neurone.core.models.UploadCheckStatus
import life.neurone.core.models.UploadVerdict
import life.neurone.shared.session.ProgrammingOutcome
import life.neurone.shared.ui.helmetRefusalMessages
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** What the person is told after a Mode 3 programming, and the message for every reason the helmet can answer with (ported from the iOS `ProtocolUploadFlowTests`). */
@OptIn(ExperimentalTestApi::class)
class ProgrammingOutcomeTest {

    // @verifies SWR-UP-02
    @Test fun a_protocol_is_reported_stored_only_when_the_helmet_holds_it_and_every_refusal_is_said_instead() {
        assertEquals(ProgrammingOutcome.Stored, ProgrammingOutcome.of(null), "accepted, or no answer at all")
        for (cause in HelmetRefusalCause.entries) {
            assertEquals(ProgrammingOutcome.Refused(cause), ProgrammingOutcome.of(cause), "$cause is not stored")
        }
    }

    // @verifies SWR-UP-02
    @Test fun the_verdict_of_a_report_gives_stored_for_an_accepted_upload_and_a_cause_for_a_refusal_or_for_other_bytes() {
        fun verdict(status: String, reason: Int?, hash: String) = UploadVerdict(status, hash, null, reason)
        val sent = "aa"
        val other = "bb"
        assertEquals(ProgrammingOutcome.Stored, ProgrammingOutcome.of(verdict("accepted", null, sent).causeFor(sent)))
        // One reason from every group, including the ones that are not about the tier or the hardware version.
        for (reason in listOf(19, 20, 27, 28, 6, 7, 5, 4, 2, 13, 25, 99)) {
            val outcome = ProgrammingOutcome.of(verdict("refused", reason, sent).causeFor(sent))
            assertTrue(outcome is ProgrammingOutcome.Refused, "reason $reason: Mode 3 must not report the protocol stored")
        }
        // A final answer for other bytes says nothing about this upload, accepted or refused.
        assertEquals(ProgrammingOutcome.Refused(HelmetRefusalCause.UNMATCHED), ProgrammingOutcome.of(verdict("accepted", null, other).causeFor(sent)))
        assertEquals(ProgrammingOutcome.Refused(HelmetRefusalCause.UNMATCHED), ProgrammingOutcome.of(verdict("refused", 6, other).causeFor(sent)))
    }

    // @verifies SWR-HV-08
    @Test fun every_cause_has_its_own_message_that_is_not_a_key() = runComposeUiTest {
        var messages: Map<HelmetRefusalCause, String> = emptyMap()
        setContent { messages = helmetRefusalMessages() }
        assertEquals(HelmetRefusalCause.entries.toSet(), messages.keys, "a message for every cause")
        messages.forEach { (cause, text) ->
            assertTrue(text.isNotBlank(), "$cause")
            assertTrue(!text.startsWith("PROTOCOL_MENU_") && !text.startsWith("protocol_menu_"), "$cause shows a key: $text")
        }
        assertEquals(messages.size, messages.values.toSet().size, "each cause says something different")
    }
}
