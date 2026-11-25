## DHF
author: NeurOne Tooling
text: **{NP-SW-CI-001 from → to}: the App push audit allows `CLAUDE.md` on its revision line only, and fails a workflow that commits what the audit does not allow; its second run reported three revision bumps (OI-SWCI-54 Q1).**
## NP-SW-CI-001
author: NeurOne Tooling
text: **§6.21.3: a commit with no merged pull request may touch `docs/`, and `CLAUDE.md` on its `**Revision:**` line only (the App's grant since #1003); the offline mode compares `assign-revisions.yml`'s `git add` and `CLAUDE_REV_LINE` with the audit. §6.21.5 step 5: the second run's three violations were the App's `CLAUDE.md` revision bumps from #1003, #1005 and #1017.**
