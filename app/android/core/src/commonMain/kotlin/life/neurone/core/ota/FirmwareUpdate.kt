package life.neurone.core.ota

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json
import life.neurone.core.models.FirmwareVersion
import life.neurone.core.net.HttpRequest
import life.neurone.core.net.HttpTransport

// Port of app/ios/NeurOne/OTA/FirmwareUpdateService.swift and the manifest and image models of OTAModels.swift.
//
// Privacy design (NP-PRIV-REM-001): the manifest request carries no device id, user id, firmware version or any
// user-correlated datum, so the server sees the same request from every client and cannot tell devices or users apart. The
// image URL is the canonical static path derived from a validated version string: no presigned, personalised or expiring
// URL. The User-Agent is a static string. The host is certificate-pinned by the transport it is given, so a manifest cannot be
// substituted to suppress a security update; the hub independently verifies the Ed25519 signature of the image either way.

/** Where the firmware is distributed from, and what the app trusts about it. */
object FirmwareConfig {
    /** `firmware.neurone.ai` is a placeholder until the domain, the CDN and the signing key exist (`UC-102`). */
    const val HOST = "firmware.neurone.ai"
    const val MANIFEST_URL = "https://$HOST/manifest.json"
    const val USER_AGENT = "NeurOne-OTA/1.0"

    fun imageUrl(version: FirmwareVersion): String = "https://$HOST/firmware/main-$version.npfw"

    // PLACEHOLDERS — replace with the production SPKI SHA-256 pins of the firmware host's certificates (a primary and a
    // backup, so a key can be rotated without a forced app update) before the first release. While they stand, no connection to
    // the host verifies, so no manifest and no image is ever taken (the safe direction).
    val PINNED_SPKI_HASHES: List<String> = listOf(
        "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
        "sha256/BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=",
    )

    // PLACEHOLDER — the fingerprint of the manufacturing root key in the bootloader, in lower-case hex. An image whose manifest
    // names another key is never sent to the hub (ISC-113). Replace at secure build time.
    val TRUSTED_KEY_FINGERPRINTS: Set<String> = setOf("4e455550deadbeef")

    const val MANIFEST_TIMEOUT_SECONDS = 30
    const val IMAGE_TIMEOUT_SECONDS = 300
}

/** The manifest the distribution endpoint serves. */
@Serializable
data class FirmwareManifest(
    val version: String,
    /** ISO 8601 date, `yyyy-MM-dd`. */
    val releaseDate: String,
    val imageSizeBytes: Int,
    /** Expected SHA-256 of the firmware binary. The hub verifies the image; the app does not recompute this (as the Swift app did not). */
    val sha256Hex: String,
    val ed25519KeyFingerprint: String,
    val isSafetyMCUFirmware: Boolean,
    val releaseNotes: String,
) {
    /** The image to show the person, or null if the version or the date is malformed. */
    fun toFirmwareImage(): FirmwareImage? {
        if (FirmwareVersion.parse(version) == null || !DATE.matches(releaseDate)) return null
        return FirmwareImage(version, releaseDate, imageSizeBytes, ed25519KeyFingerprint, isSafetyMCUFirmware, releaseNotes)
    }
}

private val DATE = Regex("""\d{4}-\d{2}-\d{2}""")

/** A firmware image as the person sees it before confirming. */
data class FirmwareImage(
    val version: String,
    val buildDate: String,
    val imageSizeBytes: Int,
    /** Hex; shown to the person before they confirm. */
    val ed25519PublicKeyFingerprint: String,
    /** The safety MCU's firmware: it needs its own explicit confirmation. */
    val isSafetyMCUFirmware: Boolean,
    val releaseNotes: String,
)

class FirmwareUpdateException(message: String) : Exception(message)

interface FirmwareUpdateProviding {
    suspend fun fetchManifest(): FirmwareManifest
    suspend fun downloadFirmware(version: String, expectedSizeBytes: Int): ByteArray
}

/** The production source: a GET through the host's pinned transport. Every failure is a [FirmwareUpdateException]. */
class FirmwareUpdateService(private val transport: HttpTransport) : FirmwareUpdateProviding {

    override suspend fun fetchManifest(): FirmwareManifest {
        val answer = transport.post(get(FirmwareConfig.MANIFEST_URL, FirmwareConfig.MANIFEST_TIMEOUT_SECONDS))
            ?: throw FirmwareUpdateException("no answer")
        if (answer.status != 200) throw FirmwareUpdateException("HTTP ${answer.status}")
        return try {
            JSON.decodeFromString(FirmwareManifest.serializer(), answer.body.decodeToString())
        } catch (e: Exception) {
            throw FirmwareUpdateException("malformed manifest")
        }
    }

    override suspend fun downloadFirmware(version: String, expectedSizeBytes: Int): ByteArray {
        val parsed = FirmwareVersion.parse(version) ?: throw FirmwareUpdateException("invalid version")
        val answer = transport.post(get(FirmwareConfig.imageUrl(parsed), FirmwareConfig.IMAGE_TIMEOUT_SECONDS))
            ?: throw FirmwareUpdateException("no answer")
        if (answer.status != 200) throw FirmwareUpdateException("HTTP ${answer.status}")
        if (answer.body.size != expectedSizeBytes) throw FirmwareUpdateException("size mismatch")
        return answer.body
    }

    private fun get(url: String, timeoutSeconds: Int) = HttpRequest(
        url, mapOf("User-Agent" to FirmwareConfig.USER_AGENT), ByteArray(0), method = "GET", timeoutSeconds = timeoutSeconds,
    )

    private companion object {
        val JSON = Json { ignoreUnknownKeys = true }
    }
}
