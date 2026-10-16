package life.neurone.web

import kotlin.coroutines.resume
import kotlinx.coroutines.suspendCancellableCoroutine
import life.neurone.shared.migration.FileTransfer

/**
 * The browser's save and open dialogs for a backup file (SWR-MG-02): a download, and a file input. A browser lets a page start
 * either only from a click, which the screen's button is. Bytes cross to JavaScript as lower-case hex, as for Bluetooth.
 */
class WebFileTransfer : FileTransfer {
    override suspend fun save(name: String, bytes: ByteArray): Boolean {
        // A download starts and the browser does not say whether the person kept it; started is the most a page can know.
        downloadFile(name, bytes.joinToString("") { (it.toInt() and 0xFF).toString(16).padStart(2, '0') })
        return true
    }

    override suspend fun open(): ByteArray? = suspendCancellableCoroutine { cont ->
        chooseFile { result ->
            if (cont.isActive) cont.resume(if (result.startsWith("ok:")) hexToBytes(result.substring(3)) else null)
        }
    }
}

private fun hexToBytes(hex: String): ByteArray = ByteArray(hex.length / 2) { hex.substring(it * 2, it * 2 + 2).toInt(16).toByte() }

@JsFun(
    """(name, hex) => {
        const bytes = new Uint8Array((hex.match(/../g) || []).map((h) => parseInt(h, 16)));
        const url = URL.createObjectURL(new Blob([bytes], { type: 'application/octet-stream' }));
        const a = document.createElement('a');
        a.href = url;
        a.download = name;
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(url), 10000);
    }""",
)
private external fun downloadFile(name: String, hex: String)

// The result is "ok:" and the file as hex, or anything else for no file chosen.
@JsFun(
    """(cb) => {
        const input = document.createElement('input');
        input.type = 'file';
        input.onchange = async () => {
            const file = input.files && input.files[0];
            if (!file) { cb('none'); return; }
            const bytes = new Uint8Array(await file.arrayBuffer());
            cb('ok:' + Array.from(bytes, (x) => x.toString(16).padStart(2, '0')).join(''));
        };
        input.addEventListener('cancel', () => cb('none'));
        input.click();
    }""",
)
private external fun chooseFile(cb: (String) -> Unit)
