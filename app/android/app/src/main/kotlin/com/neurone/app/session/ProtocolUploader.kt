package com.neurone.app.session

import com.neurone.app.ble.ConnectionState
import com.neurone.app.ble.NeurOneGattManager
import com.neurone.core.protocol.NPProtocolDefinition
import com.neurone.core.session.ProtocolChunker
import com.neurone.core.session.ProtocolSigner
import com.neurone.core.session.SessionProtocolCompiler

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
    }

    fun upload(definition: NPProtocolDefinition): Result {
        if (gatt.connectionState.value != ConnectionState.CONNECTED) {
            return Result.Failure("Hub not connected. Connect via USB-C or Bluetooth first.")
        }
        return try {
            val blob = compiler.compile(definition)
            ProtocolChunker.chunk(blob.wireFormat).forEach { gatt.writeProtocolChunk(it) }
            Result.Success
        } catch (e: Exception) {
            Result.Failure(e.message ?: "Protocol upload failed.")
        }
    }
}
