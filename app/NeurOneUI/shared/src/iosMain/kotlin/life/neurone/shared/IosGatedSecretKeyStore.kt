package life.neurone.shared

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.value
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import life.neurone.core.common.GatedSecretKeyStore
import life.neurone.core.common.SecretKeyUnavailableException
import platform.CoreFoundation.CFDictionaryCreateMutable
import platform.CoreFoundation.CFDictionarySetValue
import platform.CoreFoundation.CFMutableDictionaryRef
import platform.CoreFoundation.CFRelease
import platform.CoreFoundation.CFTypeRefVar
import platform.CoreFoundation.kCFBooleanTrue
import platform.CoreFoundation.kCFTypeDictionaryKeyCallBacks
import platform.CoreFoundation.kCFTypeDictionaryValueCallBacks
import platform.Foundation.CFBridgingRelease
import platform.Foundation.CFBridgingRetain
import platform.Foundation.NSData
import platform.Foundation.NSUserDefaults
import platform.LocalAuthentication.LAContext
import platform.LocalAuthentication.LAPolicyDeviceOwnerAuthentication
import platform.Security.SecAccessControlCreateWithFlags
import platform.Security.SecItemAdd
import platform.Security.SecItemCopyMatching
import platform.Security.SecItemDelete
import platform.Security.errSecItemNotFound
import platform.Security.errSecSuccess
import platform.Security.kSecAccessControlUserPresence
import platform.Security.kSecAttrAccessControl
import platform.Security.kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly
import platform.Security.kSecAttrAccount
import platform.Security.kSecAttrService
import platform.Security.kSecClass
import platform.Security.kSecClassGenericPassword
import platform.Security.kSecMatchLimit
import platform.Security.kSecMatchLimitOne
import platform.Security.kSecReturnData
import platform.Security.kSecUseAuthenticationContext
import platform.Security.kSecValueData
import kotlin.coroutines.resume

/**
 * The key that protects UHDR once the person's presence is established (`NP-APP-PRESENCE-001` §5, `REQ-PRES-02`, `-04`). The 32-byte
 * data key is a Keychain item whose access control is `.userPresence`: the Keychain itself, not the app, asks for Face ID, Touch ID or
 * the device passcode before it hands the item over, and the passcode is always allowed (never biometrics-only). `ThisDeviceOnly`
 * with `WhenPasscodeSet` keeps it off backups and other devices, and deletes it if the passcode is removed, which is the case the
 * backup the app insists on first is for (`REQ-PRES-08`). A new Face ID or fingerprint enrolment does not touch it (`.userPresence`,
 * not `.biometryCurrentSet`).
 *
 * **How it works.** [unlock] evaluates `LAPolicyDeviceOwnerAuthentication` on an `LAContext`, then reads the item with that same
 * context, so the person is asked once. The key is held in memory until [lock]; [load] answers from memory and, while locked, throws
 * [SecretKeyUnavailableException] ("could not ask", never "no key", `SWR-MG-15`). The read is made on a background dispatcher: it
 * blocks while the system prompt is up.
 *
 * **Not offered yet.** The migration of sealed UHDR to this key is built (`REQ-PRES-07`, in `:core`), and there is no way to turn the gate
 * off (`REQ-PRES-09`). [IosPlatformServices] does not hand this store to the shared app until the device run (`OI-PRES-07`).
 */
