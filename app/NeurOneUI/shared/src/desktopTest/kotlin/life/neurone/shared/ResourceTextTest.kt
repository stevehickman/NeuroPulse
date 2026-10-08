package life.neurone.shared

import androidx.compose.ui.test.ExperimentalTestApi
import androidx.compose.ui.test.runComposeUiTest
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.stringResource
import kotlin.test.Test
import kotlin.test.assertEquals

/**
 * What the generated Compose resources say when they are read, for the characters the Android string format
 * escapes: an apostrophe, a quote, a literal percent sign and a newline. `sync-locales --compose-res` writes
 * them for Compose Multiplatform's own compiler, which decodes less than Android's, and the first build showed
 * "hub\'s" on screen. This pins what a user reads.
 */
@OptIn(ExperimentalTestApi::class)
class ResourceTextTest {

    @Test
    fun specialCharactersReadAsWritten() = runComposeUiTest {
        var apostrophe = ""
        var quote = ""
        var percent = ""
        var percentTwice = ""
        var newline = ""
        setContent {
            apostrophe = stringResource(Res.string.and_ui_view_your_hub_s_firmware_version_and_update)
            quote = stringResource(Res.string.protocol_menu_are_you_sure_you_want_to_delete, "Alpha")
            percent = stringResource(Res.string.ota_progress_percent, "50")
            percentTwice = stringResource(Res.string.validate_msg_pbm_cw_duty, "25")
            newline = stringResource(Res.string.age_neurone_life_biometric_policy)
        }
        waitForIdle()
        assertEquals("View your hub's firmware version and update status.", apostrophe)
        assertEquals("Are you sure you want to delete \"Alpha\"? This cannot be undone.", quote)
        assertEquals("50%", percent)
        assertEquals(
            "PBM at 0 Hz is continuous wave, which has no duty cycle, but the duty is 25%. Set it to 100% or give a pulse frequency above 0.",
            percentTwice,
        )
        assertEquals('\n', newline.last())
    }
}
