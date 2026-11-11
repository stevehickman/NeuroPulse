# Principal question for review: OI-SIMFW-05 (b), the saved file's name, extension, opening and association

**Status:** Question A ANSWERED 2026-11-11 (the principal); question G ANSWERED 2026-11-11 (the principal): uniqueness is not needed; question L: first half ANSWERED 2026-11-11 (the principal): a save never overwrites a closed file, and may update a file the same session created; a 'save as' closes the file that was open (the principal, 2026-11-11); M2 ANSWERED 2026-11-11 (the principal): a 'save' appends, and a 'save as' creates a new file with the content since the last 'save' (two readings, to confirm); its second half is open; B to F, H to K, the rest of L, M1 and M3 to M7, and P1 to P8 which the answer to M2 raises, are open (DRAFT, not sent). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software Engineering (`OI-SIMFW-05`, `NP-SIM-FW-001` §7, `REQ-SIMFW-21`)
**To:** The principal (with Quality for Q5 of `oi-simfw-05-temporary-file.md`, which question E here touches)
**Response needed:** an option for each of B to F, H to K, the second half of L, M1, M3 to M7, and P1 to P8, with a short rationale, name, role and date.
**Why now:** on 2026-11-11 the principal answered Q1 of `oi-simfw-05-temporary-file.md` and gave direction for a saved copy: the person picks the directory, which is remembered; the default name is "the logged in userID followed by a timestamp"; and the extension is unique to NeurOne so the file opens on double click. That direction raises six questions it does not answer. None is decided here, and nothing in the repository acts as if any open one were answered. *(Written before the answer to A; see section 4.)*

## 1. What is known

- **The principal's words, verbatim:** "Q1: Option A by default. If someone wants to save it, then they pick the directory (which we should remember so that we continue to default to it) and they can pick a name, although the default name should be the logged in userID followed by a timestamp. The file extension should be unique to us so that it can be associated with our program to open on double click."
- **Nothing is built.** There is no file store, no save, no viewer and no installer for the twin.
- **`REQ-SIMFW-08`:** the twin's storage is synthetic, and it never mounts, opens or accepts an image of a real UHDR or SHDR partition.
- **`CLAUDE.md` §5.1:** SHDR is linked to a device ID and an opaque warranty token only, never to user identity; NeurOne never has access to UHDR.
- **`REQ-SIMFW-12`:** no string a client sent is placed in a command line of the firmware process.
- **The twin is to ship as one local container** (`NP-SIM-FW-001` §9, D-2: decided, not built).
- **Extensions the repository already uses** (found by a search of `docs/`, `app/`, `common/` and `simulator/` on 2026-11-11): `.npps`, `.npbk`, `.npfw`, `.npa`. `docs/np_conv_001.md` has rules for the names of controlled documents; no rule for a data file's extension was found in it.
- **Q4 of `oi-simfw-05-temporary-file.md` is open:** its option A is "the twin only ever creates its file and never opens an existing one".

## 2. Questions

**A. Which "userID" goes in the default file name?**

- **(1) The operating-system login name.** Costs: the name identifies a person. A saved file attached to a bug report or sent to NeurOne carries that person's login in its name. The file holds SHDR records, and `CLAUDE.md` §5.1 says SHDR is never linked to user identity. The twin's records are synthetic, so no real record is linked by this; but a file name that puts a user identity beside SHDR is the pattern §5.1 forbids, and it would be a breach if the same naming were ever used for an export from a real device. Gains: it is what "logged in userID" most plainly means, and a person recognises their own files on a shared bench machine.
- **(2) An in-app user id** (the id the companion app knows the user by). Costs: the twin has no app login today, so there is nothing to take; the same identity concern as (1) applies if that id names a person. Gains: the same id on every host.
- **(3) A pseudonymous id** made on the machine (random, stored with the remembered directory, question F). Costs: a person cannot tell whose file it is by reading the name; it must be shown or looked up. Gains: no identity leaves the machine in a file name.
- **(4) As (1) on the machine, with the name replaced by (3) when the file is exported or attached for anyone else.** Costs: two names for one file and a step that must not be skipped. Gains: both uses.

Also to settle: **the timestamp's format.** A colon is not allowed in a file name on Windows, so an ISO 8601 extended time (`14:03:07`) is not safe; the basic form (`20261111T140307Z`) is. Say whether the time is UTC or local. And **what is used when no userID is available** (a container often runs as a fixed or numeric user): a fixed word, the pseudonymous id of (3), or the save is refused until one is given.

