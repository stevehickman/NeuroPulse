package life.neurone.shared.session

import life.neurone.shared.migration.WaveformBackup
import kotlin.coroutines.cancellation.CancellationException

// Deleting the EEG recordings this device holds, when the person asks (REQ-SW03-37 (b); NP-PRIV-ANALYSIS-006 F-16). The biometric
// release says a recording stays on the device until the person deletes it; withdrawing the release is where they are offered it.
// Only the app's UHDR folder is touched: a backup is in a place the person chose and may not be reachable now, and its copies stay
// (the screen says so). Whether a withdrawal must also destroy those is a decision, not a detail (`OI-PRIV-80`).

/** The recordings in the folder: the files the app names after a hub session (`session_<id>.edf`), and nothing else. */
suspend fun UhdrFolder.recordingNames(): List<String> = store.list("session_").filter(WaveformBackup::isRecordingName)

/**
 * Delete every recording in the folder and return how many are still there afterwards: 0 when all went, or -1 when the folder could
 * not be listed, so nothing is known to be gone. A file the folder will not delete is counted, never reported as gone. Other files in
 * the folder are left alone.
 */
suspend fun UhdrFolder.deleteRecordings(): Int = try {
    for (name in recordingNames()) {
        try {
            store.delete(name)
        } catch (ex: CancellationException) {
            throw ex
        } catch (_: Exception) {
            // Counted below: what is still listed was not deleted.
        }
    }
    recordingNames().size
} catch (ex: CancellationException) {
    throw ex
} catch (_: Exception) {
    -1
}
