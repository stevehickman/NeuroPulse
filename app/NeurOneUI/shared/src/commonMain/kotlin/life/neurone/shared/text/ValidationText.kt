package life.neurone.shared.text

import life.neurone.core.protocol.NPValidationText
import life.neurone.shared.ui.ValidationStrings
import org.jetbrains.compose.resources.getString

/**
 * Makes the validator's messages readable. The core returns locale keys and positional arguments, never text
 * (CLAUDE.md §17), and resolves them through [NPValidationText], which is synchronous. Compose resources are
 * read asynchronously, so the validator's strings (`VALIDATE_*`, `SOCKET_ERR_*`) are read once here and the
 * resolver answers from that table. A host that already installed a resolver (Android's, from its own
 * resources) is left alone. Before this finishes, or if the resources cannot be read, a message renders as its
 * key, which is visible rather than silent.
 */
suspend fun installValidationText() {
    if (NPValidationText.isInstalled) return
    val templates = try {
        ValidationStrings.all.mapValues { (_, resource) -> getString(resource) }
    } catch (e: Exception) {
        return
    }
    if (NPValidationText.isInstalled) return
    NPValidationText.use { key, args -> templates[key.lowercase()]?.let { fill(it, args) } }
}

private val placeholder = Regex("%(\\d+)\\$[sd]")

/** `%1$s` is the first argument, as sync-locales writes them. */
private fun fill(template: String, args: List<String>): String =
    placeholder.replace(template) { match -> args.getOrNull(match.groupValues[1].toInt() - 1) ?: match.value }
