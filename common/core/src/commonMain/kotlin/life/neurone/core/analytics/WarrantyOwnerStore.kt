package life.neurone.core.analytics

import life.neurone.core.common.KeyValueStore
import life.neurone.core.platform.Digests
import life.neurone.core.platform.secureRandomBytes
import life.neurone.core.platform.toHex
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
    /**
     * The protocol families the owner does not offer on this helmet (`SWR-WO-01`): family ids and nothing else. Empty, the default,
     * offers every family. The owner's set, never the person's own archive (`np.archived-families`, which travels with the person's
     * protocols): kept here so that it stays with the owner's other decisions and never moves in a user's migration or backup
     * (`REQ-SHDR-10`, `OI-PRIV-60`). Nothing here says which protocols anyone has run.
     */
    val withdrawnFamilies: Set<String> = emptySet(),
) {
    /** Whether the owner offers [familyKey] on this helmet. */
    fun offers(familyKey: String): Boolean = familyKey !in withdrawnFamilies
}

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
    /**
     * Where the hub's SHDR record waits for upload, one record per helmet (`REQ-SHDR-08` (f)); `null` on a host with no hub path
     * that stages one. The record is the helmet's, under its owner's consent, so it is kept with that owner's decisions: it is
     * named by this store's key for the helmet, read only for that helmet, and deleted when the owner revokes or custody ends.
     */
    private val staging: ShdrStaging? = null,
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

    /**
     * Forget this helmet: its record and its staged SHDR record go, so it reads as never asked (`REQ-SHDR-09` (d)). A change of
     * custody ends the old owner's decisions; the new owner is asked again, and their first enrolment is epoch 1.
     */
    fun forget(hub: String) {
        clearStaged(hub)
        table.remove(hub)
    }

    /**
     * The SHDR record staged for this helmet, or `null`. Only the file named by this helmet's key is read, so one helmet's record
     * is never read for another (`REQ-SHDR-08` (f)). With no record kept for any helmet there is no salt, and nothing is read.
     */
    fun staged(hub: String): ByteArray? {
        val key = table.keyOf(hub) ?: return null
        return staging?.read(key)
    }

    /**
     * Stage [record] for this helmet, for the host's hub path to call. Refused (false) when the host has no staging or the helmet
     * has no record here: a helmet whose owner was never asked has nothing to name its file by, and nothing to send it under.
     */
    fun stage(hub: String, record: ByteArray): Boolean {
        val s = staging ?: return false
        if (record(hub) == null) return false
        val key = table.keyOf(hub) ?: return false
        s.stage(key, record)
        return true
    }

    /**
     * The families the owner does not offer on [hub] (`SWR-WO-01`, `OI-PRIV-60`), or none with no helmet named or no record kept.
     * The protocol library asks this, apart from the person's own archive.
     */
    fun withdrawnFamilies(hub: String?): Set<String> = hub?.let { record(it)?.withdrawnFamilies }.orEmpty()

    /**
     * Offer or withdraw one protocol family on [hub] (`SWR-WO-01`): the owner's choice, written to the owner's record for that helmet
     * and to nothing of the person's (`REQ-SHDR-10`, `OI-PRIV-60`).
     */
    fun setOffered(hub: String, familyKey: String, offered: Boolean) {
        update(hub) { r -> r.copy(withdrawnFamilies = if (offered) r.withdrawnFamilies - familyKey else r.withdrawnFamilies + familyKey) }
    }

    /** Delete this helmet's staged record, if there is one. Every other helmet's stays. */
    fun clearStaged(hub: String) {
        val key = table.keyOf(hub) ?: return
        staging?.clear(key)
    }

    // A family id is the protocol's own text (a script may name it), so each is written as the hex of its UTF-8 bytes: no id can
    // then hold the ',' between fields, the '.' between ids, or the tab and newline of the table around the line.
    private fun encode(r: WarrantyOwnerRecord): String =
        listOf(
            FORMAT, if (r.fleetConsent) "1" else "0", if (r.enrolled) "1" else "0", r.consentEpoch, r.programmeId,
            if (r.stealthForbidden) "1" else "0",
            r.withdrawnFamilies.sorted().joinToString(".") { it.encodeToByteArray().toHex() },
        ).joinToString(",")

    // A v1 line (five fields, written before the stealth lock existed) reads with stealth not forbidden, and a v2 line (six, written
    // before the owner's offered set moved here, OI-PRIV-60) with every family offered: that is what each meant. A line that does not
    // read as a record is a record that grants nothing and withdraws nothing.
    private fun decode(line: String): WarrantyOwnerRecord {
        val p = line.split(',')
        val epoch = p.getOrNull(3)?.toIntOrNull()
        val programme = p.getOrNull(4)?.toIntOrNull()
        val shape = (p.size == 5 && p[0] == FORMAT_V1) ||
            (p.size == 6 && p[0] == FORMAT_V2 && p[5] in FLAGS) ||
            (p.size == 7 && p[0] == FORMAT && p[5] in FLAGS)
        val withdrawn = p.getOrNull(6)?.let(::decodeFamilies) ?: emptySet()
        if (!shape || p[1] !in FLAGS || p[2] !in FLAGS || epoch == null || epoch < 0 || programme == null || withdrawn == null) {
            return WarrantyOwnerRecord()
        }
        return WarrantyOwnerRecord(
            p[1] == "1", p[2] == "1", epoch, programme, stealthForbidden = p.getOrNull(5) == "1", withdrawnFamilies = withdrawn,
        )
    }

    /** The family ids of a v3 line's last field, or `null` when any of them is not hex this store wrote. */
    private fun decodeFamilies(field: String): Set<String>? {
        if (field.isEmpty()) return emptySet()
        return field.split('.').map { h ->
            if (h.isEmpty() || h.length % 2 != 0 || !h.all { it in HEX_DIGITS }) return null
            ByteArray(h.length / 2) { h.substring(2 * it, 2 * it + 2).toInt(16).toByte() }.decodeToString()
        }.toSet()
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

        private const val FORMAT = "v3"
        private const val FORMAT_V2 = "v2"
        private const val FORMAT_V1 = "v1"
        private const val HEX_DIGITS = "0123456789abcdef"
        private val FLAGS = setOf("0", "1")
    }
}
