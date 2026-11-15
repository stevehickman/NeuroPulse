import { useState } from 'react';
import {
  NPProtocolModality,
  NPIntervalConfig,
  NPModalityParams,
  NPModalityTypeId,
  MODALITY_META,
  PBMTranscranialParams,
  PBMIntranasalParams,
  EEGNeurofeedbackParams,
  BESTacsParams,
  TDCSParams,
  VNSHRVParams,
  AudioEntrainmentParams,
  VisualStimParams,
  QEEG21chParams,
  TMSParams,
  DeepPBM1170Params,
  ClinicalTacsParams,
  HDTdcsParams,
  CervicalVnsParams,
  VibrotactileParams,
} from '../../../../common/types/protocol';
import { t } from '../../../../common/lib/i18n';
import { INTERVAL_BOUNDS, PARAM_BOUNDS } from '../lib/paramBounds';
import type { ParamBound } from '../lib/paramBounds';
import { ParamGrid, ParamInput, RangeEditor, SelectField, CheckboxField } from './ParamInputs';

// ─── Interval Controls ─────────────────────────────────────────────────────────

interface IntervalControlsProps {
  interval: NPIntervalConfig;
  onChange: (iv: NPIntervalConfig) => void;
}

export function IntervalControls({ interval, onChange }: IntervalControlsProps) {
  const isContinuous = interval.intervalOnSeconds === 0;

  return (
    <div className="interval-section">
      <div className="interval-title">{t('WEB_MOD_TIMING_INTERVAL')}</div>
      <div className="param-grid" style={{ gridTemplateColumns: 'repeat(auto-fill, minmax(130px, 1fr))' }}>
        <div className="param-field">
          <label className="param-label">{t('WEB_FIELD_MODE')}</label>
          <select
            className="form-select param-select"
            value={isContinuous ? 'continuous' : 'interval'}
            onChange={e => {
              if (e.target.value === 'continuous') {
                onChange({ ...interval, intervalOnSeconds: 0, intervalOffSeconds: 0 });
              } else {
                onChange({ ...interval, intervalOnSeconds: 30, intervalOffSeconds: 30 });
              }
            }}
          >
            <option value="continuous">{t('WEB_CONTINUOUS')}</option>
            <option value="interval">{t('WEB_PROTOCOL_INTERVAL')}</option>
          </select>
        </div>

        {!isContinuous && (
          <>
            <ParamInput
              label={t('WEB_MOD_ON_SEC')}
              value={interval.intervalOnSeconds}
              bound={INTERVAL_BOUNDS.onSeconds}
              onChange={v => onChange({ ...interval, intervalOnSeconds: v })}
            />
            <ParamInput
              label={t('WEB_MOD_OFF_SEC')}
              value={interval.intervalOffSeconds}
              bound={INTERVAL_BOUNDS.offSeconds}
              onChange={v => onChange({ ...interval, intervalOffSeconds: v })}
            />
            <ParamInput
              optional
              label={t('UI_MOD_REPEAT')}
              value={interval.repeatCount}
              bound={INTERVAL_BOUNDS.repeatCount}
              placeholder={t('WEB_MOD_UNTIL_END')}
              onChange={v => onChange({ ...interval, repeatCount: v })}
            />
          </>
        )}
      </div>
    </div>
  );
}

// ─── Param Controls ────────────────────────────────────────────────────────────

interface ParamControlsProps {
  params: NPModalityParams;
  onChange: (params: NPModalityParams) => void;
}

// Keys, not text: resolved by t() inside the render below (see NPModalityMeta).
const FREQ_PRESETS = [
  { labelKey: 'WEB_FREQ_DELTA', value: 2 },
  { labelKey: 'WEB_FREQ_THETA', value: 6 },
  { labelKey: 'WEB_FREQ_ALPHA', value: 10 },
  { labelKey: 'WEB_FREQ_BETA', value: 20 },
  { labelKey: 'WEB_FREQ_GAMMA', value: 40 },
  { labelKey: 'WEB_CUSTOM', value: -1 },
];

