# Principal question for review: OI-SIMFW-05 (b), the saved file's name, extension, opening and association

**Status:** Question A ANSWERED 2026-11-11 (the principal); B to F, and G to K which the answer raises, are open (DRAFT, not sent). Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software Engineering (`OI-SIMFW-05`, `NP-SIM-FW-001` §7, `REQ-SIMFW-21`)
**To:** The principal (with Quality for Q5 of `oi-simfw-05-temporary-file.md`, which question E here touches)
**Response needed:** an option for each of B to K, with a short rationale, name, role and date.
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

An option for each of B to K, a sentence on why for each (`CLAUDE.md` §18 wants the reason recorded), anything that would reopen it, and name, role and date. The answers are recorded in `NP-SIM-FW-001` §7 (`OI-SIMFW-05`) and in `docs/status/completed-decisions.md`, and the requirement rows they imply are written then.

## 4. Answers

**The principal, 2026-11-11, verbatim, on question A:** "Using a pseudonymous ID for the userID (the user doing the saving can pick a userID that only they know even if it doesn't match their logged in userID. It can't clash with a previously picked pseudonymous ID that another user picked). I recognize there is a challenge about data privacy here. We address it by encryption of the file at rest."

**Question A: decided.** The userID in the default name of a saved file is a pseudonymous id chosen by the person who saves. It need not match their login, and it is theirs alone to know. Against the options of section 2 this is option (3), a pseudonymous id, with one difference: option (3) proposed an id the machine makes at random, and the decision is an id the person picks. Recorded as `REQ-SIMFW-21` part (e). Not built.

**Also in the sentence, recorded as the principal's direction and not as a closed question:**

- **The id cannot clash with a pseudonymous id another user picked earlier.** How that is achieved is not said, and it pulls against "only they know" (question G).
- **The file is encrypted at rest, and that is how the principal addresses the data-privacy concern** raised under question A: an identity in a file name beside SHDR records (`CLAUDE.md` §5.1). This partly answers Q4 of `oi-simfw-05-temporary-file.md`, in its half "is the synthetic UHDR in the file encrypted": encrypted. It does not say which of that question's options (1) or (3) is meant, under which key, or whether the working temporary file is encrypted as well as a saved copy (questions H to K). Q4 is not answered.

**Not said and still open in question A itself:** the timestamp's format and time zone, and what the default name is if the person has picked no id yet. The decision makes the second easier to state (the save asks for an id), but it was not said.

**Rationale.** The sentence gives its own: the principal recognises the data-privacy challenge and addresses it by encryption at rest. Nothing further was given.

**Recorded in:** `NP-SIM-FW-001` §5 (`REQ-SIMFW-21` part (e)) and §7 (`OI-SIMFW-05`), and `docs/status/completed-decisions.md` (2026-11-11).

## 5. Questions the answer to A raises (none decided)

**G. How is "cannot clash with an id another user picked" made true, and how much uniqueness is needed?**

The tension, stated plainly: an id that only its owner knows, and an id that is checked against everyone else's, cannot both hold unless uniqueness is established without disclosing ids. Any registry that answers "taken" lets a person confirm that an id exists, and by trying many, enumerate them. And a shared registry is a store that someone holds (NeurOne, or the bench's owner) linking pseudonyms to the people who registered them, which is against the spirit of `CLAUDE.md` §5.1 (SHDR is never linked to user identity) and §5.3 (no linkage table exists).

What a clash would actually cause: two people's saved files would begin with the same characters. The timestamp in the name already separates the files, and each file is encrypted (the principal's direction), so a clash does not open one person's file to another. It would make it unclear whose file is whose when files from several people are in one directory. How much that matters is the principal's to judge.

- **(a) Unique on this machine only.** The twin keeps a local list of ids used here and refuses a repeat by a different operating-system user. Costs: it cannot see other machines, so two people on two machines can pick the same id; the local list is itself a small store of pseudonyms on a shared machine, and "taken" tells one user of the machine that another used that id. Gains: no network, no registry anyone else holds.
- **(b) No registry: a random part is appended to the id the person picks,** so a clash is negligible by construction. The picked id is the readable part. Costs: the name is longer, and the person cannot guarantee a short id is theirs alone, only that the whole of it is. Gains: nothing is stored anywhere and nothing can be enumerated. How long the random part must be follows from how many ids are expected and what clash probability is accepted: for n ids and b random bits the probability of any clash is about n squared divided by 2 to the power (b + 1). **An illustration with inputs that are mine and not proposed:** for 1,000 ids and a probability of at most one in a million, b must be at least log2(1,000² / (2 × 10⁻⁶)), about 38.9, so 39 bits, which is 8 characters of base 32. The two inputs are the principal's to set; nothing in the repository uses a figure from this.
- **(c) A shared registry of salted hashes of ids.** Costs: it is still a store someone holds; a hash of a short human-picked id is guessable by trying likely ids, so it leaks which ids exist to anyone who can query it or who obtains it, and with the salt known it can be searched offline; it needs a network and an operator. Gains: uniqueness across machines.
- **(d) A registry kept by whoever owns a bench, for that bench only.** Costs: the bench owner holds a list tying pseudonyms to the people who use the bench, which is a linkage table in small; it does nothing for a person not on a bench. Gains: uniqueness where files from several people actually meet.

