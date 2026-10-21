package life.neurone.shared.ota

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onEach
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.coroutines.yield
import life.neurone.core.ble.OtaOpcode
import life.neurone.core.models.FirmwareVersion
import life.neurone.core.models.OtaPhase
import life.neurone.core.models.OtaSession
import life.neurone.core.models.OtaStatusPacket
import life.neurone.core.ota.FirmwareConfig
import life.neurone.core.ota.FirmwareImage
import life.neurone.core.ota.FirmwareUpdateProviding
import life.neurone.core.protocol.BleTransfer
import life.neurone.shared.ble.ConnectionState

// Port of app/ios/NeurOne/OTA/OTAManager.swift — dual-bank firmware update orchestration.
//
// Per NP-FW-EMMC-001 Rev A §8: Bank A is the running firmware, Bank B the staging target. The main-processor sequence is
//   INITIATE → CHUNK (×N) → VERIFY → [hub: VERIFIED] → COMMIT → [hub reboots] → [hub: COMPLETE]
// and the hub verifies the Ed25519 signature on its scratch area before it writes the inactive bank, so the running bank is
// never touched before COMMIT (ISC-111): a disconnect or an abort at any earlier point leaves the hub as it was.
// The safety MCU's firmware is a separate flow that needs its own explicit confirmation, which is the screen's.
//
// What this adds to the Swift manager, in the safe direction: a main-processor update is refused for an image that is the safety
// MCU's and the reverse (the screen routes by the flag, but a wrong image must never reach the other processor); the transfer stops
// when the hub reports a failure or the link is lost, instead of sending the rest of the image to a hub that has refused it; and
// "no update source" is reported as unavailable, not as "up to date".

/** The part of the hub link an update needs: [NeurOneGattManager] is the production one, a fake is the test's. */
interface OtaLink {
    val connectionState: StateFlow<ConnectionState>
    val otaStatus: StateFlow<OtaStatusPacket?>
    val hubFirmwareVersion: StateFlow<String?>
    fun sendOtaCommand(opcode: OtaOpcode, payload: ByteArray = ByteArray(0))
}

sealed interface OtaError {
    data object NotConnected : OtaError
    data object ImageLoadFailed : OtaError
    data object SignatureInvalid : OtaError
    data object WrongImageKind : OtaError
    data object TransferFailed : OtaError
    data class HubRejected(val code: Int) : OtaError
    data object Timeout : OtaError
    data object VerificationFailed : OtaError
}

class OtaException(val error: OtaError) : Exception(error.toString())

