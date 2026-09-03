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
} from '../types/protocol';
import { t } from '../lib/i18n';

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
            <div className="param-field">
              <label className="param-label">{t('WEB_MOD_ON_SEC')}</label>
              <input
                type="number"
                className="form-input param-input"
                min={1}
                max={3600}
                value={interval.intervalOnSeconds}
                onChange={e => onChange({ ...interval, intervalOnSeconds: Number(e.target.value) })}
              />
            </div>
            <div className="param-field">
              <label className="param-label">{t('WEB_MOD_OFF_SEC')}</label>
              <input
                type="number"
                className="form-input param-input"
                min={0}
                max={3600}
                value={interval.intervalOffSeconds}
                onChange={e => onChange({ ...interval, intervalOffSeconds: Number(e.target.value) })}
              />
            </div>
            <div className="param-field">
              <label className="param-label">{t('UI_MOD_REPEAT')}</label>
              <input
                type="number"
                className="form-input param-input"
                min={1}
                max={1000}
                placeholder={t('WEB_MOD_UNTIL_END')}
                value={interval.repeatCount ?? ''}
                onChange={e => {
                  const v = e.target.value === '' ? undefined : Number(e.target.value);
                  onChange({ ...interval, repeatCount: v });
                }}
              />
            </div>
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

function SliderField({ label, value, min, max, step, unit, onChange }: {
  label: string; value: number; min: number; max: number; step?: number; unit?: string;
  onChange: (v: number) => void;
}) {
  return (
    <div className="param-field">
      <label className="param-label">{label}</label>
      <div className="slider-wrap">
        <input
          type="range"
          min={min} max={max} step={step ?? 1}
          value={value}
          onChange={e => onChange(Number(e.target.value))}
        />
        <span className="param-value-display">{value}{unit ?? ''}</span>
      </div>
    </div>
  );
}

