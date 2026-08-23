package life.neurone.core.protocol

// =============================================================================
// NPPS lexer — Kotlin port of the Swift `NPPSLexer`.
//
// Locked bug fixes preserved (project docs/status/completed-decisions.md "NPPS lexer/serializer"):
//   (1) Compound digit-idents: after a number's unit-reading backtrack, if the
//       cursor sits at '_', read a compound ident consuming letters/digits/'_',
//       so `660_808nm` and `660_808_1064nm` lex as a single ident, not a number
//       followed by a spurious field key.
//   (2) Hyphenated tags: '-' after an ident enters the number branch ONLY when
//       immediately followed by a digit, so `wind-down` / `all-modalities` do
//       not throw "Invalid number: -".
//   (3) `mW_cm2` unit: the unit-reading loop accepts digits in addition to
//       letters and '_'.
// =============================================================================

/** Port of Swift `NPPSError` (message + 1-based line). */
class NPPSError(val messageText: String, val line: Int) :
    Exception("Line $line: $messageText")

/** Port of the Swift `NPPSToken` enum. */
sealed class NPPSToken {
    data class Keyword(val value: String) : NPPSToken()
    data class Ident(val value: String) : NPPSToken()
    data class Str(val value: String) : NPPSToken()
    data class Num(val value: Double) : NPPSToken()
    data class NumberWithUnit(val value: Double, val unit: String) : NPPSToken()
    data class BoolTok(val value: Boolean) : NPPSToken()
    object LBrace : NPPSToken()
    object RBrace : NPPSToken()
    object LBracket : NPPSToken()
    object RBracket : NPPSToken()
    object Colon : NPPSToken()
    object Comma : NPPSToken()
    object Eof : NPPSToken()
}

/** A token paired with its source line, mirroring the Swift `(NPPSToken, Int)` tuple. */
data class NPPSLexeme(val token: NPPSToken, val line: Int)

class NPPSLexer(text: String) {

    // Swift used [Character]; Kotlin code points are handled per-char here. The
    // NPPS grammar (idents, numbers, units, quoted strings) is ASCII, so char
    // indexing matches the Swift behavior for all valid inputs.
    private val source: CharArray = text.toCharArray()
    private var pos: Int = 0
    private var line: Int = 1

    companion object {
        private val KEYWORDS: Set<String> =
            setOf("protocol", "composite", "layer", "limits", "zone", "condition")
        private val UNITS: Set<String> = setOf("Hz", "mA", "G", "mW_cm2", "m", "s", "h", "%")
    }

    fun tokenize(): List<NPPSLexeme> {
        val tokens = ArrayList<NPPSLexeme>()
        while (pos < source.size) {
            skipWhitespace()
            if (pos >= source.size) break
            val ch = source[pos]
            val tokenLine = line

            if (ch == '#') {
                // Skip comment to end of line.
                while (pos < source.size && source[pos] != '\n') pos += 1
                continue
            }

            if (ch == '\n') {
                line += 1
                pos += 1
                continue
            }

            when (ch) {
                '{' -> { tokens.add(NPPSLexeme(NPPSToken.LBrace, tokenLine)); pos += 1; continue }
                '}' -> { tokens.add(NPPSLexeme(NPPSToken.RBrace, tokenLine)); pos += 1; continue }
                '[' -> { tokens.add(NPPSLexeme(NPPSToken.LBracket, tokenLine)); pos += 1; continue }
                ']' -> { tokens.add(NPPSLexeme(NPPSToken.RBracket, tokenLine)); pos += 1; continue }
                ':' -> { tokens.add(NPPSLexeme(NPPSToken.Colon, tokenLine)); pos += 1; continue }
                ',' -> { tokens.add(NPPSLexeme(NPPSToken.Comma, tokenLine)); pos += 1; continue }
            }

            if (ch == '"') {
                val s = readString(tokenLine)
                tokens.add(NPPSLexeme(NPPSToken.Str(s), tokenLine))
                continue
            }

            // Number branch. Bug fix (2): '-' only begins a number when the next
            // character is a digit — otherwise a leading '-' is a lone unknown
            // char (hyphens inside idents are consumed by readIdent below).
            if (ch.isDigit() || (ch == '-' && pos + 1 < source.size && source[pos + 1].isDigit())) {
                val tok = readNumber(tokenLine)
                tokens.add(NPPSLexeme(tok, tokenLine))
                continue
            }

            if (ch.isLetter() || ch == '_') {
                val ident = readIdent()
                val tok = when {
                    ident == "true" -> NPPSToken.BoolTok(true)
                    ident == "false" -> NPPSToken.BoolTok(false)
                    KEYWORDS.contains(ident) -> NPPSToken.Keyword(ident)
                    else -> NPPSToken.Ident(ident)
                }
                tokens.add(NPPSLexeme(tok, tokenLine))
                continue
            }

            // Unknown character — skip (matches Swift).
            pos += 1
        }
        tokens.add(NPPSLexeme(NPPSToken.Eof, line))
        return tokens
    }

