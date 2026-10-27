package life.neurone.core.cloud

import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.buildJsonArray
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.contentOrNull
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import kotlinx.serialization.json.putJsonArray
import kotlinx.serialization.json.putJsonObject
import life.neurone.core.migration.ObjectStore
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpResponse
import life.neurone.core.net.HttpTransport

/** The two tokens a CloudKit Web Services call carries for a person's private database (Apple's "Composing Web Service Requests"). */
fun interface CloudKitCredentials {
    /** The container's API token and the person's web authentication token, or null for the second when the person has not signed in. */
    suspend fun tokens(): Pair<String, String?>
}

/**
 * An [ObjectStore] over one iCloud container's CloudKit database, through **CloudKit Web Services** (`https://api.apple-cloudkit.com`),
 * so it runs wherever an [HttpTransport] does, Android included (SWR-MG-11, `docs/np_sw_cloudstore_001.md` §3.1 and §4b). It is
 * **not** the native CloudKit framework. The pages it is written from are Apple's *Documentation Archive* (CloudKit Web Services
 * Reference, "Updated: 2016-06-13", copyright 2018); whether they are current was not established.
 *
 * **Each object is one record** of type [RECORD_TYPE] whose name is the object's name and whose only field, [FIELD], is an **Asset**.
 * An Asset, not a Bytes field, because the size limit of a record's other fields was not read (the "Data Size Limits" page is
 * not in the archive set read). The cost is that every write is three calls: `assets/upload` for an upload URL, a POST of the bytes
 * to it (which answers the asset dictionary), and `records/modify`. An object over [MAX_BYTES] throws before anything is sent (Gap 1).
 *
 * **Create-if-absent is the `create` operation**: "Create a new record. This operation fails if a record with the same record name
 * already exists" (`records/modify` page), and the error list has `EXISTS` ("The resource that you attempted to create already exists",
 * HTTP 409). A `serverErrorCode` of `EXISTS`, whether on the request or on the operation's entry, is read as "the object exists";
 * every other error, and no answer, throws. The asset bytes of a refused create are uploaded and never used. The documentation does
 * not say two simultaneous creates are arbitrated atomically; the contract test against a real account (`SWR-MG-11#contract`) is what
 * would show it. `put` is `forceReplace` ("regardless of conflicts. Creates a record if it doesn't exist"), delete is `forceDelete`.
 *
 * **Read** is `records/lookup`, which answers the asset's `downloadURL`; the bytes are fetched from it without credentials. **List** is
 * `records/query` with `recordName BEGINS_WITH`, paged by `continuationMarker`. The page says queries run on indexed fields and "indexes
 * are updated asynchronously, so they are not guaranteed to be current": **a list can miss a record just written**, and the container's
 * schema must make `recordName` queryable. Neither lookup nor create is documented as subject to that lag.
 *
 * **Sign-in is the host's.** A private database needs the person's web authentication token, obtained through Apple's sign-in redirect
 * (`AUTHENTICATION_REQUIRED` carries a `redirectURL`); this class never obtains one. The page also says a token "is intended for a
 * single round trip ... a new token is provided in the response", without saying where in the response. This class hands
 * [onWebAuthToken] a top-level `ckWebAuthToken` string of a JSON reply, and **nothing else is done about rotation**: that is the host's to check
 * against a real account. A server-to-server key (ECDSA-signed requests) is **not** supported: it reaches only the public database,
 * and the signature is cryptography for the Rust core (`CLAUDE.md` §22).
 *
 * **Names.** A record name is written as `%XX` for everything outside A-Z a-z 0-9 `-` `_` `.` `~`, and a leading `_` too, read back the
 * same way. That a record name may contain `%`, that it must be ASCII, at most 255 characters, and not start with an underscore are
 * from the native `CKRecord.ID` rules **as remembered, not read**; a name that would pass 255 characters throws. Nothing here has touched iCloud.
 */
