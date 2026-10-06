package life.neurone.core.protocol

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive

/**
 * The validator returns locale keys (the core knows no locale, CLAUDE.md §17), and `:core` cannot reach Android
 * resources, so tests that read a message resolve its keys against `locales/en.json`, the canonical English, the
 * way the app resolves them against its generated resources.
 */
object TestLocale {
    private val english: Map<String, String> by lazy {
        val text = TestLocale::class.java.getResourceAsStream("/en.json")!!.readBytes().decodeToString()
        Json.parseToJsonElement(text).jsonObject.mapValues { it.value.jsonPrimitive.content }
    }

    fun install() {
        NPValidationText.use { key, args ->
            english[key]?.let { template -> args.foldIndexed(template) { i, t, a -> t.replace("{$i}", a) } }
        }
    }
}
