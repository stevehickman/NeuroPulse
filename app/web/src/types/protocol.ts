// ─── Modality type IDs ────────────────────────────────────────────────────────

export type NPModalityTypeId =
  | 'pbm_transcranial'
  | 'pbm_intranasal'
  | 'eeg_neurofeedback'
  | 'bes_tacs'
  | 'tdcs'
  | 'vns_hrv'
  | 'audio_entrainment'
  | 'visual_stimulation'
  | 'qeeg_21ch'
  | 'tms'
  | 'pbm_deep_1170nm'
  | 'clinical_tacs'
  | 'hd_tdcs'
  | 'cervical_vns'
  | 'vibrotactile_40hz';

// ─── Modality metadata ────────────────────────────────────────────────────────

export interface NPModalityMeta {
  id: NPModalityTypeId;
  displayName: string;
  consumerName: string;
  tier: 'T1' | 'T2' | 'Accessory';
  icon: string;
  shortDescription: string;
}

export const MODALITY_META: Record<NPModalityTypeId, NPModalityMeta> = {
  pbm_transcranial: {
    id: 'pbm_transcranial',
    displayName: 'PBM Transcranial',
    consumerName: 'Photobiomodulation',
    tier: 'T1',
    icon: '💡',
    shortDescription: '660+808nm LEDs across 5 scalp zones',
  },
  pbm_intranasal: {
    id: 'pbm_intranasal',
    displayName: 'PBM Intranasal',
    consumerName: 'Intranasal Light',
    tier: 'T1',
    icon: '👃',
    shortDescription: 'Bilateral 660+808nm nasal probe',
  },
  eeg_neurofeedback: {
    id: 'eeg_neurofeedback',
    displayName: 'EEG Neurofeedback',
    consumerName: 'Brainwave Monitoring',
    tier: 'T1',
    icon: '🧠',
    shortDescription: '8-ch semi-dry EEG with closed-loop adaptation',
  },
  bes_tacs: {
    id: 'bes_tacs',
    displayName: 'BES / tACS',
    consumerName: 'Brainwave Entrainment Stimulation',
    tier: 'T1',
    icon: '⚡',
    shortDescription: '0.5–40Hz charge-balanced stimulation ≤1mA',
  },
  tdcs: {
    id: 'tdcs',
    displayName: 'tDCS',
    consumerName: 'Cortical Priming Stimulation',
    tier: 'T1',
    icon: '🔋',
    shortDescription: 'DC stimulation 0.1–2mA',
  },
  vns_hrv: {
    id: 'vns_hrv',
    displayName: 'VNS + HRV',
    consumerName: 'Vagal Stimulation & Heart Coherence',
    tier: 'T1',
    icon: '❤️',
    shortDescription: 'Auricular VNS + PPG HRV biofeedback',
  },
  audio_entrainment: {
    id: 'audio_entrainment',
    displayName: 'Neural Audio',
    consumerName: 'Neural Audio Entrainment',
    tier: 'T1',
    icon: '🎵',
    shortDescription: 'Binaural beats + bone conduction',
  },
  visual_stimulation: {
    id: 'visual_stimulation',
    displayName: 'Visual Stimulation',
    consumerName: 'Visual Entrainment',
    tier: 'T1',
    icon: '👁️',
    shortDescription: '108 micro-LEDs/lens, 0.5–100Hz',
  },
  qeeg_21ch: {
    id: 'qeeg_21ch',
    displayName: 'qEEG 21-channel',
    consumerName: 'Clinical EEG',
    tier: 'T2',
    icon: '🔬',
    shortDescription: '21-ch 10-20 wet gel + sLORETA',
  },
  tms: {
    id: 'tms',
    displayName: 'TMS',
    consumerName: 'Transcranial Magnetic Stimulation',
    tier: 'T2',
    icon: '🧲',
    shortDescription: 'Focal figure-8 coil rTMS/TBS',
  },
  pbm_deep_1170nm: {
    id: 'pbm_deep_1170nm',
    displayName: 'Deep PBM 1170nm',
    consumerName: 'Deep Photobiomodulation',
    tier: 'T2',
    icon: '🔴',
    shortDescription: 'Laser diodes, 35–40mm subcortical depth',
  },
  clinical_tacs: {
    id: 'clinical_tacs',
    displayName: 'Clinical tACS',
    consumerName: 'Clinical Entrainment',
    tier: 'T2',
    icon: '⚡⚡',
    shortDescription: '16-ch arbitrary waveform ≤4mA',
  },
  hd_tdcs: {
    id: 'hd_tdcs',
    displayName: 'HD-tDCS',
    consumerName: 'Precision Cortical Targeting',
    tier: 'T2',
    icon: '🎯',
    shortDescription: 'sLORETA-guided 4×1 ring montage',
  },
  cervical_vns: {
    id: 'cervical_vns',
    displayName: 'Cervical VNS',
    consumerName: 'Cervical Vagal Stimulation',
    tier: 'T2',
    icon: '🫀',
    shortDescription: 'Neck-worn tcVNS with cardiac interlock',
  },
  vibrotactile_40hz: {
    id: 'vibrotactile_40hz',
    displayName: '40Hz Vibrotactile',
    consumerName: '40Hz Vibrotactile',
    tier: 'Accessory',
    icon: '📳',
    shortDescription: 'Mastoid LRA, 40Hz ± 0.5Hz (provisional)',
  },
};