**H. Who holds the key that "encrypted at rest" needs?**

- **(a) A TEST key in the twin.** The twin holds test keys only (`REQ-SIMFW-08`). Costs, plainly: a test key is in the repository, so it keeps out nobody who has the code, and it protects nothing that a reader of this repository could not undo. It would make the file unreadable to ordinary tools and to no one else. Gains: nothing for the person to remember; double click needs no prompt.
- **(b) A key derived from a passphrase the person gives when saving,** needed again to open. Costs: a forgotten passphrase loses the file; opening on double click needs a prompt; a weak passphrase is a weak key. Gains: only the person can open the file, which is the only option here under which the encryption protects against someone who holds the file and the code.
- **(c) A key held in the host's own credential store, per operating-system user.** Costs: different on every platform; not available inside the container without a host-side helper; the file opens only on that machine and for that user, so it cannot be handed to someone else to examine. Gains: no passphrase; protected from other users of the machine.
- **(d) A random key per file, kept in the file's header, sealed under (b) or (c).** Costs: those of whichever seals it, and a little more format. Gains: the sealing secret can change without rewriting the file, and the header of question E has a natural place for it.

Whichever is chosen, **the opener needs the key.** That bears on question B: a viewer or a resuming twin started by double click must obtain it (a prompt under (b), the credential store under (c), nothing under (a)).

**I. What is encrypted?**

- **(1) The saved copy only.** Costs: the working temporary file is in the clear while the twin runs. It is owner-only and, where the host allows it, has no name (Q1 and Q2 of `oi-simfw-05-temporary-file.md`), and on a host where it has a name and can be left behind it would be left in the clear. Gains: the twin's own storage path is unchanged.
- **(2) The saved copy and the working temporary file.** Costs: every read and write of the twin's storage passes through the encryption, and the working file needs a key before anyone has asked to save (a key made for the session and discarded would do, and then a save re-encrypts under the key of question H). Gains: nothing is ever on disk in the clear.

**Encryption at rest does not cover the file's name.** The pseudonymous id and the timestamp are in the clear in the name whatever is chosen here. The pseudonym is what stands between the name and a person, which is why question G matters and why a registry that ties pseudonyms to people would undo it.

**J. Where does the encryption code live?** (A consequence to confirm, not a design.)

`CLAUDE.md` §22: cryptographic work is written in Rust, once, in `common/npps-core`, and is not taken from a platform or third-party library; every new crate joins `docs/np_soup_rust_001.md` in the change that adds it. File encryption for the twin falls under that rule. It follows that the build needs the work done in `common/npps-core` and reached from the twin, a SOUP entry if a new crate is used, and an open item first if any part were to be written in another language. Confirm that the rule applies to the simulator's file as it does to the product, or say that it does not.

**K. Which layer is meant: the file, the partition inside it, or both?**

Q4 of `oi-simfw-05-temporary-file.md`, option (1), was "encrypted as the device does it, under a TEST key": the twin runs the device's own UHDR key and storage code (`OI-SIMFW-52`, not built), so the UHDR partition inside the file is sealed as the device seals it. The principal's sentence speaks of the file being encrypted at rest, which reads as a different layer: the whole of what the twin writes, encrypted as a file.

- **(1) Whole-file encryption only.** The partitions inside are as the twin writes them. Costs: the device's own UHDR encryption path is not exercised by this. Gains: can be built without `OI-SIMFW-52`.
- **(2) The device's partition encryption only,** inside an otherwise clear file. Costs: the SHDR partition and the twin's synthetic state are in the clear, which is not what "the file" encrypted at rest says; waits for `OI-SIMFW-52`. Gains: the file holds what a device would.
- **(3) Both.** Costs: two layers and two keys to manage. Gains: the file is protected as a file, and the twin runs the device's path.
