// NPPS PEG Grammar — NeurOne Protocol Script
// NP-NPPS-GRAM-001 Rev 7
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
// Rev 7 (2026-10-06): `frequency: 0` on pbm_transcranial, pbm_intranasal and pbm_deep_1170nm is CW and CW has no
// duty cycle, so a duty_cycle other than 100% beside it is an error (OI-SESPWR-03). New fixture error_cw_with_duty.
//
// Rev 6 (2026-10-04): absolute quantities, one wavelength per block. Two new unit
// suffixes, `mW_cm2` (already listed, now exercised) and `dB`, and four checks in
// a protocol's modality blocks that this grammar now makes instead of leaving to
// the runtimes — because each is a way to write a stimulus that does not say what
// it is:
//   - PBM irradiance and audio level are absolute: `irradiance: 300mW_cm2`,
//     `volume: 72dB`. A percentage is a fraction of a baseline the hardware owns
//     and can change, so `intensity: 80%` / `volume: 70%` are errors, as is a
//     unit-less or wrong-unit value on the short names (the unit is the point).
//   - `pbm_transcranial` and `pbm_intranasal` take ONE wavelength per block,
//     `"<N>nm"`. The combined names `660_808nm` / `660_808_1064nm` welded
//     independent emitters together and are errors; write one block per
//     wavelength.
//   - Those fields and `wavelength` are REQUIRED: defaulting an irradiance or a
//     sound level would deliver a stimulus nobody authored.
//   - In `limits`, a percentage `max_intensity` on the PBM and audio blocks is an
//     error, not a skipped field: a ceiling dropped silently is no ceiling.
// Everything else about a field's meaning is still the runtimes' to check.
//
// Rev 4 (2026-08-23): values are now JSON-shaped. A bare value may only be a
// plain identifier [A-Za-z_][A-Za-z0-9_]*, a number (unit suffixes kept) or a
// boolean; anything else is a quoted string. Removed CompoundIdent
// (digit-leading: 660_808nm, 1064nm) and the hyphen tail of the bare-identifier
// rule (wind-down). This is also what makes montage: "10-20" parse — unquoted it
// matched neither rule and was a hard error. Shipped library migrated; the old
// readers are dropped, not kept as a fallback. See NP-NPPS-REF-001 Rev 6 §2.
//
// Rev 3 (2026-08-23): removed two modality types that no other component ever
// recognised — `hrv_biofeedback` and `pbm`. Neither appears in
// NP-NPPS-REF-001's TYPE_ID list, in nppsParser.ts's blockNameToTypeId map, in
// NPProtocolScripting.swift, or in any fixture or shipped .npps file, so a file
// using them parsed here and then yielded a null modality in every consumer.
// Both are already expressible: HRV biofeedback is `vns_hrv`'s `hrv_protocol`
// field, and 1064 nm PBM is `pbm_transcranial` with `wavelength: 1064nm`.
// ModalityType is now the same 15 types the rest of the toolchain accepts.
// (The firmware directories firmware/hrv_biofeedback/ and firmware/pbm/
// are unrelated — they are implementation modules, not NPPS block keywords.)
//
// AST node types:
//   File          = Entry[]
//   Entry         = { kind: 'single',    protocol:  ProtocolNode  }
//                 | { kind: 'composite', composite: CompositeNode }
//                 | { kind: 'limits',    limits:    LimitsNode    }
//                 | { kind: 'zone',      zone:      ZoneNode      }
//                 | { kind: 'condition', condition: ConditionNode }
//                 | { kind: 'wavelength_rules', wavelengthRules: WavelengthRulesNode }
//   ProtocolNode  = { name: string, fields: Field[], modalities: ModalityBlock[] }
//   CompositeNode = { name: string, fields: Field[], layers: LayerBlock[] }
//   LimitsNode    = { name: string|null, fields: Field[], modalityLimits: ModalityBlock[] }
//   ZoneNode      = { name: string, fields: Field[] }
//   ConditionNode = { name: string, fields: Field[] }
//   WavelengthRulesNode = { name: string, fields: Field[], channels: ChannelBlock[] }
//   ChannelBlock  = { name: string, fields: Field[] }
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

