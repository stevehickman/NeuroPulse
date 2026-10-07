package life.neurone.shared.ui

import androidx.compose.runtime.Composable
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource

/**
 * A count with its noun. Compose Multiplatform 1.7 has no plural resources, so `sync-locales --compose-res` writes
 * each plural family as flat `<name>_one` / `<name>_other` strings, and this picks between them. Every locale's
 * plural text is English today, whose rule is "one, else other"; when a locale gets real plural forms
 * (Arabic, Russian) this is where its category rule goes (OI-UI-KMP-08). The count is the first argument.
 */
@Composable
fun pluralString(one: StringResource, other: StringResource, count: Int, vararg args: Any): String =
    stringResource(if (count == 1) one else other, count, *args)
