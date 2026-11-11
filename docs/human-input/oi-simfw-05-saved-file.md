# Principal question for review: OI-SIMFW-05 (b), the saved file's name, extension, opening and association

**Status:** DRAFT, not sent. Not a controlled document; see `docs/human-input/README.md`.
**From:** NeurOne Software Engineering (`OI-SIMFW-05`, `NP-SIM-FW-001` §7, `REQ-SIMFW-21`)
**To:** The principal (with Quality for Q5 of `oi-simfw-05-temporary-file.md`, which question E here touches)
**Response needed:** an option for each of A to F, with a short rationale, name, role and date.
**Why now:** on 2026-11-11 the principal answered Q1 of `oi-simfw-05-temporary-file.md` and gave direction for a saved copy: the person picks the directory, which is remembered; the default name is "the logged in userID followed by a timestamp"; and the extension is unique to NeurOne so the file opens on double click. That direction raises six questions it does not answer. None is decided here, and nothing in the repository acts as if any were.

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

An option for each of A to F, a sentence on why for each (`CLAUDE.md` §18 wants the reason recorded), anything that would reopen it, and name, role and date. The answers are recorded in `NP-SIM-FW-001` §7 (`OI-SIMFW-05`) and in `docs/status/completed-decisions.md`, and the requirement rows they imply are written then.
