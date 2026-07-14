package com.neurone.app.data

/**
 * Pure composition rules for the SHDR upload pipeline, extracted from
 * NeurOneApplication's coroutine collectors so the wiring decisions are
 * unit-testable on plain JVM (no running CoroutineScope, no Android Keystore).
 *
 * Mirrors the iOS SHDRUploader observation logic (SHDRUploadTriggering trigger +
 * the $warrantyToken OI-BLE-01 collector).
 */
object ShdrUploadWiring {

    /**
     * OI-BLE-01 token upgrade. Adopt the hub-provisioned 32-byte TRNG warranty
     * token when it arrives. A null value (e.g. a disconnect clearing the flow)
     * must NEVER be forwarded — that would downgrade the device identity back to
     * the CSPRNG fallback and break fleet-DB device-lifetime correlation. The
     * `?.let` here is the null-drop; [ShdrUploader.upgradeDeviceToken] separately
     * rejects any non-32-byte value.
     */
    fun applyWarrantyToken(token: ByteArray?, uploader: ShdrUploader) {
        token?.let { uploader.upgradeDeviceToken(it) }
    }

    /**
     * SHDR upload gate. Upload only when the hub signalled a pending upload, the
     * warranty owner has consented (WarrantyAnalyticsGate), and the hub actually
     * staged a payload. [ShdrUploader.upload] re-checks the warranty gate as a
     * defence-in-depth backstop.
     */
    fun applyUploadPending(
        pending: Boolean,
        warrantyGateOpen: Boolean,
        stagingPayload: ByteArray?,
        uploader: ShdrUploader,
        onResult: (Boolean) -> Unit = {},
    ) {
        if (!pending || !warrantyGateOpen || stagingPayload == null) return
        uploader.upload(stagingPayload, onResult)
    }
}