class CloudKitObjectStore(
    private val transport: HttpTransport,
    private val container: String,
    private val credentials: CloudKitCredentials,
    /** How long one call may take. The host decides it, so no figure is set here. */
    private val timeoutSeconds: Int,
    /** `development` or `production`; the page names both. */
    private val environment: String = "production",
    /** `private` for the person's own data (the default), `public` or `shared`. */
    private val database: String = "private",
    private val baseUrl: String = "https://api.apple-cloudkit.com",
    /** Told the `ckWebAuthToken` a JSON reply carried, if any (see the class note). */
    private val onWebAuthToken: suspend (String) -> Unit = {},
) : ObjectStore {
    private enum class Outcome { CREATED, EXISTS }

    private suspend fun post(subpath: String, body: JsonObject): Pair<HttpResponse, JsonObject?> {
        val (api, web) = credentials.tokens()
        val q = "ckAPIToken=${percentEncode(api)}" + (web?.let { "&ckWebAuthToken=${percentEncode(it)}" } ?: "")
        val r = transport.call(
            HttpRequest(
                url = "$baseUrl/database/1/${percentEncode(container)}/$environment/$database/$subpath?$q",
                headers = mapOf("Content-Type" to "application/json"),
                body = body.toString().encodeToByteArray(),
                method = "POST",
                timeoutSeconds = timeoutSeconds,
            ),
        )
        val json = parse(r)
        json?.get("ckWebAuthToken")?.jsonPrimitive?.contentOrNull?.let { onWebAuthToken(it) }
        return r to json
    }

    private fun parse(r: HttpResponse): JsonObject? = try {
        Json.parseToJsonElement(r.body.decodeToString()).jsonObject
    } catch (e: Exception) {
        null
    }

    /** The error code of a reply: the request's own, else the first operation entry's. */
    private fun errorCode(json: JsonObject?, entry: JsonObject?): String? =
        json?.get("serverErrorCode")?.jsonPrimitive?.contentOrNull ?: entry?.get("serverErrorCode")?.jsonPrimitive?.contentOrNull

    private fun firstRecord(json: JsonObject?): JsonObject? =
        (json?.get("records") as? JsonArray)?.firstOrNull() as? JsonObject

    private fun recordName(name: String): String {
        val encoded = percentEncode(name).let { if (it.startsWith("_")) "%5F" + it.drop(1) else it }
        if (encoded.length > MAX_NAME) throw CloudStoreException("'$name' is too long for a record name")
        return encoded
    }

    private suspend fun uploadAsset(rn: String, bytes: ByteArray): JsonObject {
        if (bytes.isEmpty()) throw CloudStoreException("an empty object cannot be stored as an asset")
        if (bytes.size > MAX_BYTES) throw CloudStoreException("an asset is at most $MAX_BYTES bytes in a CloudKit upload; this one is ${bytes.size}")
        val (r, json) = post(
            "assets/upload",
            buildJsonObject {
                putJsonArray("tokens") { add(buildJsonObject { put("recordName", rn); put("recordType", RECORD_TYPE); put("fieldName", FIELD) }) }
            },
        )
        val url = try {
            ((json!!["tokens"] as JsonArray)[0] as JsonObject)["url"]!!.jsonPrimitive.content
        } catch (e: Exception) {
            throw CloudStoreException("asset upload for '$rn' got no URL: ${errorCode(json, null) ?: r.status}", r.status)
        }
        val up = transport.call(
            HttpRequest(url, mapOf("Content-Type" to "application/octet-stream"), bytes, "POST", timeoutSeconds),
        )
        return try {
            parse(up)!!["singleFile"]!!.jsonObject
        } catch (e: Exception) {
            throw CloudStoreException("asset upload of '$rn' answered ${up.status} with no asset dictionary", up.status)
        }
    }

    private suspend fun write(name: String, bytes: ByteArray, operation: String): Outcome {
        val rn = recordName(name)
        val asset = uploadAsset(rn, bytes)
        val (r, json) = post(
            "records/modify",
            buildJsonObject {
                putJsonArray("operations") {
                    add(
                        buildJsonObject {
                            put("operationType", operation)
                            putJsonObject("record") {
                                put("recordName", rn)
                                put("recordType", RECORD_TYPE)
                                putJsonObject("fields") { putJsonObject(FIELD) { put("value", asset) } }
                            }
                            putJsonArray("desiredKeys") {}
                        },
                    )
                }
            },
        )
        val code = errorCode(json, firstRecord(json))
        return when {
            code == "EXISTS" -> Outcome.EXISTS
            code != null -> throw CloudStoreException("write '$name' refused: $code", r.status)
            r.status == 200 && firstRecord(json)?.get("recordName") != null -> Outcome.CREATED
            else -> throw CloudStoreException("write '$name' answered ${r.status} with no record", r.status)
        }
    }

    override suspend fun get(name: String): ByteArray? {
        val (r, json) = post(
            "records/lookup",
            buildJsonObject {
                putJsonArray("records") { add(buildJsonObject { put("recordName", recordName(name)) }) }
                putJsonArray("desiredKeys") { add(JsonPrimitive(FIELD)) }
            },
        )
        val entry = firstRecord(json)
        when (val code = errorCode(json, entry)) {
            null -> {}
            "NOT_FOUND" -> return null
            else -> throw CloudStoreException("get '$name' refused: $code", r.status)
        }
        if (r.status != 200) throw CloudStoreException("get '$name' answered ${r.status}", r.status)
        val url = try {
            entry!!["fields"]!!.jsonObject[FIELD]!!.jsonObject["value"]!!.jsonObject["downloadURL"]!!.jsonPrimitive.content
        } catch (e: Exception) {
            throw CloudStoreException("get '$name' found a record with no download URL")
        }
        // Asset URLs are said (from memory, not the pages read) to carry a ${f} placeholder for the file name.
        val d = transport.call(HttpRequest(url.replace("\${f}", FIELD), emptyMap(), ByteArray(0), "GET", timeoutSeconds))
        if (d.status != 200) throw CloudStoreException("get '$name' download answered ${d.status}", d.status)
        return d.body
    }

    override suspend fun put(name: String, bytes: ByteArray) {
        write(name, bytes, "forceReplace")
    }

    override suspend fun createIfAbsent(name: String, bytes: ByteArray): Boolean = write(name, bytes, "create") == Outcome.CREATED

    override suspend fun delete(name: String) {
        val (r, json) = post(
            "records/modify",
            buildJsonObject {
                putJsonArray("operations") {
                    add(
                        buildJsonObject {
                            put("operationType", "forceDelete")
                            putJsonObject("record") { put("recordName", recordName(name)) }
                        },
                    )
                }
            },
        )
        val code = errorCode(json, firstRecord(json))
        if (code != null && code != "NOT_FOUND") throw CloudStoreException("delete '$name' refused: $code", r.status)
        if (code == null && r.status != 200) throw CloudStoreException("delete '$name' answered ${r.status}", r.status)
    }

    override suspend fun list(prefix: String): List<String> {
        val names = mutableListOf<String>()
        var marker: String? = null
        do {
            val (r, json) = post(
                "records/query",
                buildJsonObject {
                    putJsonObject("query") {
                        put("recordType", RECORD_TYPE)
                        if (prefix.isNotEmpty()) {
                            putJsonArray("filterBy") {
                                add(
                                    buildJsonObject {
                                        put("systemFieldName", "recordName")
                                        put("comparator", "BEGINS_WITH")
                                        putJsonObject("fieldValue") { put("value", recordName(prefix)) }
                                    },
                                )
                            }
                        }
                    }
                    putJsonArray("desiredKeys") {}
                    marker?.let { put("continuationMarker", it) }
                },
            )
            val code = errorCode(json, null)
            if (code != null) throw CloudStoreException("list '$prefix' refused: $code", r.status)
            if (r.status != 200 || json == null) throw CloudStoreException("list '$prefix' answered ${r.status}", r.status)
            for (e in json["records"]?.jsonArray ?: buildJsonArray {}) {
                val o = e as JsonObject
                o["serverErrorCode"]?.let { throw CloudStoreException("list '$prefix' had a record in error: ${it.jsonPrimitive.content}", r.status) }
                names += percentDecode(o["recordName"]!!.jsonPrimitive.content)
            }
            marker = json["continuationMarker"]?.jsonPrimitive?.contentOrNull
        } while (marker != null)
        return names.filter { it.startsWith(prefix) }
    }

    /** Web Services has no quota call in the pages read, so the free space is unknown. */
    override suspend fun freeSpace(): Long? = null

    internal companion object {
        const val RECORD_TYPE = "NpObject"
        const val FIELD = "data"

        /** The documented asset upload ceiling is "15 MB"; read as decimal megabytes, the smaller reading. */
        const val MAX_BYTES = 15_000_000
        const val MAX_NAME = 255
    }
}
