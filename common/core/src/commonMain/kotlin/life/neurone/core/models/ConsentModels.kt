package life.neurone.core.models

import kotlinx.serialization.Serializable

// Clinical consent engine types — CLAUDE.md §6. Port of iOS ConsentModels.swift.
// Use cases map to minimum necessary UHDR elements; users select use cases,
// not data elements.

// `ClinicianUseCaseTier` is generated: see ClinicianUseCaseTier.generated.kt.
// `UHDRElement` is generated: see UHDRElement.generated.kt.

/**
 * One access decision on a clinician grant: a set of UHDR elements, the day from which the
 * clinician may see data carrying them, and — as a **separate** decision — whether that approval
 * also reached backwards over data already recorded.
 *
 * CLAUDE.md §6.1 requires retroactive and prospective access to be presented as separate consent
 * decisions "even if made simultaneously". They therefore cannot collapse into one field on the
 * grant: [ClinicianUseCaseTier.uhdrElements] is timeless, so raising a grant's tier by itself
 * hands the clinician the new elements over every session ever recorded. A user who approves an
 * expansion going forward and refuses it over history has taken a position the record has to be
 * able to hold, and a tier alone cannot hold it.
 *
 * Day granularity, like every other date on this model — consistent with SessionRecord and with
 * TIME-01's discipline of not storing finer time than a decision needs.
 */
@Serializable
data class ClinicianAccessScope(
    /** The elements this one decision covers — for an expansion, only the newly added ones. */
    val elements: Set<UHDRElement>,
    /** Prospective decision: data recorded on or after this day is in scope. "yyyy-MM-dd". */
    val effectiveFromDay: String,
    /** Retroactive decision, taken separately: data recorded BEFORE that day is in scope too. */
    val includesPriorData: Boolean,
)