// ─── Modality parameter types ──────────────────────────────────────────────────

export interface PBMTranscranialParams {
  // 'all'/'front'/'rear' are legacy fixed-region selectors; 'custom' pairs with
  // customZones (legacy numeric indices); 'named' pairs with zoneRefs — the
  // Rev B model where a zone is a named set of modules (see NPZoneDefinition).
  // 'named' pairs with zoneRefs. 'clinician_selected' means the target is
  // patient-specific and CANNOT be predefined — the operator must choose the
  // sockets before the protocol can run (NP-CFG-UI-001). Used where the
  // evidence targets a lesion or other individual anatomy, e.g. perilesional
  // cortex in post-stroke rehab (pbm_neuro_protocols.md §9).
  // 'all' / 'front' / 'rear' / 'custom' are RETIRED 5-slot selectors, kept only
  // so old files still parse; every shipped protocol now uses named zones.
  zones: 'all' | 'front' | 'rear' | 'custom' | 'named' | 'clinician_selected';
  customZones?: number[];
  zoneRefs?: string[];   // names of NPZoneDefinition entries in the namespace
  wavelength: '660_808nm' | '1064nm' | '660_808_1064nm';
  intensityPercent: number;
  frequencyHz: number;
  dutyCyclePercent: number;
}

export interface PBMIntranasalParams {
  intensityPercent: number;
  frequencyHz: number;
  dutyCyclePercent: number;
}

export interface EEGNeurofeedbackParams {
  channels: 'all' | 'front' | 'central' | 'custom';
  customChannels?: string[];
  band: 'delta' | 'theta' | 'alpha' | 'beta' | 'gamma' | 'alpha_theta' | 'gamma_theta';
  closedLoopEnabled: boolean;
}

export interface BESTacsParams {
  frequencyHz: number;
  intensityMilliamps: number;
  waveform: 'sinusoidal' | 'square' | 'triangular';
}

export interface TDCSParams {
  intensityMilliamps: number;
  electrodePairs: [string, string][];
  rampSeconds: number;
}

export interface VNSHRVParams {
  frequencyHz: number;
  intensityMilliamps: number;
  hrvProtocol: 'standalone' | 'tavns_sync' | 'eeg_biofeedback' | 'combined_pbm';
  resonanceBreathingRate: number;
}

export interface AudioEntrainmentParams {
  binauralBeatsHz?: number;
  isochronicTonesHz?: number;
  noiseType?: 'pink' | 'brown';
  carrierHz: number;
  volumePercent: number;
  eegAdaptive: boolean;
  boneConductionPacer: boolean;
}

export interface VisualStimParams {
  frequencyHz: number;
  mode: 'binocular' | 'emdr' | 'retinal_pbm' | 'mode_f';
  emdrCadenceHz: number;
  enableModeF: boolean;
}

export interface QEEG21chParams {
  montage: 'standard_1020' | 'custom';
  sloretaEnabled: boolean;
  reference: 'linked_ear' | 'cz' | 'average';
}

export interface TMSParams {
  tmsProtocol: 'rTMS' | 'TBS' | 'iTBS';
  frequencyHz: number;
  intensityPercentMT: number;
  target: 'DLPFC_L' | 'DLPFC_R' | 'VLPFC_L' | 'ACC' | 'MPFC' | 'M1_L' | 'M1_R';
  pulseCount: number;
}

export interface DeepPBM1170Params {
  intensityMWcm2: number;
  frequencyHz: number;
  dutyCyclePercent: number;
}

export interface ClinicalTacsParams {
  frequencyHz: number;
  intensityMilliamps: number;
  channelCount: number;
  waveform: 'sinusoidal' | 'square' | 'triangular';
}

export interface HDTdcsParams {
  target: TMSParams['target'];
  montage: 'ring_4x1' | 'bilateral_4x1' | 'standard_2el';
  intensityMilliamps: number;
}

export interface CervicalVnsParams {
  frequencyHz: number;
  intensityMilliamps: number;
}