**B. What does "open on double click" do?** Opening a file is an input, and Q4 option A says the twin takes none.

- **(i) Examine it, read-only, in a viewer.** Nothing is loaded into a running twin. Consistent with Q4 option A. Costs: a viewer is a program to write, and any file with the extension, whoever made it, is untrusted input to its parser. The parser must refuse anything that is not a well-formed file of this type (question E), must never mount or decrypt a partition image as a device would, and must hold no key that could read a real partition. **How a renamed real UHDR image is recognised and refused has to be stated:** a real image would lack the header of question E, so a strict header check refuses it before anything else is read; that depends on the check being right, and on the viewer never falling back to "try to read it anyway". Gains: `REQ-SIMFW-08` keeps its strongest form, with no door into the twin.
- **(ii) Load it as the twin's state and resume.** Contradicts Q4 option A, and needs Q4 option B or C (a mark proving the twin wrote the file). Costs: everything listed under Q4 B: new security-relevant code, and a check standing where there was no door; every cost of (i) as well. Gains: a saved bench run can be continued.
- **(iii) Both, as separate actions,** with double click doing (i) and resume a deliberate second step.

**C. What is the extension?**

Candidates, none chosen. Each is lower-case, starts with `np` as the existing four do, and collides with none of them:

- `.nptwin` (the twin's saved state)
- `.npsim` (a simulator file)
- `.npts` (twin state; shorter, and closer to an existing extension in other software, TypeScript's `.ts`, by appearance only)

**That no other program anywhere uses a candidate is UNVERIFIED and cannot be verified from this repository.** A check would need: the IANA media-type registry and the common public file-extension lists, a search of the Windows and macOS default associations, and a search of the app stores NeurOne ships to; even then an extension is not reserved to anyone, so the header of question E is what makes a file unmistakable, not the extension. Say also **whether the working temporary file of Q1 carries the same extension.** Same: one type to handle, and a leftover temporary file would open on double click. Different, or none: a leftover is not mistaken for a file someone chose to keep.

**D. What registers the file type on the host, and what program does the double click start?**

A file association is registered per operating system (by an installer, a package, or a first-run step), and it names a program on the host. A container cannot register an association on the host, and cannot be started by a double click by itself. So the direction implies something on the host beside the container. This draft does not design it. The options to choose between:

- **(1) A small host-side launcher** that is installed, registers the type, and on double click starts the container (or the viewer of B) with the file. Costs: an installer per operating system, signing, and updates; the launcher passes a host path to the container, which is the door Q4 is about. Gains: double click works as asked.
- **(2) A host-side viewer only,** with no link to the container (fits B (i)). Costs: an installed program per operating system, as (1). Gains: nothing passes into the twin.
- **(3) No association at first;** the file is opened from inside the twin's page by choosing it. Costs: not what was asked; choosing a file in a page is still an input (B). Gains: nothing to install.
- **(4) Revisit D-2:** whether the twin ships only as a container. Costs: reopens a decided item. Gains: a native program can register its own type.

**E. Does the file state what it is, in a header?** (This is the place Q5 of `oi-simfw-05-temporary-file.md` would be answered in the file itself.)

With a unique extension, a header at the start of the file holding a magic number and a format version would let any opener refuse everything else, including a renamed image of something that is not this type. The same header is where the provenance statement would live: produced by the simulator or by a rig, the versions that produced it, that the records are synthetic, and that it is not verification evidence unless Q5 says a rig's file may be. Options: **(1) yes, a header with magic, version and provenance, and an opener that refuses a file without it;** **(2) yes, magic and version only,** provenance kept elsewhere (costs: a file found later does not say what it is); **(3) no header** (costs: the extension is the only mark, and question B's refusal of a renamed image has nothing to stand on). The magic value and the layout are design work for the build, not chosen here.

**F. Where is the remembered directory kept?**

It is a stored setting. What can be said now: it would hold a path and nothing else; it is not UHDR and not SHDR, since it says nothing about a person's biology or a device's condition (`CLAUDE.md` §5.1's two tests), though a path under a home directory usually contains a login name, which is the concern of question A in another place. Options: **(1) a local preference file on the host, per operating-system user,** in that system's usual place for application settings (costs: the container needs it mounted or passed in, and it must be readable only by its owner); **(2) in the browser's local storage for the page** (costs: lost when the browser's data is cleared, different per browser, and a path chosen in a page is the case `REQ-SIMFW-12` is about, see Q3 of `oi-simfw-05-temporary-file.md`); **(3) not remembered across sessions** (costs: not what was asked). If question A picks a pseudonymous id, it would live in the same place.

## 3. What to send back

An option for each of B to F, H to K, the second half of L, M1, M3 to M7, and P1 to P8, a sentence on why for each (`CLAUDE.md` §18 wants the reason recorded), anything that would reopen it, and name, role and date. The answers are recorded in `NP-SIM-FW-001` §7 (`OI-SIMFW-05`) and in `docs/status/completed-decisions.md`, and the requirement rows they imply are written then.

## 4. Answers

**The principal, 2026-11-11, verbatim, on question A:** "Using a pseudonymous ID for the userID (the user doing the saving can pick a userID that only they know even if it doesn't match their logged in userID. It can't clash with a previously picked pseudonymous ID that another user picked). I recognize there is a challenge about data privacy here. We address it by encryption of the file at rest."

**Question A: decided.** The userID in the default name of a saved file is a pseudonymous id chosen by the person who saves. It need not match their login, and it is theirs alone to know. Against the options of section 2 this is option (3), a pseudonymous id, with one difference: option (3) proposed an id the machine makes at random, and the decision is an id the person picks. Recorded as `REQ-SIMFW-21` part (e). Not built.

**Also in the sentence, recorded as the principal's direction and not as a closed question:**

- **The id cannot clash with a pseudonymous id another user picked earlier.** How that is achieved is not said, and it pulls against "only they know" (question G). **WITHDRAWN by the principal later the same day; see the answer on question G below.**
- **The file is encrypted at rest, and that is how the principal addresses the data-privacy concern** raised under question A: an identity in a file name beside SHDR records (`CLAUDE.md` §5.1). This partly answers Q4 of `oi-simfw-05-temporary-file.md`, in its half "is the synthetic UHDR in the file encrypted": encrypted. It does not say which of that question's options (1) or (3) is meant, under which key, or whether the working temporary file is encrypted as well as a saved copy (questions H to K). Q4 is not answered.

**Not said and still open in question A itself:** the timestamp's format and time zone, and what the default name is if the person has picked no id yet. The decision makes the second easier to state (the save asks for an id), but it was not said.

**The principal, 2026-11-11, verbatim, on uniqueness (question G):** "On uniqueness: I'm going to backtrack on this. If the file is encrypted, only the keyholder can open it anyway, and two different users will simply have files that have similar names (the filesystem will prevent them from being identical). So I don't think we need uniqueness"

**Question G: answered. Uniqueness is not needed.** The pseudonymous id does not have to be unique. The clause of the earlier answer to question A, "It can't clash with a previously picked pseudonymous ID that another user picked", is withdrawn by this sentence. The rest of that answer stands: a pseudonymous id the person picks, theirs alone to know, which need not match their login. No registry, no appended random part and no collision check is required. Options (a) to (d) under question G are not taken; they stay in section 5 as the record of what was considered. `REQ-SIMFW-21` part (e) never mentioned uniqueness and is unchanged.

**The principal's reasons, as given:** if the file is encrypted only the keyholder can open it, and two users will simply have files with similar names, which the filesystem prevents from being identical.

**Two things that reasoning rests on, recorded as consequences to confirm and not as decisions:**

1. **"Only the keyholder can open it anyway" holds only if the key is not shared.** Under question H option (a), a TEST key in the repository, anyone with the code can open any saved file, and the argument does not hold. So the withdrawal depends on the answer to H. It is noted under G and under H.
2. **"The filesystem will prevent them from being identical" holds only if the twin creates a saved copy exclusively,** so that the create fails when the name exists. A save that overwrites would destroy another person's file in a shared directory with nothing said. What the twin does when an exclusive create is refused is question L.

**Rationale for the answer to question A.** The sentence gives its own: the principal recognises the data-privacy challenge and addresses it by encryption at rest. Nothing further was given.

**Recorded in:** `NP-SIM-FW-001` §5 (`REQ-SIMFW-21` part (e)) and §7 (`OI-SIMFW-05`), and `docs/status/completed-decisions.md` (2026-11-11).

**The principal, 2026-11-11, verbatim, on question L (the principal labelled it "Q3"):** "Q3: A save never overwrites a file once that file has been closed. A save can continue to update a file that was created by that same session."

**Question L, first half: decided.**

1. Once a saved file has been closed, a save never overwrites it.
2. A save may continue to update a file that was created by that same session.

No rationale was given beyond the sentence. Recorded as `REQ-SIMFW-21` part (i). Not built. The consequence to confirm that was recorded under question G ("the filesystem will prevent them from being identical" holds only if a save never overwrites) is met by this answer for closed files.

**Question L, second half: not answered.** What the twin does when a save is refused because a closed file of that name already exists (refuse and tell the person, append a counter, or ask for another name) stays open.

**Not said, and needed before this can be built or tested** (questions M1 to M6, section 6): what "closed" means; what "update" means; how the twin knows a file is this session's; what a crash during an update leaves; from when the retention period runs for a file that is updated; and how this bears on when a save may be requested.

**The principal, 2026-11-11, verbatim, on "session" and on 'save as':** "A session defaults to the duration of a user login. Note, however, that a user command to save during a session could also be a 'save as', in which case, if any file was previously open, it is closed and cannot later be updated."

**Decided.**

1. **A session defaults to the duration of a user login.** (Recorded in full, with what it puts in question, in `oi-simfw-05-temporary-file.md` §5.)
2. **A save command during a session may be a 'save as'. When it is, any file that was previously open is closed, and a closed file cannot later be updated.**

No rationale was given beyond the sentences. The second is recorded as `REQ-SIMFW-21` part (k). Not built.

**What this tells us about "closed" (M1) and "update" (M2), and what it does not.**

- **Two events that close a file are now known:** a 'save as'; and the end of the session, which follows from the earlier answer (a save may update a file "created by that same session", so not after it) together with this definition of session.
- **A plain 'save' continues to update the file the session already has open.** That is the earlier answer restated, and the principal has since said it directly: "A second save in a session (Not a 'save as') updates the currently open file." (recorded under S3 of `oi-simfw-05-temporary-file.md` §5; `REQ-SIMFW-21` part (l)).
- **Supposed, to confirm, not decided:** after a 'save as', the session's later updates go to the new file.
- **Still not said:** whether the person can close a file explicitly without a 'save as'; what happens to an unclosed file after a crash; and what "update" consists of (M2).

**The principal, 2026-11-11, verbatim, on what "update" means (M2):** "'save' appends. 'Save as' creates a new file with all the content since the last 'save'."

**M2: decided.**

1. **A 'save' appends** to the file the session currently has open. This is option (c) of M2. Options (a), replace with the latest snapshot, and (b), a live mirror, are not taken and stay in section 6 as the record.
2. **A 'save as' creates a new file with all the content since the last 'save'.**

With the earlier answers: a 'save as' closes the file that was open, and later 'save' commands append to the new file. No rationale was given beyond the sentence. Recorded as `REQ-SIMFW-21` part (n). Not built.

**The second sentence has two readings. Neither is chosen here.**

- **Reading (A), the apparent meaning (the lead's reading, to confirm):** the new file holds only the content since the last 'save'. It is a segment. The earlier file keeps everything up to and including the last 'save', the new file continues from there, and a session's content is spread across files.
- **Reading (B), listed so the principal can rule it out:** the new file holds all the content of the session so far. This sits badly with the words "since the last 'save'", which bound the content from a point and not from the start.

Under reading (A) a saved file is no longer self-contained, which is what questions P1 to P8 (section 7) are about.

## 5. Questions the answer to A raises (none decided)

**G. How is "cannot clash with an id another user picked" made true, and how much uniqueness is needed?** **ANSWERED 2026-11-11 (the principal): uniqueness is not needed (section 4). Options (a) to (d) below are NOT TAKEN and are kept as the record of what was considered.** The answer rests on the file being encrypted under a key that is not shared, so it depends on question H: under H option (a) it would not hold.

The tension, stated plainly: an id that only its owner knows, and an id that is checked against everyone else's, cannot both hold unless uniqueness is established without disclosing ids. Any registry that answers "taken" lets a person confirm that an id exists, and by trying many, enumerate them. And a shared registry is a store that someone holds (NeurOne, or the bench's owner) linking pseudonyms to the people who registered them, which is against the spirit of `CLAUDE.md` §5.1 (SHDR is never linked to user identity) and §5.3 (no linkage table exists).

What a clash would actually cause: two people's saved files would begin with the same characters. The timestamp in the name already separates the files, and each file is encrypted (the principal's direction), so a clash does not open one person's file to another. It would make it unclear whose file is whose when files from several people are in one directory. How much that matters is the principal's to judge.