@Serializable
data class ClinicianConsentGrant(
    val id: String,
    val clinicianName: String,
    val clinicianOrganization: String,
    val tier: ClinicianUseCaseTier,
    val grantedAtDay: String,      // "yyyy-MM-dd" — day granularity, consistent with SessionRecord
    /**
     * The last day this grant is in force ("yyyy-MM-dd"). §5.1 makes clinician access time-limited, so a grant
     * needs one: `ConsentStore.grantClinicianAccess` refuses a grant without it, and a stored grant that has none
     * (written before `OI-PRIV-06`) is not in force. Read it through [isInForce].
     */
    val expiresAtDay: String? = null,
    val isActive: Boolean = true,
    /**
     * The access decisions this grant is made of, newest last. Null because a grant written
     * before the §6.1 expansion workflow existed has none; null is "never went through the
     * workflow", which is not the same as "went through it and came out with no access".
     * Read it through [effectiveScopes], never directly.
     */
    val accessScopes: List<ClinicianAccessScope>? = null,
    /**
     * The use cases the user actually consented to ([life.neurone.core.consent.ConsentEngine]
     * library IDs).
     *
     * §6.1's principle is that *clinicians select use cases, never data elements*, so this is the
     * record of what was asked in the user's own terms. It is **not** what decides access —
     * [accessScopes] is, and it carries the elements as derived on the day of the grant. Keeping
     * both means a later edit to the library cannot reach backwards into a grant already made,
     * and a grant can still be explained in the words it was granted in.
     *
     * Null for a grant written before `OI-CONSENT-04`, the same "never went through it"
     * convention [accessScopes] uses.
     */
    val useCaseIds: List<String>? = null,
) {
    /**
     * Whether the clinician may see anything under this grant on [today] ("yyyy-MM-dd"): it is active, it has an
     * expiry, and that day has not passed. The expiry day itself is the last day in force, which is what
     * "Expires <day>" on the dashboard says.
     *
     * **Every read of what a grant lets a clinician see or do goes through this** (`REQ-CONSENT-18`):
     * [elementsVisibleForDataRecordedOn], `ConsentEngine.accessDifferential` and `ConsentStore.approveExpansion`.
     * [approvedElements] and [forwardOnlyElements] are the record of what was decided, which the dashboard still
     * shows for a lapsed grant, and are not such a read.
     */
    fun isInForce(today: String): Boolean {
        val expiry = expiresAtDay ?: return false
        return isActive && today <= expiry
    }

    /**
     * The scopes as decided. For a grant built with none, the tier's elements from the grant day onwards,
     * **with no earlier sessions**.
     *
     * The fallback used to include prior data, so that a grant written before the §6.1 workflow kept what a
     * bare [tier] had meant. No such grant exists: nothing was deployed before the workflow (the principal,
     * 2026-11-18; `OI-PRIV-06` point 6). The fallback is therefore reached only by a grant made in code without
     * a scope, where nobody was asked the history question, and it takes the answer that reaches least
     * (`REQ-CONSENT-16` c). Every grant the form makes carries the person's own answer.
     */
    val effectiveScopes: List<ClinicianAccessScope>
        get() = accessScopes ?: listOf(
            ClinicianAccessScope(
                elements = tier.uhdrElements,
                effectiveFromDay = grantedAtDay,
                includesPriorData = false,
            ),
        )

    /**
     * Everything this grant reaches, ignoring when the data was recorded. This is the dashboard's
     * "what can they see" line; for the time-aware answer use [elementsVisibleForDataRecordedOn].
     */
    val approvedElements: Set<UHDRElement>
        get() = effectiveScopes.flatMap { it.elements }.toSet()

    /**
     * Elements the clinician may see, on [today], in data recorded on [day] ("yyyy-MM-dd") — the
     * predicate that makes the retroactive decision mean something. A scope contributes when the
     * data is on or after its effective day, or when the user separately approved prior data for
     * it. Nothing is visible under a grant that is not in force on [today] ([isInForce]).
     *
     * ISO-8601 day strings compare correctly as strings, which is why they are stored that way.
     */
    fun elementsVisibleForDataRecordedOn(day: String, today: String): Set<UHDRElement> =
        if (!isInForce(today)) {
            emptySet()
        } else {
            effectiveScopes
                .filter { day >= it.effectiveFromDay || it.includesPriorData }
                .flatMap { it.elements }
                .toSet()
        }

    /**
     * Elements this grant reaches only from some day onwards — what the user kept out of the
     * clinician's view of their earlier sessions. Surfaced on the dashboard so a "going forward
     * only" answer stays visible after the decision, rather than vanishing into the record.
     */
    val forwardOnlyElements: Set<UHDRElement>
        get() = effectiveScopes.filterNot { it.includesPriorData }.flatMap { it.elements }.toSet()
}

/**
 * A clinician's request to widen an existing grant, held until the user decides.
 *
 * §6.1's workflow is *differential consent document → persistent user notification → user
 * approves / denies / asks questions → retroactive access is a separate decision*. This type is
 * the persistent half: it survives process death because a notification the user can lose by
 * backgrounding the app is not a notification, and it stays pending after a question is sent,
 * because asking is not deciding.
 */
@Serializable
data class ClinicianAccessExpansionRequest(
    val id: String,
    val grantId: String,
    val fromTier: ClinicianUseCaseTier,
    val toTier: ClinicianUseCaseTier,
    val requestedAtDay: String,
    val decision: ExpansionDecision? = null,
) {
    /**
     * The two §6.1 decisions are kept apart in the record, not merged into one Boolean:
     * [ExpansionDecision.Approved] carries the prospective answer, and its [includesPriorData]
     * carries the retroactive one that was asked separately after it.
     */
    @Serializable
    sealed interface ExpansionDecision {
        @Serializable
        data class Approved(val decidedOnDay: String, val includesPriorData: Boolean) :
            ExpansionDecision

        @Serializable
        data class Denied(val decidedOnDay: String) : ExpansionDecision

        @Serializable
        data class QuestionSent(val sentOnDay: String) : ExpansionDecision
    }

    /**
     * Still awaiting a decision. A sent question leaves the request pending — the user has not
     * approved or denied anything, and the notification must not disappear as though they had.
     */
    val isPending: Boolean
        get() = decision == null || decision is ExpansionDecision.QuestionSent

    val questionSentOnDay: String?
        get() = (decision as? ExpansionDecision.QuestionSent)?.sentOnDay
}