export interface VibrotactileParams {
  intensityG: number;
  syncToAudio: boolean;
  syncToVisual: boolean;
}

// ─── Params map and discriminated union ───────────────────────────────────────

export type ModalityParamsMap = {
  pbm_transcranial: PBMTranscranialParams;
  pbm_intranasal: PBMIntranasalParams;
  eeg_neurofeedback: EEGNeurofeedbackParams;
  bes_tacs: BESTacsParams;
  tdcs: TDCSParams;
  vns_hrv: VNSHRVParams;
  audio_entrainment: AudioEntrainmentParams;
  visual_stimulation: VisualStimParams;
  qeeg_21ch: QEEG21chParams;
  tms: TMSParams;
  pbm_deep_1170nm: DeepPBM1170Params;
  clinical_tacs: ClinicalTacsParams;
  hd_tdcs: HDTdcsParams;
  cervical_vns: CervicalVnsParams;
  vibrotactile_40hz: VibrotactileParams;
};

export type NPModalityParams = {
  [K in NPModalityTypeId]: { type: K; params: ModalityParamsMap[K] };
}[NPModalityTypeId];

// ─── Default params ────────────────────────────────────────────────────────────

export function defaultParams<T extends NPModalityTypeId>(type: T): ModalityParamsMap[T] {
  const defaults: ModalityParamsMap = {
    pbm_transcranial: {
      zones: 'all',
      wavelength: '660_808nm',
      intensityPercent: 75,
      frequencyHz: 40,
      dutyCyclePercent: 25,
    },
    pbm_intranasal: {
      intensityPercent: 60,
      frequencyHz: 10,
      dutyCyclePercent: 50,
    },
    eeg_neurofeedback: {
      channels: 'all',
      band: 'alpha',
      closedLoopEnabled: true,
    },
    bes_tacs: {
      frequencyHz: 40,
      intensityMilliamps: 0.5,
      waveform: 'sinusoidal',
    },
    tdcs: {
      intensityMilliamps: 1.0,
      electrodePairs: [['Fp1', 'Fp2']],
      rampSeconds: 30,
    },
    vns_hrv: {
      frequencyHz: 25,
      intensityMilliamps: 0.5,
      hrvProtocol: 'standalone',
      resonanceBreathingRate: 6,
    },
    audio_entrainment: {
      binauralBeatsHz: 40,
      carrierHz: 200,
      volumePercent: 70,
      eegAdaptive: true,
      boneConductionPacer: false,
    },
    visual_stimulation: {
      frequencyHz: 40,
      mode: 'binocular',
      emdrCadenceHz: 1,
      enableModeF: false,
    },
    qeeg_21ch: {
      montage: 'standard_1020',
      sloretaEnabled: true,
      reference: 'linked_ear',
    },
    tms: {
      tmsProtocol: 'rTMS',
      frequencyHz: 10,
      intensityPercentMT: 110,
      target: 'DLPFC_L',
      pulseCount: 3000,
    },
    pbm_deep_1170nm: {
      intensityMWcm2: 500,
      frequencyHz: 10,
      dutyCyclePercent: 50,
    },
    clinical_tacs: {
      frequencyHz: 40,
      intensityMilliamps: 2.0,
      channelCount: 8,
      waveform: 'sinusoidal',
    },
    hd_tdcs: {
      target: 'DLPFC_L',
      montage: 'ring_4x1',
      intensityMilliamps: 1.5,
    },
    cervical_vns: {
      frequencyHz: 25,
      intensityMilliamps: 1.0,
    },
    vibrotactile_40hz: {
      intensityG: 0.9,
      syncToAudio: true,
      syncToVisual: true,
    },
  };
  return defaults[type] as ModalityParamsMap[T];
}

// ─── Interval config ───────────────────────────────────────────────────────────

export interface NPIntervalConfig {
  intervalOnSeconds: number;   // 0 = continuous
  intervalOffSeconds: number;
  repeatCount?: number;        // undefined = until_end
}

// ─── Protocol modality block ───────────────────────────────────────────────────

export interface NPProtocolModality {
  id: string;
  modalityParams: NPModalityParams;
  interval: NPIntervalConfig;
  enabled: boolean;
}

// ─── Timing ───────────────────────────────────────────────────────────────────

export type NPTimingMode =
  | { type: 'duration'; seconds: number }
  | { type: 'interval_count'; count: number };

// ─── Protocol definition ───────────────────────────────────────────────────────