- **(a) Unique on this machine only.** The twin keeps a local list of ids used here and refuses a repeat by a different operating-system user. Costs: it cannot see other machines, so two people on two machines can pick the same id; the local list is itself a small store of pseudonyms on a shared machine, and "taken" tells one user of the machine that another used that id. Gains: no network, no registry anyone else holds.
- **(b) No registry: a random part is appended to the id the person picks,** so a clash is negligible by construction. The picked id is the readable part. Costs: the name is longer, and the person cannot guarantee a short id is theirs alone, only that the whole of it is. Gains: nothing is stored anywhere and nothing can be enumerated. How long the random part must be follows from how many ids are expected and what clash probability is accepted: for n ids and b random bits the probability of any clash is about n squared divided by 2 to the power (b + 1). **An illustration with inputs that are mine and not proposed:** for 1,000 ids and a probability of at most one in a million, b must be at least log2(1,000² / (2 × 10⁻⁶)), about 38.9, so 39 bits, which is 8 characters of base 32. The two inputs are the principal's to set; nothing in the repository uses a figure from this.
- **(c) A shared registry of salted hashes of ids.** Costs: it is still a store someone holds; a hash of a short human-picked id is guessable by trying likely ids, so it leaks which ids exist to anyone who can query it or who obtains it, and with the salt known it can be searched offline; it needs a network and an operator. Gains: uniqueness across machines.
- **(d) A registry kept by whoever owns a bench, for that bench only.** Costs: the bench owner holds a list tying pseudonyms to the people who use the bench, which is a linkage table in small; it does nothing for a person not on a bench. Gains: uniqueness where files from several people actually meet.

