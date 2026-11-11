# Principal question for review: OI-SIMFW-05 (a), the rules for the twin's temporary storage file

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software Engineering (`OI-SIMFW-05`, `NP-SIM-FW-001` §7, `REQ-SIMFW-21`)
**To:** The principal (with Quality for Q5, and whoever owns bench and first-article test, not yet named, for Q3 and Q5)
**Response needed:** an option for each of Q1 to Q5, with a short rationale, name, role and date.
**Why now:** the principal decided on 2026-11-11 that the twin stores everything to a file, temporary unless saving is specifically requested (`oi-simfw-05-storage-block-device.md`, ANSWERED). The file store is not built. The sentence leaves five things open that the build cannot start without, and choosing any of them here would be deciding for the principal. Nothing in the repository acts as if these were answered.

## 1. What is known

- **The decision, verbatim:** "Since this will also be used as part of the test bench, store everything to a file (including UHDR and SHDR records - and synthetic state). The file should be temporary unless there is a specific request to save it for later examination. Otherwise, the temporary file should disappear when the user exits the session."
- **What is built today:** one RAM medium under the Config instance; no file, no path (`NP-SIM-FW-001` §27).
- **The existing block device can sit on a file unchanged** (probe run 2026-11-11, scratch code, not committed): `np_lfs_powerbd` takes a caller-owned buffer, and a shared mapping of a file works as that buffer. On the Config geometry (16 MiB) littlefs formatted, mounted and wrote through it; the file reopened by path in a new mapping mounted and read the data back; and after the file was unlinked while still open the mapping kept working and the path was gone. **Not tried:** the SHDR (512 MiB) and UHDR (6,903 MiB) geometries, any host but Linux, and a crash of the host.
- **`REQ-SIMFW-08` stands:** the twin's storage is synthetic, it never mounts, opens or accepts an image of a real UHDR or SHDR partition, and it holds test signing keys only. `CLAUDE.md` §5.1: NeurOne never has access to UHDR.
- **`REQ-SIMFW-06` and `REQ-SIMFW-10`:** a simulator run is never verification evidence; a scenario run on a rig could be, with the runner and models validated.
- **The twin is to ship as one local container** (`NP-SIM-FW-001` §9, D-2, decided; not built). Inside a container the "user's machine" is the container's filesystem unless a directory is mounted from the host.

## 2. What is assumed