export interface NPProtocolDefinition {
  id: string;
  name: string;
  description: string;
  author: string;
  version: string;
  tags: string[];
  createdAt: string;
  modifiedAt: string;
  isPredefined: boolean;
  isReadOnly?: boolean;
  timingMode: NPTimingMode;
  modalities: NPProtocolModality[];
  // Rev B: clinical context. `conditions` lists condition names that MUST each
  // resolve to an NPConditionDefinition in the namespace. `references` links to
  // documents describing the protocol / its evidence (openable in a browser).
  conditions?: string[];
  references?: NPProtocolReference[];
}

// ─── Reference links ───────────────────────────────────────────────────────────

// A reference is either a bare URL/path string, or a [label, url] pair.
export type NPProtocolReference = string | { label: string; url: string };

export function referenceUrl(r: NPProtocolReference): string {
  return typeof r === 'string' ? r : r.url;
}

export function referenceLabel(r: NPProtocolReference): string {
  return typeof r === 'string' ? r : r.label;
}

// ─── Zone definition (named set of modules) ────────────────────────────────────

// Element-type names mirror firmware np_elem_type_t (np_module_map.h).
export type NPElementType =
  | 'led_660' | 'led_808' | 'led_1064' | 'led_1170'
  | 'eeg_electrode' | 'tes_electrode' | 'vns_contact' | 'ntc'
  | 'pd_forward' | 'pd_back' | 'ir_prox' | 'hall' | 'dual_electrode';

// A zone is a named SET OF MODULES, defined as an explicit list of socket
// (major) addresses — the first half of the firmware two-level
// (socket:element) scheme (np_module_map.h). Listing sockets directly makes
// arbitrary, non-contiguous zones definable. An optional element-type filter
// restricts which elements within those modules the zone selects.
export interface NPZoneDefinition {
  name: string;
  id?: string;
  description?: string;
  sockets: number[];        // socket (major) addresses — the modules in this zone
  types?: NPElementType[];  // optional element-type filter within those modules
  excludeTypes?: boolean;   // false: include only `types`; true: exclude them
  isPredefined?: boolean;
}

// ─── Condition definition (name → external reference) ──────────────────────────

export interface NPConditionDefinition {
  name: string;
  id?: string;
  link: string;             // external definition (e.g. Wikipedia / MeSH)
  description?: string;
  code?: string;            // optional ICD-11 / SNOMED / MeSH code
}

// ─── Composite ────────────────────────────────────────────────────────────────

export interface NPCompositeLayer {
  id: string;
  protocolName: string;
  startOffsetSeconds: number;
  durationSeconds?: number;
  intensityScale: number;
}

export interface NPCompositeProtocol {
  id: string;
  name: string;
  description: string;
  author: string;
  version: string;
  tags: string[];
  createdAt: string;
  modifiedAt: string;
  isPredefined: boolean;
  isReadOnly?: boolean;
  layers: NPCompositeLayer[];
  conflictResolution: 'merge' | 'sequential' | 'override';
  conditions?: string[];
  references?: NPProtocolReference[];
}

// ─── Entry union ───────────────────────────────────────────────────────────────

export type NPProtocolEntry =
  | { kind: 'single'; protocol: NPProtocolDefinition }
  | { kind: 'composite'; composite: NPCompositeProtocol };

// ─── Namespace ─────────────────────────────────────────────────────────────────
//
// All .npps files under the protocol directory tree load into ONE namespace.
// Zones and conditions are cross-referenceable by name across files; protocol
// `zoneRefs` and `conditions` are validated against this namespace after all
// files are loaded (see NP-NPPS-REF-001 §1.6).

export interface NPNamespace {
  entries: NPProtocolEntry[];
  zones: Map<string, NPZoneDefinition>;
  conditions: Map<string, NPConditionDefinition>;
}

export function entryId(e: NPProtocolEntry): string {
  return e.kind === 'single' ? e.protocol.id : e.composite.id;
}

export function entryName(e: NPProtocolEntry): string {
  return e.kind === 'single' ? e.protocol.name : e.composite.name;
}

export function entryDescription(e: NPProtocolEntry): string {
  return e.kind === 'single' ? e.protocol.description : e.composite.description;
}

export function entryTags(e: NPProtocolEntry): string[] {
  return e.kind === 'single' ? e.protocol.tags : e.composite.tags;
}

export function entryIsPredefined(e: NPProtocolEntry): boolean {
  return e.kind === 'single' ? e.protocol.isPredefined : e.composite.isPredefined;
}

export function entryIsReadOnly(e: NPProtocolEntry): boolean {
  return e.kind === 'single' ? (e.protocol.isReadOnly ?? false) : (e.composite.isReadOnly ?? false);
}

export function requiredModalities(e: NPProtocolEntry): Set<NPModalityTypeId> {
  if (e.kind === 'composite') return new Set<NPModalityTypeId>();
  return new Set(e.protocol.modalities.filter(m => m.enabled).map(m => m.modalityParams.type));
}
