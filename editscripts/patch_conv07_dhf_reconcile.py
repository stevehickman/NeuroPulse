"""
OI-CONV-07 (GitHub #394): make NP-DHF-001 the index it is declared to be.

The interim precedence rule (2026-09-15) says "the DHF is the source of truth and wins where it
and docs/status/document-register.md disagree". Reconciling the three registers against the files
themselves on 2026-09-25 found that rule resting on an index that was incomplete and stale:

  * controlled Markdown documents with no NP-DHF-001 master-index row at all (among them
    NP-CONV-001 and the IEC 62304 SOUP record NP-SOUP-LFS-001);
  * rows whose Rev cell lags the document's own **Revision:** field, some by many revisions.

For a Markdown document the file's front matter is the authority for its own revision -- it is the
document -- so this script takes Rev and Date from the file and nothing else. It does NOT touch:

  * .docx / .pdf rows (binary; revision text is not reliably extractable, NP-CONV-001 OI-CONV-04);
  * docs/superseded/ (NP-CONV-001 Sec 1.1: rename forward, never backward -- retired records keep
    the revision label they were written with);
  * the Title, Status or description text of an existing row (that is authored content, and
    changing it is a per-item decision, not a reconciliation).

Missing rows are added to a new Sec 5.15, clearly labelled as indexed at reconciliation, with the
category assigned from the serial's family. Moving any of them into its thematic subsection is a
later, reviewed edit.

scripts/check-dhf-index.ts keeps this true afterwards.

Idempotent: re-running is a no-op once the DHF agrees with the files.
"""

import glob
import os
import re

DHF = "docs/np_dhf_001.md"
SECTION = "### 5.15 Documents indexed at the OI-CONV-07 reconciliation (2026-09-25)"

CATEGORY = [
    (r"^NP-FW-|^NP-SOUP-|^NP-MOD-ID-", "SPEC-FW"),
    (r"^NP-HW-|^NP-THERM-|^NP-PWR|^NP-EMC-|^NP-ENV-|^NP-HELMET-|^NP-OPT-|^NP-HEX-|^NP-DRV-", "SPEC-HW"),
    (r"^NP-TOOL-", "SPEC-TOOL"),
    (r"^NP-RISK-|^NP-FMEA-|^NP-RM-", "RISK"),
    (r"^NP-FAI-", "FAI"),
    (r"^NP-PROC-", "PROC"),
    (r"^NP-COORD-|^NP-PLAN-|^NP-REV-|^NP-ART-|^NP-COST-|^NP-ACC-", "COORD"),
    (r"^NP-REG-", "REG"),
    (r"^NP-CLIN-|^NP-BIB-|^NP-IRB-|^NP-COND-", "CLIN"),
    (r"^NP-QMS-|^NP-CONV-|^NP-DHF-|^NP-DP-|^NP-PMS-|^NP-DT-", "QMS"),
    (r"^NP-SES-|^NP-NPPS-", "SES"),
    (r"^NP-APP-|^NP-API-|^NP-SW-|^NP-ANALYTICS-|^NP-INFRA-|^NP-INT-", "APP"),
    (r"^NP-PRIV-|^NP-LEGAL-", "PRIV"),
    (r"^NP-SEC-", "SEC"),
    (r"^NP-FEAS-", "FEAS"),
]
STATUS_WORDS = ["DESIGN STUDY", "BASELINED", "SUPERSEDED", "ARCHIVED", "ACTIVE", "DRAFT"]


def front(path):
    t = open(path, encoding="utf-8").read()
    head = t.split("\n---", 1)[0]
    def f(name):
        m = re.search(r"^\*\*" + name + r":\*\*\s*(.+?)\s*$", head, re.M)
        return m.group(1) if m else None
    title = re.search(r"^# (.+)$", head, re.M)
    return {
        "id": f("Document"),
        "rev": f("Revision"),
        "date": f("Date"),
        "status": f("Status") or "",
        "title": title.group(1).strip() if title else "",
    }


def status_word(s):
    plain = re.sub(r"[*`_]", "", s).upper()
    for w in STATUS_WORDS:
        if plain.startswith(w) or plain.startswith("⚠ " + w):
            return w
    for w in STATUS_WORDS:
        if w in plain[:40]:
            return w
    return "ACTIVE"


def category(sid):
    for pat, cat in CATEGORY:
        if re.search(pat, sid):
            return cat
    return "QMS"


def main():
    text = open(DHF, encoding="utf-8").read()
    lines = text.split("\n")
    changed, added = [], []

    docs = []
    for p in sorted(glob.glob("docs/*.md")):
        fm = front(p)
        if not fm["id"] or not fm["rev"] or not re.fullmatch(r"[0-9]+", fm["rev"]):
            continue
        docs.append((p, fm))

    for p, fm in docs:
        link = "](./" + os.path.basename(p) + ")"
        idx = [i for i, l in enumerate(lines)
               if l.startswith("|") and link in l and not l.startswith("| ~~")]
        # Master-index rows only: the first cell is the serial (possibly bold).
        idx = [i for i in idx if re.match(r"^\|\s*\**" + re.escape(fm["id"]) + r"\**\s*\|", lines[i])]
        if not idx:
            added.append((p, fm))
            continue
        for i in idx:
            cells = lines[i].split(" | ")
            if len(cells) < 5:
                continue
            cur = re.sub(r"[*\s]", "", cells[2])
            lead = re.match(r"[0-9]+", cur)
            if lead and lead.group(0) == fm["rev"]:
                continue
            cells[2] = fm["rev"]
            if fm["date"] and re.fullmatch(r"[0-9]{4}-[0-9]{2}-[0-9]{2}", fm["date"].strip()):
                cells[3] = fm["date"].strip()
            lines[i] = " | ".join(cells)
            changed.append((fm["id"], cur, fm["rev"]))

    if added and SECTION not in text:
        anchor = next(i for i, l in enumerate(lines) if l.startswith("## 6. Firmware Source Code"))
        block = [
            SECTION,
            "",
            "These controlled documents had **no master-index row** in §5.1–§5.14 when `OI-CONV-07` "
            "reconciled the registers against the files (GitHub #394). Rev and Date are taken from "
            "each file's own front matter, and Category from the serial's family. **Moving a row "
            "into its thematic subsection is a later, reviewed edit.** Until then this subsection "
            "is where they are indexed, and `scripts/check-dhf-index.ts` keeps every row's Rev equal "
            "to its file's.",
            "",
            "| Doc number | Title | Rev | Date | File | Status | Category |",
            "|---|---|---|---|---|---|---|",
        ]
        for p, fm in added:
            title = fm["title"].replace("|", "/")
            date = fm["date"].strip() if fm["date"] else "—"
            block.append(
                f"| {fm['id']} | {title} | {fm['rev']} | {date} | "
                f"[{os.path.basename(p)}](./{os.path.basename(p)}) | {status_word(fm['status'])} | "
                f"{category(fm['id'])} |"
            )
        block += ["", "---", ""]
        lines[anchor:anchor] = block

    open(DHF, "w", encoding="utf-8").write("\n".join(lines))
    print(f"Rev cells corrected: {len(changed)}")
    for sid, old, new in changed:
        print(f"  {sid}: {old} -> {new}")
    print(f"Rows added (Sec 5.15): {len(added) if SECTION not in text else 0}")
    for p, fm in (added if SECTION not in text else []):
        print(f"  {fm['id']}  {p}")


if __name__ == "__main__":
    main()
