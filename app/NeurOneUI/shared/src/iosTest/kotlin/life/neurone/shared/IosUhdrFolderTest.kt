package life.neurone.shared

import kotlinx.cinterop.CPointer
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.ObjCObjectVar
import platform.Foundation.NSError
import platform.Foundation.NSFileManager
import platform.Foundation.NSFileProtectionCompleteUntilFirstUserAuthentication
import platform.Foundation.NSFileProtectionKey
import platform.Foundation.NSTemporaryDirectory
import platform.Foundation.NSUUID
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

// The protection class on the UHDR waveform files (REQ-UHDRFP-01). A recording file manager stands in for the real one, because the
// simulator does not enforce Data Protection and so cannot show what was asked of it (as the Swift app's UHDRFileProtectionTests).
// Runs on the iOS simulator only (:shared:iosSimulatorArm64Test, the kotlin-ios-ci workflow); nothing here runs on Linux.
@OptIn(ExperimentalForeignApi::class)
class IosUhdrFolderTest {

    private class RecordingFileManager(var failing: Boolean = false) : NSFileManager() {
        val calls = mutableListOf<Pair<String, Any?>>()

        override fun setAttributes(attributes: Map<Any?, *>, ofItemAtPath: String, error: CPointer<ObjCObjectVar<NSError?>>?): Boolean {
            if (failing) return false
            calls += ofItemAtPath to attributes[NSFileProtectionKey]
            return true
        }
    }

    private fun folderWithTwoRecordings(): String {
        val root = NSTemporaryDirectory() + "uhdr-fp-" + NSUUID().UUIDString
        val real = NSFileManager.defaultManager
        real.createDirectoryAtPath(root, withIntermediateDirectories = true, attributes = null, error = null)
        real.createFileAtPath("$root/session_1.edf", contents = null, attributes = null)
        real.createFileAtPath("$root/session_2.edf", contents = null, attributes = null)
        return root
    }

    // @verifies REQ-UHDRFP-01#b
    @Test
    fun theClassIsTheStrongestOneABackgroundBackupCanStillRead() {
        assertEquals(NSFileProtectionCompleteUntilFirstUserAuthentication, IosUhdrFolder.PROTECTION_CLASS, "`complete` would make the nightly backup fail while the phone is locked")
    }

    // @verifies REQ-UHDRFP-01#a
    @Test
    fun theFolderAndEveryFileInItGetTheClassAndALandedFileGetsItToo() {
        val root = folderWithTwoRecordings()
        val fm = RecordingFileManager()
        val folder = IosUhdrFolder(root, fm)
        assertEquals(setOf(root, "$root/session_1.edf", "$root/session_2.edf"), fm.calls.map { it.first }.toSet())
        assertTrue(fm.calls.all { it.second == IosUhdrFolder.PROTECTION_CLASS }, "every call asks for the one class")
        fm.calls.clear()
        folder.protect("session_1.edf")
        assertEquals(1, fm.calls.size)
        assertEquals("$root/session_1.edf", fm.calls[0].first)
        assertEquals(IosUhdrFolder.PROTECTION_CLASS, fm.calls[0].second)
    }

    // @verifies REQ-UHDRFP-01#c
    @Test
    fun aFailureIsReportedAndDoesNotThrowOrStopTheRest() {
        val root = folderWithTwoRecordings()
        val folder = IosUhdrFolder(root, RecordingFileManager(failing = true))
        folder.protect("session_1.edf")
    }
}
