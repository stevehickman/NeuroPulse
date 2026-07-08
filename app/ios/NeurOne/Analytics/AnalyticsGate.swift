// AnalyticsGate.swift — RENAMED
//
// This type has been renamed to ResearchAnalyticsGate to reflect that it governs
// research analytics only (PostHog SDK, gated on research consent), distinct from
// WarrantyAnalyticsGate (SHDR fleet uploads, gated on warranty consent).
//
// See ResearchAnalyticsGate.swift for the authoritative implementation.
// See WarrantyAnalyticsGate.swift for the warranty-consent gate.
//
// This compatibility typealias allows callers that have not yet been migrated to compile.
// Remove this file once all callers reference ResearchAnalyticsGate directly.
typealias AnalyticsGate = ResearchAnalyticsGate