// -- Absolute-quantity checks (Rev 6). `fail` is Peggy's error(): it throws a
// SyntaxError carrying the block's location. --

const RETIRED_WAVELENGTHS = {
  '660_808nm': ['660nm', '808nm'],
  '660_808_1064nm': ['660nm', '808nm', '1064nm'],
};

function findField(fields, key) {
  return fields.find(f => f.key === key);
}

function hasUnit(value, unit) {
  return value !== null && typeof value === 'object' && 'number' in value && value.unit === unit;
}

// `short` carries its unit as a suffix and must; `canonical` has the unit in its
// name and takes a bare number (or the matching suffix).
function checkQuantity(type, fields, short, canonical, unit, example, fail) {
  const s = findField(fields, short);
  const c = findField(fields, canonical);
  if (!s && !c) fail(`${type}: ${short} is required (${example}); a defaulted dose is a stimulus nobody authored`);
  if (s && !hasUnit(s.value, unit)) {
    const pct = s.value && typeof s.value === 'object' && s.value.unit === '%';
    fail(pct
      ? `${type}: ${short} in % is refused — a percentage is a fraction of a baseline the hardware can change; write ${example}`
      : `${type}: ${short} must carry its unit, e.g. ${example}`);
  }
  if (c && typeof c.value !== 'number' && !hasUnit(c.value, unit)) {
    fail(`${type}: ${canonical} takes a number in ${unit.replace('/', '_')}`);
  }
}

function numberOf(f) {
  if (!f) return undefined;
  if (typeof f.value === 'number') return f.value;
  if (f.value !== null && typeof f.value === 'object' && 'number' in f.value) return f.value.number;
  return undefined;
}

// `frequency: 0` selects continuous wave, and CW has no duty cycle (OI-SESPWR-03).
// A CW block that also states a duty other than 100 % contradicts itself.
function checkContinuousWave(type, fields, fail) {
  const freq = numberOf(findField(fields, 'frequency') || findField(fields, 'frequency_hz'));
  const duty = numberOf(findField(fields, 'duty_cycle') || findField(fields, 'duty_cycle_percent'));
  if (freq === 0 && duty !== undefined && duty !== 100) {
    fail(`${type}: frequency: 0 selects continuous wave, which has no duty cycle, but duty_cycle is ${duty}%. ` +
      `Remove duty_cycle for CW, or give a pulse frequency above 0 for a pulsed train`);
  }
}

function checkAbsoluteQuantities(type, fields, fail) {
  if (type === 'pbm_transcranial' || type === 'pbm_intranasal' || type === 'pbm_deep_1170nm') {
    checkContinuousWave(type, fields, fail);
  }
  const pbm = type === 'pbm_transcranial' || type === 'pbm_intranasal';
  const audio = type === 'audio_entrainment';
  const visual = type === 'visual_stimulation';
  if (!pbm && !audio && !visual) return;

  const retired = audio ? ['volume_percent'] : ['intensity', 'intensity_percent'];
  for (const key of retired) {
    if (findField(fields, key)) {
      fail(`${type}: '${key}' is a percentage of a baseline the hardware owns and can change, so it does not state the stimulus` +
        (audio ? ' — write the level in dB, e.g. volume: 72dB'
          : visual ? ' — visual_stimulation has no percentage intensity and no absolute one is defined yet; remove it'
            : ' — write irradiance in mW/cm², e.g. irradiance: 300mW_cm2'));
    }
  }
  if (pbm) {
    const w = findField(fields, 'wavelength');
    if (!w) fail(`${type}: wavelength is required, one value per block, e.g. wavelength: "810nm"`);
    if (typeof w.value !== 'string') fail(`${type}: wavelength must be a quoted string such as "810nm"`);
    if (RETIRED_WAVELENGTHS[w.value]) {
      fail(`${type}: wavelength "${w.value}" is retired — it welded independent emitters into one block; ` +
        `write one block per wavelength (${RETIRED_WAVELENGTHS[w.value].map(x => '"' + x + '"').join(' and ')}), each with its own irradiance`);
    }
    if (!/^[0-9]+(\.[0-9]+)?nm$/.test(w.value)) fail(`${type}: wavelength "${w.value}" is not a wavelength such as "810nm"`);
    checkQuantity(type, fields, 'irradiance', 'irradiance_mw_cm2', 'mW/cm2', 'irradiance: 300mW_cm2', fail);
  }
  if (audio) checkQuantity(type, fields, 'volume', 'volume_db', 'dB', 'volume: 72dB', fail);
}

