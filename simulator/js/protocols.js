/**
 * Protocol templates for the NeuroPulse helmet simulator.
 * These mirror the NPPS (NeuroPulse Protocol Script) predefined templates
 * from docs/npps-reference.md, expressed as JSON for the session engine.
 *
 * duration   — session length in seconds
 * modalities — which subsystems fire; each has its own parameter set
 * phases     — optional time-segmented phase list (for timeline rendering)
 */

export const PROTOCOLS = {
  alpha_calm: {
    id: 'alpha_calm',
    name: 'Alpha Calm',
    description: 'Alpha-frequency PBM + binaural entrainment for sustained relaxation and stress reduction.',
    tier: 'T1',
    duration: 1200,
    modalities: {
      pbm: {
        active: true,
        zones: ['ZM-01', 'ZM-02', 'ZM-03', 'ZM-04', 'ZM-05'],
        wavelengths: ['660nm', '808nm'],
        frequency: 10,
        dutyCycle: 0.25,
        dose_jcm2: 10,
      },
      eeg: { active: true, channels: ['Fp1','Fp2','F3','F4','C3','C4','P3','P4'] },
      audio: { active: true, binaural_hz: 10, base_hz: 200, type: 'binaural' },
    },
    phases: [
      { label: 'Ramp', start: 0, end: 60, color: '#224422' },
      { label: 'Main', start: 60, end: 1140, color: '#335533' },
      { label: 'Ramp-down', start: 1140, end: 1200, color: '#224422' },
    ],
  },

  focus_prime: {
    id: 'focus_prime',
    name: 'Focus Prime',
    description: 'Beta/gamma PBM + isochronic tones for executive function and working memory.',
    tier: 'T1',
    duration: 900,
    modalities: {
      pbm: {
        active: true,
        zones: ['ZM-01', 'ZM-02', 'ZM-03'],
        wavelengths: ['660nm', '808nm'],
        frequency: 20,
        dutyCycle: 0.25,
        dose_jcm2: 8,
      },
      eeg: { active: true, channels: ['Fp1','Fp2','F3','F4','C3','C4','P3','P4'] },
      bes: { active: true, frequency: 20, intensity_ma: 0.5 },
      audio: { active: true, binaural_hz: 20, base_hz: 200, type: 'isochronic' },
    },
    phases: [
      { label: 'Ramp', start: 0, end: 30, color: '#224455' },
      { label: 'Main', start: 30, end: 870, color: '#334466' },
      { label: 'Ramp-down', start: 870, end: 900, color: '#224455' },
    ],
  },

  gamma_40hz: {
    id: 'gamma_40hz',
    name: 'Gamma 40 Hz',
    description: 'Full-spectrum 40 Hz gamma protocol — visual flicker + audio + PBM. Based on Tsai lab GENUS multi-sensory paradigm.',
    tier: 'T1',
    duration: 1200,
    modalities: {
      pbm: {
        active: true,
        zones: ['ZM-01', 'ZM-02', 'ZM-03', 'ZM-04', 'ZM-05'],
        wavelengths: ['660nm', '808nm'],
        frequency: 40,
        dutyCycle: 0.25,
        dose_jcm2: 12,
      },
      eeg: { active: true, channels: ['Fp1','Fp2','F3','F4','C3','C4','P3','P4'] },
      visual: { active: true, frequency: 40, mode: 'photic_driving' },
      audio: { active: true, binaural_hz: 40, base_hz: 200, type: 'isochronic' },
    },
    phases: [
      { label: 'Ramp', start: 0, end: 60, color: '#442244' },
      { label: 'Main', start: 60, end: 1140, color: '#553355' },
      { label: 'Ramp-down', start: 1140, end: 1200, color: '#442244' },
    ],
  },

  sleep_deep: {
    id: 'sleep_deep',
    name: 'Sleep Deep',
    description: 'Delta-frequency entrainment for sleep onset. Low-intensity PBM + sub-threshold BES.',
    tier: 'T1',
    duration: 1800,
    modalities: {
      pbm: {
        active: true,
        zones: ['ZM-01', 'ZM-04', 'ZM-05'],
        wavelengths: ['660nm', '808nm'],
        frequency: 2,
        dutyCycle: 0.25,
        dose_jcm2: 6,
      },
      eeg: { active: true, channels: ['Fp1','Fp2','F3','F4','C3','C4','P3','P4'] },
      bes: { active: true, frequency: 2, intensity_ma: 0.3 },
      audio: { active: true, binaural_hz: 2, base_hz: 100, type: 'binaural' },
    },
    phases: [
      { label: 'Onset', start: 0, end: 300, color: '#112244' },
      { label: 'Deep', start: 300, end: 1500, color: '#112255' },
      { label: 'Fade', start: 1500, end: 1800, color: '#112244' },
    ],
  },

  anxiety_hrv: {
    id: 'anxiety_hrv',
    name: 'Anxiety Relief + HRV',
    description: 'HRV coherence biofeedback with resonance breathing + taVNS synchronised stimulation + alpha PBM.',
    tier: 'T1',
    duration: 1500,
    modalities: {
      pbm: {
        active: true,
        zones: ['ZM-01', 'ZM-02', 'ZM-03'],
        wavelengths: ['660nm', '808nm'],
        frequency: 10,
        dutyCycle: 0.25,
        dose_jcm2: 8,
      },
      eeg: { active: true, channels: ['Fp1','Fp2','F3','F4','C3','C4','P3','P4'] },
      vns: { active: true, frequency: 25, intensity_ma: 1.0, hrv_sync: true },
      audio: { active: true, binaural_hz: 10, base_hz: 200, type: 'binaural', breathing_pacer: true, breathing_bpm: 6 },
    },
    phases: [
      { label: 'Baseline', start: 0, end: 120, color: '#224433' },
      { label: 'RF Sweep', start: 120, end: 300, color: '#335544' },
      { label: 'Coherence', start: 300, end: 1380, color: '#336644' },
      { label: 'Cool-down', start: 1380, end: 1500, color: '#224433' },
    ],
  },

  combined_4modal: {
    id: 'combined_4modal',
    name: 'Combined Standard',
    description: 'Four-modality combined protocol: PBM + BES + VNS-HRV + audio. Replicates 2025 multi-modal RCT design (PBM + qEEG NF + HRV biofeedback).',
    tier: 'T1',
    duration: 1800,
    modalities: {
      pbm: {
        active: true,
        zones: ['ZM-01', 'ZM-02', 'ZM-03', 'ZM-04', 'ZM-05'],
        wavelengths: ['660nm', '808nm'],
        frequency: 40,
        dutyCycle: 0.25,
        dose_jcm2: 12,
      },
      eeg: { active: true, channels: ['Fp1','Fp2','F3','F4','C3','C4','P3','P4'] },
      bes: { active: true, frequency: 40, intensity_ma: 0.5 },
      vns: { active: true, frequency: 25, intensity_ma: 1.0, hrv_sync: true },
      audio: { active: true, binaural_hz: 40, base_hz: 200, type: 'binaural', breathing_pacer: true, breathing_bpm: 6 },
    },
    phases: [
      { label: 'Ramp', start: 0, end: 60, color: '#443311' },
      { label: 'Main', start: 60, end: 1740, color: '#554422' },
      { label: 'Ramp-down', start: 1740, end: 1800, color: '#443311' },
    ],
  },

  pbm_vascular: {
    id: 'pbm_vascular',
    name: 'PBM Vascular Baseline',
    description: 'Continuous-wave full-zone PBM for vascular baseline and circadian rhythm support.',
    tier: 'T1',
    duration: 1200,
    modalities: {
      pbm: {
        active: true,
        zones: ['ZM-01', 'ZM-02', 'ZM-03', 'ZM-04', 'ZM-05'],
        wavelengths: ['660nm', '808nm'],
        frequency: 0,
        dutyCycle: 1.0,
        dose_jcm2: 20,
      },
      eeg: { active: false },
    },
    phases: [
      { label: 'CW', start: 0, end: 1200, color: '#551100' },
    ],
  },

  pbm_1064_deep: {
    id: 'pbm_1064_deep',
    name: 'Deep NIR — 1064 nm',
    description: 'Three-wavelength PBM using 1064 nm smart zone modules: surface (660 nm) → cortical (808 nm) → NIR-II (1064 nm). Working memory protocol based on Yao et al. (2022).',
    tier: 'T1',
    duration: 1200,
    modalities: {
      pbm: {
        active: true,
        zones: ['ZM-01', 'ZM-02', 'ZM-03', 'ZM-04', 'ZM-05'],
        wavelengths: ['660nm', '808nm', '1064nm'],
        frequency: 10,
        dutyCycle: 0.25,
        dose_jcm2: 10,
      },
      eeg: { active: true, channels: ['Fp1','Fp2','F3','F4','C3','C4','P3','P4'] },
      audio: { active: true, binaural_hz: 10, base_hz: 200, type: 'binaural' },
    },
    phases: [
      { label: 'Ramp', start: 0, end: 60, color: '#443300' },
      { label: 'Main', start: 60, end: 1140, color: '#665500' },
      { label: 'Ramp-down', start: 1140, end: 1200, color: '#443300' },
    ],
  },

  tms_depression: {
    id: 'tms_depression',
    name: 'TMS — Depression (rTMS)',
    description: '10 Hz rTMS to left DLPFC + PBM + EEG-guided HD-tDCS. T2 Pro protocol.',
    tier: 'T2',
    duration: 2400,
    modalities: {
      pbm: {
        active: true,
        zones: ['ZM-01', 'ZM-02', 'ZM-03'],
        wavelengths: ['660nm', '808nm', '1064nm'],
        frequency: 10,
        dutyCycle: 0.25,
        dose_jcm2: 10,
      },
      eeg: { active: true, channels: ['Fp1','Fp2','F3','F4','C3','C4','P3','P4'] },
      tms: { active: true, frequency: 10, intensity_pct: 110, target: 'DLPFC_L', pattern: 'rTMS' },
      hd_tdcs: { active: true, target: 'DLPFC_L', intensity_ma: 2.0 },
    },
    phases: [
      { label: 'EEG Map', start: 0, end: 300, color: '#112244' },
      { label: 'HD-tDCS', start: 300, end: 900, color: '#224466' },
      { label: 'rTMS', start: 900, end: 2100, color: '#336688' },
      { label: 'Post-EEG', start: 2100, end: 2400, color: '#224466' },
    ],
  },
};

export const PROTOCOL_IDS = Object.keys(PROTOCOLS);