/** A frequency is chosen from the standard bands or typed, so it keeps a select beside its typed field and sits with
 *  the typed fields in the grid (ParamGrid groups anything that carries a `bound` there). */
function FrequencyField({ label, value, bound, onChange }: {
  label: string; value: number; bound: ParamBound;
  onChange: (v: number) => void;
}) {
  const preset = FREQ_PRESETS.find(p => p.value === value);
  const isCustom = !preset || preset.value === -1;

  return (
    <div className="param-field" style={{ minWidth: 260 }}>
      <label className="param-label">{label}</label>
      <div style={{ display: 'flex', gap: 6 }}>
        <select
          className="form-select"
          style={{ flex: 1 }}
          value={preset && !isCustom ? value : -1}
          onChange={e => {
            const v = Number(e.target.value);
            if (v !== -1) onChange(v);
          }}
        >
          {FREQ_PRESETS.map(p => (
            <option key={p.value} value={p.value}>{t(p.labelKey)}</option>
          ))}
        </select>
        <RangeEditor value={value} bound={bound} unit="Hz" ariaLabel={label} fieldOnly onChange={v => v !== undefined && onChange(v)} />
      </div>
    </div>
  );
}

export function ParamControls({ params, onChange }: ParamControlsProps) {
  function update<T extends NPModalityParams>(updatedParams: T['params']): void {
    onChange({ type: params.type, params: updatedParams } as NPModalityParams);
  }

  switch (params.type) {
    case 'pbm_transcranial': {
      const p = params.params as PBMTranscranialParams;
      return (
        <ParamGrid>
          <SelectField label={t('MODALITY_ZONES')} value={p.zones} onChange={v => update<typeof params>({ ...p, zones: v as PBMTranscranialParams['zones'] })}
            options={[
              { value: 'all', label: t('WEB_ZONES_ALL_5') },
              { value: 'front', label: t('WEB_ZONES_FRONT') },
              { value: 'rear', label: t('WEB_ZONES_REAR') },
              { value: 'custom', label: t('WEB_CUSTOM') },
            ]}
          />
          <SelectField label={t('MODALITY_WAVELENGTH')} value={p.wavelength} onChange={v => update<typeof params>({ ...p, wavelength: v as PBMTranscranialParams['wavelength'] })}
            options={[
              { value: '660nm', label: t('WEB_WL_660') },
              { value: '808nm', label: t('WEB_WL_808') },
              { value: '1064nm', label: t('WEB_WL_1064') },
            ]}
          />
          <ParamInput label={t('WEB_MOD_IRRADIANCE')} value={p.irradianceMWcm2} bound={PARAM_BOUNDS.pbmTranscranial.irradiance} unit="mW/cm²" onChange={v => update<typeof params>({ ...p, irradianceMWcm2: v })} />
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} bound={PARAM_BOUNDS.pbmTranscranial.frequency} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <ParamInput label={t('MODALITY_DUTY_CYCLE')} value={p.dutyCyclePercent} bound={PARAM_BOUNDS.pbmTranscranial.duty} unit="%" onChange={v => update<typeof params>({ ...p, dutyCyclePercent: v })} />
        </ParamGrid>
      );
    }

    case 'pbm_intranasal': {
      const p = params.params as PBMIntranasalParams;
      return (
        <ParamGrid>
          <SelectField label={t('MODALITY_WAVELENGTH')} value={p.wavelength} onChange={v => update<typeof params>({ ...p, wavelength: v })}
            options={[
              { value: '660nm', label: t('WEB_WL_660') },
              { value: '808nm', label: t('WEB_WL_808') },
            ]}
          />
          <ParamInput label={t('WEB_MOD_IRRADIANCE')} value={p.irradianceMWcm2} bound={PARAM_BOUNDS.pbmIntranasal.irradiance} unit="mW/cm²" onChange={v => update<typeof params>({ ...p, irradianceMWcm2: v })} />
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} bound={PARAM_BOUNDS.pbmIntranasal.frequency} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <ParamInput label={t('MODALITY_DUTY_CYCLE')} value={p.dutyCyclePercent} bound={PARAM_BOUNDS.pbmIntranasal.duty} unit="%" onChange={v => update<typeof params>({ ...p, dutyCyclePercent: v })} />
        </ParamGrid>
      );
    }

    case 'eeg_neurofeedback': {
      const p = params.params as EEGNeurofeedbackParams;
      return (
        <ParamGrid>
          <SelectField label={t('MODALITY_CHANNELS')} value={p.channels} onChange={v => update<typeof params>({ ...p, channels: v as EEGNeurofeedbackParams['channels'] })}
            options={[
              { value: 'all', label: t('WEB_CH_ALL_8') },
              { value: 'front', label: t('WEB_CH_FRONTAL') },
              { value: 'central', label: t('WEB_CH_CENTRAL') },
              { value: 'custom', label: t('WEB_CUSTOM') },
            ]}
          />
          <SelectField label={t('WEB_MOD_TARGET_BAND')} value={p.band} onChange={v => update<typeof params>({ ...p, band: v as EEGNeurofeedbackParams['band'] })}
            options={[
              { value: 'delta', label: t('WEB_BAND_DELTA') },
              { value: 'theta', label: t('WEB_BAND_THETA') },
              { value: 'alpha', label: t('WEB_BAND_ALPHA') },
              { value: 'beta', label: t('WEB_BAND_BETA') },
              { value: 'gamma', label: t('WEB_BAND_GAMMA') },
              { value: 'alpha_theta', label: t('WEB_BAND_ALPHA_THETA') },
              { value: 'gamma_theta', label: t('WEB_BAND_GAMMA_THETA') },
            ]}
          />
          <CheckboxField label={t('WEB_MOD_CLOSED_LOOP')} value={p.closedLoopEnabled} onChange={v => update<typeof params>({ ...p, closedLoopEnabled: v })} />
        </ParamGrid>
      );
    }

    case 'bes_tacs': {
      const p = params.params as BESTacsParams;
      return (
        <ParamGrid>
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} bound={PARAM_BOUNDS.bes.frequency} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <ParamInput label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} bound={PARAM_BOUNDS.bes.intensity} unit="mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          <SelectField label={t('MODALITY_WAVEFORM')} value={p.waveform} onChange={v => update<typeof params>({ ...p, waveform: v as BESTacsParams['waveform'] })}
            options={[
              { value: 'sinusoidal', label: t('WEB_WAVE_SINUSOIDAL') },
              { value: 'square', label: t('WEB_WAVE_SQUARE') },
              { value: 'triangular', label: t('WEB_WAVE_TRIANGULAR') },
            ]}
          />
        </ParamGrid>
      );
    }

    case 'tdcs': {
      const p = params.params as TDCSParams;
      return (
        <ParamGrid>
          <ParamInput label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} bound={PARAM_BOUNDS.tdcs.intensity} unit="mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          <ParamInput label={t('WEB_MOD_RAMP')} value={p.rampSeconds} bound={PARAM_BOUNDS.tdcs.ramp} unit="sec" onChange={v => update<typeof params>({ ...p, rampSeconds: v })} />
          {/* OI-CHARGE-04: the geometry the 40 µC/cm² ceiling divides by. Not
              derivable from the pairs below — those name 10-20 sites, not pad
              sizes — and the device enforces against whatever is declared
              here, so it has to be authored. */}
          <ParamInput
            label={t('MODALITY_TDCS_ELECTRODE_AREA_LABEL')}
            value={p.electrodeAreaCm2}
            bound={PARAM_BOUNDS.tdcs.electrodeArea}
            help={t('MODALITY_TDCS_ELECTRODE_AREA_HELP')}
            onChange={v => update<typeof params>({ ...p, electrodeAreaCm2: v })}
          />
          <div className="param-field" style={{ flex: '0 0 100%' }}>
            <label className="param-label">{t('WEB_MOD_ELECTRODE_PAIRS')}</label>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
              {p.electrodePairs.map(([a, c], i) => (
                <div key={i} style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
                  <input
                    className="form-input"
                    style={{ width: 80 }}
                    value={a}
                    placeholder={t('WEB_MOD_ANODE')}
                    onChange={e => {
                      const pairs = [...p.electrodePairs];
                      pairs[i] = [e.target.value, c];
                      update<typeof params>({ ...p, electrodePairs: pairs });
                    }}
                  />
                  <span style={{ color: 'var(--text-muted)' }}>→</span>
                  <input
                    className="form-input"
                    style={{ width: 80 }}
                    value={c}
                    placeholder={t('WEB_MOD_CATHODE')}
                    onChange={e => {
                      const pairs = [...p.electrodePairs];
                      pairs[i] = [a, e.target.value];
                      update<typeof params>({ ...p, electrodePairs: pairs });
                    }}
                  />
                  {p.electrodePairs.length > 1 && (
                    <button className="btn btn-ghost btn-sm" onClick={() => {
                      const pairs = p.electrodePairs.filter((_, j) => j !== i);
                      update<typeof params>({ ...p, electrodePairs: pairs });
                    }}>×</button>
                  )}
                </div>
              ))}
              {p.electrodePairs.length < 3 && (
                <button className="btn btn-ghost btn-sm" style={{ alignSelf: 'flex-start' }}
                  onClick={() => update<typeof params>({ ...p, electrodePairs: [...p.electrodePairs, ['Fp1', 'Fp2']] })}>
                  {t('WEB_MOD_ADD_PAIR')}
                </button>
              )}
            </div>
          </div>
        </ParamGrid>
      );
    }

    case 'vns_hrv': {
      const p = params.params as VNSHRVParams;
      return (
        <ParamGrid>
          <FrequencyField label={t('WEB_MOD_VNS_FREQUENCY')} value={p.frequencyHz} bound={PARAM_BOUNDS.vns.frequency} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <ParamInput label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} bound={PARAM_BOUNDS.vns.intensity} unit="mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          <SelectField label={t('WEB_MOD_HRV_PROTOCOL')} value={p.hrvProtocol} onChange={v => update<typeof params>({ ...p, hrvProtocol: v as VNSHRVParams['hrvProtocol'] })}
            options={[
              { value: 'standalone', label: t('WEB_HRV_STANDALONE') },
              { value: 'tavns_sync', label: t('WEB_HRV_TAVNS_SYNC') },
              { value: 'eeg_biofeedback', label: t('WEB_HRV_EEG_BIOFEEDBACK') },
              { value: 'combined_pbm', label: t('WEB_HRV_COMBINED_PBM') },
            ]}
          />
          <ParamInput label={t('UI_MOD_BREATHING_RATE')} value={p.resonanceBreathingRate} bound={PARAM_BOUNDS.vns.breathingRate} unit="BPM" onChange={v => update<typeof params>({ ...p, resonanceBreathingRate: v })} />
        </ParamGrid>
      );
    }

    case 'audio_entrainment': {
      const p = params.params as AudioEntrainmentParams;
      return (
        <ParamGrid>
          <ParamInput
            optional
            label={t('WEB_MOD_BINAURAL_BEATS')}
            value={p.binauralBeatsHz}
            bound={PARAM_BOUNDS.audio.beatRate}
            placeholder={t('UI_NONE')}
            onChange={v => update<typeof params>({ ...p, binauralBeatsHz: v })}
          />
          <ParamInput
            optional
            label={t('WEB_MOD_ISOCHRONIC_TONES')}
            value={p.isochronicTonesHz}
            bound={PARAM_BOUNDS.audio.beatRate}
            placeholder={t('UI_NONE')}
            onChange={v => update<typeof params>({ ...p, isochronicTonesHz: v })}
          />
          <SelectField label={t('WEB_MOD_NOISE_TYPE')} value={p.noiseType ?? ''} onChange={v => update<typeof params>({ ...p, noiseType: v ? v as AudioEntrainmentParams['noiseType'] : undefined })}
            options={[
              { value: '', label: t('UI_NONE') },
              { value: 'pink', label: t('WEB_NOISE_PINK') },
              { value: 'brown', label: t('WEB_NOISE_BROWN') },
            ]}
          />
          <ParamInput label={t('WEB_MOD_CARRIER')} value={p.carrierHz} bound={PARAM_BOUNDS.audio.carrier} unit="Hz" onChange={v => update<typeof params>({ ...p, carrierHz: v })} />
          <ParamInput label={t('WEB_MOD_VOLUME')} value={p.volumeDb} bound={PARAM_BOUNDS.audio.volume} unit="dB SPL" onChange={v => update<typeof params>({ ...p, volumeDb: v })} />
          <CheckboxField label={t('WEB_MOD_EEG_ADAPTIVE')} value={p.eegAdaptive} onChange={v => update<typeof params>({ ...p, eegAdaptive: v })} />
          <CheckboxField label={t('WEB_MOD_BONE_CONDUCTION')} value={p.boneConductionPacer} onChange={v => update<typeof params>({ ...p, boneConductionPacer: v })} />
        </ParamGrid>
      );
    }

    case 'visual_stimulation': {
      const p = params.params as VisualStimParams;
      return (
        <ParamGrid>
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} bound={PARAM_BOUNDS.visual.frequency} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SelectField label={t('WEB_FIELD_MODE')} value={p.mode} onChange={v => update<typeof params>({ ...p, mode: v as VisualStimParams['mode'] })}
            options={[
              { value: 'binocular', label: t('WEB_VIS_BINOCULAR') },
              { value: 'emdr', label: t('WEB_VIS_EMDR') },
              { value: 'retinal_pbm', label: t('WEB_VIS_RETINAL_PBM') },
              { value: 'mode_f', label: t('WEB_VIS_MODE_F') },
            ]}
          />
          {p.mode === 'emdr' && (
            <ParamInput label={t('WEB_MOD_EMDR_CADENCE')} value={p.emdrCadenceHz} bound={PARAM_BOUNDS.visual.emdrCadence} unit="Hz" onChange={v => update<typeof params>({ ...p, emdrCadenceHz: v })} />
          )}
          <CheckboxField label={t('WEB_MOD_ENABLE_MODE_F')} value={p.enableModeF} onChange={v => update<typeof params>({ ...p, enableModeF: v })} />
        </ParamGrid>
      );
    }

    case 'qeeg_21ch': {
      const p = params.params as QEEG21chParams;
      return (
        <ParamGrid>
          <SelectField label={t('WEB_MOD_MONTAGE')} value={p.montage} onChange={v => update<typeof params>({ ...p, montage: v as QEEG21chParams['montage'] })}
            options={[{ value: 'standard_1020', label: t('WEB_MONTAGE_1020') }, { value: 'custom', label: t('WEB_CUSTOM') }]}
          />
          <SelectField label={t('UI_MOD_REFERENCE')} value={p.reference} onChange={v => update<typeof params>({ ...p, reference: v as QEEG21chParams['reference'] })}
            options={[
              { value: 'linked_ear', label: t('WEB_REF_LINKED_EAR') },
              { value: 'cz', label: t('WEB_REF_CZ') },
              { value: 'average', label: t('WEB_REF_AVERAGE') },
            ]}
          />
          <CheckboxField label={t('WEB_MOD_SLORETA')} value={p.sloretaEnabled} onChange={v => update<typeof params>({ ...p, sloretaEnabled: v })} />
        </ParamGrid>
      );
    }

    case 'tms': {
      const p = params.params as TMSParams;
      return (
        <ParamGrid>
          <SelectField label={t('WEB_MOD_PROTOCOL')} value={p.tmsProtocol} onChange={v => update<typeof params>({ ...p, tmsProtocol: v as TMSParams['tmsProtocol'] })}
            options={[{ value: 'rTMS', label: t('WEB_TMS_RTMS') }, { value: 'TBS', label: t('WEB_TMS_TBS') }, { value: 'iTBS', label: t('WEB_TMS_ITBS') }]}
          />
          <SelectField label={t('WEB_MOD_TARGET')} value={p.target} onChange={v => update<typeof params>({ ...p, target: v as TMSParams['target'] })}
            options={[
              { value: 'DLPFC_L', label: t('WEB_TARGET_DLPFC_L') },
              { value: 'DLPFC_R', label: t('WEB_TARGET_DLPFC_R') },
              { value: 'VLPFC_L', label: t('WEB_TARGET_VLPFC_L') },
              { value: 'ACC', label: t('WEB_TARGET_ACC') },
              { value: 'MPFC', label: t('WEB_TARGET_MPFC') },
              { value: 'M1_L', label: t('WEB_TARGET_M1_L') },
              { value: 'M1_R', label: t('WEB_TARGET_M1_R') },
            ]}
          />
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} bound={PARAM_BOUNDS.tms.frequency} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <ParamInput label={t('WEB_MOD_INTENSITY')} value={p.intensityPercentMT} bound={PARAM_BOUNDS.tms.intensity} unit="% MT" onChange={v => update<typeof params>({ ...p, intensityPercentMT: v })} />
          <ParamInput label={t('UI_MOD_PULSE_COUNT')} value={p.pulseCount} bound={PARAM_BOUNDS.tms.pulseCount} onChange={v => update<typeof params>({ ...p, pulseCount: v })} />
        </ParamGrid>
      );
    }

    case 'pbm_deep_1170nm': {
      const p = params.params as DeepPBM1170Params;
      return (
        <ParamGrid>
          <ParamInput label={t('WEB_MOD_INTENSITY')} value={p.intensityMWcm2} bound={PARAM_BOUNDS.pbmDeep.intensity} unit="mW/cm²" onChange={v => update<typeof params>({ ...p, intensityMWcm2: v })} />
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} bound={PARAM_BOUNDS.pbmDeep.frequency} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <ParamInput label={t('MODALITY_DUTY_CYCLE')} value={p.dutyCyclePercent} bound={PARAM_BOUNDS.pbmDeep.duty} unit="%" onChange={v => update<typeof params>({ ...p, dutyCyclePercent: v })} />
        </ParamGrid>
      );
    }

    case 'clinical_tacs': {
      const p = params.params as ClinicalTacsParams;
      return (
        <ParamGrid>
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} bound={PARAM_BOUNDS.clinicalTacs.frequency} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <ParamInput label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} bound={PARAM_BOUNDS.clinicalTacs.intensity} unit="mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          {/* max is the driver's channel count, one per T2 cap electrode, single-sourced
              from NPHardwareLimits. It read 16 until OI-TACS-01 widened the hub wire
              mask on 2026-09-07; before that the encoder clamped whatever was typed. */}
          <ParamInput label={t('WEB_MOD_CHANNEL_COUNT')} value={p.channelCount} bound={PARAM_BOUNDS.clinicalTacs.channelCount} onChange={v => update<typeof params>({ ...p, channelCount: v })} />
          <SelectField label={t('MODALITY_WAVEFORM')} value={p.waveform} onChange={v => update<typeof params>({ ...p, waveform: v as ClinicalTacsParams['waveform'] })}
            options={[
              { value: 'sinusoidal', label: t('WEB_WAVE_SINUSOIDAL') },
              { value: 'square', label: t('WEB_WAVE_SQUARE') },
              { value: 'triangular', label: t('WEB_WAVE_TRIANGULAR') },
            ]}
          />
        </ParamGrid>
      );
    }

    case 'hd_tdcs': {
      const p = params.params as HDTdcsParams;
      return (
        <ParamGrid>
          <SelectField label={t('WEB_MOD_TARGET')} value={p.target} onChange={v => update<typeof params>({ ...p, target: v as HDTdcsParams['target'] })}
            options={[
              { value: 'DLPFC_L', label: t('WEB_TARGET_DLPFC_L') }, { value: 'DLPFC_R', label: t('WEB_TARGET_DLPFC_R') },
              { value: 'VLPFC_L', label: t('WEB_TARGET_VLPFC_L') }, { value: 'ACC', label: t('WEB_TARGET_ACC') },
              { value: 'MPFC', label: t('WEB_TARGET_MPFC') }, { value: 'M1_L', label: t('WEB_TARGET_M1_L') }, { value: 'M1_R', label: t('WEB_TARGET_M1_R') },
            ]}
          />
          <SelectField label={t('WEB_MOD_MONTAGE')} value={p.montage} onChange={v => update<typeof params>({ ...p, montage: v as HDTdcsParams['montage'] })}
            options={[
              { value: 'ring_4x1', label: t('WEB_MONTAGE_RING_4X1') },
              { value: 'bilateral_4x1', label: t('WEB_MONTAGE_BILATERAL_4X1') },
              { value: 'standard_2_electrode', label: t('WEB_MONTAGE_STANDARD_2EL') },
            ]}
          />
          <ParamInput label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} bound={PARAM_BOUNDS.hdTdcs.intensity} unit="mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
        </ParamGrid>
      );
    }

    case 'cervical_vns': {
      const p = params.params as CervicalVnsParams;
      return (
        <ParamGrid>
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} bound={PARAM_BOUNDS.cervicalVns.frequency} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <ParamInput label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} bound={PARAM_BOUNDS.cervicalVns.intensity} unit="mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
        </ParamGrid>
      );
    }

    case 'vibrotactile_40hz': {
      const p = params.params as VibrotactileParams;
      return (
        <ParamGrid>
          <ParamInput label={t('WEB_MOD_INTENSITY')} value={p.intensityG} bound={PARAM_BOUNDS.vibrotactile.intensity} unit="G" onChange={v => update<typeof params>({ ...p, intensityG: v })} />
          <CheckboxField label={t('WEB_MOD_SYNC_AUDIO')} value={p.syncToAudio} onChange={v => update<typeof params>({ ...p, syncToAudio: v })} />
          <CheckboxField label={t('WEB_MOD_SYNC_VISUAL')} value={p.syncToVisual} onChange={v => update<typeof params>({ ...p, syncToVisual: v })} />
        </ParamGrid>
      );
    }

    default: {
      // exhaustive check
      return (
        <div style={{ color: 'var(--error)', fontSize: 13 }}>
          {t('WEB_MOD_UNKNOWN_TYPE', { 0: (params as NPModalityParams).type })}
        </div>
      );
    }
  }
}