enum class ContactFrequency { WEEKLY, MONTHLY, QUARTERLY }

/** How the person asked to be reached about research (§6.2 L1): the two channels the L1 screen names. */
enum class ContactChannel { EMAIL, PHONE }

/**
 * A research contact method the app can use: a channel and a value in that channel's form
 * (`REQ-CONSENT-23`). The L1 field takes "email or phone", so the channel is read from what was typed.
 *
 * It used to be stored as whatever was typed, so the field could hold anything, including text about
 * the person's health, and L1 could be on with nothing in it (`OI-PRIV-06` point 8).
 */
data class ResearchContact(val channel: ContactChannel, val value: String) {
    companion object {
        /** RFC 5321 §4.5.3.1.3: a path is at most 256 octets with its angle brackets, so an address is at most 254. */
        private const val MAX_EMAIL_LENGTH = 254

        /** ITU-T E.164: an international number has at most 15 digits. */
        private const val MAX_PHONE_DIGITS = 15

        private val phoneSeparators = setOf(' ', '-', '(', ')', '.')

        /**
         * [text] as a contact, or null if it is neither an email address nor a phone number. An address has
         * one `@`, something before it, a dotted domain after it and no whitespace. A number is digits with
         * an optional leading `+`, once spaces, hyphens, dots and brackets are dropped; it is stored without
         * them.
         */
        fun parse(text: String): ResearchContact? {
            val trimmed = text.trim()
            if ('@' in trimmed) {
                val local = trimmed.substringBefore('@')
                val domain = trimmed.substringAfter('@')
                val ok = trimmed.length <= MAX_EMAIL_LENGTH && trimmed.none { it.isWhitespace() } &&
                    local.isNotEmpty() && '@' !in domain &&
                    domain.split('.').let { labels -> labels.size >= 2 && labels.all { it.isNotEmpty() } }
                return if (ok) ResearchContact(ContactChannel.EMAIL, trimmed) else null
            }
            val compact = trimmed.filterNot { it in phoneSeparators }
            val digits = compact.removePrefix("+")
            val ok = digits.isNotEmpty() && digits.length <= MAX_PHONE_DIGITS && digits.all { it in '0'..'9' }
            return if (ok) ResearchContact(ContactChannel.PHONE, compact) else null
        }
    }
}

/**
 * When a consent was last withdrawn: the day, and how many times that one decision has been withdrawn,
 * as `yyyy-MM-dd#NNNNNN` (`REQ-CONSENT-24`, `SWR-MG-04`).
 *
 * A withdrawal has to be ordered against another withdrawal of the same decision, on this phone or on
 * the person's other one, and nothing else. A day and a count do that: the text orders by day and then
 * by count, the count travels in a migration bundle so the two phones count the same decision, and two
 * equal marks mean both sides already stopped. It used to be a UTC instant to the second, which is finer
 * time than the decision needs (`OI-PRIV-06` point 7).
 */
object WithdrawalMark {
    private const val COUNT_DIGITS = 6
    private const val MAX_COUNT = 999_999

    /** The mark for a withdrawal on [today], after [previous] (the mark this decision already holds, if any). */
    fun next(previous: String?, today: String): String {
        val count = ((previous?.let(::countOf) ?: 0) + 1).coerceAtMost(MAX_COUNT)
        return today + "#" + count.toString().padStart(COUNT_DIGITS, '0')
    }

    fun dayOf(mark: String): String = mark.substringBefore('#')

