package life.neurone.core.consumable

// Consumable kinds and session-count thresholds. Port of iOS
// ConsumableInventory.swift. Each sessionLimit must equal the threshold in the Trigger cell of
// docs/reference/commercial-model.md §2.3 — scripts/check-consumable-triggers.ts enforces it.
// Session counts come from the hub CONSUMABLE_STATUS GATT characteristic
// (SHDR-class — device condition, no user biology).

enum class ConsumableKind(val rawValue: Int) {
    INTRANASAL_SLEEVES(0),
    ELECTRODE_HYDROGEL(1),
    VNS_PADS(2),
    AUDIO_CUP_FOAM(3);

    val sessionLimit: Int
        get() = when (this) {
            INTRANASAL_SLEEVES -> 1     // single use
            // UNVALIDATED PLACEHOLDER — OI-ACC-09. Midpoint of a 30–60 range with no stated source,
            // and no document names the mechanism this count drives. The intended trigger is an
            // impedance trend (a condition measurement), which nothing implements yet.
            ELECTRODE_HYDROGEL -> 45
            // UNVALIDATED PLACEHOLDER — OI-VNSCLIP-07. Exposure count, mechanism = electrochemical
            // degradation from VNS current; 30 is the midpoint of a quoted 20–40, not a derived life.
            VNS_PADS -> 30
            // UNVALIDATED PLACEHOLDER — OI-ACC-04. Exposure count, mechanism = compression set
            // under wear. The value is NOT derived from that mechanism: it was back-derived from
            // the retired "6–12 months" calendar interval (OI-ACC-02), which CLAUDE.md §2.3 forbids
            // and which this device cannot implement anyway — no VBAT rail, so wall time dies on
            // every disconnect. Kept rather than re-derived, because re-deriving from a void
            // derivation is not an improvement. Same defect class as NP-FW-EMMC-002 §G.2's
            // accelerometer thresholds. To be superseded by a condition measurement — loss of seal
            // read by an in-cup noise-cancelling mic (principal, 2026-09-23; OI-AUDIOHW-08) — not
            // by arithmetic. Comfort/hygiene replacement is the user's call and gets no prompt.
            AUDIO_CUP_FOAM -> 150
        }

    // Remind when this many sessions remain in the current consumable unit.
    // Intranasal is single-use: alert only when at/past limit, not before
    // (iOS lowThreshold bug fix, NP-PRIV-ANALYSIS-003 session — preserved).
    val lowThreshold: Int
        get() = when (this) {
            INTRANASAL_SLEEVES -> 0
            ELECTRODE_HYDROGEL -> 8
            VNS_PADS -> 4
            AUDIO_CUP_FOAM -> 20
        }

    val reminderPriority: ReminderPriority
        get() = when (this) {
            INTRANASAL_SLEEVES -> ReminderPriority.SAFETY_BLOCKING // hygiene — single use
            ELECTRODE_HYDROGEL -> ReminderPriority.PERFORMANCE_CRITICAL
            VNS_PADS -> ReminderPriority.PERFORMANCE_CRITICAL
            AUDIO_CUP_FOAM -> ReminderPriority.COMFORT_LONGEVITY
        }

    // Stable URL slug for the shop product page — survives enum reordering.
    val shopSlug: String
        get() = when (this) {
            INTRANASAL_SLEEVES -> "intranasal-sleeves"
            ELECTRODE_HYDROGEL -> "electrode-hydrogel-tips"
            VNS_PADS -> "vns-clip-pads"
            AUDIO_CUP_FOAM -> "audio-cup-foam"
        }

    val orderUrl: String get() = "https://neurone.life/consumables/$shopSlug"
}

enum class ReminderPriority {
    SAFETY_BLOCKING,      // cannot be dismissed — blocks session start
    PERFORMANCE_CRITICAL, // snooze max 3×
    COMFORT_LONGEVITY,    // snooze max 5×
}

data class ConsumableState(
    val kind: ConsumableKind,
    val sessionCount: Int,   // sessions since last replacement (from hub SHDR)
    val snoozeCount: Int = 0,
) {
    val sessionsRemaining: Int get() = maxOf(0, kind.sessionLimit - sessionCount)
    val isLow: Boolean get() = sessionsRemaining <= kind.lowThreshold
    val isExceeded: Boolean get() = sessionCount >= kind.sessionLimit
    val maxSnooze: Int
        get() = if (kind.reminderPriority == ReminderPriority.COMFORT_LONGEVITY) 5 else 3
    val canSnooze: Boolean
        get() = kind.reminderPriority != ReminderPriority.SAFETY_BLOCKING &&
            snoozeCount < maxSnooze
}

data class ConsumableReminder(val state: ConsumableState) {
    val isBlocking: Boolean
        get() = state.kind.reminderPriority == ReminderPriority.SAFETY_BLOCKING &&
            state.isExceeded
}
