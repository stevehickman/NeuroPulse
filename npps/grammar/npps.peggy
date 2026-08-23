// NPPS PEG Grammar — NeurOne Protocol Script
// NP-NPPS-GRAM-001 Rev 3
// Single source of truth for NPPS syntax.
// Generate parser: bunx peggy npps.peggy --format es
//
// Rev B (2026-07-17): zones are no longer fixed hardware indices. A zone is a
// NAMED SET OF MODULES, defined by a top-level `zone` block and referenced by
// name from protocol modality blocks. A set of predefined zones ships with the
// app in 00-zones.npps; users may define their own in any .npps file. Nothing in
// the grammar or the toolchain knows what a zone MEANS — several shipped zones
// are named after brain lobes, but that is authored data, not a concept the
// system models (ZONE-1, NP-HEX-ZM-001 §3.3). Added top-level
// `condition` blocks (condition name → external reference link) and protocol
// `conditions` / `references` fields. All loaded files share ONE namespace
// (see NP-NPPS-REF-001 §1.6): a zone or condition defined in any file under the
// protocol directory tree is referenceable by name from any other file.
//
// Rev 3 (2026-08-23): removed two modality types that no other component ever
// recognised — `hrv_biofeedback` and `pbm_1064nm`. Neither appears in
// NP-NPPS-REF-001's TYPE_ID list, in nppsParser.ts's blockNameToTypeId map, in
// NPProtocolScripting.swift, or in any fixture or shipped .npps file, so a file
// using them parsed here and then yielded a null modality in every consumer.
// Both are already expressible: HRV biofeedback is `vns_hrv`'s `hrv_protocol`
// field, and 1064 nm PBM is `pbm_transcranial` with `wavelength: 1064nm`.
// ModalityType is now the same 15 types the rest of the toolchain accepts.
// (The firmware directories firmware/hrv_biofeedback/ and firmware/pbm_1064nm/
// are unrelated — they are implementation modules, not NPPS block keywords.)
//
// AST node types:
//   File          = Entry[]
//   Entry         = { kind: 'single',    protocol:  ProtocolNode  }
//                 | { kind: 'composite', composite: CompositeNode }
//                 | { kind: 'limits',    limits:    LimitsNode    }
//                 | { kind: 'zone',      zone:      ZoneNode      }
//                 | { kind: 'condition', condition: ConditionNode }
//   ProtocolNode  = { name: string, fields: Field[], modalities: ModalityBlock[] }
//   CompositeNode = { name: string, fields: Field[], layers: LayerBlock[] }
//   LimitsNode    = { name: string|null, fields: Field[], modalityLimits: ModalityBlock[] }
//   ZoneNode      = { name: string, fields: Field[] }
//   ConditionNode = { name: string, fields: Field[] }
//   ModalityBlock = { type: string, fields: Field[] }
//   LayerBlock    = { name: string, fields: Field[] }
//   Field         = { key: string, value: Value }
//   Value         = string | number | boolean | Value[] | { number: number, unit: string }
//                 | { ident: string }

{{
// -- Helpers available in all action blocks --

function makeNumber(sign, intPart, fracPart) {
  const raw = (sign || '') + intPart + (fracPart || '');
  return parseFloat(raw);
}
}}

// ─────────────────────────── Top-level ───────────────────────────

File
  = _ entries:Entry* _ { return entries; }

Entry
  = ProtocolEntry
  / CompositeEntry
  / LimitsEntry
  / ZoneEntry
  / ConditionEntry

// ─────────────────────────── Protocol ────────────────────────────

ProtocolEntry
  = "protocol" __ name:StringLiteral _ "{" _ body:ProtocolBody _ "}" _
    { return { kind: 'single', protocol: { name, ...body } }; }

