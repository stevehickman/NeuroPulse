# NPPS PEG Grammar

**Document:** NP-NPPS-GRAM-001 Rev 6  
**Status:** ACTIVE  
**Date:** 2026-10-04

> **Rev 6 (2026-10-04): absolute quantities and one wavelength per block** (NP-NPPS-REF-001 Rev 18 §4.1a, §4.1b). The grammar gains the `dB` unit suffix (`mW_cm2` was already listed) and, for the first time, makes semantic checks itself, in a protocol's modality blocks: `intensity` / `intensity_percent` on the optical modalities and `volume_percent` on audio are errors; `irradiance` and `volume` must carry `mW_cm2` and `dB` (a `%`, a wrong unit or a bare number is an error); the combined wavelength names `660_808nm` and `660_808_1064nm` are errors; `wavelength` and an irradiance (PBM) or a volume (audio) are required. In a `limits` block `max_intensity` on the PBM and audio sub-blocks is an error. Each is a way to write a stimulus that does not say what it is, which is why they are here rather than left to the runtimes. Everything else about a field's meaning is still theirs. Five new `error_*` fixtures assert the refusals and `absolute_quantities.npps` the accepted form. `npps-parser.mjs` was regenerated with Peggy 5.1.0, after regenerating the Rev 5 grammar reproduced the committed parser byte for byte.

> **Rev 5 (2026-09-29): a `wavelength_rules` top-level block** (`WavelengthRulesEntry`, `ChannelBlock`), NP-NPPS-REF-001 Rev 17 §7a. Nothing else in the grammar changes. A modality block's fields were already generic `Field`s, so the new `start` field (§5) and a single-wavelength `wavelength: "810nm"` (§4.1a) needed no rule: they are checked by the runtimes, as a zone's socket ids are. `npps-parser.mjs` was regenerated with Peggy 5.1.0. Regenerating the Rev 4 grammar reproduced the committed parser byte for byte first, so the diff is the new rule only. New fixtures: `npps/fixtures/per_wavelength_series.npps` and `wavelength_rules.npps`.

## Overview

`npps.peggy` is the formal PEG grammar for the NeurOne Protocol Script (NPPS) language. It is the **single source of truth** for NPPS syntax. The hand-written Swift lexer/parser (`NPProtocolScripting.swift`) and TypeScript parser (`nppsParser.ts`) must accept exactly the language this grammar defines.

## Generating a parser

```bash
bunx peggy npps.peggy --format es
```

This produces `npps.js` (ES module). Import and use:

```typescript
import * as npps from './npps.js';

const ast = npps.parse(source);
```

For CommonJS output:

```bash
bunx peggy npps.peggy --format commonjs
```

## Language coverage

The grammar covers the complete NPPS language:

- **Protocol blocks** -- single session definitions with metadata fields and modality blocks
- **Composite blocks** -- multi-layer session compositions with timing offsets
- **Limits blocks** -- per-helmet, per-individual, or global safety limits
- **Zone and condition blocks** -- named socket sets and condition links (NP-NPPS-REF-001 §8, §9)
- **Wavelength-rules blocks** (Rev 5) -- per-channel windows that map a protocol's stated PBM wavelength onto an emitter channel (NP-NPPS-REF-001 §7a)
- **15 modality types** -- `pbm_transcranial`, `pbm_intranasal`, `pbm_deep_1170nm`, `eeg_neurofeedback`, `bes_tacs`, `tdcs`, `vns_hrv`, `audio_entrainment`, `visual_stimulation`, `qeeg_21ch`, `tms`, `clinical_tacs`, `hd_tdcs`, `cervical_vns`, `vibrotactile_40hz` — the same 15 `nppsParser.ts`, `NPProtocolScripting.swift` and NP-NPPS-REF-001 §12 accept. (`hrv_biofeedback` and `pbm` were removed at Rev 3: no other component recognised them, and both are already expressible — as `vns_hrv`'s `hrv_protocol` field and `pbm_transcranial` with `wavelength: "1064nm"`.) A `pbm_transcranial` block states one wavelength, `wavelength: "810nm"`, or a legacy channel name (NP-NPPS-REF-001 §4.1a); every modality block may carry `start` (§5).
- **Value types** -- strings, numbers (with optional unit suffix), booleans, arrays (including nested), and bare identifiers `[A-Za-z_][A-Za-z0-9_]*`. Rev 4 removed `CompoundIdent` (digit-leading, `660_808nm`) and the hyphen tail of the bare-identifier rule (`wind-down`): such values are now quoted strings, so every value maps onto a JSON scalar. That is also what makes `montage: "10-20"` parse — unquoted it matched neither rule.
- **Comments** -- `#` to end-of-line (full-line and inline)
- **Unit suffixes** -- `Hz`, `%`, `mA`, `s`, `m`, `mW_cm2`, `dB`

## AST node types

The parser produces an array of `Entry` nodes:

```
File = Entry[]

Entry
  = { kind: 'single',    protocol:  ProtocolNode  }
  | { kind: 'composite', composite: CompositeNode }
  | { kind: 'limits',    limits:    LimitsNode    }
  | { kind: 'zone',      zone:      ZoneNode      }
  | { kind: 'condition', condition: ConditionNode }
  | { kind: 'wavelength_rules', wavelengthRules: WavelengthRulesNode }   -- Rev 5

ProtocolNode = {
  name: string,
  fields: Field[],
  modalities: ModalityBlock[]
}

CompositeNode = {
  name: string,
  fields: Field[],
  layers: LayerBlock[]
}

LimitsNode = {
  name: string | null,
  fields: Field[],
  modalityLimits: ModalityBlock[]
}

ZoneNode      = { name: string, fields: Field[] }
ConditionNode = { name: string, fields: Field[] }

WavelengthRulesNode = {
  name: string,
  fields: Field[],               -- level, description
  channels: ChannelBlock[]       -- channel "led_808" { nominal_nm, min_nm, max_nm }
}

ModalityBlock = { type: string, fields: Field[] }
LayerBlock    = { name: string, fields: Field[] }
ChannelBlock  = { name: string, fields: Field[] }
Field         = { key: string, value: Value }

Value
  = string                              -- string literal
  | number                              -- bare number (no unit)
  | boolean                             -- true / false
  | { number: number, unit: string }    -- number with unit suffix
  | { ident: string }                   -- bare or compound identifier
  | Value[]                             -- array (possibly nested)
```

## Lexical rules

| Token | Rule |
|-------|------|
| Comments | `#` to end of line |
| Strings | Double-quoted, escapes: `\"` `\\` `\n` `\t` |
| Numbers | Optional `-`, digits, optional `.digits`, optional unit suffix |
| Digit-leading values | Not a token: `"660_808nm"`, `"10-20"` must be quoted (Rev 4) |
| Hyphenated values | Not a token: `"wind-down"` must be quoted (Rev 4) |
| Absolute quantities | `irradiance: 300mW_cm2`, `volume: 72dB` — the unit suffix is required on these two keys (Rev 6) |
| Booleans | `true` and `false` (not followed by `[a-zA-Z0-9_]`) |
| Whitespace | Spaces, tabs, newlines are insignificant (newlines act as field separators) |
| Keywords | `protocol`, `composite`, `limits`, `zone`, `condition`, `wavelength_rules`, `layer`, `channel`, modality type names |

## Relationship to other parsers

- **Swift** (`NPProtocolScripting.swift`) -- hand-written lexer/parser for the iOS app; must accept the same language
- **TypeScript** (`nppsParser.ts`) -- hand-written parser for the web app; must accept the same language
- **Peggy-generated** -- reference parser generated from this grammar; use for conformance testing

When the Swift or TypeScript parsers diverge from this grammar, this grammar is authoritative.

## Swift codegen options

No mature Swift PEG codegen exists as of 2026. Evaluated options:

- **packcc** (C PEG generator) → generates C parser, callable from Swift via C interop. Adds a C build dependency; workable but heavy.
- **citron** (Swift LALR parser generator) → LALR, not PEG; different grammar class. Would require rewriting the grammar.
- **swift-parsing** (Point-Free) → combinator library, not codegen from a grammar file. Requires hand-translating the PEG rules into Swift combinators.

**Current approach:** the Swift parser remains hand-written (`NPProtocolScripting.swift`). Conformance is enforced by the **shared test fixtures** in `npps/fixtures/` — both parsers must produce identical output for every fixture. The PEG grammar is the spec; the fixtures are the cross-platform conformance test.