class OtaManager(
    private val link: OtaLink,
    private val source: FirmwareUpdateProviding?,
    private val scope: CoroutineScope,
    /** Fingerprints of the signing keys the app will send an image for (ISC-113). Empty trusts none. */
    private val trustedFingerprints: Set<String> = FirmwareConfig.TRUSTED_KEY_FINGERPRINTS,
    private val verifyTimeoutMs: Long = 60_000L,
    private val completeTimeoutMs: Long = 120_000L,
) {
    private val _phase = MutableStateFlow(OtaPhase.IDLE)
    val phase: StateFlow<OtaPhase> = _phase

    private val _progressPercent = MutableStateFlow(0)
    val progressPercent: StateFlow<Int> = _progressPercent

    private val _lastError = MutableStateFlow<OtaError?>(null)
    val lastError: StateFlow<OtaError?> = _lastError

    private val _availableUpdate = MutableStateFlow<FirmwareImage?>(null)
    val availableUpdate: StateFlow<FirmwareImage?> = _availableUpdate

    /** True when the last look for an update got no manifest, or the host has no source: the screen says so and does not say "up to date". */
    private val _updatesUnavailable = MutableStateFlow(false)
    val updatesUnavailable: StateFlow<Boolean> = _updatesUnavailable

    private val _session = MutableStateFlow<OtaSession?>(null)
    val session: StateFlow<OtaSession?> = _session

    private var running: Job? = null
    private var checking: Job? = null

    init {
        // Hub-reported phase and progress augment the local byte count (OTA_STATUS notify).
        link.otaStatus.filterNotNull().onEach { packet ->
            val newPhase = OtaPhase.from(packet.phaseRaw) ?: OtaPhase.IDLE
            _phase.value = newPhase
            if (packet.progressPercent > _progressPercent.value) _progressPercent.value = packet.progressPercent
            if (packet.isError) {
                _lastError.value = OtaError.HubRejected(packet.errorCode)
            } else if (newPhase == OtaPhase.COMPLETE) {
                _progressPercent.value = 100
            }
        }.launchIn(scope)
        // Look for an update whenever the hub connects (ISC-107).
        link.connectionState.onEach { if (it == ConnectionState.CONNECTED) checkForUpdates() }.launchIn(scope)
    }

    /** ISC-107. A manifest that cannot be fetched is not an error to show on every reconnect: it only marks updates unavailable. */
    fun checkForUpdates() {
        checking?.cancel()
        checking = scope.launch { refreshManifest() }
    }

    internal suspend fun refreshManifest() {
        val src = source
        if (src == null) {
            _availableUpdate.value = null
            _updatesUnavailable.value = true
            return
        }
        val manifest = try {
            src.fetchManifest()
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            _updatesUnavailable.value = true
            return
        }
        _updatesUnavailable.value = false
        val image = manifest.toFirmwareImage()
        val available = FirmwareVersion.parse(manifest.version)
        val hubVersion = link.hubFirmwareVersion.value
        val current = hubVersion?.let { FirmwareVersion.parse(it) }
        _availableUpdate.value = when {
            image == null -> null
            available != null && current != null && available > current -> image
            hubVersion == null -> image // the hub's version is unknown: show it and let the person decide
            else -> null
        }
    }

    /** Start the main-processor update in the background; the screen watches [phase], [progressPercent], [session] and [lastError]. */
    fun start(image: FirmwareImage) {
        if (running?.isActive == true) return
        running = scope.launch { try { beginUpdate(image) } catch (e: OtaException) { /* kept in lastError */ } }
    }

    /** Start the safety-MCU update. The caller has put the explicit confirmation to the person. */
    fun startSafetyMcu(image: FirmwareImage) {
        if (running?.isActive == true) return
        running = scope.launch { try { beginSafetyMcuUpdate(image) } catch (e: OtaException) { /* kept in lastError */ } }
    }

    suspend fun beginUpdate(image: FirmwareImage) {
        if (image.isSafetyMCUFirmware) fail(OtaError.WrongImageKind)
        transfer(image, OtaOpcode.INITIATE, OtaOpcode.CHUNK, mainProcessor = true)
    }

    suspend fun beginSafetyMcuUpdate(image: FirmwareImage) {
        if (!image.isSafetyMCUFirmware) fail(OtaError.WrongImageKind)
        transfer(image, OtaOpcode.SAFETY_MCU_BEGIN, OtaOpcode.SAFETY_MCU_CHUNK, mainProcessor = false)
    }

    private suspend fun transfer(image: FirmwareImage, begin: OtaOpcode, chunk: OtaOpcode, mainProcessor: Boolean) {
        // ISC-113: the key is checked before ANY hub communication or download.
        if (image.ed25519PublicKeyFingerprint.lowercase() !in trustedFingerprints) fail(OtaError.SignatureInvalid)
        if (link.connectionState.value != ConnectionState.CONNECTED) fail(OtaError.NotConnected)
        val src = source ?: fail(OtaError.ImageLoadFailed)

        val data = try {
            src.downloadFirmware(image.version, image.imageSizeBytes)
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            fail(OtaError.ImageLoadFailed)
        }

        _session.value = OtaSession(totalBytes = data.size.toLong())
        _phase.value = OtaPhase.PREPARING
        _progressPercent.value = 0
        _lastError.value = null

        // INITIATE: the hub clears its scratch area and prepares to receive.
        send(begin)
        _phase.value = OtaPhase.TRANSFERRING

        // CHUNK × N, with byte-level progress (ISC-110). 80% of the bar is the transfer.
        var offset = 0
        var index = 0
        while (offset < data.size) {
            checkStillGoing()
            val end = minOf(offset + BleTransfer.OTA_CHUNK_SIZE, data.size)
            val payload = ByteArray(2 + end - offset)
            payload[0] = (index and 0xFF).toByte()
            payload[1] = ((index shr 8) and 0xFF).toByte()
            data.copyInto(payload, 2, offset, end)
            send(chunk, payload)
            index++
            offset = end
            _session.value = OtaSession(totalBytes = data.size.toLong(), sentBytes = offset.toLong())
            _progressPercent.value = maxOf(_progressPercent.value, maxOf(1, (offset.toLong() * 80 / data.size).toInt()))
            yield()
        }

        if (mainProcessor) {
            // VERIFY: the hub runs Ed25519 + SHA-256 on the scratch area and answers VERIFIED or FAILED.
            checkStillGoing()
            _phase.value = OtaPhase.VERIFYING
            _progressPercent.value = maxOf(_progressPercent.value, 82)
            send(OtaOpcode.VERIFY)
            waitForPhase(OtaPhase.VERIFIED, verifyTimeoutMs)
            _progressPercent.value = maxOf(_progressPercent.value, 90)

            // COMMIT: the hub writes the inactive bank, stages the boot swap and resets.
            _phase.value = OtaPhase.APPLYING
            _progressPercent.value = maxOf(_progressPercent.value, 93)
            send(OtaOpcode.COMMIT)
        } else {
            _phase.value = OtaPhase.VERIFYING
            _progressPercent.value = maxOf(_progressPercent.value, 82)
            send(OtaOpcode.SAFETY_MCU_COMMIT)
        }
        waitForCompletion()
    }

    /** ISC-111: nothing is sent to a hub that is gone or has refused the image; the running bank was never touched. */
    private fun checkStillGoing() {
        if (_phase.value == OtaPhase.FAILED) {
            val code = (_lastError.value as? OtaError.HubRejected)?.code ?: 0xFFFF
            fail(OtaError.HubRejected(code))
        }
        if (link.connectionState.value != ConnectionState.CONNECTED) fail(OtaError.TransferFailed)
    }

    /** Stop an update that has not been committed. After COMMIT the hub is writing its bank and an abort means nothing. */
    fun abort() {
        if (_phase.value == OtaPhase.APPLYING) return
        running?.cancel()
        running = null
        link.sendOtaCommand(OtaOpcode.ABORT)
        _phase.value = OtaPhase.IDLE
        _session.value = null
        _progressPercent.value = 0
    }

    private fun send(opcode: OtaOpcode, payload: ByteArray = ByteArray(0)) {
        if (link.connectionState.value != ConnectionState.CONNECTED) fail(OtaError.TransferFailed)
        link.sendOtaCommand(opcode, payload)
    }

    private suspend fun waitForPhase(target: OtaPhase, timeoutMs: Long) {
        val reached = withTimeoutOrNull(timeoutMs) { _phase.first { it == target || it == OtaPhase.FAILED } }
        when (reached) {
            null -> fail(OtaError.Timeout)
            OtaPhase.FAILED -> fail(OtaError.VerificationFailed)
            else -> Unit
        }
    }

    /** ISC-112: the hub reboots after COMMIT; allow time for it to reconnect and boot-verify. */
    private suspend fun waitForCompletion() {
        val reached = withTimeoutOrNull(completeTimeoutMs) { _phase.first { it == OtaPhase.COMPLETE || it == OtaPhase.FAILED } }
        when (reached) {
            null -> fail(OtaError.Timeout)
            OtaPhase.FAILED -> fail(OtaError.HubRejected(0xFFFF))
            else -> Unit
        }
    }

    private fun fail(error: OtaError): Nothing {
        _lastError.value = error
        throw OtaException(error)
    }
}