ProtocolBody
  = members:ProtocolMember*
    {
      const fields = [];
      const modalities = [];
      for (const m of members) {
        if (m._type === 'field') fields.push({ key: m.key, value: m.value });
        else modalities.push({ type: m.type, fields: m.fields });
      }
      return { fields, modalities };
    }

ProtocolMember
  = _ m:ModalityBlock _ { return m; }
  / _ f:Field _         { return { _type: 'field', ...f }; }

// ─────────────────────────── Composite ───────────────────────────

CompositeEntry
  = "composite" __ name:StringLiteral _ "{" _ body:CompositeBody _ "}" _
    { return { kind: 'composite', composite: { name, ...body } }; }

CompositeBody
  = members:CompositeMember*
    {
      const fields = [];
      const layers = [];
      for (const m of members) {
        if (m._type === 'layer') layers.push({ name: m.name, fields: m.fields });
        else fields.push({ key: m.key, value: m.value });
      }
      return { fields, layers };
    }

CompositeMember
  = _ l:LayerBlock _ { return l; }
  / _ f:Field _      { return { _type: 'field', ...f }; }

LayerBlock
  = "layer" __ name:StringLiteral _ "{" _ fields:LayerField* _ "}" _
    { return { _type: 'layer', name, fields }; }

LayerField
  = _ f:Field _ { return { key: f.key, value: f.value }; }

// ─────────────────────────── Zone ────────────────────────────────
//
// A zone is a NAMED SET OF MODULES, defined as an explicit list of socket
// (major) addresses — the first half of the firmware two-level
// (socket:element) scheme (np_module_map). Listing sockets directly makes
// arbitrary, non-contiguous zones definable:
//   sockets: [int, ...]      → the modules (by socket id) in this zone
// Optional element-type filter: types: [<elem-type>, ...] with exclude_types.
// The predefined zones ship as socket lists in 00-zones.npps — that file is the
// only place a zone is defined.

ZoneEntry
  = "zone" __ name:StringLiteral _ "{" _ fields:ZoneField* _ "}" _
    { return { kind: 'zone', zone: { name, fields } }; }

ZoneField
  = _ f:Field _ { return { key: f.key, value: f.value }; }

// ─────────────────────────── Condition ───────────────────────────
//
// A condition definition pairs a standard condition name with a link to an
// external definition (e.g. Wikipedia / MeSH). Protocols reference conditions
// by name from their `conditions` field; every referenced name MUST resolve to
// a condition block loaded into the namespace.

ConditionEntry
  = "condition" __ name:StringLiteral _ "{" _ fields:ConditionField* _ "}" _
    { return { kind: 'condition', condition: { name, fields } }; }

ConditionField
  = _ f:Field _ { return { key: f.key, value: f.value }; }

// ─────────────────────────── Limits ──────────────────────────────

LimitsEntry
  = "limits" __ name:StringLiteral _ "{" _ body:LimitsBody _ "}" _
    { return { kind: 'limits', limits: { name, ...body } }; }
  / "limits" _ "{" _ body:LimitsBody _ "}" _
    { return { kind: 'limits', limits: { name: null, ...body } }; }

LimitsBody
  = members:LimitsMember*
    {
      const fields = [];
      const modalityLimits = [];
      for (const m of members) {
        if (m._type !== 'field') modalityLimits.push({ type: m.type, fields: m.fields });
        else fields.push({ key: m.key, value: m.value });
      }
      return { fields, modalityLimits };
    }

LimitsMember
  = _ m:ModalityBlock _ { return m; }
  / _ f:Field _         { return { _type: 'field', ...f }; }

// ─────────────────────────── Modality block ──────────────────────

ModalityBlock
  = type:ModalityType _ "{" _ fields:ModalityField* _ "}"
    { return { _type: 'modality', type, fields }; }

ModalityField
  = _ f:Field _ { return { key: f.key, value: f.value }; }