// ─── Modality Block ────────────────────────────────────────────────────────────

interface ModalityEditorBlockProps {
  modality: NPProtocolModality;
  onChange: (m: NPProtocolModality) => void;
  onDelete: () => void;
}

export function ModalityEditorBlock({ modality, onChange, onDelete }: ModalityEditorBlockProps) {
  const [expanded, setExpanded] = useState(true);
  const meta = MODALITY_META[modality.modalityParams.type];
  const isT2 = meta.tier === 'T2';
  const isAccessory = meta.tier === 'Accessory';

  function toggleEnabled(e: React.MouseEvent) {
    e.stopPropagation();
    onChange({ ...modality, enabled: !modality.enabled });
  }

  return (
    <div className={`modality-block${!modality.enabled ? ' disabled' : ''}`}>
      <div className="modality-block-header" onClick={() => setExpanded(e => !e)}>
        <div
          className={`modality-enable-toggle${modality.enabled ? ' on' : ''}`}
          onClick={toggleEnabled}
          title={modality.enabled ? t('WEB_MOD_DISABLE') : t('WEB_MOD_ENABLE')}
        />
        <span className="modality-icon-display">{meta.icon}</span>
        <div>
          <div className="modality-name">{t(meta.displayNameKey)}</div>
        </div>
        <div className="modality-block-actions">
          {(isT2 || isAccessory) && (
            <span style={{
              fontSize: 10, fontWeight: 700, padding: '2px 6px', borderRadius: 8,
              background: isAccessory ? 'var(--warning-dim)' : 'var(--t2-dim)',
              color: isAccessory ? 'var(--accessory-color)' : 'var(--t2-color)',
            }}>
              {meta.tier}
            </span>
          )}
          <button className="modality-expand-btn" onClick={e => { e.stopPropagation(); setExpanded(v => !v); }}>
            {expanded ? '▲' : '▼'}
          </button>
          <button
            className="modality-expand-btn"
            style={{ color: 'var(--error)' }}
            onClick={e => { e.stopPropagation(); onDelete(); }}
            title={t('WEB_MOD_REMOVE')}
          >
            ✕
          </button>
        </div>
      </div>

      {expanded && (
        <div className="modality-block-body">
          {isT2 && (
            <div className="t2-warning">
              {t('WEB_MOD_REQUIRES_T2')}
            </div>
          )}
          {isAccessory && (
            <div className="t2-warning" style={{ background: 'var(--warning-dim)', borderColor: 'rgba(245,158,11,0.3)', color: 'var(--warning)' }}>
              {t('WEB_MOD_REQUIRES_ACCESSORY')}
            </div>
          )}
          <ParamControls
            params={modality.modalityParams}
            onChange={newParams => onChange({ ...modality, modalityParams: newParams })}
          />
          <IntervalControls
            interval={modality.interval}
            onChange={iv => onChange({ ...modality, interval: iv })}
          />
        </div>
      )}
    </div>
  );
}