**H. Who holds the key that "encrypted at rest" needs?** *(The principal's withdrawal of uniqueness, question G, rests on "only the keyholder can open it": that is true under options (b), (c) and (d) and false under option (a). To confirm with the answer to this question.)*

- **(a) A TEST key in the twin.** The twin holds test keys only (`REQ-SIMFW-08`). Costs, plainly: a test key is in the repository, so it keeps out nobody who has the code, and it protects nothing that a reader of this repository could not undo. It would make the file unreadable to ordinary tools and to no one else. Gains: nothing for the person to remember; double click needs no prompt.
- **(b) A key derived from a passphrase the person gives when saving,** needed again to open. Costs: a forgotten passphrase loses the file; opening on double click needs a prompt; a weak passphrase is a weak key. Gains: only the person can open the file, which is the only option here under which the encryption protects against someone who holds the file and the code.
- **(c) A key held in the host's own credential store, per operating-system user.** Costs: different on every platform; not available inside the container without a host-side helper; the file opens only on that machine and for that user, so it cannot be handed to someone else to examine. Gains: no passphrase; protected from other users of the machine.
- **(d) A random key per file, kept in the file's header, sealed under (b) or (c).** Costs: those of whichever seals it, and a little more format. Gains: the sealing secret can change without rewriting the file, and the header of question E has a natural place for it.

Whichever is chosen, **the opener needs the key.** That bears on question B: a viewer or a resuming twin started by double click must obtain it (a prompt under (b), the credential store under (c), nothing under (a)).

**I. What is encrypted?**

- **(1) The saved copy only.** Costs: the working temporary file is in the clear while the twin runs. It is owner-only and, where the host allows it, has no name (Q1 and Q2 of `oi-simfw-05-temporary-file.md`), and on a host where it has a name and can be left behind it would be left in the clear. Gains: the twin's own storage path is unchanged.
- **(2) The saved copy and the working temporary file.** Costs: every read and write of the twin's storage passes through the encryption, and the working file needs a key before anyone has asked to save (a key made for the session and discarded would do, and then a save re-encrypts under the key of question H). Gains: nothing is ever on disk in the clear.

**Encryption at rest does not cover the file's name.** The pseudonymous id and the timestamp are in the clear in the name whatever is chosen here. The pseudonym is what stands between the name and a person. (Question G is answered: no registry of pseudonyms is to exist, so nothing ties one to a person.)

**J. Where does the encryption code live?** (A consequence to confirm, not a design.)

`CLAUDE.md` §22: cryptographic work is written in Rust, once, in `common/npps-core`, and is not taken from a platform or third-party library; every new crate joins `docs/np_soup_rust_001.md` in the change that adds it. File encryption for the twin falls under that rule. It follows that the build needs the work done in `common/npps-core` and reached from the twin, a SOUP entry if a new crate is used, and an open item first if any part were to be written in another language. Confirm that the rule applies to the simulator's file as it does to the product, or say that it does not.

**K. Which layer is meant: the file, the partition inside it, or both?**

Q4 of `oi-simfw-05-temporary-file.md`, option (1), was "encrypted as the device does it, under a TEST key": the twin runs the device's own UHDR key and storage code (`OI-SIMFW-52`, not built), so the UHDR partition inside the file is sealed as the device seals it. The principal's sentence speaks of the file being encrypted at rest, which reads as a different layer: the whole of what the twin writes, encrypted as a file.

- **(1) Whole-file encryption only.** The partitions inside are as the twin writes them. Costs: the device's own UHDR encryption path is not exercised by this. Gains: can be built without `OI-SIMFW-52`.
- **(2) The device's partition encryption only,** inside an otherwise clear file. Costs: the SHDR partition and the twin's synthetic state are in the clear, which is not what "the file" encrypted at rest says; waits for `OI-SIMFW-52`. Gains: the file holds what a device would.
- **(3) Both.** Costs: two layers and two keys to manage. Gains: the file is protected as a file, and the twin runs the device's path.

**L. What does the twin do when a saved copy's name already exists?** (Raised by the answer to G. It needs an answer only for the build.) **First half ANSWERED 2026-11-11 (the principal; section 4): a save never overwrites a file once it has been closed, and may continue to update a file the same session created. Second half, options (a) to (c) below, OPEN.** The answer removes the silent-overwrite case; it does not remove the need for a defined behaviour when a closed file of that name exists.

For the filesystem to keep two files from being identical, the twin has to create a saved copy exclusively: the create fails if the name exists, and nothing is overwritten. An overwriting save would silently destroy another person's file in a shared directory, which is the fault the project refuses elsewhere: a thing is rejected and said so, never silently reshaped (`CLAUDE.md` §3). **To confirm first:** that a save never overwrites an existing file. Then, when the exclusive create is refused:

- **(a) Refuse, and tell the person.** Costs: the person has to save again; with the default name they would get a new timestamp a second later, so a retry normally succeeds. Gains: nothing happens that was not asked for.
- **(b) Append a counter to the name and save.** Costs: the file is saved under a name the person did not see beforehand, so they must be told the name actually used, or it is a silent change of what they asked for. Gains: the save succeeds at once.
- **(c) Ask the person for another name.** Costs: a prompt in the middle of a save; a bench script has nobody to ask. Gains: the person chooses.

The default name, a pseudonymous id followed by a timestamp, already makes a collision improbable: it needs two saves with the same id into one directory within the timestamp's resolution, or a person typing a name that exists. The resolution of the timestamp is part of question A's open remainder (its format).

## 6. Questions the answer to L raises (none decided)

**M1. What does "closed" mean?** *(Since M2 was answered, 2026-11-11: with an append-only file, a final authenticated record saying "closed" would let an opener tell a closed file from an unclosed one; see P4.)* *(Partly answered 2026-11-11, section 4. Known: a 'save as' closes the file that was open, and the end of the session closes it; a session defaults to the duration of a user login, so option (a) below as worded, "the twin process exits", is not the principal's meaning unless N2 of `oi-simfw-05-temporary-file.md` is answered that way. Option (c) is ruled out by the principal's two answers taken together: a plain save updates the open file. Still open: whether the person can close a file explicitly, as in (b); what happens to an unclosed file after a crash (M3); and (d).)*

- **(a) The session ends.** The file is closed when the twin process exits. Under the second sentence, every save during the session updates the one file, and the file becomes fixed at exit. Costs: a crash ends the session without a deliberate close (M3, M4); a long session has one file that keeps changing.
- **(b) The person closes it,** by an explicit "finish and keep" action. Saves before that update the file; after it the file is fixed and a further save starts a new file. Costs: one more action to explain, and a session that ends without it needs a rule (closed at exit, as (a)). Gains: the person decides when a record is final.
- **(c) Each save writes the file and closes it at once,** so every later save in the session is a new file. **This contradicts the second sentence of the decision,** which lets a save continue to update a file the session created; under (c) there is never an open file to update. Listed because it is the simplest reading of "save", and to be ruled out or the second sentence explained.
- **(d) Closed when the twin has not written to it for a stated time.** Costs: needs a figure nobody has given, and a person cannot tell by looking whether the next save will update or start a new file. Gains: none that (a) or (b) lacks.

**M2. What does "update" mean?** **ANSWERED 2026-11-11 (the principal; section 4): a 'save' appends, option (c). Options (a) and (b) below are NOT TAKEN and are kept as the record.** *(The note that follows was written before the answer.)* *(Not answered. The principal's sentence of 2026-11-11, "A second save in a session (Not a 'save as') updates the currently open file", settles that a save command updates the open file; it does not say whether the update replaces the content with the latest snapshot, mirrors continuously, or appends. Known since 2026-11-11: a plain save updates the file the session has open, a 'save as' closes that file and, it is supposed, later updates go to the new one. Options (a), (b) and (c) below are all still open. Because a session is a login and may span several runs of the twin, an update can come from a later run than the one that created the file.)*

- **(a) Replace with the latest snapshot.** Each later save in the session replaces the content of the session's one file (the person presses save again). Costs: the earlier snapshot is gone, by the person's own action. Gains: one file per session, always whole.
- **(b) A live mirror.** Once saved, the file is kept in step continuously while the twin runs. Costs: the twin writes its state to two places for the whole session; the saved file is exposed to a crash in the middle of an update at any moment, not only when the person saves (M4); the working file of Q1 and Q2 stays separate and unlinked either way. Gains: the file is current when the session ends without a final save.
- **(c) Append-only.** Later saves add to the file; nothing already written changes. Costs: the file grows; the format has to hold a sequence of states. Gains: nothing already saved is ever lost, and a crash can damage only the last addition (M4).

**M3. How does the twin know a file is this session's?**

"Created by that same session" needs something only the creating session has. One way would be a session identifier, made at start and held only by the running process, recorded in the file's header (which the encryption direction would cover). It must not be satisfied by a file that merely carries the same pseudonymous id in its name: the id is the person's across sessions, and ids need not be unique (question G), so the name proves nothing about the session. To confirm that rule. Then, **a file left by a session that crashed was never closed.** What does a restarted twin do with it? Options: (i) treat it as closed from now on: no session may update it (the session that made it is gone), and it is examined as it is; (ii) offer to recover it into a new file, leaving the original untouched; (iii) let the next session of the same person continue it. (iii) needs a way to prove "the same person", which the name does not give, and stretches "that same session"; it is listed only to be ruled out or chosen deliberately.

**M4. What does a crash during an update leave?**

The file is encrypted at rest (the principal's direction), so a half-written update can make the whole file unreadable unless the format is built for it. Options, none chosen:

- **Write a complete new version beside the file and replace it atomically.** A replace of a file the session itself created and has not closed is allowed by the decision. Costs: twice the space during a save; "atomic" depends on the filesystem, and on network or mapped container directories it is weaker (general knowledge, not from a repository file). Gains: the file is always either the old version or the new.
- **An append-only format in which each addition is separately authenticated.** Costs: a more elaborate format and opener. Gains: a crash loses at most the last addition; fits M2 (c).
- **Accept that a crash during an update can lose the file.** Costs: a person loses what they asked to keep, though by a fault and not silently if the opener says the file is damaged. Gains: nothing to build.

The file-format question (E) has to cover this. A mark inside the header, or a final authenticated record, saying the file was closed would let an opener tell a closed file from one that never was, which M1, M3 and M5 all need.

**M5. From when does the retention period run, and is an unclosed file offered for deletion?**

*(Since M2 was answered, 2026-11-11: under reading (A) the files of one session form a chain, the retention clock as recorded is per file, and deleting an earlier file can leave later ones unusable; see P3.)*

"Kept for one week" (`REQ-SIMFW-21` part (f)) needs a start, and with files that are updated while open there are three candidates: **from creation** (costs: a file from a long session, or one continued over days if that is ever allowed, can be expired the moment it is closed); **from closing** (costs: an unclosed file left by a crash has no start, so it needs its own rule); **from the last update** (costs: the same for a crashed file, though the last write time serves; under M2 (b) the period restarts continuously while the twin runs). And for a file left unclosed by a crash: when does its period start, and is it listed and offered for deletion with the expired copies, or shown apart as a damaged or unfinished file? This extends point R0 of `oi-simfw-05-temporary-file.md` (counted from saving or from last opening).

**M6. How this bears on "when a save may be requested".**

Continuing to update a file during a session suggests that a save can be requested during the session, since there is otherwise nothing to continue. It informs that question (Q3 of `oi-simfw-05-temporary-file.md`, options A, B, C) and does not answer it: a save chosen at start whose file is then updated through the session (M2 (b)) fits the sentence equally. **Since answered separately by the principal, 2026-11-11: both at launch and by a command during the session (`oi-simfw-05-temporary-file.md` §5).** Q3's open list is now one point: how the directory is picked safely.

**M7. 'Save as': the details the sentence does not give.** Each is a question.

- **The new name or directory is the same as the open file's.** Is that an update of the open file (a plain save), or is it refused because a 'save as' must make a new file? If it is treated as a 'save as', the file would be closed and then written again, which the closed rule forbids, so it cannot simply proceed.
- **The new name belongs to a closed file.** This is the second half of question L, still open: refuse and tell the person, append a counter, or ask for another name.
- **Is 'save as' available at launch?** A save can be requested at launch (`REQ-SIMFW-21` part (j)). At launch no file is yet open in this run, though under a session that spans runs one may be open from an earlier run (N2, N4 of `oi-simfw-05-temporary-file.md`): does a launch-time request with a name close it?
- **In a container** the same limits apply as to any save: the new file can only be in the mapped directory, since a running container cannot gain a mount (C1 of `oi-simfw-05-temporary-file.md`).
- **Is the file that a 'save as' closes complete at that moment?** That depends on what "update" means (M2): under a snapshot it is whatever the last save wrote; under a live mirror it is current.

## 7. Questions the answer to M2 raises (none decided)

**P1. What is "content", and where is the base?**

The principal decided that the twin stores everything in a file: the UHDR and SHDR records and the synthetic state. Something that is appended on each save must then be a record of what changed since the last save, a journal, and not another copy of whole partitions. **That is an inference from "appends", not something the principal said.** If it is right, the file format is a base state followed by appended records; for a file-system image those records would be the changed blocks, or the operations that changed them. The first file of a session must then hold a base (the initial state) or refer to one. Under reading (A) a file created by 'save as' has no base of its own. Where is the base: only in the first file of the session; copied into each new file (which is reading (B) in effect); or is the initial state always the same empty part, so that no base need be stored? Costs: the first makes every later file depend on the first; the second makes files large and repeats content; the third holds only if a session always starts from an empty part, which a session that spans runs of the twin (N2 of `oi-simfw-05-temporary-file.md`) may not.

**P2. Chains of files.** Under reading (A) a file made by 'save as' can be used only together with the earlier files of the same session.

- **(a) Each file records the identity of the one before it:** a session identifier and a sequence number, and a digest of the previous file's final state, so an opener can check it holds a complete, unaltered chain. Costs: more in the header; a renamed or moved file is still recognised, but a missing one makes the rest unusable and the opener must say so.
- **(b) Each file is independent.** This contradicts reading (A); it is reading (B).
- **(c) The opener opens one file at a time and says what is missing.** Costs: a person sees a fragment and has to find the rest themselves. Gains: the simplest opener.

This bears on question B (what a double click on one file of a chain does), question E (the header, which would carry the chain information with the magic, version and provenance) and the session identifier of M3.

**P3. The expiry offer and dependent files.** It is decided that the twin lists expired copies and offers to delete them (`REQ-SIMFW-21` part (g)). Under reading (A), deleting an earlier file of a chain can leave the later files unusable, and the retention clock is per file (M5). **Without a rule, an expiry offer could invite a person to destroy the base of a saved run.** Options: the offer lists a chain as one unit and warns when a deletion would orphan a later file (costs: the twin must find the whole chain, P2); or it lists files singly and says what depends on each (costs: the person has to work out the effect); or the retention period of a chain is that of its newest file, so the files of a chain expire together (costs: an old file is kept longer than its own week; this changes what part (f) means for a chain and would need the principal's word).

**P4. Appending, encryption and a crash** (M4). An append-only file in which each appended record is separately authenticated was one of M4's three options and is now the natural one: the answer settles M2 and narrows M4. It does not settle: the key (question H); whether every file of a chain uses the same key (if not, the opener needs several); or what a crash in the middle of an append leaves, which would be a truncated last record that the opener has to recognise and discard without rejecting the file. A final authenticated record marking the file closed would tell a closed file from an unclosed one (M1).

**P5. The first 'save' of a session, and a 'save as' with no earlier 'save'.** Under reading (A), what is "since the last 'save'" when there has been none? It is supposed that it means since the start of the session, to be confirmed and not decided here. With a session that defaults to a login and may span runs of the twin, "the start of the session" is itself something to pin down (N1, N2 of `oi-simfw-05-temporary-file.md`).

**P6. A 'save' with nothing new since the last one.** An append of nothing, which leaves the file as it was and says so; or a refusal. A small question, for the build.

**P7. A save requested at launch** (S1 of `oi-simfw-05-temporary-file.md`). The answer makes a save an explicit command that appends. A launch-time request then means either "open a file at start and append to it on each save command" or "append continuously as the twin runs". Not decided; S1 stays open.

**P8. What the twin must track** (a build question, listed). Under Q2 option D the working file is unnamed and separate. A saved file is now a journal and not a copy of the working file, so a save no longer means copying the working file through its open handle, as was noted earlier under Q3. To append what changed since the last save, the twin has to know what changed: a change log, or write counters per block. How is for the build.