function checkLimits(modalityLimits, fail) {
  for (const m of modalityLimits) {
    if ((m.type === 'pbm_transcranial' || m.type === 'pbm_intranasal' || m.type === 'audio_entrainment')
        && findField(m.fields, 'max_intensity')) {
      fail(`${m.type} limits: max_intensity is a percentage ceiling and is retired — write ` +
        (m.type === 'audio_entrainment' ? 'max_volume_db' : 'max_irradiance_mw_cm2'));
    }
  }
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
  / WavelengthRulesEntry

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
        else {
          checkAbsoluteQuantities(m.type, m.fields, msg => error(msg, m._loc));
          modalities.push({ type: m.type, fields: m.fields });
        }
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

// ─────────────────────── Wavelength rules ────────────────────────
//
// Rev 5 (2026-09-29, NP-NPPS-REF-001 §7a). Maps a protocol's stated PBM
// wavelength onto an emitter channel:
//   wavelength_rules "Name" { level: global|user  description: "…"
//     channel "led_808" { nominal_nm: 808  min_nm: 798  max_nm: 840 } … }
// The channel name, the level and the numeric fields are checked by the
// runtimes, not here: this grammar fixes the shape, as it does for zones.

WavelengthRulesEntry
  = "wavelength_rules" __ name:StringLiteral _ "{" _ members:WavelengthRulesMember* _ "}" _
    {
      const fields = [];
      const channels = [];
      for (const m of members) {
        if (m._type === 'channel') channels.push({ name: m.name, fields: m.fields });
        else fields.push({ key: m.key, value: m.value });
      }
      return { kind: 'wavelength_rules', wavelengthRules: { name, fields, channels } };
    }

WavelengthRulesMember
  = _ c:ChannelBlock _ { return c; }
  / _ f:Field _        { return { _type: 'field', ...f }; }

ChannelBlock
  = "channel" __ name:StringLiteral _ "{" _ fields:ChannelField* _ "}" _
    { return { _type: 'channel', name, fields }; }

ChannelField
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
        if (m._type !== 'field') modalityLimits.push({ type: m.type, fields: m.fields, _loc: m._loc });
        else fields.push({ key: m.key, value: m.value });
      }
      checkLimits(modalityLimits, (msg) => error(msg));
      return { fields, modalityLimits: modalityLimits.map(m => ({ type: m.type, fields: m.fields })) };
    }

LimitsMember
  = _ m:ModalityBlock _ { return m; }
  / _ f:Field _         { return { _type: 'field', ...f }; }

// ─────────────────────────── Modality block ──────────────────────

ModalityBlock
  = type:ModalityType _ "{" _ fields:ModalityField* _ "}"
    { return { _type: 'modality', type, fields, _loc: location() }; }

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
  = "mW_cm2" !IdentContinue { return 'mW/cm2'; }
  / "dB"     !IdentContinue { return 'dB'; }
  / "Hz"     { return 'Hz'; }
  / "mA"     { return 'mA'; }
  / "%"      { return '%'; }
  / "s"  !IdentContinue { return 's'; }
  / "m"  !IdentContinue { return 'm'; }

// Bare identifier — a plain word: [A-Za-z_][A-Za-z0-9_]*, the same shape a key
// takes. Rev 4 removed the two rules that admitted awkward bare values, so that
// every value is a plain ident, a number, a bool or a quoted string — each of
// which maps onto a JSON scalar:
//   - CompoundIdent, digit-leading (660_808nm, 1064nm, 660_808_1064nm)
//   - the hyphen tail of HyphenatedIdent (wind-down, all-modalities)
// Both must now be quoted. Quoting also gives `10-20` — the only montage value
// NP-NPPS-REF-001 §4.9 documents — a spelling that parses at all: it matched
// neither rule, so `montage: 10-20` was a hard parse error everywhere.
BareIdent
  = id:IdentStart { return { ident: id }; }

IdentStart "identifier"
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
