package life.neurone.core.migration

import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.buildJsonObject
import kotlinx.serialization.json.jsonArray
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.put
import life.neurone.core.npps.NppsCore
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse

/** A route says it is paired, or it is not (NP-PRIV-ANALYSIS-007 L19, `OI-PRIV-66`). */
class BackupRouteTests {

    private fun offered(vararg routes: RouteDescriptor): List<String> =
        NppsCore.migration(buildJsonObject {
            put("op", "offerRoutes")
            put("kind", "uhdr")
            put("routes", JsonArray(routes.map { it.toJson() }))
        })["offered"]!!.jsonArray.map { it.jsonPrimitive.content }

    // @verifies SWR-MG-10
    @Test
    fun aRouteThatDoesNotSayItIsPairedIsNotPaired() {
        val bare = object : RouteDescriptor {
            override val id = "bare"
            override val atomicClaim = true
        }
        assertFalse(bare.authenticatedPairing)
        assertFalse(ObjectStoreRoute("s", atomicClaim = true).authenticatedPairing)
        assertFalse(InMemoryBackupRoute().authenticatedPairing)
        assertFalse(ObjectStoreRoute("s", atomicClaim = true).toJson()["authenticatedPairing"]!!.jsonPrimitive.boolean)
    }

    // @verifies SWR-MG-10
    @Test
    fun aDirectRouteWithoutTheFlagIsWithheldAndTheLinkRouteIsOffered() {
        val directUnstated = object : RouteDescriptor {
            override val id = "direct-unstated"
            override val atomicClaim = true
            override val direct = true
        }
        assertEquals(listOf("link", "cloud"), offered(directUnstated, LINK_ROUTE, ObjectStoreRoute("cloud", atomicClaim = true)))
    }
}
