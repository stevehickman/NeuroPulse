package life.neurone.shared

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.alloc
import kotlinx.cinterop.memScoped
import kotlinx.cinterop.ptr
import kotlinx.cinterop.value
import life.neurone.core.common.SecretKeyStore
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
import platform.Security.SecItemAdd
import platform.Security.SecItemCopyMatching
import platform.Security.SecItemDelete
import platform.Security.errSecItemNotFound
import platform.Security.errSecSuccess
import platform.Security.kSecAttrAccessible
import platform.Security.kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
import platform.Security.kSecAttrAccount
import platform.Security.kSecAttrService
import platform.Security.kSecClass
import platform.Security.kSecClassGenericPassword
import platform.Security.kSecMatchLimit
import platform.Security.kSecMatchLimitOne
import platform.Security.kSecReturnData
import platform.Security.kSecValueData

/**
 * The data key that protects the app's UHDR-class state (SWR-MG-06), kept in the iOS Keychain. The item is not backed up and
 * does not move to another device (`ThisDeviceOnly`), and is readable after the first unlock so a background launch can open
 * the data. [destroy] deletes the Keychain item, so what the app sealed is unreadable from then on.
 */
@OptIn(ExperimentalForeignApi::class)
class IosSecretKeyStore(
    private val service: String = "life.neurone.data-key",
    private val account: String = "np-data-key",
) : SecretKeyStore {

    /** A query for this item. The caller releases it. */
    private fun query(): CFMutableDictionaryRef {
        val q = CFDictionaryCreateMutable(null, 0, kCFTypeDictionaryKeyCallBacks.ptr, kCFTypeDictionaryValueCallBacks.ptr)!!
        CFDictionarySetValue(q, kSecClass, kSecClassGenericPassword)
        CFDictionarySetValue(q, kSecAttrService, CFBridgingRetain(service))
        CFDictionarySetValue(q, kSecAttrAccount, CFBridgingRetain(account))
        return q
    }

    override fun load(): ByteArray? = memScoped {
        val q = query()
        CFDictionarySetValue(q, kSecReturnData, kCFBooleanTrue)
        CFDictionarySetValue(q, kSecMatchLimit, kSecMatchLimitOne)
        val result = alloc<CFTypeRefVar>()
        val status = SecItemCopyMatching(q, result.ptr)
        CFRelease(q)
        // Only the Keychain's own "not found" is "no key". Any other status (a Keychain that is locked until the first unlock, an
        // interaction that is not allowed) is "could not ask": answering null would let a new key replace the real one.
        if (status == errSecItemNotFound) return@memScoped null
        if (status != errSecSuccess) throw SecretKeyUnavailableException("the Keychain did not answer (status $status)")
        val data = CFBridgingRelease(result.value) as? NSData ?: throw SecretKeyUnavailableException("the Keychain gave no data")
        data.toByteArray()
    }

    override fun store(key: ByteArray) {
        destroy()
        val q = query()
        CFDictionarySetValue(q, kSecValueData, CFBridgingRetain(key.toNSData()))
        CFDictionarySetValue(q, kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly)
        val status = SecItemAdd(q, null)
        CFRelease(q)
        check(status == errSecSuccess) { "could not keep the data key in the Keychain (status $status)" }
    }

    override fun destroy() {
        val q = query()
        SecItemDelete(q)
        CFRelease(q)
        check(load() == null) { "the data key is still in the Keychain" }
    }
}