    fun countOf(mark: String): Int = mark.substringAfter('#', "").toIntOrNull() ?: 0

    /** [mark] has the one form that orders as text: a real day, `#`, and a six-digit count above zero. */
    fun isValid(mark: String): Boolean {
        val day = dayOf(mark)
        val count = mark.substringAfter('#', "")
        return runCatching { kotlinx.datetime.LocalDate.parse(day).toString() == day }.getOrDefault(false) &&
            count.length == COUNT_DIGITS && count.all { it in '0'..'9' } && count.toInt() > 0
    }
}

/**
 * The four consent layers (CLAUDE.md §6.2).
 *
 * Layers are not screens: since Rev 37 they are presented across two screens
 * (S1 = L4 + L1, S2 = L2 + L3), but the layer identities are what this type, the
 * withdrawal surfaces and the document set all key on.
 *
 * L2 and L3 are orthogonal and must never be collapsed into one field. L2 is **scope**
 * (which research areas); L3 is **posture** (ask-me-each-time versus pre-approved).
 * "All nine categories, but ask me about each study" is a real and distinct position,
 * expressible only while both survive (§6.2.2).
 */
@Serializable
data class ResearchConsentState(
    // L1: Contact consent
    val contactConsentGranted: Boolean = false,
    /**
     * The contact method, in the form [ResearchContact.parse] stores, or empty. `ConsentStore` writes it
     * with [contactChannel]: a commit with L1 on and no usable method does not turn L1 on, and turning L1
     * off erases the method (`REQ-CONSENT-23`).
     */
    val contactMethod: String = "",
    val contactChannel: ContactChannel? = null,
    val contactFrequency: ContactFrequency = ContactFrequency.MONTHLY,
    // `isPoaHolder` was here with no reader and no writer (`OI-PRIV-06` point 8). The POA workflow is not built
    // (`REQ-CONSENT-17`); its fields arrive with it. A stored record that still carries the key decodes without it.

    // L2: Category consent (9 research categories)
    val categoryConsents: Map<ResearchCategory, Boolean> =
        ResearchCategory.entries.associateWith { false },

    // L3: Blanket consent
    val blanketConsentGranted: Boolean = false,
    /**
     * The day the user last withdrew blanket consent, or null if they never have.
     *
     * **"Never asked" and "said stop" are different states, and §6.0 treats them differently.**
     * `blanketConsentGranted == false` is true of both, so the flag alone cannot tell them apart —
     * the same shape as §6.2.5's teardown guard, which keys on the transition rather than the value
     * for exactly this reason. Set by `ConsentStore` on the true→false transition and by nothing
     * else: the consent UI commits a whole [ResearchConsentState] and cannot see a transition, and
     * a screen able to clear this could erase the record of the user having said stop.
     */
    val blanketConsentWithdrawnOnDay: String? = null,

    // L4: Results + community
    val resultsOptIn: Boolean = false,
    val suggestionPortalOptIn: Boolean = false,

    /**
     * The [WithdrawalMark] (day and count, `yyyy-MM-dd#NNNNNN`) of each consent's last withdrawal, or null if it never
     * was withdrawn (`SWR-MG-04`).
     * Like [blanketConsentWithdrawnOnDay] these are the store's to set on a true→false transition and nothing else's, and
     * a later re-grant does not clear them: a withdrawal is a record that stays (ruling, issue #569). They exist so a
     * restore can carry "said stop" to another phone, where the later time wins and a grant is never added. They are
     * UHDR-class and never reach SHDR or analytics.
     */
    val blanketConsentWithdrawnAt: String? = null,
    val contactConsentWithdrawnAt: String? = null,
    val resultsOptInWithdrawnAt: String? = null,
    val suggestionPortalOptInWithdrawnAt: String? = null,
    val categoryWithdrawnAt: Map<ResearchCategory, String> = emptyMap(),
) {
    val hasAnyResearchConsent: Boolean
        get() = contactConsentGranted || blanketConsentGranted ||
            categoryConsents.values.any { it }

    /**
     * The user withdrew blanket consent and has not re-granted it.
     *
     * §6.0: withdrawing L3 "stops **ALL** research data flows" — all of them, not only the
     * pre-approved ones. So this outranks whatever L2 categories are still ticked: withdrawal does
     * not un-tick them, and nine stale checkboxes are not a fresh decision to resume. Re-granting
     * blanket consent clears the condition, because that *is* a fresh decision.
     */
    val blanketConsentWithdrawn: Boolean
        get() = blanketConsentWithdrawnOnDay != null && !blanketConsentGranted

    /**
     * True when every research category is selected. Drives the Select-all affordance's
     * checked state (§6.2.3). Says nothing about [blanketConsentGranted] — selecting all
     * nine categories is L2 scope, not L3 posture.
     */
    val allCategoriesSelected: Boolean
        get() = ResearchCategory.entries.all { categoryConsents[it] == true }

    /**
     * Set or clear all nine categories at once.
     *
     * Deliberately does NOT touch [blanketConsentGranted] in either direction. Ticking nine
     * boxes expresses breadth of interest; the blanket toggle surrenders the right to be
     * consulted, and inferring the second from the first attributes to the user a decision
     * they did not make (§6.2.3). Coupling them here would also mean a later un-tick could
     * trigger the research-analytics teardown that only blanket withdrawal should cause.
     */
    fun withAllCategories(granted: Boolean): ResearchConsentState =
        copy(categoryConsents = ResearchCategory.entries.associateWith { granted })
}

