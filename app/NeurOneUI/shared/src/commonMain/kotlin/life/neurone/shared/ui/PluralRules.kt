package life.neurone.shared.ui

/** A CLDR plural category (`OI-UI-KMP-08`). */
enum class PluralCategory { ZERO, ONE, TWO, FEW, MANY, OTHER }

/**
 * The CLDR cardinal plural category of a whole-number count, for a language subtag (`ru`, not `ru-RU`).
 *
 * Written out for the eleven locales (`locales/`) because Compose Multiplatform 1.7 has no plural resources and
 * no CLDR data of its own. Counts here are integers, so only the integer operands of the CLDR rules apply
 * (`v = 0`, `f = 0`). A language with no rule here reads as English: one for exactly 1, else other, which is what
 * this function did before the other rules existed. `scripts/check-plural-rules.ts` diffs this table against the
 * runtime's `Intl.PluralRules` for every locale, so a rule that drifts fails CI instead of showing a wrong noun.
 */
fun pluralCategory(language: String, count: Int): PluralCategory {
    val n = if (count < 0) -(count.toLong()) else count.toLong()
    val exactMillions = n != 0L && n % 1_000_000L == 0L
    return when (language.lowercase()) {
        "ar" -> when {
            n == 0L -> PluralCategory.ZERO
            n == 1L -> PluralCategory.ONE
            n == 2L -> PluralCategory.TWO
            n % 100 in 3..10 -> PluralCategory.FEW
            n % 100 in 11..99 -> PluralCategory.MANY
            else -> PluralCategory.OTHER
        }
        "ru" -> when {
            n % 10 == 1L && n % 100 != 11L -> PluralCategory.ONE
            n % 10 in 2..4 && n % 100 !in 12..14 -> PluralCategory.FEW
            else -> PluralCategory.MANY
        }
        "fr" -> when {
            n == 0L || n == 1L -> PluralCategory.ONE
            exactMillions -> PluralCategory.MANY
            else -> PluralCategory.OTHER
        }
        "es", "ca" -> when {
            n == 1L -> PluralCategory.ONE
            exactMillions -> PluralCategory.MANY
            else -> PluralCategory.OTHER
        }
        "hi", "bn" -> if (n == 0L || n == 1L) PluralCategory.ONE else PluralCategory.OTHER
        "id", "zh" -> PluralCategory.OTHER
        else -> if (n == 1L) PluralCategory.ONE else PluralCategory.OTHER
    }
}