// ─── Modality Picker ───────────────────────────────────────────────────────────

interface ModalityPickerProps {
  onSelect: (typeId: NPModalityTypeId) => void;
  onClose: () => void;
}

const TIER_ORDER = ['T1', 'T2', 'Accessory'] as const;

export function ModalityPicker({ onSelect, onClose }: ModalityPickerProps) {
  const [selected, setSelected] = useState<NPModalityTypeId | null>(null);

  const byTier: Record<string, typeof MODALITY_META[NPModalityTypeId][]> = {
    T1: [], T2: [], Accessory: [],
  };
  for (const meta of Object.values(MODALITY_META)) {
    byTier[meta.tier].push(meta);
  }

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal" onClick={e => e.stopPropagation()}>
        <div className="modal-header">
          <span className="modal-title">{t('UI_MOD_ADD_TITLE')}</span>
          <button className="modal-close" onClick={onClose}>×</button>
        </div>
        <div className="modal-body">
          {TIER_ORDER.map(tier => (
            <div key={tier}>
              <div className="picker-group-title">
                {tier === 'T1'
                  ? t('WEB_TIER_T1_GROUP')
                  : tier === 'T2'
                    ? t('WEB_TIER_T2_GROUP')
                    : t('WEB_TIER_ACCESSORY_GROUP')}
              </div>
              {byTier[tier].map(meta => (
                <div
                  key={meta.id}
                  className={`picker-item${selected === meta.id ? ' selected' : ''}`}
                  onClick={() => setSelected(meta.id)}
                >
                  <span className="picker-icon">{meta.icon}</span>
                  <div className="picker-info">
                    <div className="picker-name">{t(meta.displayNameKey)}</div>
                    <div className="picker-desc">{t(meta.shortDescriptionKey)}</div>
                  </div>
                  <span className={`picker-tier ${meta.tier}`}>{meta.tier}</span>
                </div>
              ))}
            </div>
          ))}
        </div>
        <div className="modal-footer">
          <button className="btn btn-secondary" onClick={onClose}>{t('COMMON_CANCEL')}</button>
          <button
            className="btn btn-primary"
            disabled={!selected}
            onClick={() => { if (selected) { onSelect(selected); onClose(); } }}
          >
            {t('UI_MOD_ADD_TITLE')}
          </button>
        </div>
      </div>
    </div>
  );
}
