# NPPS PEG Grammar

**Document:** NP-NPPS-GRAM-001 Rev A  
**Status:** ACTIVE  
**Date:** 2026-06-28

## Overview

`npps.peggy` is the formal PEG grammar for the NeuroPulse Protocol Script (NPPS) language. It is the **single source of truth** for NPPS syntax. The hand-written Swift lexer/parser (`NPProtocolScripting.swift`) and TypeScript parser (`nppsParser.ts`) must accept exactly the language this grammar defines.

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
- **17 modality types** -- `pbm_transcranial`, `pbm_intranasal`, `pbm_deep_1170nm`, `eeg_neurofeedback`, `bes_tacs`, `tdcs`, `vns_hrv`, `audio_entrainment`, `visual_stimulation`, `qeeg_21ch`, `tms`, `clinical_tacs`, `hd_tdcs`, `cervical_vns`, `vibrotactile_40hz`, `hrv_biofeedback`, `pbm_1064nm`
- **Value types** -- strings, numbers (with optional unit suffix), booleans, arrays (including nested), compound identifiers (`660_808nm`), bare/hyphenated identifiers (`wind-down`)
- **Comments** -- `#` to end-of-line (full-line and inline)
- **Unit suffixes** -- `Hz`, `%`, `mA`, `s`, `m`, `mW_cm2`

## AST node types

The parser produces an array of `Entry` nodes:

```
File = Entry[]

Entry
  = { kind: 'single',    protocol:  ProtocolNode  }
  | { kind: 'composite', composite: CompositeNode }
  | { kind: 'limits',    limits:    LimitsNode    }

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

ModalityBlock = { type: string, fields: Field[] }
LayerBlock    = { name: string, fields: Field[] }
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
| Compound idents | Digits followed by `_` or letter continues as one token (`660_808nm`) |
| Hyphenated idents | `wind-down` parses as a single identifier in value position |
| Booleans | `true` and `false` (not followed by `[a-zA-Z0-9_]`) |
| Whitespace | Spaces, tabs, newlines are insignificant (newlines act as field separators) |
| Keywords | `protocol`, `composite`, `limits`, `layer`, modality type names |

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
