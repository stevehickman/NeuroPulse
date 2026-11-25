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
     * Turn the gate on. The caller has already checked [PresenceEnablement] (`REQ-PRES-08`). The host establishes the person's presence
     * once ([reason] is the prompt's text) so the key is made, and returns false, changing nothing, if it cannot. **It moves no data:**
     * storage is next opened gated, and the first unlock seals each value again under the gated key (`REQ-PRES-07`,
     * [EncryptedKeyValueStore.resealFromLegacy]), so a failure cannot lose anything.
     */
    suspend fun enable(reason: String): Boolean

    // `destroy()` (inherited) is the erase: it removes the key and, with it, the gate (`isEnabled` reads false afterwards), because an erase
    // returns the device to factory state, gate included (`OI-PRES-10`). It is never offered as a switch (`REQ-PRES-09`).
    // There is no way to turn the gate off, and none may be added: UHDR protection is never made weaker (CLAUDE.md §5.1, REQ-PRES-09).
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
 * ([onForeground]) and whenever the host can wake it ([tick]). The clocks are injected so the rule is testable.
 *
 * **The time away is measured on a monotonic clock** ([nowMs]), which the person cannot set (`OI-PRIV-62`, M9): on the wall clock,
 * setting the time back while the app was away made the elapsed time negative and kept the app unlocked. A monotonic clock may stop
 * while the device sleeps, so the wall clock ([wallMs]) is asked as well, as a second witness: **the lock closes when either clock
 * says the timeout has passed, or when either has gone backwards** (a negative elapsed time is expired, never "not yet"). Moving the
 * wall clock forward can only lock sooner.
 */
class PresenceLock(
    private val key: GatedSecretKeyStore,
    /** A monotonic clock in milliseconds (any origin): one the person cannot set. */
    private val nowMs: () -> Long,
    val backgroundTimeoutMs: Long = BACKGROUND_TIMEOUT_MS,
    /** The wall clock in milliseconds, or null: a second witness for time the monotonic clock did not count (the device asleep). */
    private val wallMs: (() -> Long)? = null,
) {
    private val _state = MutableStateFlow(PresenceState.LOCKED)
    val state: StateFlow<PresenceState> = _state

    private var backgroundedAtMs: Long? = null
    private var backgroundedAtWallMs: Long? = null
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
            backgroundedAtWallMs = null
            set(PresenceState.UNLOCKED)
        }
        return ok
    }

    /** Open the lock with a passphrase, on a host whose key is a [PassphraseKey] (`REQ-PRES-06`). Any other key answers `FAILED`. */
    suspend fun unlockWithPassphrase(passphrase: String): PassphraseResult {
        if (isUnlocked) return PassphraseResult.OK
        val passphraseKey = key as? PassphraseKey ?: return PassphraseResult.FAILED
        val result = runCatching { passphraseKey.unlockWithPassphrase(passphrase) }.getOrDefault(PassphraseResult.FAILED)
        if (result == PassphraseResult.OK && key.isUnlocked) {
            backgroundedAtMs = null
            backgroundedAtWallMs = null
            set(PresenceState.UNLOCKED)
        } else if (result == PassphraseResult.OK) {
            return PassphraseResult.FAILED
        }
        return result
    }

    /** The key behind this lock when it is a passphrase (so the screen asks for one instead of the platform), else null. */
    val passphraseKey: PassphraseKey? get() = key as? PassphraseKey

    fun lock() {
        key.lock()
        backgroundedAtMs = null
        backgroundedAtWallMs = null
        set(PresenceState.LOCKED)
    }

    /** The app left the foreground. */
    fun onBackground() {
        if (isUnlocked && backgroundedAtMs == null) {
            backgroundedAtMs = nowMs()
            backgroundedAtWallMs = wallMs?.invoke()
        }
    }

    /** The app came back: if it was away for the timeout or longer, it is locked before anything is shown. */
    fun onForeground() {
        tick()
        backgroundedAtMs = null
        backgroundedAtWallMs = null
    }

    /** Lock if the app has been in the background for the timeout. A host that can run a timer in the background calls this from it. */
    fun tick() {
        val since = backgroundedAtMs ?: return
        if (!isUnlocked) return
        val sinceWall = backgroundedAtWallMs
        val wallExpired = sinceWall != null && wallMs?.let { expired(it() - sinceWall) } == true
        if (expired(nowMs() - since) || wallExpired) lock()
    }

    /** A time away of [elapsedMs] has used up the timeout: it reached it, or it is negative (a clock went back, `OI-PRIV-62` M9). */
    private fun expired(elapsedMs: Long): Boolean = elapsedMs < 0 || elapsedMs >= backgroundTimeoutMs

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
