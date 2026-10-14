package life.neurone.shared.session

import life.neurone.shared.ble.ConnectionState
import life.neurone.shared.ble.NeurOneGattManager
import life.neurone.core.protocol.NPModalityType
import life.neurone.core.protocol.NPLimitsSet
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPProtocolValidator
import life.neurone.core.protocol.NPZoneCaution
import life.neurone.core.session.ProtocolChunker
import life.neurone.core.session.ProtocolSigner
import life.neurone.core.session.HubDescriptorCompiler
import kotlinx.coroutines.delay
import life.neurone.core.models.UploadCheck
import life.neurone.core.models.UploadCheckStatus
import life.neurone.core.models.UploadVerdict

// Mode-2 protocol upload: compile the definition to the signed binary descriptor of
// NP-FW-HUB-001 §4 (OI-AND-WIRE-01), chunk it to BLE-MTU frames, and write each to the hub's
// PROTOCOL_UPLOAD characteristic. Compile + chunk are pure-JVM (unit-tested in :core); this
// class only sequences the GATT writes.
class ProtocolUploader(
    private val gatt: NeurOneGattManager,
    signer: ProtocolSigner,
) {
    private val compiler = HubDescriptorCompiler(signer)

    sealed interface Result {
        /** Every chunk was written. [sentHash] is the SHA-256 of the exact bytes sent, which [verify] waits for the helmet to echo. */
        data class Success(val sentHash: String) : Result
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

    /**
     * @param deviceSerial the 32-byte replay guard the hub checks (§4.2). Omitted, it is the one
     *   read from the hub's DEVICE_SERIAL characteristic; null on both is bench-only, where a hub
     *   with a provisioned serial refuses the descriptor (OI-AND-WIRE-02).
     * @param clinicianSockets operator-chosen 1-based sockets for `clinician_selected` PBM targets.
     * @param acknowledgedCautions the ids from [cautions] the author acknowledged; without them a protocol in the
     *   caution zone is refused by the compiler.
     */
    fun upload(
        definition: NPProtocolDefinition,
        deviceSerial: ByteArray? = null,
        clinicianSockets: List<Int>? = null,
        acknowledgedCautions: List<String> = emptyList(),
    ): Result {
        if (gatt.connectionState.value != ConnectionState.CONNECTED) {
            return Result.Failure("Hub not connected. Connect via USB-C or Bluetooth first.")
        }
        checkCervicalGate(definition)?.let { return it }
        return try {
            val blob = compiler.compile(definition, deviceSerial ?: gatt.deviceSerial, clinicianSockets, acknowledgedCautions)
            ProtocolChunker.chunk(blob.blob).forEach { gatt.writeProtocolChunk(it) }
            Result.Success(UploadCheck.hashOf(blob.blob))
        } catch (e: Exception) {
            Result.Failure(e.message ?: "Protocol upload failed.")
        }
    }

    /**
     * Wait for the helmet's report of the upload whose bytes hash to [sentHash] (SWR-UP-01, SWR-UP-03). The app reads
     * `UPLOAD_CHECK` every [pollMs] for up to [timeoutMs] and never sends the protocol again: an upload that has not
     * been answered is resolved by reading the helmet and not by a second write. A report is for this upload when it is
     * accepted or refused and carries [sentHash]. If none arrives in time, the last report is passed on as it is: a final
     * report with another hash means the helmet holds other bytes (a truncated or altered upload), and no report, an
     * empty or a pending one is no answer. What each of those means for the used mark is the library's rule
     * (`NPProtocolLibrary.recordUploadResult`), not decided here.
     */
    suspend fun verify(sentHash: String, timeoutMs: Long = 10_000L, pollMs: Long = 500L): UploadVerdict {
        fun forThisUpload(c: UploadCheck?) =
            c != null && c.receivedHash == sentHash && c.status != UploadCheckStatus.PENDING && c.status != UploadCheckStatus.NONE
        var waited = 0L
        while (true) {
            gatt.requestUploadCheck()
            delay(pollMs)
            waited += pollMs
            val check = gatt.uploadCheck.value
            if (forThisUpload(check) || waited >= timeoutMs) return UploadVerdict.of(check)
        }
    }

    /**
     * What the author must acknowledge before [upload] will compile this protocol (docs/reference/safety-zones.md).
     * Empty when it is safe; a protocol in the danger zone is a validation error and is refused whatever is
     * acknowledged. The caller shows the acknowledgement screen and passes the ids back; nothing is kept between calls.
     */
    fun cautions(definition: NPProtocolDefinition): List<NPZoneCaution> =
        NPProtocolValidator(NPLimitsSet(name = "unlimited")).validate(definition).zoneCautions

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
