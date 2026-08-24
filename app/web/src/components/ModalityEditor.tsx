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

// ─── Interval Controls ─────────────────────────────────────────────────────────

interface IntervalControlsProps {
  interval: NPIntervalConfig;
  onChange: (iv: NPIntervalConfig) => void;
}

export function IntervalControls({ interval, onChange }: IntervalControlsProps) {
  const isContinuous = interval.intervalOnSeconds === 0;

  return (
    <div className="interval-section">
      <div className="interval-title">Timing / Interval</div>
      <div className="param-grid" style={{ gridTemplateColumns: 'repeat(auto-fill, minmax(130px, 1fr))' }}>
        <div className="param-field">
          <label className="param-label">Mode</label>
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
            <option value="continuous">Continuous</option>
            <option value="interval">Interval</option>
          </select>
        </div>

        {!isContinuous && (
          <>
            <div className="param-field">
              <label className="param-label">On (sec)</label>
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
              <label className="param-label">Off (sec)</label>
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
              <label className="param-label">Repeat</label>
              <input
                type="number"
                className="form-input param-input"
                min={1}
                max={1000}
                placeholder="Until end"
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
      <label className="param-label">{label}{unit ? ` (${unit})` : ''}</label>
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

const FREQ_PRESETS = [
  { label: 'Delta 2Hz', value: 2 },
  { label: 'Theta 6Hz', value: 6 },
  { label: 'Alpha 10Hz', value: 10 },
  { label: 'Beta 20Hz', value: 20 },
  { label: 'Gamma 40Hz', value: 40 },
  { label: 'Custom', value: -1 },
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
            <option key={p.value} value={p.value}>{p.label}</option>
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
          <SelectField label="Zones" value={p.zones} onChange={v => update<typeof params>({ ...p, zones: v as PBMTranscranialParams['zones'] })}
            options={[
              { value: 'all', label: 'All 5 zones' },
              { value: 'front', label: 'Frontal' },
              { value: 'rear', label: 'Parieto-occipital' },
              { value: 'custom', label: 'Custom' },
            ]}
          />
          <SelectField label="Wavelength" value={p.wavelength} onChange={v => update<typeof params>({ ...p, wavelength: v as PBMTranscranialParams['wavelength'] })}
            options={[
              { value: '660_808nm', label: '660 + 808nm' },
              { value: '1064nm', label: '1064nm (Smart Module)' },
              { value: '660_808_1064nm', label: '660 + 808 + 1064nm' },
            ]}
          />
          <SliderField label="Intensity" value={p.intensityPercent} min={10} max={100} unit="%" onChange={v => update<typeof params>({ ...p, intensityPercent: v })} />
          <FrequencyField label="Frequency" value={p.frequencyHz} min={0} max={100} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label="Duty Cycle" value={p.dutyCyclePercent} min={5} max={100} unit="%" onChange={v => update<typeof params>({ ...p, dutyCyclePercent: v })} />
        </div>
      );
    }

    case 'pbm_intranasal': {
      const p = params.params as PBMIntranasalParams;
      return (
        <div className="param-grid">
          <SliderField label="Intensity" value={p.intensityPercent} min={10} max={100} unit="%" onChange={v => update<typeof params>({ ...p, intensityPercent: v })} />
          <FrequencyField label="Frequency" value={p.frequencyHz} min={0} max={40} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label="Duty Cycle" value={p.dutyCyclePercent} min={5} max={100} unit="%" onChange={v => update<typeof params>({ ...p, dutyCyclePercent: v })} />
        </div>
      );
    }

    case 'eeg_neurofeedback': {
      const p = params.params as EEGNeurofeedbackParams;
      return (
        <div className="param-grid">
          <SelectField label="Channels" value={p.channels} onChange={v => update<typeof params>({ ...p, channels: v as EEGNeurofeedbackParams['channels'] })}
            options={[
              { value: 'all', label: 'All 8 channels' },
              { value: 'front', label: 'Frontal (Fp1/2, F3/4)' },
              { value: 'central', label: 'Central (C3/4, P3/4)' },
              { value: 'custom', label: 'Custom' },
            ]}
          />
          <SelectField label="Target Band" value={p.band} onChange={v => update<typeof params>({ ...p, band: v as EEGNeurofeedbackParams['band'] })}
            options={[
              { value: 'delta', label: 'Delta (0.5–4Hz)' },
              { value: 'theta', label: 'Theta (4–8Hz)' },
              { value: 'alpha', label: 'Alpha (8–13Hz)' },
              { value: 'beta', label: 'Beta (13–30Hz)' },
              { value: 'gamma', label: 'Gamma (30–100Hz)' },
              { value: 'alpha_theta', label: 'Alpha/Theta ratio' },
              { value: 'gamma_theta', label: 'Gamma+Theta coupled' },
            ]}
          />
          <CheckboxField label="Closed-loop EEG adaptive" value={p.closedLoopEnabled} onChange={v => update<typeof params>({ ...p, closedLoopEnabled: v })} />
        </div>
      );
    }

    case 'bes_tacs': {
      const p = params.params as BESTacsParams;
      return (
        <div className="param-grid">
          <FrequencyField label="Frequency" value={p.frequencyHz} min={0.5} max={40} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label="Intensity" value={p.intensityMilliamps} min={0.1} max={1.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          <SelectField label="Waveform" value={p.waveform} onChange={v => update<typeof params>({ ...p, waveform: v as BESTacsParams['waveform'] })}
            options={[
              { value: 'sinusoidal', label: 'Sinusoidal' },
              { value: 'square', label: 'Square' },
              { value: 'triangular', label: 'Triangular' },
            ]}
          />
        </div>
      );
    }

    case 'tdcs': {
      const p = params.params as TDCSParams;
      return (
        <div className="param-grid">
          <SliderField label="Intensity" value={p.intensityMilliamps} min={0.1} max={2.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          <NumberField label="Ramp" value={p.rampSeconds} min={5} max={120} unit="sec" onChange={v => update<typeof params>({ ...p, rampSeconds: v })} />
          <div className="param-field" style={{ flex: '0 0 100%' }}>
            <label className="param-label">Electrode Pairs (anode / cathode)</label>
            <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
              {p.electrodePairs.map(([a, c], i) => (
                <div key={i} style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
                  <input
                    className="form-input"
                    style={{ width: 80 }}
                    value={a}
                    placeholder="Anode"
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
                    placeholder="Cathode"
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
                  + Add pair
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
          <FrequencyField label="VNS Frequency" value={p.frequencyHz} min={1} max={25} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label="Intensity" value={p.intensityMilliamps} min={0.1} max={2.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          <SelectField label="HRV Protocol" value={p.hrvProtocol} onChange={v => update<typeof params>({ ...p, hrvProtocol: v as VNSHRVParams['hrvProtocol'] })}
            options={[
              { value: 'standalone', label: 'Standalone Coherence' },
              { value: 'tavns_sync', label: 'HRV + taVNS Sync' },
              { value: 'eeg_biofeedback', label: 'HRV + EEG Dual Biofeedback' },
              { value: 'combined_pbm', label: 'HRV + PBM Combined' },
            ]}
          />
          <SliderField label="Breathing Rate" value={p.resonanceBreathingRate} min={4} max={7} step={0.5} unit=" BPM" onChange={v => update<typeof params>({ ...p, resonanceBreathingRate: v })} />
        </div>
      );
    }

    case 'audio_entrainment': {
      const p = params.params as AudioEntrainmentParams;
      return (
        <div className="param-grid">
          <div className="param-field">
            <label className="param-label">Binaural Beats (Hz)</label>
            <div style={{ display: 'flex', gap: 6 }}>
              <input
                type="number"
                className="form-input"
                style={{ flex: 1 }}
                min={0.5} max={100} step={0.5}
                value={p.binauralBeatsHz ?? ''}
                placeholder="None"
                onChange={e => update<typeof params>({ ...p, binauralBeatsHz: e.target.value ? Number(e.target.value) : undefined })}
              />
            </div>
          </div>
          <div className="param-field">
            <label className="param-label">Isochronic Tones (Hz)</label>
            <input
              type="number"
              className="form-input"
              min={0.5} max={100} step={0.5}
              value={p.isochronicTonesHz ?? ''}
              placeholder="None"
              onChange={e => update<typeof params>({ ...p, isochronicTonesHz: e.target.value ? Number(e.target.value) : undefined })}
            />
          </div>
          <SelectField label="Noise Type" value={p.noiseType ?? ''} onChange={v => update<typeof params>({ ...p, noiseType: v ? v as AudioEntrainmentParams['noiseType'] : undefined })}
            options={[
              { value: '', label: 'None' },
              { value: 'pink', label: 'Pink noise' },
              { value: 'brown', label: 'Brown noise' },
            ]}
          />
          <NumberField label="Carrier" value={p.carrierHz} min={80} max={1000} unit="Hz" onChange={v => update<typeof params>({ ...p, carrierHz: v })} />
          <SliderField label="Volume" value={p.volumePercent} min={0} max={100} unit="%" onChange={v => update<typeof params>({ ...p, volumePercent: v })} />
          <CheckboxField label="EEG adaptive" value={p.eegAdaptive} onChange={v => update<typeof params>({ ...p, eegAdaptive: v })} />
          <CheckboxField label="Bone conduction pacer" value={p.boneConductionPacer} onChange={v => update<typeof params>({ ...p, boneConductionPacer: v })} />
        </div>
      );
    }

    case 'visual_stimulation': {
      const p = params.params as VisualStimParams;
      return (
        <div className="param-grid">
          <FrequencyField label="Frequency" value={p.frequencyHz} min={0.5} max={100} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SelectField label="Mode" value={p.mode} onChange={v => update<typeof params>({ ...p, mode: v as VisualStimParams['mode'] })}
            options={[
              { value: 'binocular', label: 'Binocular flicker' },
              { value: 'emdr', label: 'EMDR L/R alternation' },
              { value: 'retinal_pbm', label: 'Retinal PBM' },
              { value: 'mode_f', label: 'Mode F (NIR retinal, invisible)' },
            ]}
          />
          {p.mode === 'emdr' && (
            <NumberField label="EMDR Cadence" value={p.emdrCadenceHz} min={0.2} max={4} step={0.1} unit="Hz" onChange={v => update<typeof params>({ ...p, emdrCadenceHz: v })} />
          )}
          <CheckboxField label="Enable Mode F (808nm)" value={p.enableModeF} onChange={v => update<typeof params>({ ...p, enableModeF: v })} />
        </div>
      );
    }

    case 'qeeg_21ch': {
      const p = params.params as QEEG21chParams;
      return (
        <div className="param-grid">
          <SelectField label="Montage" value={p.montage} onChange={v => update<typeof params>({ ...p, montage: v as QEEG21chParams['montage'] })}
            options={[{ value: 'standard_1020', label: '10-20 Standard' }, { value: 'custom', label: 'Custom' }]}
          />
          <SelectField label="Reference" value={p.reference} onChange={v => update<typeof params>({ ...p, reference: v as QEEG21chParams['reference'] })}
            options={[
              { value: 'linked_ear', label: 'Linked ear (A1/A2)' },
              { value: 'cz', label: 'Cz reference' },
              { value: 'average', label: 'Average reference' },
            ]}
          />
          <CheckboxField label="sLORETA source imaging" value={p.sloretaEnabled} onChange={v => update<typeof params>({ ...p, sloretaEnabled: v })} />
        </div>
      );
    }

    case 'tms': {
      const p = params.params as TMSParams;
      return (
        <div className="param-grid">
          <SelectField label="Protocol" value={p.tmsProtocol} onChange={v => update<typeof params>({ ...p, tmsProtocol: v as TMSParams['tmsProtocol'] })}
            options={[{ value: 'rTMS', label: 'rTMS' }, { value: 'TBS', label: 'TBS' }, { value: 'iTBS', label: 'iTBS' }]}
          />
          <SelectField label="Target" value={p.target} onChange={v => update<typeof params>({ ...p, target: v as TMSParams['target'] })}
            options={[
              { value: 'DLPFC_L', label: 'DLPFC Left' },
              { value: 'DLPFC_R', label: 'DLPFC Right' },
              { value: 'VLPFC_L', label: 'VLPFC Left' },
              { value: 'ACC', label: 'ACC' },
              { value: 'MPFC', label: 'MPFC' },
              { value: 'M1_L', label: 'M1 Left' },
              { value: 'M1_R', label: 'M1 Right' },
            ]}
          />
          <FrequencyField label="Frequency" value={p.frequencyHz} min={1} max={50} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <NumberField label="Intensity" value={p.intensityPercentMT} min={80} max={130} unit="% MT" onChange={v => update<typeof params>({ ...p, intensityPercentMT: v })} />
          <NumberField label="Pulse Count" value={p.pulseCount} min={100} max={6000} step={100} onChange={v => update<typeof params>({ ...p, pulseCount: v })} />
        </div>
      );
    }

    case 'pbm_deep_1170nm': {
      const p = params.params as DeepPBM1170Params;
      return (
        <div className="param-grid">
          <SliderField label="Intensity" value={p.intensityMWcm2} min={100} max={1000} step={50} unit=" mW/cm²" onChange={v => update<typeof params>({ ...p, intensityMWcm2: v })} />
          <FrequencyField label="Frequency" value={p.frequencyHz} min={0} max={40} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label="Duty Cycle" value={p.dutyCyclePercent} min={5} max={100} unit="%" onChange={v => update<typeof params>({ ...p, dutyCyclePercent: v })} />
        </div>
      );
    }

    case 'clinical_tacs': {
      const p = params.params as ClinicalTacsParams;
      return (
        <div className="param-grid">
          <FrequencyField label="Frequency" value={p.frequencyHz} min={0.5} max={100} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label="Intensity" value={p.intensityMilliamps} min={0.1} max={4.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
          <NumberField label="Channel Count" value={p.channelCount} min={2} max={16} onChange={v => update<typeof params>({ ...p, channelCount: v })} />
          <SelectField label="Waveform" value={p.waveform} onChange={v => update<typeof params>({ ...p, waveform: v as ClinicalTacsParams['waveform'] })}
            options={[
              { value: 'sinusoidal', label: 'Sinusoidal' },
              { value: 'square', label: 'Square' },
              { value: 'triangular', label: 'Triangular' },
            ]}
          />
        </div>
      );
    }

    case 'hd_tdcs': {
      const p = params.params as HDTdcsParams;
      return (
        <div className="param-grid">
          <SelectField label="Target" value={p.target} onChange={v => update<typeof params>({ ...p, target: v as HDTdcsParams['target'] })}
            options={[
              { value: 'DLPFC_L', label: 'DLPFC Left' }, { value: 'DLPFC_R', label: 'DLPFC Right' },
              { value: 'VLPFC_L', label: 'VLPFC Left' }, { value: 'ACC', label: 'ACC' },
              { value: 'MPFC', label: 'MPFC' }, { value: 'M1_L', label: 'M1 Left' }, { value: 'M1_R', label: 'M1 Right' },
            ]}
          />
          <SelectField label="Montage" value={p.montage} onChange={v => update<typeof params>({ ...p, montage: v as HDTdcsParams['montage'] })}
            options={[
              { value: 'ring_4x1', label: '4×1 Ring (focal)' },
              { value: 'bilateral_4x1', label: 'Bilateral 4×1' },
              { value: 'standard_2_electrode', label: 'Standard 2-electrode' },
            ]}
          />
          <SliderField label="Intensity" value={p.intensityMilliamps} min={0.1} max={2.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
        </div>
      );
    }

    case 'cervical_vns': {
      const p = params.params as CervicalVnsParams;
      return (
        <div className="param-grid">
          <FrequencyField label="Frequency" value={p.frequencyHz} min={1} max={25} onChange={v => update<typeof params>({ ...p, frequencyHz: v })} />
          <SliderField label="Intensity" value={p.intensityMilliamps} min={0.1} max={2.0} step={0.1} unit=" mA" onChange={v => update<typeof params>({ ...p, intensityMilliamps: v })} />
        </div>
      );
    }

    case 'vibrotactile_40hz': {
      const p = params.params as VibrotactileParams;
      return (
        <div className="param-grid">
          <SliderField label="Intensity" value={p.intensityG} min={0.6} max={1.2} step={0.05} unit=" G" onChange={v => update<typeof params>({ ...p, intensityG: v })} />
          <CheckboxField label="Sync to audio" value={p.syncToAudio} onChange={v => update<typeof params>({ ...p, syncToAudio: v })} />
          <CheckboxField label="Sync to visual" value={p.syncToVisual} onChange={v => update<typeof params>({ ...p, syncToVisual: v })} />
        </div>
      );
    }

    default: {
      // exhaustive check
      return <div style={{ color: 'var(--error)', fontSize: 13 }}>Unknown modality type: {(params as NPModalityParams).type}</div>;
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
          title={modality.enabled ? 'Disable' : 'Enable'}
        />
        <span className="modality-icon-display">{meta.icon}</span>
        <div>
          <div className="modality-name">{meta.displayName}</div>
          <div className="modality-consumer-name">{meta.consumerName}</div>
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
            title="Remove modality"
          >
            ✕
          </button>
        </div>
      </div>

      {expanded && (
        <div className="modality-block-body">
          {isT2 && (
            <div className="t2-warning">
              🔬 Requires T2 (Pro) hardware
            </div>
          )}
          {isAccessory && (
            <div className="t2-warning" style={{ background: 'var(--warning-dim)', borderColor: 'rgba(245,158,11,0.3)', color: 'var(--warning)' }}>
              📳 Requires optional accessory (provisional)
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
          <span className="modal-title">Add Modality</span>
          <button className="modal-close" onClick={onClose}>×</button>
        </div>
        <div className="modal-body">
          {TIER_ORDER.map(tier => (
            <div key={tier}>
              <div className="picker-group-title">{tier === 'T1' ? 'T1 — Home' : tier === 'T2' ? 'T2 — Pro' : 'Accessories'}</div>
              {byTier[tier].map(meta => (
                <div
                  key={meta.id}
                  className={`picker-item${selected === meta.id ? ' selected' : ''}`}
                  onClick={() => setSelected(meta.id)}
                >
                  <span className="picker-icon">{meta.icon}</span>
                  <div className="picker-info">
                    <div className="picker-name">{meta.displayName}</div>
                    <div className="picker-desc">{meta.shortDescription}</div>
                  </div>
                  <span className={`picker-tier ${meta.tier}`}>{meta.tier}</span>
                </div>
              ))}
            </div>
          ))}
        </div>
        <div className="modal-footer">
          <button className="btn btn-secondary" onClick={onClose}>Cancel</button>
          <button
            className="btn btn-primary"
            disabled={!selected}
            onClick={() => { if (selected) { onSelect(selected); onClose(); } }}
          >
            Add Modality
          </button>
        </div>
      </div>
    </div>
  );
}