function SelectField({ label, value, options, onChange }: {
  label: string;
  value: string;
  options: { value: string; label: string }[];
  onChange: (v: string) => void;
}) {
  return (
    <div className="param-field">
      <label className="param-label">{label}</label>
      <select
        className="form-select param-select"
        value={value}
        onChange={e => onChange(e.target.value)}
      >
        {options.map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
      </select>
    </div>
  );
}

function NumberField({ label, value, min, max, step, unit, onChange }: {
  label: string; value: number; min?: number; max?: number; step?: number; unit?: string;
  onChange: (v: number) => void;
}) {
  return (
    <div className="param-field">
      <label className="param-label">
        {unit ? t('WEB_MOD_LABEL_WITH_UNIT', { 0: label, 1: unit }) : label}
      </label>
      <input
        type="number"
        className="form-input param-input"
        min={min} max={max} step={step}
        value={value}
        onChange={e => onChange(Number(e.target.value))}
      />
    </div>
  );
}

function CheckboxField({ label, value, onChange }: {
  label: string; value: boolean; onChange: (v: boolean) => void;
}) {
  return (
    <div className="param-field" style={{ justifyContent: 'flex-end' }}>
      <label className="param-checkbox-row">
        <input
          type="checkbox"
          checked={value}
          onChange={e => onChange(e.target.checked)}
        />
        <span>{label}</span>
      </label>
    </div>
  );
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

function FrequencyField({ label, value, onChange, min, max }: {
  label: string; value: number; min?: number; max?: number;
  onChange: (v: number) => void;
}) {
  const preset = FREQ_PRESETS.find(p => p.value === value);
  const isCustom = !preset || preset.value === -1;

  return (
    <div className="param-field" style={{ minWidth: 200 }}>
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
        <input
          type="number"
          className="form-input"
          style={{ width: 70 }}
          min={min ?? 0.5} max={max ?? 100} step={0.5}
          value={value}
          onChange={e => onChange(Number(e.target.value))}
        />
        <span style={{ alignSelf: 'center', fontSize: 12, color: 'var(--text-muted)' }}>Hz</span>
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
        <div className="param-grid">
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
              { value: '660_808nm', label: t('WEB_WL_660_808') },
              { value: '1064nm', label: t('WEB_WL_1064') },
              { value: '660_808_1064nm', label: t('WEB_WL_660_808_1064') },
            ]}
          />
          <SliderField label={t('WEB_MOD_INTENSITY')} value={p.intensityPercent} min={10} max={100} unit="%" onChange={v => update<typeof params>({ ...p, intensityPercent: v })} />
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} min={0} max={100} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label={t('MODALITY_DUTY_CYCLE')} value={p.dutyCyclePercent} min={5} max={100} unit="%" onChange={v => update<typeof params>({ ...p, dutyCyclePercent: v })} />
        </div>
      );
    }

    case 'pbm_intranasal': {
      const p = params.params as PBMIntranasalParams;
      return (
        <div className="param-grid">
          <SliderField label={t('WEB_MOD_INTENSITY')} value={p.intensityPercent} min={10} max={100} unit="%" onChange={v => update<typeof params>({ ...p, intensityPercent: v })} />
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} min={0} max={40} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label={t('MODALITY_DUTY_CYCLE')} value={p.dutyCyclePercent} min={5} max={100} unit="%" onChange={v => update<typeof params>({ ...p, dutyCyclePercent: v })} />
        </div>
      );
    }

    case 'eeg_neurofeedback': {
      const p = params.params as EEGNeurofeedbackParams;
      return (
        <div className="param-grid">
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
        </div>
      );
    }

    case 'bes_tacs': {
      const p = params.params as BESTacsParams;
      return (
        <div className="param-grid">
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} min={0.5} max={40} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} min={0.1} max={1.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          <SelectField label={t('MODALITY_WAVEFORM')} value={p.waveform} onChange={v => update<typeof params>({ ...p, waveform: v as BESTacsParams['waveform'] })}
            options={[
              { value: 'sinusoidal', label: t('WEB_WAVE_SINUSOIDAL') },
              { value: 'square', label: t('WEB_WAVE_SQUARE') },
              { value: 'triangular', label: t('WEB_WAVE_TRIANGULAR') },
            ]}
          />
        </div>
      );
    }

    case 'tdcs': {
      const p = params.params as TDCSParams;
      return (
        <div className="param-grid">
          <SliderField label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} min={0.1} max={2.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          <NumberField label={t('WEB_MOD_RAMP')} value={p.rampSeconds} min={5} max={120} unit="sec" onChange={v => update<typeof params>({ ...p, rampSeconds: v })} />
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
        </div>
      );
    }

    case 'vns_hrv': {
      const p = params.params as VNSHRVParams;
      return (
        <div className="param-grid">
          <FrequencyField label={t('WEB_MOD_VNS_FREQUENCY')} value={p.frequencyHz} min={1} max={25} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} min={0.1} max={2.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          <SelectField label={t('WEB_MOD_HRV_PROTOCOL')} value={p.hrvProtocol} onChange={v => update<typeof params>({ ...p, hrvProtocol: v as VNSHRVParams['hrvProtocol'] })}
            options={[
              { value: 'standalone', label: t('WEB_HRV_STANDALONE') },
              { value: 'tavns_sync', label: t('WEB_HRV_TAVNS_SYNC') },
              { value: 'eeg_biofeedback', label: t('WEB_HRV_EEG_BIOFEEDBACK') },
              { value: 'combined_pbm', label: t('WEB_HRV_COMBINED_PBM') },
            ]}
          />
          <SliderField label={t('UI_MOD_BREATHING_RATE')} value={p.resonanceBreathingRate} min={4} max={7} step={0.5} unit=" BPM" onChange={v => update<typeof params>({ ...p, resonanceBreathingRate: v })} />
        </div>
      );
    }

    case 'audio_entrainment': {
      const p = params.params as AudioEntrainmentParams;
      return (
        <div className="param-grid">
          <div className="param-field">
            <label className="param-label">{t('WEB_MOD_BINAURAL_BEATS')}</label>
            <div style={{ display: 'flex', gap: 6 }}>
              <input
                type="number"
                className="form-input"
                style={{ flex: 1 }}
                min={0.5} max={100} step={0.5}
                value={p.binauralBeatsHz ?? ''}
                placeholder={t('UI_NONE')}
                onChange={e => update<typeof params>({ ...p, binauralBeatsHz: e.target.value ? Number(e.target.value) : undefined })}
              />
            </div>
          </div>
          <div className="param-field">
            <label className="param-label">{t('WEB_MOD_ISOCHRONIC_TONES')}</label>
            <input
              type="number"
              className="form-input"
              min={0.5} max={100} step={0.5}
              value={p.isochronicTonesHz ?? ''}
              placeholder={t('UI_NONE')}
              onChange={e => update<typeof params>({ ...p, isochronicTonesHz: e.target.value ? Number(e.target.value) : undefined })}
            />
          </div>
          <SelectField label={t('WEB_MOD_NOISE_TYPE')} value={p.noiseType ?? ''} onChange={v => update<typeof params>({ ...p, noiseType: v ? v as AudioEntrainmentParams['noiseType'] : undefined })}
            options={[
              { value: '', label: t('UI_NONE') },
              { value: 'pink', label: t('WEB_NOISE_PINK') },
              { value: 'brown', label: t('WEB_NOISE_BROWN') },
            ]}
          />
          <NumberField label={t('WEB_MOD_CARRIER')} value={p.carrierHz} min={80} max={1000} unit="Hz" onChange={v => update<typeof params>({ ...p, carrierHz: v })} />
          <SliderField label={t('WEB_MOD_VOLUME')} value={p.volumePercent} min={0} max={100} unit="%" onChange={v => update<typeof params>({ ...p, volumePercent: v })} />
          <CheckboxField label={t('WEB_MOD_EEG_ADAPTIVE')} value={p.eegAdaptive} onChange={v => update<typeof params>({ ...p, eegAdaptive: v })} />
          <CheckboxField label={t('WEB_MOD_BONE_CONDUCTION')} value={p.boneConductionPacer} onChange={v => update<typeof params>({ ...p, boneConductionPacer: v })} />
        </div>
      );
    }

    case 'visual_stimulation': {
      const p = params.params as VisualStimParams;
      return (
        <div className="param-grid">
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} min={0.5} max={100} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SelectField label={t('WEB_FIELD_MODE')} value={p.mode} onChange={v => update<typeof params>({ ...p, mode: v as VisualStimParams['mode'] })}
            options={[
              { value: 'binocular', label: t('WEB_VIS_BINOCULAR') },
              { value: 'emdr', label: t('WEB_VIS_EMDR') },
              { value: 'retinal_pbm', label: t('WEB_VIS_RETINAL_PBM') },
              { value: 'mode_f', label: t('WEB_VIS_MODE_F') },
            ]}
          />
          {p.mode === 'emdr' && (
            <NumberField label={t('WEB_MOD_EMDR_CADENCE')} value={p.emdrCadenceHz} min={0.2} max={4} step={0.1} unit="Hz" onChange={v => update<typeof params>({ ...p, emdrCadenceHz: v })} />
          )}
          <CheckboxField label={t('WEB_MOD_ENABLE_MODE_F')} value={p.enableModeF} onChange={v => update<typeof params>({ ...p, enableModeF: v })} />
        </div>
      );
    }

    case 'qeeg_21ch': {
      const p = params.params as QEEG21chParams;
      return (
        <div className="param-grid">
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
        </div>
      );
    }

    case 'tms': {
      const p = params.params as TMSParams;
      return (
        <div className="param-grid">
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
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} min={1} max={50} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <NumberField label={t('WEB_MOD_INTENSITY')} value={p.intensityPercentMT} min={80} max={130} unit="% MT" onChange={v => update<typeof params>({ ...p, intensityPercentMT: v })} />
          <NumberField label={t('UI_MOD_PULSE_COUNT')} value={p.pulseCount} min={100} max={6000} step={100} onChange={v => update<typeof params>({ ...p, pulseCount: v })} />
        </div>
      );
    }

    case 'pbm_deep_1170nm': {
      const p = params.params as DeepPBM1170Params;
      return (
        <div className="param-grid">
          <SliderField label={t('WEB_MOD_INTENSITY')} value={p.intensityMWcm2} min={100} max={1000} step={50} unit=" mW/cm²" onChange={v => update<typeof params>({ ...p, intensityMWcm2: v })} />
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} min={0} max={40} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label={t('MODALITY_DUTY_CYCLE')} value={p.dutyCyclePercent} min={5} max={100} unit="%" onChange={v => update<typeof params>({ ...p, dutyCyclePercent: v })} />
        </div>
      );
    }

    case 'clinical_tacs': {
      const p = params.params as ClinicalTacsParams;
      return (
        <div className="param-grid">
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} min={0.5} max={100} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} min={0.1} max={4.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          <NumberField label={t('WEB_MOD_CHANNEL_COUNT')} value={p.channelCount} min={2} max={16} onChange={v => update<typeof params>({ ...p, channelCount: v })} />
          <SelectField label={t('MODALITY_WAVEFORM')} value={p.waveform} onChange={v => update<typeof params>({ ...p, waveform: v as ClinicalTacsParams['waveform'] })}
            options={[
              { value: 'sinusoidal', label: t('WEB_WAVE_SINUSOIDAL') },
              { value: 'square', label: t('WEB_WAVE_SQUARE') },
              { value: 'triangular', label: t('WEB_WAVE_TRIANGULAR') },
            ]}
          />
        </div>
      );
    }

    case 'hd_tdcs': {
      const p = params.params as HDTdcsParams;
      return (
        <div className="param-grid">
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
          <SliderField label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} min={0.1} max={2.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
        </div>
      );
    }

    case 'cervical_vns': {
      const p = params.params as CervicalVnsParams;
      return (
        <div className="param-grid">
          <FrequencyField label={t('MODALITY_FREQUENCY')} value={p.frequencyHz} min={1} max={25} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label={t('WEB_MOD_INTENSITY')} value={p.intensityMilliamps} min={0.1} max={2.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
        </div>
      );
    }

    case 'vibrotactile_40hz': {
      const p = params.params as VibrotactileParams;
      return (
        <div className="param-grid">
          <SliderField label={t('WEB_MOD_INTENSITY')} value={p.intensityG} min={0.6} max={1.2} step={0.05} unit=" G" onChange={v => update<typeof params>({ ...p, intensityG: v })} />
          <CheckboxField label={t('WEB_MOD_SYNC_AUDIO')} value={p.syncToAudio} onChange={v => update<typeof params>({ ...p, syncToAudio: v })} />
          <CheckboxField label={t('WEB_MOD_SYNC_VISUAL')} value={p.syncToVisual} onChange={v => update<typeof params>({ ...p, syncToVisual: v })} />
        </div>
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
          <div className="modality-consumer-name">{t(meta.consumerNameKey)}</div>
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