/**
 * The nine research areas of §6.2's L2 layer.
 *
 * [wireName] is the **identity**: it is what a signed study descriptor names a category by, and it
 * is covered by the signature, so changing one invalidates every descriptor already issued.
 * [displayName] happens to return the same string today and must not be assumed to keep doing so —
 * `:core` cannot yet reference `R.string` (CLAUDE.md §17, `check-locale-strings.ts` PENDING_PATHS),
 * and when it can, the display side becomes a key lookup while the wire side stays put.
 */
enum class ResearchCategory(val wireName: String) {
    ALZHEIMERS_AND_DEMENTIA("Alzheimer's / Dementia"),
    DEPRESSION("Depression"),
    PTSD("PTSD"),
    TBI("Traumatic Brain Injury"),
    SLEEP("Sleep Disorders"),
    ATTENTION("Attention / ADHD"),
    PARKINSONS("Parkinson's Disease"),
    HEALTHY_AGEING("Healthy Ageing"),
    VISUAL_HEALTH("Visual Health"),
    ;

    /** Display text. Same string as [wireName] today; see the note on this enum. */
    val displayName: String get() = wireName
}

/**
 * One study this device has joined, and what it has sent to it.
 *
 * §5.3's audit trail is four fields: study ID, descriptor hash, the day an extract was transmitted and its byte
 * count. The last two say something was sent, so they are empty until something is. They used to be written at
 * enrolment, as that day and 0 bytes, which recorded a transmission that had not happened (`OI-PRIV-06` point 4).
 * No extractor exists yet (`OI-CONSENT-07`), so today every record has [transmittedAtDay] null.
 *
 * [enrolledOnDay] and [withdrawnAtDay] are not in §5.3's list: they are the person's decisions about a study.
 * Nothing uploads this record today, and whether any of it may leave the device as SHDR is `OI-PRIV-07`.
 */
