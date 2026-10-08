package life.neurone.desktop

/**
 * JNI binding of the desktop Bluetooth LE central (`common/btle-jni`, btleplug). The Rust entry points
 * are named for this class (`Java_life_neurone_desktop_BtleNative_native*`), so renaming it means
 * renaming them. Commands return at once; events are read with [nativePoll].
 */
internal object BtleNative {
    init {
        life.neurone.core.platform.NativeLibrary.load("neurone_btle_jni")
    }

    @JvmStatic external fun nativeCreate(): Long
    @JvmStatic external fun nativeFree(handle: Long)
    @JvmStatic external fun nativePoll(handle: Long, timeoutMs: Int): ByteArray?
    @JvmStatic external fun nativeRefresh(handle: Long)
    @JvmStatic external fun nativeStartScan(handle: Long, service: String)
    @JvmStatic external fun nativeStopScan(handle: Long)
    @JvmStatic external fun nativeConnect(handle: Long, device: String)
    @JvmStatic external fun nativeDisconnect(handle: Long)
    @JvmStatic external fun nativeDiscover(handle: Long, service: String, wanted: String)
    @JvmStatic external fun nativeEnableNotifications(handle: Long, characteristic: String)
    @JvmStatic external fun nativeRead(handle: Long, characteristic: String)
    @JvmStatic external fun nativeWrite(handle: Long, characteristic: String, value: ByteArray)
}

/** The radio as [DesktopBleCentral] drives it: the JNI library in production, a fake in tests. */
internal interface BtleLink {
    fun poll(timeoutMs: Int): ByteArray?
    fun refresh()
    fun startScan(service: String)
    fun stopScan()
    fun connect(device: String)
    fun disconnect()
    fun discover(service: String, wanted: List<String>)
    fun enableNotifications(characteristic: String)
    fun read(characteristic: String)
    fun write(characteristic: String, value: ByteArray)

    /** Stops the radio. Called once, after the last [poll] has returned. */
    fun close()
}

internal class JniBtleLink private constructor(private val handle: Long) : BtleLink {
    override fun poll(timeoutMs: Int) = BtleNative.nativePoll(handle, timeoutMs)
    override fun refresh() = BtleNative.nativeRefresh(handle)
    override fun startScan(service: String) = BtleNative.nativeStartScan(handle, service)
    override fun stopScan() = BtleNative.nativeStopScan(handle)
    override fun connect(device: String) = BtleNative.nativeConnect(handle, device)
    override fun disconnect() = BtleNative.nativeDisconnect(handle)
    override fun discover(service: String, wanted: List<String>) =
        BtleNative.nativeDiscover(handle, service, wanted.joinToString(","))
    override fun enableNotifications(characteristic: String) = BtleNative.nativeEnableNotifications(handle, characteristic)
    override fun read(characteristic: String) = BtleNative.nativeRead(handle, characteristic)
    override fun write(characteristic: String, value: ByteArray) = BtleNative.nativeWrite(handle, characteristic, value)
    override fun close() = BtleNative.nativeFree(handle)

    companion object {
        /** null when the native library is missing or the radio runtime would not start. */
        fun open(): JniBtleLink? = try {
            BtleNative.nativeCreate().takeIf { it != 0L }?.let(::JniBtleLink)
        } catch (_: UnsatisfiedLinkError) {
            null
        }
    }
}
