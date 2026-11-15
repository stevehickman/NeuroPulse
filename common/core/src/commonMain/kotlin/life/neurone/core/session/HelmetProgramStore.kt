package life.neurone.core.session

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonNull
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.common.KeyValueStore
import life.neurone.core.npps.NppsCore
import life.neurone.core.protocol.NPModalityParams
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPWavelengthRules
import life.neurone.core.protocol.toCoreJson

/**
 * What this install last programmed onto a helmet for Mode 3, so the app can say when the wavelength rules in force no longer map the stored protocol
 * the way it was compiled (`SWR-HV-03`, `NP-NPPS-REF-001` §7a.1, `OI-WRV-01`; the principal, 2026-11-15, relayed by the parent session: the
 * record is kept app-side, in UHDR, and is not read back from the helmet).
 *
 * **It is UHDR** (`CLAUDE.md` §5.1): it says which protocol was delivered to a person. It lives in the app's sealed key-value store, so it is
 * sealed at rest and, where the presence gate is on, unreadable while locked (`NP-APP-PRESENCE-001`). The key is a per-install key
 * ([KEY] in `common/npps/migration-keys.json`, never moved by a migration). It holds no device id, serial or warranty token and no time, and
 * NeurOne never receives it: it is in no SHDR upload, log, telemetry or export path. One record, because the helmet holds one stored
 * protocol; a later programming replaces it, and forgetting the hub removes it. The app cannot tell which helmet the record describes (it holds
 * no helmet id on purpose), so a person with two helmets may be told about the other one's protocol, which costs a re-upload offer and nothing else.
 *
 * The verdict is the core's (`library` op `rulesDrift`), by content. Nothing here decides what is stale, and nothing here re-uploads: the app
 * offers a new upload and the person chooses.
 */
class HelmetProgramStore(private val kv: KeyValueStore) {

    companion object {
        const val KEY = "np.helmet.programmed"
        private val json = Json { ignoreUnknownKeys = true }

        /** The PBM wavelengths a protocol states, as written, once each: every enabled transcranial or intranasal block. */
        fun pbmWavelengths(definition: NPProtocolDefinition): List<String> =
            definition.modalities.filter { it.enabled }.mapNotNull {
                when (val p = it.params) {
                    is NPModalityParams.PbmTranscranial -> p.params.wavelength.rawValue
                    is NPModalityParams.PbmIntranasal -> p.params.wavelength.rawValue
                    else -> null
                }
            }.distinct()

        private fun hex(b: ByteArray): String = buildString(b.size * 2) {
            for (x in b) {
                val v = x.toInt() and 0xFF
                append("0123456789abcdef"[v shr 4]).append("0123456789abcdef"[v and 0x0F])
            }
        }
    }

    data class Record(val protocolName: String, val stampHex: String, val wavelengths: List<String>)

    /** A wavelength the stored protocol maps differently under the rules now: `was` and `now` are channel names, null for refused. */
    data class Change(val wavelength: String, val was: String?, val now: String?)

    sealed interface Verdict {
        /** The stored descriptor was compiled under the windows now in force. */
        data object Current : Verdict
        /** The descriptor states no rules (an upload made before the stamp), so nothing is judged. */
        data object NotStated : Verdict
        /** Other windows, but this protocol maps every wavelength as it did: the app says so and does nothing. */
        data object Differs : Verdict
        /** Some wavelength moves channel, is refused, or is newly delivered: the app marks the stored descriptor out of date and offers a new upload. */
        data class Stale(val changes: List<Change>) : Verdict
    }

    fun record(): Record? {
        val raw = kv.getString(KEY) ?: return null
        return runCatching {
            val o = json.parseToJsonElement(raw).jsonObject
            Record(
                o["protocolName"]!!.jsonPrimitive.content,
                o["stampHex"]!!.jsonPrimitive.content,
                o["wavelengths"]!!.jsonArray.map { it.jsonPrimitive.content },
            )
        }.getOrNull()   // a record that does not read is no record: never a guess about what the helmet holds
    }

    /** The helmet accepted [descriptor] for Mode 3: keep what the notice needs. Replaces any earlier record. A descriptor with no readable stamp keeps none. */
    fun remember(definition: NPProtocolDefinition, descriptor: ByteArray) {
        val stamp = runCatching {
            NppsCore.library(buildJsonObject {
                put("op", "descriptorStamp")
                put("descriptorHex", hex(descriptor))
            })["stampHex"]!!.jsonPrimitive.content
        }.getOrNull()
        if (stamp == null) { forget(); return }
        val rec = buildJsonObject {
            put("protocolName", definition.name)
            put("stampHex", stamp)
            put("wavelengths", JsonArray(pbmWavelengths(definition).map(::JsonPrimitive)))
        }
        kv.putString(KEY, rec.toString())
    }

    /** The helmet is no longer this install's (the hub was forgotten), or the record cannot be trusted. */
    fun forget() = kv.remove(KEY)

    /** The core's verdict on the stored protocol under [rules], the rules in force; null when nothing was programmed. */
    fun verdict(rules: NPWavelengthRules): Verdict? {
        val r = record() ?: return null
        val out = runCatching {
            NppsCore.library(buildJsonObject {
                put("op", "rulesDrift")
                put("stampHex", r.stampHex)
                put("wavelengths", JsonArray(r.wavelengths.map(::JsonPrimitive)))
                put("rules", toCoreJson(rules))
            })
        }.getOrNull() ?: return null
        return when (out["state"]?.jsonPrimitive?.contentOrNull) {
            "current" -> Verdict.Current
            "notStated" -> Verdict.NotStated
            "differs" -> Verdict.Differs
            "stale" -> Verdict.Stale(out["changes"]!!.jsonArray.map {
                val c = it.jsonObject
                fun ch(k: String) = c[k]?.takeIf { v -> v !is JsonNull }?.jsonPrimitive?.content
                Change(c["wavelength"]!!.jsonPrimitive.content, ch("was"), ch("now"))
            })
            else -> null
        }
    }
}