@Serializable
data class StudyParticipationRecord(
    val id: String,
    val studyId: String,
    val descriptorHash: String,     // SHA-256 of signed study descriptor
    /** The day the device joined the study. Null only in a record stored before `OI-PRIV-06`, until it is read. */
    val enrolledOnDay: String? = null,
    /** The day an extract was last transmitted, or null if none has been. Day granularity. */
    val transmittedAtDay: String? = null,
    /** Bytes transmitted so far. 0 with [transmittedAtDay] null means nothing has been sent. */
    val extractBytes: Long = 0,
    /**
     * The day this participation stopped, or null while it is active. Written by `ConsentStore` and nothing else:
     * on a withdrawal from the study, and on a blanket or category withdrawal that leaves the study without a
     * consent covering it (`REQ-CONSENT-19`).
     */
    val withdrawnAtDay: String? = null,
) {
    val isActive: Boolean get() = withdrawnAtDay == null

    /**
     * A record stored before `OI-PRIV-06` carries its enrolment day in [transmittedAtDay] with 0 bytes. Read as
     * what it was: enrolled that day, nothing sent.
     */
    fun normalised(): StudyParticipationRecord =
        if (enrolledOnDay == null && extractBytes == 0L) {
            copy(enrolledOnDay = transmittedAtDay, transmittedAtDay = null)
        } else {
            this
        }
}

/**
 * A stored consent record the app could not read, kept as it was found (`REQ-CONSENT-20`).
 *
 * A record that fails to decode used to fall back to defaults, and the next save wrote those defaults over it:
 * a person who had said stop became a person who was never asked. The raw text is kept here instead, and for the
 * records research admission depends on, nothing is admitted until the person decides again ([redecidedOnDay]).
 */
@Serializable
data class QuarantinedConsentBlob(
    /** The storage key the record was under. */
    val key: String,
    /** The record exactly as stored. UHDR-class, like the record it came from; it never leaves the device. */
    val blob: String,
    val quarantinedOnDay: String,
    /** The day the person next committed a research-consent decision, or null if they have not yet. */
    val redecidedOnDay: String? = null,
)

// ── Signed study descriptor (§5.3 step 1) ────────────────────────────────────

/**
 * A study as NeurOne's research service issues it: what it wants, how it undertakes to anonymise
 * it, and a signature over all of that.
 *
 * **This, not [StudyInvitation], is what enters the device.** The invitation the user reads is
 * *derived* from a descriptor on-device, after the signature verifies — so every sentence on the
 * consent surface is one the app wrote, and the party asking for access supplies facts about the
 * study rather than the prose describing it. The descriptor used to be skipped entirely:
 * `addInvitation` took a finished invitation, plain-language "what they cannot see" list and all,
 * on trust from whatever called it (`OI-CONSENT-03`).
 */
@Serializable
data class StudyDescriptor(
    val studyId: String,
    /**
     * The study's own name. The one field that is server-supplied text and stays that way: it
     * names a specific study the way a part number names a part (CLAUDE.md §17).
     */
    val studyTitle: String,
    val researchCategories: List<ResearchCategory>,
    /**
     * The UHDR elements the study asks for. What it *cannot* see is derived from this, not
     * asserted alongside it — see [StudyInvitation.cannotLearn].
     */
    val requestedElements: Set<UHDRElement>,
    /**
     * §5.3 anonymisation parameters, checked against their floors at ingestion. The device is the
     * only place they can be checked, because §5.3 puts the anonymisation on the device.
     */
    val kAnonymity: Int,
    val dateRoundingDays: Int,
    val issuedOnDay: String,
    /**
     * The last day the device may act on this descriptor ("yyyy-MM-dd"), set and signed by the issuer. After it
     * the descriptor is refused and an invitation made from it can no longer be accepted (`REQ-CONSENT-09` i).
     * The issuer sets it because how long a study recruits is the study's fact; a maximum age chosen on the
     * device would be a number with no derivation (`OI-PRIV-06` point 5).
     */
    val expiresOnDay: String,
    /**
     * Detached signature over the canonical descriptor bytes. Never parsed here: it is the
     * verifier's input, and nothing derived from an unverified descriptor may reach the user.
     */
    val signature: String,
)

// ── Study descriptor verification (§5.3 step 1) ──────────────────────────────