ModalityType "modality type"
  = "pbm_transcranial"
  / "pbm_intranasal"
  / "pbm_deep_1170nm"
  / "eeg_neurofeedback"
  / "bes_tacs"
  / "tdcs"
  / "vns_hrv"
  / "audio_entrainment"
  / "visual_stimulation"
  / "qeeg_21ch"
  / "tms"
  / "clinical_tacs"
  / "hd_tdcs"
  / "cervical_vns"
  / "vibrotactile_40hz"

// ─────────────────────────── Fields ──────────────────────────────

Field
  = key:Identifier _ ":" _ value:Value
    { return { key, value }; }

// ─────────────────────────── Values ──────────────────────────────

Value "value"
  = BooleanLiteral
  / NumberWithUnit
  / StringLiteral
  / ArrayLiteral
  / CompoundIdent
  / BareIdent

BooleanLiteral
  = "true"  !IdentContinue { return true; }
  / "false" !IdentContinue { return false; }

// Number with optional unit suffix.
// Must check that a bare number is not followed by _ or letter
// (which would make it a compound ident like 660_808nm).
NumberWithUnit
  = sign:"-"? int:Digits frac:("." Digits)? unit:UnitSuffix
    {
      const n = makeNumber(sign, int, frac ? '.' + frac[1] : null);
      return { number: n, unit };
    }
  / sign:"-"? int:Digits frac:("." Digits)? !("_" / [a-zA-Z])
    {
      return makeNumber(sign, int, frac ? '.' + frac[1] : null);
    }

UnitSuffix "unit suffix"
  = "mW_cm2" { return 'mW/cm2'; }
  / "Hz"     { return 'Hz'; }
  / "mA"     { return 'mA'; }
  / "%"      { return '%'; }
  / "s"  !IdentContinue { return 's'; }
  / "m"  !IdentContinue { return 'm'; }

// Compound identifier: starts with digits, continues with _ or letter.
// Examples: 660_808nm, 660_808_1064nm, 40Hz_mode
CompoundIdent
  = head:Digits tail:CompoundIdentTail+
    { return { ident: head + tail.join('') }; }

CompoundIdentTail
  = "_" chars:IdentChars { return '_' + chars; }
  / chars:$([a-zA-Z] IdentChars?) { return chars; }

IdentChars
  = $([a-zA-Z0-9_]+)

// Bare identifier — a plain word, possibly with hyphens (for tags like "wind-down").
BareIdent
  = id:HyphenatedIdent { return { ident: id }; }

HyphenatedIdent "identifier"
  = head:IdentStart tail:$(("-" IdentStart)*)
    { return head + tail; }

IdentStart
  = $([a-zA-Z_] [a-zA-Z0-9_]*)

StringLiteral "string"
  = '"' chars:StringChar* '"' { return chars.join(''); }

StringChar
  = "\\\""  { return '"'; }
  / "\\\\"  { return '\\'; }
  / "\\n"   { return '\n'; }
  / "\\t"   { return '\t'; }
  / [^"\\]

ArrayLiteral "array"
  = "[" _ elements:ArrayElements? _ "]"
    { return elements || []; }

ArrayElements
  = head:Value tail:(_ "," _ Value)*
    { return [head, ...tail.map(t => t[3])]; }

// ─────────────────────────── Identifiers (field keys) ────────────

Identifier "identifier"
  = $([a-zA-Z_] [a-zA-Z0-9_]*)

IdentContinue
  = [a-zA-Z0-9_]

Digits "digits"
  = $([0-9]+)

// ─────────────────────────── Whitespace and comments ─────────────

// Optional whitespace (spaces, tabs, newlines, comments)
_ "whitespace"
  = (WhitespaceChar / Comment)*

// Required whitespace (at least one space/tab/newline/comment)
__ "required whitespace"
  = (WhitespaceChar / Comment)+

WhitespaceChar
  = [ \t\n\r]

Comment "comment"
  = "#" [^\n\r]* ("\n" / "\r\n" / "\r" / !.)
