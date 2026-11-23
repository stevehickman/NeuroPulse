package life.neurone.core.analytics

import life.neurone.core.common.KeyValueStore
import life.neurone.core.platform.Digests
import life.neurone.core.platform.secureRandomBytes
import life.neurone.core.protocol.PersistedKeys

/**
 * What the warranty owner has decided for one helmet (`REQ-SHDR-08`, `REQ-SHDR-09`). No field says anything about a person who
 * wears the helmet: the subject is the entity that registered it (`CLAUDE.md` §6.0).
 */
data class WarrantyOwnerRecord(
    /** SHDR fleet telemetry may be uploaded for this helmet. */
    val fleetConsent: Boolean = false,
    /** This helmet is enrolled in the characterisation programme named by [programmeId] (NP-FW-EMMC-002 §H). */
    val enrolled: Boolean = false,
    /** Zero means "never enrolled". It only increases, except at a change of custody. */
    val consentEpoch: Int = 0,
    val programmeId: Int = 0,
    /**
     * The owner forbids stealth mode on this helmet (`SWR-IND-02`, `OI-PRIV-37`): the app then asks the helmet for its indicators
     * on, whatever the active profile chose. A choice about the owner's helmet, not about a person. False, the default, leaves
     * the choice to the wearer. The app enforces it; the hub cannot tell who asks, so it does not.
     */
    val stealthForbidden: Boolean = false,
)

/**
 * The warranty owner's store: every record that belongs to the warranty owner and not to a user, one per helmet
 * (`REQ-SHDR-10`; the principal, 2026-11-20, `OI-PRIV-04`).
 *
 * It is one value of its own ([RECORDS_KEY]), with its own salt, and shares no key with any user data. It is not an item of a user
 * migration or of the UHDR backup, so a user's data moving to a new phone never brings an owner's decision with it. Its own
 * backup, restore and transfer are separate from the user's and are not built (`OI-PRIV-20`); until they are, a record stays on
 * the install that made it.
 *
 * A record is keyed as the device tokens are (`REQ-SHDR-05` (d)): by `SHA-256(salt ‖ identifier)`, never by the helmet's
 * identifier, which on Android is its Bluetooth hardware address. [HubTokenTable] is the table for both; this value is not the
 * token table's and has a salt of its own.
 *
 * WHAT AN EARLIER VERSION KEPT IS NOT READ. That was one consent flag and one enrolment for the whole install, which cannot be
 * attributed to a helmet, so they open nothing (ruling Q2: ask again). The four keys are removed when the store is built; a run
 * that cannot write yet leaves them in place, where nothing reads them.
 *
 * NOT READABLE WHILE LOCKED. [RECORDS_KEY] is not in the device tier (`NP-APP-PRESENCE-001` §4): a key joins that list only by a
 * ruling (`REQ-PRES-05`), and none has been given for this one. So on an install with the presence gate on, a locked run reads no
 * record, the gate is closed and nothing is uploaded until the person unlocks (`OI-PRIV-20` (4)).
 *
 * This class must never reference `ConsentStore` or `ResearchAnalyticsGate`.
 */
class WarrantyOwnerStore(
    private val store: KeyValueStore,
    randomBytes: (Int) -> ByteArray = ::secureRandomBytes,
    sha256: (ByteArray) -> ByteArray = Digests::sha256,
) {
    private val table = HubTokenTable(
        read = { store.getString(RECORDS_KEY) },
        write = { store.putString(RECORDS_KEY, it) },
        randomBytes = randomBytes,
        sha256 = sha256,
    )

    init {
        LEGACY_KEYS.forEach(store::remove)
    }

    /**
     * The record kept for this helmet, or `null` when the owner has never been asked about it. A line that does not read as a
     * record is a record that grants nothing.
     */
    fun record(hub: String): WarrantyOwnerRecord? = table.load(hub)?.let(::decode)

    /** Change this helmet's record, starting from one that grants nothing when there is none. */
    fun update(hub: String, change: (WarrantyOwnerRecord) -> WarrantyOwnerRecord) {
        table.save(hub, encode(change(record(hub) ?: WarrantyOwnerRecord())))
    }

    private fun encode(r: WarrantyOwnerRecord): String =
        listOf(
            FORMAT, if (r.fleetConsent) "1" else "0", if (r.enrolled) "1" else "0", r.consentEpoch, r.programmeId,
            if (r.stealthForbidden) "1" else "0",
        ).joinToString(",")

    // A v1 line (five fields, written before the stealth lock existed) reads with stealth not forbidden, which is what it meant.
    private fun decode(line: String): WarrantyOwnerRecord {
        val p = line.split(',')
        val epoch = p.getOrNull(3)?.toIntOrNull()
        val programme = p.getOrNull(4)?.toIntOrNull()
        val shape = (p.size == 5 && p[0] == FORMAT_V1) || (p.size == 6 && p[0] == FORMAT && p[5] in FLAGS)
        if (!shape || p[1] !in FLAGS || p[2] !in FLAGS || epoch == null || epoch < 0 || programme == null) {
            return WarrantyOwnerRecord()
        }
        return WarrantyOwnerRecord(p[1] == "1", p[2] == "1", epoch, programme, stealthForbidden = p.getOrNull(5) == "1")
    }

    companion object {
        /** The one value this store keeps. Per install, sealed at rest, and in no user bundle (`common/npps/migration-keys.json`). */
        const val RECORDS_KEY = "np.warranty-owner.records"

        /** Written by versions before the per-helmet store; removed, never read. */
        val LEGACY_KEYS = listOf(
            PersistedKeys.WARRANTY_CONSENT_KEY,
            PersistedKeys.ENROLMENT_KEY,
            PersistedKeys.CONSENT_EPOCH_KEY,
            PersistedKeys.PROGRAMME_ID_KEY,
        )

        private const val FORMAT = "v2"
        private const val FORMAT_V1 = "v1"
        private val FLAGS = setOf("0", "1")
    }
}