@OptIn(ExperimentalForeignApi::class)
class IosGatedSecretKeyStore(
    private val service: String = "life.neurone.uhdr-key",
    private val account: String = "np-uhdr-key",
) : GatedSecretKeyStore {
    private var dataKey: ByteArray? = null
    private var unlocked = false

    override val isUnlocked: Boolean get() = unlocked

    override val isEnabled: Boolean get() = NSUserDefaults.standardUserDefaults.boolForKey(ENABLED)

    /**
     * Turn the gate on: the person passes the prompt once and the flag is kept. No data moves here. The next launch opens storage gated,
     * and the first unlock seals each value again under this key (`REQ-PRES-07`); the Keychain item is made by the first write ([store]).
     */
    override suspend fun enable(reason: String): Boolean {
        if (!evaluate(LAContext(), reason)) return false
        NSUserDefaults.standardUserDefaults.setBool(true, forKey = ENABLED)
        return true
    }

    override suspend fun unlock(reason: String): Boolean {
        if (unlocked) return true
        val context = LAContext()
        if (!evaluate(context, reason)) return false
        val read = withContext(Dispatchers.Default) { read(context) }
        return when (read) {
            is Read.Found -> { dataKey = read.bytes; unlocked = true; true }
            Read.NoItem -> { dataKey = null; unlocked = true; true }
            Read.Failed -> false
        }
    }

    override fun lock() {
        dataKey?.fill(0)
        dataKey = null
        unlocked = false
    }

    override fun load(): ByteArray? {
        if (!unlocked) throw SecretKeyUnavailableException("the person's presence has not been established")
        return dataKey?.copyOf()
    }

    override fun store(key: ByteArray) {
        if (!unlocked) throw SecretKeyUnavailableException("the person's presence has not been established")
        delete()
        val access = SecAccessControlCreateWithFlags(null, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlUserPresence, null)
            ?: throw SecretKeyUnavailableException("the Keychain could not make the access control")
        val q = query()
        CFDictionarySetValue(q, kSecValueData, CFBridgingRetain(key.toNSData()))
        CFDictionarySetValue(q, kSecAttrAccessControl, access)
        val status = SecItemAdd(q, null)
        CFRelease(q)
        CFRelease(access)
        if (status != errSecSuccess) throw SecretKeyUnavailableException("could not keep the data key in the Keychain (status $status)")
        dataKey?.fill(0)
        dataKey = key.copyOf()
    }

    override fun destroy() {
        // Deleting needs no presence.
        val status = delete()
        check(status == errSecSuccess || status == errSecItemNotFound) { "the data key is still in the Keychain (status $status)" }
        lock()
    }

    private fun delete(): Int {
        val q = query()
        val status = SecItemDelete(q)
        CFRelease(q)
        return status
    }

    private fun query(): CFMutableDictionaryRef {
        val q = CFDictionaryCreateMutable(null, 0, kCFTypeDictionaryKeyCallBacks.ptr, kCFTypeDictionaryValueCallBacks.ptr)!!
        CFDictionarySetValue(q, kSecClass, kSecClassGenericPassword)
        CFDictionarySetValue(q, kSecAttrService, CFBridgingRetain(service))
        CFDictionarySetValue(q, kSecAttrAccount, CFBridgingRetain(account))
        return q
    }

    private sealed interface Read {
        class Found(val bytes: ByteArray) : Read
        data object NoItem : Read
        data object Failed : Read
    }

    /** Read the item through [context], which has just been evaluated, so the Keychain does not ask a second time. */
    private fun read(context: LAContext): Read = memScoped {
        val q = query()
        CFDictionarySetValue(q, kSecReturnData, kCFBooleanTrue)
        CFDictionarySetValue(q, kSecMatchLimit, kSecMatchLimitOne)
        CFDictionarySetValue(q, kSecUseAuthenticationContext, CFBridgingRetain(context))
        val result = alloc<CFTypeRefVar>()
        val status = SecItemCopyMatching(q, result.ptr)
        CFRelease(q)
        if (status == errSecItemNotFound) return@memScoped Read.NoItem
        if (status != errSecSuccess) return@memScoped Read.Failed
        val data = CFBridgingRelease(result.value) as? NSData ?: return@memScoped Read.Failed
        Read.Found(data.toByteArray())
    }

    /** Face ID, Touch ID or the device passcode. True only if the person passed. */
    private suspend fun evaluate(context: LAContext, reason: String): Boolean = suspendCancellableCoroutine { cont ->
        context.evaluatePolicy(LAPolicyDeviceOwnerAuthentication, localizedReason = reason) { success, _ ->
            if (cont.isActive) cont.resume(success)
        }
    }

    private companion object {
        const val ENABLED = "np.presence.enabled"
    }
}
