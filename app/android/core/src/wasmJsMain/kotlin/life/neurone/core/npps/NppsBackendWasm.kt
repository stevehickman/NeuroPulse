package life.neurone.core.npps

internal actual fun defaultNppsBackend(): NppsBackend = UnwiredNppsBackend("the web (wasmJs) target")