    private fun skipWhitespace() {
        while (pos < source.size) {
            val ch = source[pos]
            if (ch == ' ' || ch == '\t' || ch == '\r') {
                pos += 1
            } else {
                break
            }
        }
    }

    private fun readString(tokenLine: Int): String {
        pos += 1 // skip opening "
        val result = StringBuilder()
        while (pos < source.size) {
            val ch = source[pos]
            if (ch == '"') { pos += 1; return result.toString() }
            if (ch == '\\' && pos + 1 < source.size) {
                pos += 1
                when (source[pos]) {
                    'n' -> result.append('\n')
                    't' -> result.append('\t')
                    '"' -> result.append('"')
                    '\\' -> result.append('\\')
                    else -> result.append(source[pos])
                }
                pos += 1
                continue
            }
            result.append(ch)
            pos += 1
        }
        throw NPPSError("Unterminated string", tokenLine)
    }

    private fun readNumber(tokenLine: Int): NPPSToken {
        val numStr = StringBuilder()
        if (pos < source.size && source[pos] == '-') { numStr.append('-'); pos += 1 }
        while (pos < source.size && (source[pos].isDigit() || source[pos] == '.')) {
            numStr.append(source[pos])
            pos += 1
        }
        val value = numStr.toString().toDoubleOrNull()
            ?: throw NPPSError("Invalid number: $numStr", tokenLine)

        // Check for a unit suffix (no space between number and unit in e.g. 20Hz,
        // 1.5mA, 25%, 2m, 30s, 1h, 0.9G, 500mW_cm2).
        // Bug fix (3): the unit loop accepts digits (for the '2' in mW_cm2) in
        // addition to letters and '_'.
        val unitStart = pos
        val unitStr = StringBuilder()
        while (pos < source.size &&
            (source[pos].isLetter() || source[pos].isDigit() || source[pos] == '_')
        ) {
            unitStr.append(source[pos])
            pos += 1
        }
        val unit = unitStr.toString()
        if (unit.isNotEmpty() && UNITS.contains(unit)) {
            return NPPSToken.NumberWithUnit(value, unit)
        }

        // Backtrack: those chars might be an identifier.
        pos = unitStart
        // Bug fix (1): compound names that start with digits and continue with
        // '_' (wavelength values like "660_808nm" or "660_808_1064nm").
        if (pos < source.size && source[pos] == '_') {
            val compound = StringBuilder(numStr)
            while (pos < source.size &&
                (source[pos].isLetter() || source[pos].isDigit() || source[pos] == '_')
            ) {
                compound.append(source[pos])
                pos += 1
            }
            return NPPSToken.Ident(compound.toString())
        }
        return NPPSToken.Num(value)
    }

    private fun readIdent(): String {
        val result = StringBuilder()
        while (pos < source.size &&
            (source[pos].isLetter() || source[pos].isDigit() || source[pos] == '_')
        ) {
            result.append(source[pos])
            pos += 1
        }
        return result.toString()
    }
}
