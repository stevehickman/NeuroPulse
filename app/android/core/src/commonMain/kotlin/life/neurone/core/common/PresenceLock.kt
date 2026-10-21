package life.neurone.core.common

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

// UHDR presence gate, the part every host shares (NP-APP-PRESENCE-001, REQ-PRES-01 to -03, -08). The principal ruled on 2026-10-21 that
// reading UHDR requires the person's presence on every platform; this file is the lock and the rule for when it closes. What a host
// supplies is only the platform's key binding and prompt, through [GatedSecretKeyStore].

/**
 * A data key the platform releases only after the person's presence is established (a biometric or the device passcode on Android and
 * iOS; a passphrase on a host that has no credential). While it is locked, [load] throws [SecretKeyUnavailableException], which the
 * stores already treat as "could not ask" and never as "no key" (`SWR-MG-15`): nothing is made, replaced or read as ciphertext.
 */
interface GatedSecretKeyStore : SecretKeyStore {
    /** Whether the key can be read right now. */
    val isUnlocked: Boolean

    /** Ask the platform to establish the person's presence. True only if it was. [reason] is the text the platform's prompt shows. */
    suspend fun unlock(reason: String): Boolean

    /** Forget that presence was established: the key is not readable again until [unlock]. Destroying the key needs no presence. */
    fun lock()

    /**
     * Whether the person has the gate turned on. Read before storage is opened: a host answers false until the person turns it on, and
     * the app then opens storage ungated, exactly as before.
     */
    val isEnabled: Boolean

    /**
     * Turn the gate on. The caller has already checked [PresenceEnablement] (`REQ-PRES-08`). The host moves any sealed UHDR to the gated
     * key (`REQ-PRES-07`) and returns false, changing nothing, if it cannot. Takes effect when storage is next opened.
     */
    suspend fun enable(): Boolean

    /** Turn the gate off: the host moves the sealed UHDR back to an ungated key. False, changing nothing, if it cannot. */
    suspend fun disable(): Boolean
}

enum class PresenceState {
    /** Cold start, or the background timeout ran out, or the person locked it: UHDR is not readable. */
    LOCKED,

    /** The person's presence was established and has not lapsed. */
    UNLOCKED,
}

/**
 * The lock itself: written once, so every host locks on the same rule (`REQ-PRES-03`). It starts **locked** (a cold start always asks).
 * It locks again when the app has been in the background for [backgroundTimeoutMs] or more, checked when the app returns
 * ([onForeground]) and whenever the host can wake it ([tick]). The clock is injected so the rule is testable.
 */
class PresenceLock(
    private val key: GatedSecretKeyStore,
    private val nowMs: () -> Long,
    val backgroundTimeoutMs: Long = BACKGROUND_TIMEOUT_MS,
) {
    private val _state = MutableStateFlow(PresenceState.LOCKED)
    val state: StateFlow<PresenceState> = _state

    private var backgroundedAtMs: Long? = null
    private val listeners = mutableListOf<(PresenceState) -> Unit>()

    val isUnlocked: Boolean get() = _state.value == PresenceState.UNLOCKED

    /** Run [listener] each time the state changes (a store drops its cached key on lock and applies what it held on unlock). */
    fun onStateChanged(listener: (PresenceState) -> Unit) {
        listeners += listener
    }

    /** Ask the platform for the person's presence. A refusal or a cancel leaves it locked. */
    suspend fun unlock(reason: String): Boolean {
        if (isUnlocked) return true
        val ok = runCatching { key.unlock(reason) }.getOrDefault(false) && key.isUnlocked
        if (ok) {
            backgroundedAtMs = null
            set(PresenceState.UNLOCKED)
        }
        return ok
    }

    fun lock() {
        key.lock()
        backgroundedAtMs = null
        set(PresenceState.LOCKED)
    }

    /** The app left the foreground. */
    fun onBackground() {
        if (isUnlocked && backgroundedAtMs == null) backgroundedAtMs = nowMs()
    }

    /** The app came back: if it was away for the timeout or longer, it is locked before anything is shown. */
    fun onForeground() {
        tick()
        backgroundedAtMs = null
    }

    /** Lock if the app has been in the background for the timeout. A host that can run a timer in the background calls this from it. */
    fun tick() {
        val since = backgroundedAtMs ?: return
        if (isUnlocked && nowMs() - since >= backgroundTimeoutMs) lock()
    }

    private fun set(next: PresenceState) {
        if (_state.value == next) return
        _state.value = next
        listeners.toList().forEach { it(next) }
    }

    companion object {
        /** How long the app may be in the background before the person is asked again. The principal accepted 5 minutes for now (2026-10-21); nothing derives it (`UC-105`, `OI-PRES-02`). */
        const val BACKGROUND_TIMEOUT_MS = 5 * 60 * 1000L
    }
}

/**
 * The rule that the gate is not turned on before the person has a backup to restore from (`REQ-PRES-08`; the principal, 2026-10-21): a
 * lost credential or a lost keystore key makes the sealed UHDR unreadable, and a backup under a key the person holds is the way back.
 */
object PresenceEnablement {
    sealed interface Decision {
        data object Allowed : Decision

        /** The app asks the person to make a backup first, and does not gate until one exists. */
        data object BackupRequired : Decision
    }

    /** [hasRecoverableBackup]: a backup the person completed, under a recovery code or passphrase they hold, exists. */
    fun decide(hasRecoverableBackup: Boolean): Decision = if (hasRecoverableBackup) Decision.Allowed else Decision.BackupRequired
}
