package life.neurone.core.analytics

/** Which of the warranty owner's two general-consent questions is due (`REQ-SHDR-12` (a), (b)). */
enum class GeneralConsentPromptKind {
    /** Prompt 1: this helmet's characterisation window has ended. */
    WINDOW_ENDED,

    /** Prompt 2: an app update carries a content set the review gate fixed, newer than any this owner was asked about. */
    CONTENT_SET,
}

/**
 * One question due for one helmet's warranty owner. [shown] is the content set the question is about (the one in force in this
 * build); [added] is what it covers that the owner has not said yes to, by item id, empty when nothing is. [reportsOn] is the
 * owner's present answer, which silence keeps.
 */
data class GeneralConsentPrompt(
    val kind: GeneralConsentPromptKind,
    val shown: GeneralShdrContentSet,
    val added: List<String>,
    val reportsOn: Boolean,
)

/** The owner's answer to a prompt. No answer at all (a dismissal, leaving the page, a timeout) is passed as `null`. */
enum class GeneralConsentAnswer {
    /** Yes to general consent at the content shown. */
    AGREE,

    /** No change: the consent stays as it was, at its content level. The default. */
    KEEP,

    /** Stop the reports for this helmet. */
    STOP,
}

/**
 * When the warranty owner's two general-consent prompts are due, and what each answer does (`REQ-SHDR-12`; `NP-FW-NVRAM-001`
 * D-39; the principal, 2026-11-26).
 *
 * - Prompt 1 is due once a helmet's characterisation window has ended ([CharacterisationEnrolmentGate.endWindow]) and its owner
 *   has not yet been asked about it.
 * - Prompt 2 is due when this build's content set is one the review gate fixed (version above the baseline) and is newer than any
 *   set this helmet's owner has been asked about or said yes to. It is due for every owner who has answered the reports question,
 *   yes or no, because the notice is the same for every owner (`CLAUDE.md` §6.3). An owner never asked is asked when they first
 *   turn reports on, with the set in force then, and that grant answers prompt 2 for it.
 * - With both due, prompt 1 is asked, showing the set in force, and its answer answers prompt 2 for that set as well: the owner
 *   is asked about one content once.
 *
 * Each prompt is a fresh question whose default is no change. A dismissal or no answer keeps the consent exactly at its content
 * level. A prompt, once shown, is recorded as asked, so it shows once per owner and helmet. No answer enrols characterisation.
 *
 * A content history that is not well formed ([GeneralShdrContent.isWellFormed]) makes no prompt due and changes nothing.
 *
 * The question is the warranty owner's alone (`CLAUDE.md` §6.0, `REQ-CONSENT-01`): this class must never reference
 * `ConsentStore` or `ResearchAnalyticsGate`, and it holds nothing about a person.
 */
class GeneralShdrConsentPrompts(
    private val records: WarrantyOwnerStore,
    private val warrantyGate: WarrantyAnalyticsGate,
    private val sets: List<GeneralShdrContentSet> = GeneralShdrContent.history,
) {

    /** The prompt due for this helmet's owner, or `null` when none is (or no helmet is named). */
    fun due(hub: String?): GeneralConsentPrompt? {
        if (hub == null || !GeneralShdrContent.isWellFormed(sets)) return null
        val r = records.record(hub) ?: return null
        val set = sets.last()
        // What the owner's present yes covers; with reports off they have agreed to nothing, so everything beyond the baseline is new.
        val added = if (r.fleetConsent) GeneralShdrContent.added(sets, r.contentLevel, set) else set.fields
        val kind = when {
            r.windowEnded && !r.windowAsked -> GeneralConsentPromptKind.WINDOW_ENDED
            r.fleetDecided && set.version > 0 && set.version > r.contentAsked &&
                !(r.fleetConsent && r.contentLevel >= set.version) -> GeneralConsentPromptKind.CONTENT_SET
            else -> return null
        }
        return GeneralConsentPrompt(kind, set, added, r.fleetConsent)
    }

    /**
     * Record the owner's [answer] to [prompt] for this helmet; `null` is no answer, which changes nothing but the record that the
     * prompt was put. The prompt is marked as put when it is first shown ([markShown]), so an answer is taken for a prompt already
     * shown, as long as it still applies: its set is the one in force and, for prompt 1, the helmet's window has ended. Otherwise
     * (a build with another set, a helmet whose record was forgotten since) it returns false and records nothing.
     */
    fun answer(hub: String?, prompt: GeneralConsentPrompt, answer: GeneralConsentAnswer?): Boolean {
        if (hub == null || !GeneralShdrContent.isWellFormed(sets) || prompt.shown != sets.last()) return false
        val r = records.record(hub) ?: return false
        val applies = when (prompt.kind) {
            GeneralConsentPromptKind.WINDOW_ENDED -> r.windowEnded
            GeneralConsentPromptKind.CONTENT_SET -> r.fleetDecided && prompt.shown.version > 0
        }
        if (!applies) return false
        records.update(hub) {
            it.copy(
                windowAsked = it.windowAsked || prompt.kind == GeneralConsentPromptKind.WINDOW_ENDED,
                contentAsked = maxOf(it.contentAsked, prompt.shown.version),
            )
        }
        when (answer) {
            GeneralConsentAnswer.AGREE -> warrantyGate.grant(hub, prompt.shown.version)
            GeneralConsentAnswer.STOP -> warrantyGate.revoke(hub)
            GeneralConsentAnswer.KEEP, null -> Unit
        }
        return true
    }

    /** The prompt has been put to the owner and left unanswered: the same as no answer. */
    fun markShown(hub: String?, prompt: GeneralConsentPrompt): Boolean = answer(hub, prompt, null)
}
