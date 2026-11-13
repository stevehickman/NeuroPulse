package life.neurone.shared.ui

import androidx.compose.runtime.Composable
import androidx.compose.ui.text.intl.Locale
import life.neurone.shared.i18n.PluralCategory
import life.neurone.shared.i18n.pluralCategory
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource

/**
 * One plural family: the resource for each CLDR category. Compose Multiplatform 1.7 has no plural resources, so
 * `sync-locales --compose-res` writes each family as flat `<name>_one` ... `<name>_other` strings, and every locale
 * declares every category it has (an untranslated one reads as that locale's `_other`). `zero`, `two`, `few` and
 * `many` are null where a caller has no resource for them; the category then reads as `other`.
 */
class PluralForms(
    val one: StringResource,
    val other: StringResource,
    val zero: StringResource? = null,
    val two: StringResource? = null,
    val few: StringResource? = null,
    val many: StringResource? = null,
) {
    fun resourceFor(category: PluralCategory): StringResource = when (category) {
        PluralCategory.ZERO -> zero
        PluralCategory.ONE -> one
        PluralCategory.TWO -> two
        PluralCategory.FEW -> few
        PluralCategory.MANY -> many
        PluralCategory.OTHER -> other
    } ?: other
}

/**
 * A count with its noun, in the current locale's CLDR category (`OI-UI-KMP-08`, rules in [pluralCategory]).
 * The count is the first format argument.
 */
@Composable
fun pluralString(forms: PluralForms, count: Int, vararg args: Any): String =
    stringResource(forms.resourceFor(pluralCategory(Locale.current.language, count)), count, *args)

/** The English-shaped call, kept for families whose callers pass only `one` and `other`. */
@Composable
fun pluralString(one: StringResource, other: StringResource, count: Int, vararg args: Any): String =
    pluralString(PluralForms(one, other), count, *args)
