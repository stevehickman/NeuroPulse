# NeuroPulse Document Front Matter Templates

All `.md` documents in `docs/` must use standardized front matter. The front matter ends at the first horizontal rule (`---`).

## Rules

1. **Title must NOT contain the revision** — the revision is a separate field. The title is stable across revisions.
2. **Field ordering is fixed** — use the order shown in each template below.
3. **All templates share a common base** — the first 12 fields are always present (some may be "N/A" or "—").
4. **Type-specific fields** appear after the base fields and before the horizontal rule.
5. **Bold field labels** — use `**Field:**` format.

## Common Base Fields (all document types, in this order)

```
# {Document Title in Title Case}

**Project:** NeuroPulse
**Document:** {NP-XXX-NNN}
**Revision:** {A, B, C... or Rev A, Rev B...}
**Date:** {YYYY-MM-DD}
**Status:** {DRAFT | ACTIVE | BASELINED | SUPERSEDED | ARCHIVED}
**Effective Date:** {YYYY-MM-DD}
**Author:** {Name (Role)}
**Approved By:** {Name, Title}
**References:** {Comma-separated list of related NP-* documents, standards, issues}
**Related Issues:** {GitHub Issue #NN, PR #NN}
**Gate:** {NP-COORD-001 GN-NN or N/A}
**IEC 62304 Class:** {SW-NN Class X or N/A}
```

## Type-Specific Templates

### 1. QMS Procedure

For: QMS manual, CAPA, design controls, risk management plan, software development plan.

Additional fields after base:

```
**Applicable Standard:** {ISO/IEC standard reference}
**Next Review:** {YYYY-MM-DD or "Annual from effective date"}
```

### 2. Specification (Firmware, Hardware, Tooling, Session Protocol)

For: NP-FW-*, NP-HW-*, NP-TOOL-*, NP-PROC-*, NP-SES-* documents.

Additional fields after base:

```
**Supersedes:** {NP-XXX-NNN Rev X or "None"}
**Parent Document:** {NP-XXX-NNN Rev X or "None"}
```

### 3. Plan / Roadmap

For: Design plan, remediation plan, app roadmap, coordination checklist.

Additional fields after base:

```
**Supersedes:** {NP-XXX-NNN Rev X or "None"}
**Change Summary:** {Brief description of changes from previous revision}
**Review Cadence:** {Minimum frequency of review and update}
```

### 4. Analysis / Audit / FMEA

For: Privacy analysis, app audit, FMEA, traceability matrix.

Additional fields after base:

```
**Jurisdiction Scope:** {Global, US federal, EU/EEA, etc. — or N/A for non-privacy}
**Change Summary:** {Brief description of changes from previous revision}
```

### 5. Legal / Compliance / Privacy

For: BAA template, POA procedure, breach response plan, privacy notice, telemetry policy, anonymisation spec.

Additional fields after base:

```
**Applicable Standard:** {Regulatory/legal references governing this document}
**Next Review:** {YYYY-MM-DD or "Annual from effective date"}
**Jurisdiction Scope:** {Global, US federal + state, EU/EEA, etc.}
```

### 6. Regulatory

For: 510(k) pre-submission, regulatory opinion briefs.

Additional fields after base:

```
**Prepared For:** {FDA CDRH division or regulatory body}
**Applicable Standard:** {IEC/ISO/CFR references}
```

### 7. Reference / Guide

For: NPPS language reference, bibliography addendum, infrastructure guide, configuration reference.

No additional fields beyond base. Omit fields that are genuinely not applicable (e.g., IEC 62304 Class for a language reference).

---

## Status Definitions

| Status | Meaning |
|--------|---------|
| DRAFT | Under development, not yet reviewed or approved |
| ACTIVE | Approved and in effect |
| BASELINED | Frozen for a specific gate or milestone |
| SUPERSEDED | Replaced by a newer revision or document |
| ARCHIVED | No longer in use, retained for historical reference |

## Notes

- When a field is not applicable, use "N/A" or "—" rather than omitting it, to maintain visual consistency.
- `Revision` uses letter format (A, B, C...) for formal QMS documents. "Rev A" prefix is acceptable.
- `Date` is the date of the current revision, not the original creation date.
- `Effective Date` may differ from `Date` for documents requiring approval before they take effect.
- `Author` should include role in parentheses when the person is acting in an interim capacity.
- `Change Summary` is omitted for Rev A documents (first issue).
