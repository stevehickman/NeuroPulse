package life.neurone.shared

import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.suspendCancellableCoroutine
import platform.HealthKit.HKAnchoredObjectQuery
import platform.HealthKit.HKHealthStore
import platform.HealthKit.HKMetricPrefixMilli
import platform.HealthKit.HKObjectType
import platform.HealthKit.HKQuantityTypeIdentifierHeartRate
import platform.HealthKit.HKQuantityTypeIdentifierHeartRateVariabilitySDNN
import platform.HealthKit.HKQuantitySample
import platform.HealthKit.HKQuantityType
import platform.HealthKit.HKUnit
import platform.HealthKit.countUnit
import platform.HealthKit.minuteUnit
import platform.HealthKit.secondUnitWithMetricPrefix
import platform.HealthKit.HKObjectQueryNoLimit
import kotlin.coroutines.resume

/**
 * HealthKit, read-only, for the live HRV display of one session: the same rules as the Swift `HealthKitSessionReader` (ISC-94 to
 * ISC-96). Permission is asked only when [start] is called, which the shared app does only while an HRV biofeedback session runs; the
 * share (write) set is always empty and nothing here calls a HealthKit write; the two values are held in memory and cleared by
 * [stopAndClear]; nothing is stored or sent. HealthKit never says whether a read was granted, so a denial shows as no values.
 */
@OptIn(ExperimentalForeignApi::class)
class IosHealthSignals : HealthSignals {
    private val store = HKHealthStore()
    private var queries: List<HKAnchoredObjectQuery> = emptyList()

    private val hrv = MutableStateFlow<Double?>(null)
    private val heartRate = MutableStateFlow<Double?>(null)

    override val hrvSdnnMilliseconds: StateFlow<Double?> = hrv.asStateFlow()
    override val heartRateBpm: StateFlow<Double?> = heartRate.asStateFlow()

    override suspend fun start() {
        if (queries.isNotEmpty() || !HKHealthStore.isHealthDataAvailable()) return
        val hrvType = HKQuantityType.quantityTypeForIdentifier(HKQuantityTypeIdentifierHeartRateVariabilitySDNN) ?: return
        val heartRateType = HKQuantityType.quantityTypeForIdentifier(HKQuantityTypeIdentifierHeartRate) ?: return

        val asked = suspendCancellableCoroutine { done ->
            // Read only: the share set is empty.
            store.requestAuthorizationToShareTypes(emptySet<HKObjectType>(), readTypes = setOf(hrvType, heartRateType)) { ok, _ ->
                done.resume(ok)
            }
        }
        if (!asked) return

        queries = listOf(
            watch(hrvType, HKUnit.secondUnitWithMetricPrefix(HKMetricPrefixMilli), hrv),
            watch(heartRateType, HKUnit.countUnit().unitDividedByUnit(HKUnit.minuteUnit()), heartRate),
        )
    }

    override fun stopAndClear() {
        queries.forEach { store.stopQuery(it) }
        queries = emptyList()
        hrv.value = null
        heartRate.value = null
    }

    /** Publish the newest sample of [type] in [unit] to [into], now and as new ones arrive. */
    private fun watch(type: HKQuantityType, unit: HKUnit, into: MutableStateFlow<Double?>): HKAnchoredObjectQuery {
        val handler: (HKAnchoredObjectQuery?, List<*>?, List<*>?, platform.HealthKit.HKQueryAnchor?, platform.Foundation.NSError?) -> Unit =
            { _, samples, _, _, error ->
                if (error == null) {
                    val latest = samples?.filterIsInstance<HKQuantitySample>()?.maxByOrNull { it.endDate.timeIntervalSinceReferenceDate }
                    if (latest != null) into.value = latest.quantity.doubleValueForUnit(unit)
                }
            }
        val query = HKAnchoredObjectQuery(type, null, null, HKObjectQueryNoLimit, handler)
        query.updateHandler = handler
        store.executeQuery(query)
        return query
    }
}
