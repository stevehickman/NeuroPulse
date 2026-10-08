package life.neurone.core.platform

import java.io.File

/**
 * Loads a JNI library by its bare name. A packaged desktop app carries its libraries in the app's resource
 * directory (`compose.application.resources.dir`, set by the Compose launcher), which is not on
 * `java.library.path`, so that directory is tried first. Everywhere else (`gradle :desktop:run`, the tests,
 * Android) the property is unset and this is `System.loadLibrary`. Throws `UnsatisfiedLinkError` when neither finds it.
 */
object NativeLibrary {
    fun load(name: String) {
        val dir = System.getProperty("compose.application.resources.dir")
        if (dir != null) {
            val file = File(dir, System.mapLibraryName(name))
            if (file.isFile) {
                System.load(file.absolutePath)
                return
            }
        }
        System.loadLibrary(name)
    }
}
