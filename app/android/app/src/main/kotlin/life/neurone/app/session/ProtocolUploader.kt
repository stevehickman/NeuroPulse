package life.neurone.app.session

import life.neurone.app.ble.ConnectionState
import life.neurone.app.ble.NeurOneGattManager
import life.neurone.core.protocol.NPModalityType
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.session.ProtocolChunker
import life.neurone.core.session.ProtocolSigner
import life.neurone.core.session.SessionProtocolCompiler

// Mode-2 protocol upload (parity with iOS SessionProtocolUploader): compile the definition
// to the signed wire descriptor, chunk it to BLE-MTU frames, and write each to the hub's
// PROTOCOL_UPLOAD characteristic. Compile + chunk are pure-JVM (unit-tested in :core); this
// class only sequences the GATT writes.
class ProtocolUploader(
    private val gatt: NeurOneGattManager,
    signer: ProtocolSigner,
) {
    private val compiler = SessionProtocolCompiler(signer)

    sealed interface Result {
        data object Success : Result
        data class Failure(val message: String) : Result
        /** An unread cardiac cutoff from an offline session — read the summary first. */
        data object CervicalRestartBlocked : Result
        /** Another person on this device has an outstanding cutoff — confirm the profile is yours. */
        data object DifferentPersonConfirmationRequired : Result
    }

    private var differentPersonConfirmed = false

    /** One-shot: covers the next cervical upload only. */
    fun confirmDifferentPerson() {
        differentPersonConfirmed = true
    }

    fun upload(definition: NPProtocolDefinition): Result {
        if (gatt.connectionState.value != ConnectionState.CONNECTED) {
            return Result.Failure("Hub not connected. Connect via USB-C or Bluetooth first.")
        }
        checkCervicalGate(definition)?.let { return it }
        return try {
            val blob = compiler.compile(definition)
            ProtocolChunker.chunk(blob.wireFormat).forEach { gatt.writeProtocolChunk(it) }
            Result.Success
        } catch (e: Exception) {
            Result.Failure(e.message ?: "Protocol upload failed.")
        }
    }

    /**
     * NP-SW-FAULTMSG-001 P4 and the per-user cardiac scope (iOS SessionProtocolUploader parity).
     * The safety MCU holds every cutoff regardless (P1); these make the app say why, and put a
     * profile switch on the record. A blocked user can still start a cervical session: the
     * device holds cervical VNS and the re-enable confirmation runs inside it.
     */
    private fun checkCervicalGate(definition: NPProtocolDefinition): Result? {
        if (definition.modalities.none { it.enabled && it.modalityType == NPModalityType.CERVICAL_VNS }) return null
        if (gatt.cervicalRestartBlocked) return Result.CervicalRestartBlocked
        if (gatt.cervicalOutstandingForAnotherUser) {
            if (!differentPersonConfirmed) return Result.DifferentPersonConfirmationRequired
            differentPersonConfirmed = false
        }
        return null
    }
}
