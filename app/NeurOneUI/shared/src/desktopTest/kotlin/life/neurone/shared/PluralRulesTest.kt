package life.neurone.shared

import life.neurone.shared.ui.PluralCategory
import life.neurone.shared.ui.pluralCategory
import kotlin.test.Test
import kotlin.test.assertEquals

/** The shared UI picks the plural category by CLDR rule, for every locale (`REQ-SW03-09`, `OI-UI-KMP-08`). */
class PluralRulesTest {

    // @verifies REQ-SW03-09
    @Test
    fun everyLocaleChoosesTheCategoryTheCldrGives() {
        assertEquals(11, pluralGoldens.size, "the goldens cover the eleven locales")
        for ((tag, rows) in pluralGoldens) {
            val language = tag.substringBefore('-')
            for ((count, expected) in rows) {
                assertEquals(
                    expected,
                    pluralCategory(language, count).name.lowercase(),
                    "$tag, count $count",
                )
            }
        }
    }

    // @verifies REQ-SW03-09
    @Test
    fun aLanguageWithNoRuleReadsAsEnglish() {
        assertEquals(PluralCategory.ONE, pluralCategory("xx", 1))
        assertEquals(PluralCategory.OTHER, pluralCategory("xx", 5))
    }
}
