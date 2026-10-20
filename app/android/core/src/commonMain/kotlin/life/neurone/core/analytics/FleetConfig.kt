package life.neurone.core.analytics

/**
 * Where the SHDR fleet upload goes and what it is sealed to, for every host (privacy review Finding 6, `OI-FLEET-01` (1),
 * `REQ-SHDR-07`). One definition: the Android app, the desktop app and the iOS host all read it, and
 * `scripts/check-release-pins.ts --release` refuses a release build while any value here is a placeholder.
 *
 * The record is sealed to the fleet *gateway's* key and posted to the *relay*, so the server that reads it never sees the network
 * address, and the relay never sees the record. A host connects to the relay only, so the certificate pins are the relay's.
 */
object FleetConfig {
    /** The fleet service as the gateway forwards to it: the authority and path of the sealed request, never connected to from the phone. */
    const val FLEET_AUTHORITY = "fleet.neurone.life"
    const val FLEET_PATH = "/v1/shdr"
    const val DEVICE_TOKEN_HEADER = "X-NP-Device-Token"

    /** The relay the phone connects to (`NP-INFRA-001` §3.3b). Run by a party other than the gateway's operator; a placeholder until it exists. */
    const val RELAY_HOST = "relay.neurone.internal"
    const val RELAY_URL = "https://$RELAY_HOST/relay"

    /** The gateway's key configuration (hex, one configuration); empty until the gateway exists, and then nothing is uploaded. */
    const val GATEWAY_KEY_CONFIG_HEX = ""

    // PLACEHOLDERS — replace with production SPKI SHA-256 pins of the RELAY's certificates before deploy (a primary and a backup).
    val PINNED_SPKI_HASHES: List<String> = listOf(
        "sha256/AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
        "sha256/BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=",
    )
}
