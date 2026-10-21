package life.neurone.shared.session

import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.ensureActive
import kotlin.coroutines.cancellation.CancellationException
import kotlin.coroutines.coroutineContext
import life.neurone.core.migration.ObjectStore
import life.neurone.core.models.SessionStatus

// Mode 4: the EDF+ recording of a finished session, fetched from the hub (CLAUDE.md §4.6). The recording is raw EEG and HRV waveform
// (UHDR, CLAUDE.md §5.1), so it is kept where only the app can reach it, on a path the clear overwrites, and it is never read, rewritten
// or sent anywhere: the app is a passthrough (iOS ISC-55) and the hub's own anonymity gate has already made the header anonymous
// (REQ-SW02-02). Common to every host that has a place to keep it.

/** The folder on this device where downloaded recordings are kept, and what the host does to a file that lands in it. */
interface UhdrFolder {
    val store: ObjectStore

    /**
     * Give a file that has landed the host's protection: iOS sets its file-protection class and leaves it out of backups
     * (`REQ-UHDRFP-01`); the desktop makes it owner-only. Best effort: a host that cannot says nothing, and the download still succeeds.
     */
    fun protect(name: String)
}

/** What a recording is called in the folder: one file per hub session, named by the hub's session id. */
fun edfFileName(sessionId: Long): String = "session_" + sessionId + ".edf"

/** The hub's session id for a session that finished, or null when it never assigned one (epoch 0): there is then nothing to fetch. */
fun edfSessionIdFrom(epoch: Long): Long? = if (epoch == 0L) null else epoch

/** Whether the download button is offered: only for a completed session the hub gave an id (`REQ-SW03-13`). */
fun shouldShowSessionDownload(status: SessionStatus, epoch: Long): Boolean = status == SessionStatus.COMPLETED && epoch != 0L

/** Why a download did not end in a file. The screen words each from a locale key. */
enum class EdfProblem {
    /** The hub is not connected, so it cannot be asked. */
    NOT_CONNECTED,

    /** Nothing arrived in the folder in time. The person may choose the file the hub saved. */
    NO_DATA,

    /** The file the person chose is not an EDF+ recording. */
    NOT_EDF,

    /** The folder would not take the file. */
    NOT_SAVED,
}

sealed interface EdfState {
    data object Idle : EdfState
    data object Downloading : EdfState
    data class Downloaded(val fileName: String) : EdfState
    data class Failed(val problem: EdfProblem) : EdfState
}

/**
 * The download of one session's recording. It asks the hub (the GATT trigger) and then watches the UHDR folder for the file, as the Swift
 * app does; because nothing on the hub is known to put a file there on every host, it also takes a file the person chooses ([adopt]).
 * One download at a time: a second is refused while one runs, so one session never gets two entries.
 */
class EdfDownloadModel(
    private val folder: UhdrFolder,
    private val connected: () -> Boolean,
    private val ask: (sessionId: Long) -> Unit,
    private val wait: suspend (millis: Long) -> Unit = { delay(it) },
) {
    private val current = MutableStateFlow<EdfState>(EdfState.Idle)
    val state: StateFlow<EdfState> = current.asStateFlow()

    /** Ask the hub for [sessionId] and wait for the file to appear. */
    suspend fun download(sessionId: Long) {
        if (current.value == EdfState.Downloading) return
        if (!connected()) { current.value = EdfState.Failed(EdfProblem.NOT_CONNECTED); return }
        current.value = EdfState.Downloading
        val name = edfFileName(sessionId)
        try {
            ask(sessionId)
            var waited = 0L
            while (waited < TIMEOUT_MILLIS) {
                if (folder.store.get(name) != null) {
                    folder.protect(name)
                    current.value = EdfState.Downloaded(name)
                    return
                }
                wait(POLL_MILLIS)
                waited += POLL_MILLIS
                coroutineContext.ensureActive()
            }
            current.value = EdfState.Failed(EdfProblem.NO_DATA)
        } catch (ex: CancellationException) {
            current.value = EdfState.Idle
            throw ex
        } catch (ex: Exception) {
            current.value = EdfState.Failed(EdfProblem.NOT_SAVED)
        }
    }

    /** Keep [bytes], a recording the person chose, as the file for [sessionId]. It must look like an EDF+ file, and is stored unchanged. */
    suspend fun adopt(sessionId: Long, bytes: ByteArray) {
        if (current.value == EdfState.Downloading) return
        if (!looksLikeEdf(bytes)) { current.value = EdfState.Failed(EdfProblem.NOT_EDF); return }
        val name = edfFileName(sessionId)
        try {
            folder.store.put(name, bytes)
            folder.protect(name)
            current.value = EdfState.Downloaded(name)
        } catch (ex: CancellationException) {
            throw ex
        } catch (ex: Exception) {
            current.value = EdfState.Failed(EdfProblem.NOT_SAVED)
        }
    }

    /** Forget the last outcome (a new session, or the person leaving the screen). */
    fun reset() { if (current.value != EdfState.Downloading) current.value = EdfState.Idle }

    companion object {
        /** The Swift app's wait for the file, and how often it looks. */
        const val TIMEOUT_MILLIS = 30_000L
        const val POLL_MILLIS = 500L

        /** An EDF header is 256 fixed bytes and begins with the version "0" padded to eight characters (EDF specification). */
        const val HEADER_BYTES = 256

        fun looksLikeEdf(bytes: ByteArray): Boolean =
            bytes.size >= HEADER_BYTES && bytes[0] == '0'.code.toByte() && (1 until 8).all { bytes[it] == ' '.code.toByte() }
    }
}