/**
 * The result of checking a descriptor's signature.
 *
 * [Unavailable] is a third answer rather than a flavour of [Rejected] because "we cannot check"
 * and "we checked and it is forged" are different facts about the world, and the device says
 * which. Both refuse ingestion.
 */
sealed interface StudyDescriptorVerification {
    /**
     * Signature checks out. Carries the SHA-256 of the exact bytes that were signed — the §5.3
     * audit-trail hash, produced by the only component that has those bytes.
     */
    data class Verified(val descriptorHash: String) : StudyDescriptorVerification
    data object Rejected : StudyDescriptorVerification

    /** No signing key is configured, so no descriptor can be checked at all. */
    data object Unavailable : StudyDescriptorVerification
}

/**
 * Port for descriptor signature verification. §5.3 locks that study descriptors are
 * cryptographically signed; the key distribution and the fetch that carries them belong to the
 * NeurOne study service, which does not exist yet (`OI-CONSENT-07`).
 */
fun interface StudyDescriptorVerifier {
    fun verify(descriptor: StudyDescriptor): StudyDescriptorVerification
}

/**
 * The default verifier, and the reason `ConsentStore` takes one at all: it verifies nothing and
 * admits nothing.
 *
 * A store that ingested descriptors without a verifier would let whoever builds the transport
 * decide whether §5.3's signature is checked, and the store could not tell a verified descriptor
 * from a fabricated one. With this as the default the decision is not theirs to skip: until a real
 * verifier is injected, ingestion refuses everything and says why.
 */
object RefusingStudyDescriptorVerifier : StudyDescriptorVerifier {
    override fun verify(descriptor: StudyDescriptor): StudyDescriptorVerification =
        StudyDescriptorVerification.Unavailable
}

// ── Study descriptor ingestion outcome (§6.3) ────────────────────────────────

/**
 * What the device did with a descriptor. Every refusal names its reason, because "nothing
 * appeared" is the one outcome a user cannot distinguish from "nothing was sent".
 */
sealed interface StudyDescriptorAdmission {
    /** Admitted, as an invitation of this posture. */
    data class Admitted(
        val posture: StudyInvitation.Posture,
        val descriptorHash: String,
    ) : StudyDescriptorAdmission

    data class Refused(val reason: Reason) : StudyDescriptorAdmission

    enum class Reason {
        /** No signing key configured — the state of every device today (`OI-CONSENT-07`). */
        VERIFIER_UNAVAILABLE,
        SIGNATURE_INVALID,

        /**
         * k < 10 or date rounding < 1 week (§5.3). A study whose anonymisation is below the locked
         * floor is not one the user may be asked to consent to.
         */
        ANONYMISATION_BELOW_FLOOR,

        /**
         * The descriptor's expiry day has passed, is not a day, or is before its issue day
         * (`REQ-CONSENT-09` i). An issuer's undertaking does not stand for ever.
         */
        DESCRIPTOR_EXPIRED,

        /**
         * L1 is the shared precondition for every delivery path, invitations and engagement
         * notifications alike (§6.2.1).
         */
        NO_CONTACT_CONSENT,

        /**
         * The study asks for an element no extract may carry, because it cannot be made k-anonymous (§5.3):
         * `ConsentEngine.ELEMENTS_NO_EXTRACT_MAY_CARRY`. A property of the study, like the floors.
         */
        REQUESTS_ELEMENT_NO_EXTRACT_MAY_CARRY,

        /**
         * A stored consent record could not be read and the person has not decided again since
         * (`REQ-CONSENT-20`). The device cannot tell "never asked" from "said stop", so it admits nothing.
         */
        CONSENT_RECORD_UNREADABLE,

        /** No L2 category on this descriptor is consented, and L3 is off. */
        CATEGORY_NOT_CONSENTED,

        /** The user holds no research consent at any layer. */
        NO_RESEARCH_CONSENT,

