# Principal question for review: OI-SIMFW-05 (a), the rules for the twin's temporary storage file

**Status:** Q1 and Q2 ANSWERED 2026-11-11 (the principal); Q3 partly answered by the Q1 answer; Q4 partly answered by the answer to question A of `oi-simfw-05-saved-file.md` (the file is encrypted at rest); the rest of Q3 and Q4, and Q5, are open (DRAFT, not sent). The questions the answer raises are drafted in `oi-simfw-05-saved-file.md`. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software Engineering (`OI-SIMFW-05`, `NP-SIM-FW-001` §7, `REQ-SIMFW-21`)
**To:** The principal (with Quality for Q5, and whoever owns bench and first-article test, not yet named, for Q3 and Q5)
**Response needed:** an option for each of Q1 to Q5, with a short rationale, name, role and date.
**Why now:** the principal decided on 2026-11-11 that the twin stores everything to a file, temporary unless saving is specifically requested (`oi-simfw-05-storage-block-device.md`, ANSWERED). The file store is not built. The sentence leaves five things open that the build cannot start without, and choosing any of them here would be deciding for the principal. Nothing in the repository acts as if the open ones were answered. *(Written before the answers to Q1 and Q2; see section 5.)*

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

## 5. Answers

**The principal, 2026-11-11, verbatim, on Q1:** "Q1: Option A by default. If someone wants to save it, then they pick the directory (which we should remember so that we continue to default to it) and they can pick a name, although the default name should be the logged in userID followed by a timestamp. The file extension should be unique to us so that it can be associated with our program to open on double click."

**Q1: decided, option A.** The working temporary file lives in the system temporary directory, under a random name, readable and writable by its owner only (0600 on POSIX). "By default" is the principal's word; no other location for the working file was named. Recorded as `REQ-SIMFW-21` part (d). Not built.

**The principal, 2026-11-11, verbatim, on Q2:** "Q2: option D"

**Q2: decided, option D,** as section 3 words it: B where the host allows it, C elsewhere.

- **B:** the file is created and then unlinked while open, so it has no name from the first moment and cannot outlive the process, whatever ends it.
- **C:** the file is deleted on normal exit, and at every start the twin removes files left behind by sessions that are no longer running.

No rationale was given beyond the choice. Recorded in `REQ-SIMFW-21` part (b). Not built.

**What the choice of D does not say, kept open:**

- **The sub-question asked under Q2, whether a leftover after a host power cut is acceptable if it is removed at the next start, or must be impossible, was not answered in words.** What D implies: where B applies a leftover is impossible, because the file has no name; where C applies a file can be left by a crash, a kill or a power cut and stays until the twin next starts on that machine, which may be never. **To confirm:** is that leftover acceptable on a host where only C is available? Yes, or no (and then such a host is not supported until it has an equivalent of B).
- **Which hosts count as "allows B".** Known: on Linux, create-then-unlink was shown to work with the existing block device (section 1). macOS and other POSIX hosts were not tried. Windows has no identical operation; the nearest is a delete-on-close handle, which the system honours when the last handle closes, including after a kill, but the file keeps a name while open and other programs can open it. The twin does not run on Windows today. **Open question: does Windows count as allowing B, or is it C?** *(i) Windows is B, by the delete-on-close handle.* Costs: the file is visible by name during the session, so it is not the same guarantee as on POSIX; whether it is removed after a power cut was not established here. Gains: no leftover after a crash or a kill. *(ii) Windows is C.* Costs: a leftover after a crash, a kill or a power cut until the next start. Gains: one plain mechanism, with the name available for saving. *(iii) Decide when the twin first runs on Windows.* Costs: the store is designed without knowing. Gains: nothing is promised for a host that does not run it.
- **How a dead session's file is told from a live one's under C.** A lock held by the live process, or a recorded process id; process ids are reused, so an id alone can spare a dead session's file or condemn a live one's. This is an engineering question for the build. It is listed and not answered.

**A consequence of D for Q3, stated and not decided:** where B applies the working file has no name, so a save cannot copy it by name; it has to copy through the open handle, from inside the twin. Where C applies the file has a name and could be copied by name. So "save" is one operation inside the twin on a B host, and could be either on a C host. Which it is on a C host, and whether the two kinds of host should behave the same, is part of Q3.

**Q3: partly answered. Q3 is not closed.** The sentence gives direction for a saved copy, recorded here as the principal's stated direction and not as an answer to the whole question:

1. The person picks the directory. The twin remembers it and defaults to it the next time.
2. The person may pick a name. The default name is "the logged in userID followed by a timestamp".
3. The file extension is unique to NeurOne, so the file can be associated with our program and opened on double click.

Against the options drafted under Q3, point 1 is nearest to "(2) a directory named by the person" but chosen at the time of saving and remembered, and point 2 lets the person name the file, which none of the three drafted options did. It is not option (3) as drafted unless the path travels in a client message, which the sentence does not say.

**Still open under Q3:**

- **When a save may be requested:** at start, during the session, or both (options A, B, C of Q3). Not said.
- **How long a saved copy is kept, and who deletes it.** Not said.
- **How the person picks the directory, and what that means for `REQ-SIMFW-12`.** "They pick the directory" is a person's choice on the host. Two ways it could be made are different things. *A native picker on the host* (a dialog run by a program on the host, or a setting given when the twin is started): the path never passes through a browser message, and `REQ-SIMFW-12` is untouched; it needs a host-side program or a start-time setting. *A path typed into the page:* the path arrives in a client message, so a client chooses where on the machine a file is written. `REQ-SIMFW-12` as written forbids a client string in a command line of the firmware process; a typed path would have to be handled by the bridge alone, validated, and never passed to that process, and whether that is acceptable is the principal's to say. A browser's own file-save dialog is a third way: the browser writes the file where the person chose and the page never learns the path, but then the twin cannot remember the directory, which point 1 asks for.
- **What happens in the container.** A directory the person picks is a directory on the host; the container sees it only if it is mounted. Whether the mount is fixed when the container starts, and what "pick" then means, is not said.

**Raised by the answer and not decided** (drafted in `oi-simfw-05-saved-file.md`): which "userID" and the identity it puts in a file name beside SHDR records; what "open on double click" does, against Q4 option A; the extension itself; what registers the file type on the host when the twin is a container; a header that states what the file is (Q5); where the remembered directory is kept.

**Q4: partly answered. Q4 is not closed.** In answering question A of `oi-simfw-05-saved-file.md` on 2026-11-11 the principal said, verbatim: "I recognize there is a challenge about data privacy here. We address it by encryption of the file at rest." Recorded as the principal's stated direction. It answers the half of Q4 that asks whether the synthetic UHDR in the file is encrypted: encrypted. **Still open under Q4:**

- **The first half, what keeps a real person's UHDR out of the twin** (options A, B, C). Not addressed by the sentence.
- **Which of options (1) and (3) is meant.** Option (2), in the clear, is ruled out by the sentence. Whether the encryption is the device's own partition encryption, a whole-file encryption, or both is question K of `oi-simfw-05-saved-file.md`.
- **Under which key** (question H there), and **whether the working temporary file is encrypted as well as a saved copy** (question I there).

**Rationale.** None was given beyond the sentences quoted.

**Recorded in:** `NP-SIM-FW-001` §5 (`REQ-SIMFW-21` parts (b) and (d)) and §7 (`OI-SIMFW-05`), and `docs/status/completed-decisions.md` (2026-11-11). Q5 is unanswered, and Q3 and Q4 are answered only in part.