- That "the session" is one run of the twin process (from its start to its exit). If it means something else (a browser tab, a container's lifetime, one protocol session), Q2 changes.
- That the UHDR and SHDR records the sentence names are the twin's synthetic ones. `REQ-SIMFW-08` and `CLAUDE.md` §5.1 already require that; the sentence is not read as changing them.

Say so if either is wrong.

## 3. Questions

**Q1. Where does the temporary file live, what is it called, and who may read it?**

- **(A) The system temporary directory, a random name, owner-only (0600 on POSIX).** Costs: on a shared machine the name is visible to other users even though the content is not; in the container it is the container's own temporary directory, gone with the container. Gains: nothing to configure; the operating system's own cleanup applies.
- **(B) A directory the twin owns under the user's home or data directory, random name, owner-only, the directory owner-only too.** Costs: a new directory to create and to document; nothing else cleans it. Gains: not visible to other users at all; leftovers are easy to find (Q2).
- **(C) A directory named by the person who starts the twin, no default.** Costs: the twin does not start without it; a path now comes from outside, and it must be shown that it can only ever name a new file (Q4). Gains: a bench can put it on the disk it wants.

One file holding every partition, or one file per partition, is an engineering detail unless the principal wants it fixed; say if so.

**Q2. How is "disappears when the user exits the session" made true?**

- **(A) Delete on normal exit only.** Costs: a crash, a kill or a power cut leaves the file behind, so the sentence is not true in those cases. Gains: simplest; the file has a name all session, so saving (Q3) is a copy by name.
- **(B) Create, then unlink while open (POSIX).** The file has no name from the first moment and cannot outlive the process, whatever ends it. Costs: there is no name to copy from, so saving (Q3) must copy through the open handle or be chosen at start; Windows has no identical operation (the nearest are a delete-on-close handle, which the system honours when the last handle closes, including after a kill, but which other programs can open differently; the twin does not run on Windows today). Gains: true after a crash or a kill. Shown to work with the existing block device on Linux (section 1).
- **(C) Delete on normal exit, and at every start remove files left by sessions that are no longer running.** Costs: a leftover survives until the next start, which may be never; the twin has to tell a dead session's file from a live one's (a lock or a process id, and process ids are reused). Gains: works the same on every host; keeps the name for Q3.
- **(D) B where the host allows it, C elsewhere.** Costs: two behaviours to test and to explain. Gains: the strongest guarantee each host can give.

Also for the principal: **is a leftover after a host power cut acceptable** if it is removed at the next start, or must it be impossible? Only B (and its Windows nearest) makes it impossible, and only while the file is unnamed.

**Q3. What is "a specific request to save it for later examination"?**

Who asks: (i) the person at the page, (ii) the person who starts the twin, (iii) a bench script. Any or all.

- **(A) A choice at start only** (a flag or a setting when the twin is launched). Costs: the wish to keep a run usually comes after something went wrong, when it is too late. Gains: no command during the session; the file can be named from the start; nothing a browser client sends can cause a write to disk.
- **(B) A command during the session** (a button on the page and the same message from a script). Costs: a client message now causes a file to be written, so the bridge must validate it as it does every other command (`REQ-SIMFW-12`): the client may ask for a save but may not supply the path. Gains: a run can be kept after the fact.
- **(C) Both.**

Where the saved copy goes and what it is called: **(1)** a fixed directory the twin owns, with a name the twin builds (date, time and a random part), nothing from the client; **(2)** a directory named at start by the person who launches the twin, name built by the twin; **(3)** a path the requester gives in full. Option 3 lets a client choose where on the machine a file is written, which the bridge's rules exist to prevent. In the container, a saved copy survives only if the directory is mounted from the host; say whether that mount is part of the product.

Also: **how long is a saved copy kept, and who deletes it?** The sentence does not say. Options: until the person deletes it; or the twin lists and offers to delete saved copies.

**Q4. What keeps a real person's UHDR out of the twin, and is the synthetic UHDR in the file encrypted?**

What keeps real UHDR out:

- **(A) The twin only ever creates its file and never opens an existing one.** No command, flag or message loads a file. A saved copy (Q3) is for examination by other tools and cannot be loaded back. Costs: a saved run cannot be resumed or replayed from its state. Gains: `REQ-SIMFW-08` holds by construction: there is no input through which any image, real or not, enters.
- **(B) The twin may load a file, but only one it wrote, proved by a mark it checks** (for example a keyed tag under a key that exists only in the twin). Costs: the mark and its check are new security-relevant code (Rust, `CLAUDE.md` §22); a check is weaker than having no door; a real partition image fails the check only if the check is right. Gains: a saved run can be resumed.
- **(C) As B, and also refuse anything that decrypts or mounts as a real partition would.** Costs: the twin would need to recognise a real image, which is hard to show complete. Gains: a second barrier.

Whichever is chosen, the twin holds TEST keys only and no production key (`REQ-SIMFW-08`), so a real UHDR partition, which is sealed under a key derived from the user's biometric (`CLAUDE.md` §5.1), could not be read by it even if it were loaded. That is a second line, not the first.

Is the synthetic UHDR in the file encrypted:

- **(1) Encrypted as the device does it, under a TEST key.** The twin runs the device's own UHDR key and storage code (`OI-SIMFW-52`, not built), so what is in the file is what the device would write. Costs: waits for that code to run in the twin; a saved file needs the TEST key to examine. Gains: the twin exercises the real path, which is the reason for a twin; the bench sees what the device stores.
- **(2) In the clear, for the bench.** Costs: the twin's UHDR partition is then not what the device's code writes, so the device's encryption path is not run and a bench result about storage says nothing about the device; a habit of clear UHDR files, even synthetic, is one to weigh against `CLAUDE.md` §5.1's "protection only goes up" (that rule is about real UHDR; whether it should also bind the synthetic case is the principal's to say). Gains: a saved file can be read with ordinary tools.
- **(3) Encrypted in the file, with a separate export in the clear on request.** Costs: two forms to keep in step. Gains: both uses.

**Q5. What may a saved file be used as evidence for?**

- **(A) Nothing. It is a debugging aid.** Consistent with `REQ-SIMFW-06` as written. Costs: a bench run's saved file cannot go into a design-history file. Gains: no validation burden on the twin, the bridge or the file format.
- **(B) Nothing from a simulator run; from a bench run on a rig, a saved file may be verification evidence under `REQ-SIMFW-10`.** Costs: the file store, its format and the save path fall under the validation `REQ-SIMFW-10` requires, and the file must record what produced it (rig or simulator, versions) in a way that cannot be mistaken; `OI-SIMFW-10` (whether a rig exists, and what its hub is) is still unanswered. Gains: the bench use the principal named produces something that can be kept as a record.
- **(C) Decide when a rig exists.** Costs: the file format is designed without knowing whether it must carry provenance. Gains: nothing is promised early.

Whichever is chosen: should every saved file carry, in itself, a statement of what it is (simulator or rig, synthetic data, not verification evidence unless B applies)? Recommended yes, so a file found later cannot be taken for something else.

## 4. What to send back

An option for each question, the two assumptions of section 2 confirmed or corrected, a sentence on why for each (`CLAUDE.md` §18 wants the reason recorded), anything that would reopen it, and name, role and date. The answers are recorded in `NP-SIM-FW-001` §7 (`OI-SIMFW-05`) and in `docs/status/completed-decisions.md`, and the requirement rows they imply are written then.