        /**
         * The user withdrew blanket consent (§6.0). Distinct from [NO_RESEARCH_CONSENT]: one is a
         * user who was never asked, the other a user who said stop, and only the second is a
         * decision the device is declining to re-open.
         */
        RESEARCH_CONSENT_WITHDRAWN,

        /**
         * This study was already decided or already withdrawn from. §5.3 and §6.3 step 6 make
         * withdrawal block future descriptor processing; re-asking would be the device forgetting
         * an answer the user already gave.
         */
        STUDY_ALREADY_DECIDED,
    }
}

// ── Per-project consent notification (§6.3) ──────────────────────────────────

/**
 * A study as the user sees it — derived from a verified [StudyDescriptor], never received
 * ready-made.
 */
@Serializable
data class StudyInvitation(
    val studyId: String,
    val studyTitle: String,
    val researchCategories: List<ResearchCategory>,
    val approvedElements: Set<UHDRElement>,
    /**
     * SHA-256 of the signed descriptor this was derived from (§5.3 audit trail). Carried onto the
     * participation record on acceptance, which used to record the literal string `"pending"`
     * because there was no descriptor to hash.
     */
    val descriptorHash: String,
    val kAnonymity: Int,
    val dateRoundingDays: Int,
    val posture: Posture,
    val receivedOnDay: String,
    /** The descriptor's expiry day. Null only on an invitation stored before descriptors carried one. */
    val expiresOnDay: String? = null,
    val decision: StudyDecision? = null,
) {
    /**
     * **Which question the device is putting to the user, and what silence means.**
     *
     * §6.2 locks that an L3 user "still receives per-study *engagement* notifications, **not
     * consent requests**" — they pre-approved NeurOne-reviewed research and may opt out per study.
     * So the same study reaches an L2 user and an L3 user as two different things, and the default
     * on no answer is opposite: an unanswered consent request means *not participating*, an unread
     * engagement notification means *participating*.
     *
     * One shape for both would have to pick one of those defaults for everyone. That is the reason
     * this type could not simply be persisted as it was.
     */
    enum class Posture {
        /** L2 path: the user is being asked, and is not in the study until they say yes. */
        CONSENT_REQUEST,

        /**
         * L3 path: the user pre-approved this study, is in it already, and is being told — with a
         * per-study opt-out that goes through `withdrawFromStudy`.
         */
        ENGAGEMENT_NOTIFICATION,
    }

    /** §6.3 step 4's three responses: Yes / No / Ask a question. */
    @Serializable
    sealed interface StudyDecision {
        @Serializable
        data class Accepted(val decidedOnDay: String) : StudyDecision

        @Serializable
        data class Declined(val decidedOnDay: String) : StudyDecision

        @Serializable
        data class QuestionSent(val sentOnDay: String) : StudyDecision
    }

    /**
     * The elements this study cannot see — **derived here, never asserted by the descriptor.**
     *
     * §6.3 step 3 requires the invitation to be "explicit about what researchers CAN and CANNOT
     * see". Taking the second half as prose from the party asking for access lets that party write
     * it, and it drifts from the first half by construction the moment either changes. It is the
     * complement of [approvedElements], so the app computes it.
     */
    val cannotLearn: Set<UHDRElement>
        get() = UHDRElement.entries.toSet() - approvedElements

    /**
     * Still in the user's inbox. A sent question leaves it open — asking is not deciding, the same
     * rule [ClinicianAccessExpansionRequest.isPending] holds for §6.1.
     */
    val isOpen: Boolean
        get() = decision == null || decision is StudyDecision.QuestionSent

    /**
     * Whether an unanswered invitation means the user is in the study. True only for an engagement
     * notification, where L3 already answered.
     */
    val participatesWithoutAnswer: Boolean
        get() = posture == Posture.ENGAGEMENT_NOTIFICATION

    val questionSentOnDay: String?
        get() = (decision as? StudyDecision.QuestionSent)?.sentOnDay
}
