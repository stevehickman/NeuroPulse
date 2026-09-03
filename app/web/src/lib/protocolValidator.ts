import {
  NPProtocolDefinition,
  NPProtocolEntry,
  NPProtocolModality,
} from '../types/protocol';
import {
  NPLimitsSet,
  NPValidationIssue,
  NPValidationResult,
  LimitSource,
} from '../types/limits';
import { NPHardwareLimits } from './hardwareLimits';
import { t } from './i18n';

// ─── Result builder ────────────────────────────────────────────────────────────

function makeResult(issues: NPValidationIssue[]): NPValidationResult {
  return {
    issues,
    isValid: !issues.some(i => i.severity === 'error'),
    hasWarnings: issues.some(i => i.severity === 'warning'),
    errors: issues.filter(i => i.severity === 'error'),
    warnings: issues.filter(i => i.severity === 'warning'),
  };
}

/** Locale key per limit source, for the "(global)" suffix on a limit value. */
const LIMIT_SOURCE_KEY: Record<LimitSource, string> = {
  hardware: 'VALIDATE_SOURCE_HARDWARE',
  global: 'VALIDATE_SOURCE_GLOBAL',
  helmet: 'VALIDATE_SOURCE_HELMET',
  individual: 'VALIDATE_SOURCE_INDIVIDUAL',
};

function issue(
  severity: 'error' | 'warning',
  modality: string | undefined,
  parameterKey: string,
  parameterDisplayName: string,
  actualValueDescription: string,
  limitValueDescription: string,
  limitSource: LimitSource,
  message: string
): NPValidationIssue {
  return {
    id: crypto.randomUUID(),
    severity,
    modality,
    parameterKey,
    parameterDisplayName,
    actualValueDescription,
    limitValueDescription: t('VALIDATE_LIMIT_WITH_SOURCE', {
      0: limitValueDescription,
      1: t(LIMIT_SOURCE_KEY[limitSource]),
    }),
    limitSource,
    message,
  };
}

// ─── Per-modality validation ───────────────────────────────────────────────────

function validateModality(
  block: NPProtocolModality,
  lim: NPLimitsSet,
  issues: NPValidationIssue[]
): void {
  const p = block.modalityParams;
  const hw = NPHardwareLimits;

  switch (p.type) {
    case 'pbm_transcranial': {
      const l = lim.pbmTranscranial;
      // Hardware limits
      if (p.params.dutyCyclePercent > hw.pbmDutyCycleMaxPercent) {
        issues.push(issue(
          'error', 'pbm_transcranial', 'dutyCyclePercent', t('VALIDATE_PARAM_DUTY_CYCLE'),
          `${p.params.dutyCyclePercent}%`, `${hw.pbmDutyCycleMaxPercent}%`, 'hardware',
          t('VALIDATE_MSG_PBM_TRANSCRANIAL_DUTYCYCLEPERCENT', { 0: p.params.dutyCyclePercent, 1: hw.pbmDutyCycleMaxPercent })
        ));
      }
      if (p.params.frequencyHz < 0) {
        issues.push(issue(
          'error', 'pbm_transcranial', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
          `${p.params.frequencyHz} Hz`, '≥0 Hz', 'hardware',
          t('VALIDATE_MSG_PBM_TRANSCRANIAL_FREQUENCYHZ')
        ));
      }
      // Dosage limits
      if (l?.maxIntensityPercent != null && p.params.intensityPercent > l.maxIntensityPercent) {
        issues.push(issue(
          'error', 'pbm_transcranial', 'intensityPercent', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityPercent}%`, `${l.maxIntensityPercent}%`, 'global',
          t('VALIDATE_MSG_PBM_TRANSCRANIAL_INTENSITYPERCENT', { 0: p.params.intensityPercent, 1: l.maxIntensityPercent })
        ));
      }
      if (l?.maxFrequencyHz != null && p.params.frequencyHz > l.maxFrequencyHz) {
        issues.push(issue(
          'error', 'pbm_transcranial', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
          `${p.params.frequencyHz} Hz`, `${l.maxFrequencyHz} Hz`, 'global',
          t('VALIDATE_MSG_PBM_TRANSCRANIAL_FREQUENCYHZ_2', { 0: p.params.frequencyHz, 1: l.maxFrequencyHz })
        ));
      }
      if (l?.maxDutyCyclePercent != null && p.params.dutyCyclePercent > l.maxDutyCyclePercent) {
        issues.push(issue(
          'error', 'pbm_transcranial', 'dutyCyclePercent', t('VALIDATE_PARAM_DUTY_CYCLE'),
          `${p.params.dutyCyclePercent}%`, `${l.maxDutyCyclePercent}%`, 'global',
          t('VALIDATE_MSG_PBM_TRANSCRANIAL_DUTYCYCLEPERCENT_2', { 0: p.params.dutyCyclePercent, 1: l.maxDutyCyclePercent })
        ));
      }
      break;
    }

    case 'pbm_intranasal': {
      const l = lim.pbmIntranasal;
      // Hardware
      if (p.params.dutyCyclePercent > hw.pbmDutyCycleMaxPercent) {
        issues.push(issue(
          'error', 'pbm_intranasal', 'dutyCyclePercent', t('VALIDATE_PARAM_DUTY_CYCLE'),
          `${p.params.dutyCyclePercent}%`, `${hw.pbmDutyCycleMaxPercent}%`, 'hardware',
          t('VALIDATE_MSG_PBM_INTRANASAL_DUTYCYCLEPERCENT', { 0: p.params.dutyCyclePercent, 1: hw.pbmDutyCycleMaxPercent })
        ));
      }
      // Dosage
      if (l?.maxIntensityPercent != null && p.params.intensityPercent > l.maxIntensityPercent) {
        issues.push(issue(
          'error', 'pbm_intranasal', 'intensityPercent', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityPercent}%`, `${l.maxIntensityPercent}%`, 'global',
          t('VALIDATE_MSG_PBM_INTRANASAL_INTENSITYPERCENT', { 0: p.params.intensityPercent, 1: l.maxIntensityPercent })
        ));
      }
      break;
    }

    case 'eeg_neurofeedback': {
      const l = lim.eegNeurofeedback;
      if (l?.requireClosedLoop && !p.params.closedLoopEnabled) {
        issues.push(issue(
          'error', 'eeg_neurofeedback', 'closedLoopEnabled', t('VALIDATE_PARAM_CLOSED_LOOP'),
          'Disabled', 'Required', 'global',
          t('VALIDATE_MSG_EEG_NEUROFEEDBACK_CLOSEDLOOPENABLED')
        ));
      }
      if (l?.allowedBands != null && !l.allowedBands.includes(p.params.band)) {
        issues.push(issue(
          'error', 'eeg_neurofeedback', 'band', t('VALIDATE_PARAM_BAND'),
          p.params.band, l.allowedBands.join('/'), 'global',
          t('VALIDATE_MSG_EEG_NEUROFEEDBACK_BAND', { 0: p.params.band, 1: l.allowedBands.join(', ') })
        ));
      }
      break;
    }

    case 'bes_tacs': {
      const l = lim.besTacs;
      // Hardware
      if (p.params.intensityMilliamps > hw.besTacsMaxMilliamps) {
        issues.push(issue(
          'error', 'bes_tacs', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${hw.besTacsMaxMilliamps} mA`, 'hardware',
          t('VALIDATE_MSG_BES_TACS_INTENSITYMILLIAMPS', { 0: p.params.intensityMilliamps, 1: hw.besTacsMaxMilliamps })
        ));
      }
      if (p.params.frequencyHz < hw.besTacsMinHz || p.params.frequencyHz > hw.besTacsMaxHz) {
        issues.push(issue(
          'error', 'bes_tacs', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
          `${p.params.frequencyHz} Hz`, `${hw.besTacsMinHz}–${hw.besTacsMaxHz} Hz`, 'hardware',
          t('VALIDATE_MSG_BES_TACS_FREQUENCYHZ', { 0: p.params.frequencyHz, 1: hw.besTacsMinHz, 2: hw.besTacsMaxHz })
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'bes_tacs', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          t('VALIDATE_MSG_BES_TACS_INTENSITYMILLIAMPS_2', { 0: p.params.intensityMilliamps, 1: l.maxIntensityMilliamps })
        ));
      }
      if (l?.maxFrequencyHz != null && p.params.frequencyHz > l.maxFrequencyHz) {
        issues.push(issue(
          'error', 'bes_tacs', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
          `${p.params.frequencyHz} Hz`, `${l.maxFrequencyHz} Hz`, 'global',
          t('VALIDATE_MSG_BES_TACS_FREQUENCYHZ_2', { 0: p.params.frequencyHz, 1: l.maxFrequencyHz })
        ));
      }
      if (l?.minFrequencyHz != null && p.params.frequencyHz < l.minFrequencyHz) {
        issues.push(issue(
          'error', 'bes_tacs', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
          `${p.params.frequencyHz} Hz`, `≥${l.minFrequencyHz} Hz`, 'global',
          t('VALIDATE_MSG_BES_TACS_FREQUENCYHZ_3', { 0: p.params.frequencyHz, 1: l.minFrequencyHz })
        ));
      }
      break;
    }

    case 'tdcs': {
      const l = lim.tdcs;
      // Hardware
      if (p.params.intensityMilliamps < hw.tdcsMinMilliamps || p.params.intensityMilliamps > hw.tdcsMaxMilliamps) {
        issues.push(issue(
          'error', 'tdcs', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${hw.tdcsMinMilliamps}–${hw.tdcsMaxMilliamps} mA`, 'hardware',
          t('VALIDATE_MSG_TDCS_INTENSITYMILLIAMPS', { 0: p.params.intensityMilliamps, 1: hw.tdcsMinMilliamps, 2: hw.tdcsMaxMilliamps })
        ));
      }
      if (p.params.electrodePairs.length > hw.tdcsMaxElectrodePairs) {
        issues.push(issue(
          'error', 'tdcs', 'electrodePairs', t('VALIDATE_PARAM_ELECTRODE_PAIRS'),
          `${p.params.electrodePairs.length}`, `≤${hw.tdcsMaxElectrodePairs}`, 'hardware',
          t('VALIDATE_MSG_TDCS_ELECTRODEPAIRS', { 0: p.params.electrodePairs.length, 1: hw.tdcsMaxElectrodePairs })
        ));
      }
      if (p.params.rampSeconds < hw.tdcsRampSeconds) {
        issues.push(issue(
          'error', 'tdcs', 'rampSeconds', t('VALIDATE_PARAM_RAMP_TIME'),
          `${p.params.rampSeconds}s`, `${hw.tdcsRampSeconds}s`, 'hardware',
          t('VALIDATE_MSG_TDCS_RAMPSECONDS', { 0: p.params.rampSeconds, 1: hw.tdcsRampSeconds })
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'tdcs', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          t('VALIDATE_MSG_TDCS_INTENSITYMILLIAMPS_2', { 0: p.params.intensityMilliamps, 1: l.maxIntensityMilliamps })
        ));
      }
      break;
    }

    case 'vns_hrv': {
      const l = lim.vnsHrv;
      // Hardware
      if (p.params.intensityMilliamps > hw.vnsMaxMilliamps) {
        issues.push(issue(
          'error', 'vns_hrv', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${hw.vnsMaxMilliamps} mA`, 'hardware',
          t('VALIDATE_MSG_VNS_HRV_INTENSITYMILLIAMPS', { 0: p.params.intensityMilliamps, 1: hw.vnsMaxMilliamps })
        ));
      }
      if (p.params.frequencyHz < hw.vnsMinHz || p.params.frequencyHz > hw.vnsMaxHz) {
        issues.push(issue(
          'error', 'vns_hrv', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
          `${p.params.frequencyHz} Hz`, `${hw.vnsMinHz}–${hw.vnsMaxHz} Hz`, 'hardware',
          t('VALIDATE_MSG_VNS_HRV_FREQUENCYHZ', { 0: p.params.frequencyHz, 1: hw.vnsMinHz, 2: hw.vnsMaxHz })
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'vns_hrv', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          t('VALIDATE_MSG_VNS_HRV_INTENSITYMILLIAMPS_2', { 0: p.params.intensityMilliamps, 1: l.maxIntensityMilliamps })
        ));
      }
      if (l?.maxFrequencyHz != null && p.params.frequencyHz > l.maxFrequencyHz) {
        issues.push(issue(
          'error', 'vns_hrv', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
          `${p.params.frequencyHz} Hz`, `${l.maxFrequencyHz} Hz`, 'global',
          t('VALIDATE_MSG_VNS_HRV_FREQUENCYHZ_2', { 0: p.params.frequencyHz, 1: l.maxFrequencyHz })
        ));
      }
      if (l?.allowedProtocols != null && !l.allowedProtocols.includes(p.params.hrvProtocol)) {
        issues.push(issue(
          'error', 'vns_hrv', 'hrvProtocol', t('VALIDATE_PARAM_HRV_PROTOCOL'),
          p.params.hrvProtocol, l.allowedProtocols.join('/'), 'global',
          t('VALIDATE_MSG_VNS_HRV_HRVPROTOCOL', { 0: p.params.hrvProtocol, 1: l.allowedProtocols.join(', ') })
        ));
      }
      break;
    }

    case 'audio_entrainment': {
      const l = lim.audioEntrainment;
      if (l?.maxVolumePercent != null && p.params.volumePercent > l.maxVolumePercent) {
        issues.push(issue(
          'error', 'audio_entrainment', 'volumePercent', t('VALIDATE_PARAM_VOLUME'),
          `${p.params.volumePercent}%`, `${l.maxVolumePercent}%`, 'global',
          t('VALIDATE_MSG_AUDIO_ENTRAINMENT_VOLUMEPERCENT', { 0: p.params.volumePercent, 1: l.maxVolumePercent })
        ));
      }
      if (
        l?.maxBinauralBeatsHz != null &&
        p.params.binauralBeatsHz != null &&
        p.params.binauralBeatsHz > l.maxBinauralBeatsHz
      ) {
        issues.push(issue(
          'error', 'audio_entrainment', 'binauralBeatsHz', t('VALIDATE_PARAM_BINAURAL_BEAT'),
          `${p.params.binauralBeatsHz} Hz`, `${l.maxBinauralBeatsHz} Hz`, 'global',
          t('VALIDATE_MSG_AUDIO_ENTRAINMENT_BINAURALBEATSHZ', { 0: p.params.binauralBeatsHz, 1: l.maxBinauralBeatsHz })
        ));
      }
      if (
        l?.maxIsochronicTonesHz != null &&
        p.params.isochronicTonesHz != null &&
        p.params.isochronicTonesHz > l.maxIsochronicTonesHz
      ) {
        issues.push(issue(
          'error', 'audio_entrainment', 'isochronicTonesHz', t('VALIDATE_PARAM_ISOCHRONIC_TONE'),
          `${p.params.isochronicTonesHz} Hz`, `${l.maxIsochronicTonesHz} Hz`, 'global',
          t('VALIDATE_MSG_AUDIO_ENTRAINMENT_ISOCHRONICTONESHZ', { 0: p.params.isochronicTonesHz, 1: l.maxIsochronicTonesHz })
        ));
      }
      break;
    }

    case 'visual_stimulation': {
      const l = lim.visualStimulation;
      // Hardware ceiling
      if (p.params.frequencyHz > hw.visualMaxHz) {
        issues.push(issue(
          'error', 'visual_stimulation', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
          `${p.params.frequencyHz} Hz`, `${hw.visualMaxHz} Hz`, 'hardware',
          t('VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ', { 0: p.params.frequencyHz, 1: hw.visualMaxHz })
        ));
      }
      // Photoparoxysmal risk zone (3–30 Hz)
      const inRiskZone =
        p.params.frequencyHz >= hw.visualHighRiskMinHz &&
        p.params.frequencyHz <= hw.visualHighRiskMaxHz;
      if (inRiskZone) {
        const blockHighRisk = l?.blockHighRiskRange ?? false;
        if (blockHighRisk) {
          issues.push(issue(
            'error', 'visual_stimulation', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
            `${p.params.frequencyHz} Hz`,
            t('VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_2_ARG1', { 0: hw.visualHighRiskMinHz, 1: hw.visualHighRiskMaxHz }),
            'global',
            t('VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_2', { 0: p.params.frequencyHz, 1: hw.visualHighRiskMinHz, 2: hw.visualHighRiskMaxHz })
          ));
        } else {
          issues.push(issue(
            'warning', 'visual_stimulation', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
            `${p.params.frequencyHz} Hz`,
            t('VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_3_ARG1', { 0: hw.visualHighRiskMinHz, 1: hw.visualHighRiskMaxHz }),
            'hardware',
            t('VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_3', { 0: p.params.frequencyHz, 1: hw.visualHighRiskMinHz, 2: hw.visualHighRiskMaxHz })
          ));
        }
      }
      // Dosage limits
      if (l?.maxFrequencyHz != null && p.params.frequencyHz > l.maxFrequencyHz) {
        issues.push(issue(
          'error', 'visual_stimulation', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
          `${p.params.frequencyHz} Hz`, `${l.maxFrequencyHz} Hz`, 'global',
          t('VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_4', { 0: p.params.frequencyHz, 1: l.maxFrequencyHz })
        ));
      }
      if (l?.minFrequencyHz != null && p.params.frequencyHz < l.minFrequencyHz) {
        issues.push(issue(
          'error', 'visual_stimulation', 'frequencyHz', t('VALIDATE_PARAM_FREQUENCY'),
          `${p.params.frequencyHz} Hz`, `≥${l.minFrequencyHz} Hz`, 'global',
          t('VALIDATE_MSG_VISUAL_STIMULATION_FREQUENCYHZ_5', { 0: p.params.frequencyHz, 1: l.minFrequencyHz })
        ));
      }
      if (l?.allowedModes != null && !l.allowedModes.includes(p.params.mode)) {
        issues.push(issue(
          'error', 'visual_stimulation', 'mode', t('VALIDATE_PARAM_MODE'),
          p.params.mode, l.allowedModes.join('/'), 'global',
          t('VALIDATE_MSG_VISUAL_STIMULATION_MODE', { 0: p.params.mode, 1: l.allowedModes.join(', ') })
        ));
      }
      break;
    }

    case 'qeeg_21ch': {
      // No dedicated hardware limits for qEEG beyond electrode safety
      // No dosage limits interface defined for qEEG
      break;
    }

    case 'tms': {
      const l = lim.tms;
      if (l?.maxIntensityPercentMT != null && p.params.intensityPercentMT > l.maxIntensityPercentMT) {
        issues.push(issue(
          'error', 'tms', 'intensityPercentMT', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityPercentMT}% MT`, `${l.maxIntensityPercentMT}% MT`, 'global',
          t('VALIDATE_MSG_TMS_INTENSITYPERCENTMT', { 0: p.params.intensityPercentMT, 1: l.maxIntensityPercentMT })
        ));
      }
      if (l?.maxPulsesPerSession != null && p.params.pulseCount > l.maxPulsesPerSession) {
        issues.push(issue(
          'error', 'tms', 'pulseCount', t('VALIDATE_PARAM_PULSES'),
          `${p.params.pulseCount}`, `${l.maxPulsesPerSession}`, 'global',
          t('VALIDATE_MSG_TMS_PULSECOUNT', { 0: p.params.pulseCount, 1: l.maxPulsesPerSession })
        ));
      }
      if (l?.allowedProtocols != null && !l.allowedProtocols.includes(p.params.tmsProtocol)) {
        issues.push(issue(
          'error', 'tms', 'tmsProtocol', t('VALIDATE_PARAM_PROTOCOL'),
          p.params.tmsProtocol, l.allowedProtocols.join('/'), 'global',
          t('VALIDATE_MSG_TMS_TMSPROTOCOL', { 0: p.params.tmsProtocol, 1: l.allowedProtocols.join(', ') })
        ));
      }
      if (l?.allowedTargets != null && !l.allowedTargets.includes(p.params.target)) {
        issues.push(issue(
          'error', 'tms', 'target', t('VALIDATE_PARAM_TARGET'),
          p.params.target, l.allowedTargets.join('/'), 'global',
          t('VALIDATE_MSG_TMS_TARGET', { 0: p.params.target, 1: l.allowedTargets.join(', ') })
        ));
      }
      break;
    }

    case 'pbm_deep_1170nm': {
      const l = lim.pbmDeep1170nm;
      if (l?.maxIntensityMWcm2 != null && p.params.intensityMWcm2 > l.maxIntensityMWcm2) {
        issues.push(issue(
          'error', 'pbm_deep_1170nm', 'intensityMWcm2', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMWcm2} mW/cm²`, `${l.maxIntensityMWcm2} mW/cm²`, 'global',
          t('VALIDATE_MSG_PBM_DEEP_1170NM_INTENSITYMWCM2', { 0: p.params.intensityMWcm2, 1: l.maxIntensityMWcm2 })
        ));
      }
      if (p.params.dutyCyclePercent > hw.pbmDutyCycleMaxPercent) {
        issues.push(issue(
          'error', 'pbm_deep_1170nm', 'dutyCyclePercent', t('VALIDATE_PARAM_DUTY_CYCLE'),
          `${p.params.dutyCyclePercent}%`, `${hw.pbmDutyCycleMaxPercent}%`, 'hardware',
          t('VALIDATE_MSG_PBM_DEEP_1170NM_DUTYCYCLEPERCENT', { 0: p.params.dutyCyclePercent, 1: hw.pbmDutyCycleMaxPercent })
        ));
      }
      break;
    }

    case 'clinical_tacs': {
      const l = lim.clinicalTacs;
      // Hardware
      if (p.params.intensityMilliamps > hw.clinicalTacsMaxMilliamps) {
        issues.push(issue(
          'error', 'clinical_tacs', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${hw.clinicalTacsMaxMilliamps} mA`, 'hardware',
          t('VALIDATE_MSG_CLINICAL_TACS_INTENSITYMILLIAMPS', { 0: p.params.intensityMilliamps, 1: hw.clinicalTacsMaxMilliamps })
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'clinical_tacs', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          t('VALIDATE_MSG_CLINICAL_TACS_INTENSITYMILLIAMPS_2', { 0: p.params.intensityMilliamps, 1: l.maxIntensityMilliamps })
        ));
      }
      break;
    }

    case 'hd_tdcs': {
      const l = lim.hdTdcs;
      // Hardware
      if (p.params.intensityMilliamps > hw.hdTdcsMaxMilliampsPerElectrode) {
        issues.push(issue(
          'error', 'hd_tdcs', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${hw.hdTdcsMaxMilliampsPerElectrode} mA`, 'hardware',
          t('VALIDATE_MSG_HD_TDCS_INTENSITYMILLIAMPS', { 0: p.params.intensityMilliamps, 1: hw.hdTdcsMaxMilliampsPerElectrode })
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'hd_tdcs', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          t('VALIDATE_MSG_HD_TDCS_INTENSITYMILLIAMPS_2', { 0: p.params.intensityMilliamps, 1: l.maxIntensityMilliamps })
        ));
      }
      if (l?.allowedMontages != null && !l.allowedMontages.includes(p.params.montage)) {
        issues.push(issue(
          'error', 'hd_tdcs', 'montage', t('VALIDATE_PARAM_MONTAGE'),
          p.params.montage, l.allowedMontages.join('/'), 'global',
          t('VALIDATE_MSG_HD_TDCS_MONTAGE', { 0: p.params.montage, 1: l.allowedMontages.join(', ') })
        ));
      }
      break;
    }

    case 'cervical_vns': {
      const l = lim.cervicalVns;
      // Hardware — cardiac interlock always active at firmware level
      if (p.params.intensityMilliamps > hw.cervicalVnsMaxMilliamps) {
        issues.push(issue(
          'error', 'cervical_vns', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${hw.cervicalVnsMaxMilliamps} mA`, 'hardware',
          t('VALIDATE_MSG_CERVICAL_VNS_INTENSITYMILLIAMPS', { 0: p.params.intensityMilliamps })
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'cervical_vns', 'intensityMilliamps', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          t('VALIDATE_MSG_CERVICAL_VNS_INTENSITYMILLIAMPS_2', { 0: p.params.intensityMilliamps, 1: l.maxIntensityMilliamps })
        ));
      }
      if (l?.maxSessionDurationSeconds != null) {
        issues.push(issue(
          'warning', 'cervical_vns', 'sessionDuration', t('VALIDATE_PARAM_SESSION_DURATION'),
          'Configured', `Max ${l.maxSessionDurationSeconds}s`, 'global',
          t('VALIDATE_MSG_CERVICAL_VNS_SESSIONDURATION', { 0: l.maxSessionDurationSeconds })
        ));
      }
      break;
    }

    case 'vibrotactile_40hz': {
      const l = lim.vibrotactile40hz;
      // Hardware
      if (p.params.intensityG > hw.vibrotactileMaxG || p.params.intensityG < hw.vibrotactileMinG) {
        issues.push(issue(
          'error', 'vibrotactile_40hz', 'intensityG', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityG} G`,
          `${hw.vibrotactileMinG}–${hw.vibrotactileMaxG} G`, 'hardware',
          t('VALIDATE_MSG_VIBROTACTILE_40HZ_INTENSITYG', { 0: p.params.intensityG, 1: hw.vibrotactileMinG, 2: hw.vibrotactileMaxG })
        ));
      }
      // Dosage
      if (l?.maxIntensityG != null && p.params.intensityG > l.maxIntensityG) {
        issues.push(issue(
          'error', 'vibrotactile_40hz', 'intensityG', t('VALIDATE_PARAM_INTENSITY'),
          `${p.params.intensityG} G`, `${l.maxIntensityG} G`, 'global',
          t('VALIDATE_MSG_VIBROTACTILE_40HZ_INTENSITYG_2', { 0: p.params.intensityG, 1: l.maxIntensityG })
        ));
      }
      break;
    }

    default: {
      // Exhaustive check — TypeScript will warn if a case is missing
      const _exhaustive: never = p;
      void _exhaustive;
      break;
    }
  }
}

// ─── Protocol-level validation ─────────────────────────────────────────────────

export function validateProtocol(
  definition: NPProtocolDefinition,
  resolvedLimits: NPLimitsSet
): NPValidationResult {
  const issues: NPValidationIssue[] = [];
  const enabled = definition.modalities.filter(m => m.enabled);

  // Protocol-level checks
  if (enabled.length === 0) {
    issues.push(issue(
      'error', undefined, 'modalities', t('VALIDATE_PARAM_MODALITIES'),
      '0', '≥1', 'hardware',
      t('VALIDATE_MSG_GENERAL_MODALITIES')
    ));
  }

  if (definition.timingMode.type === 'duration') {
    const dur = definition.timingMode.seconds;
    if (dur < 60) {
      issues.push(issue(
        'warning', undefined, 'duration', t('VALIDATE_PARAM_DURATION'),
        `${dur}s`, '60s', 'hardware',
        t('VALIDATE_MSG_GENERAL_DURATION')
      ));
    }
    if (dur > 7200) {
      issues.push(issue(
        'warning', undefined, 'duration', t('VALIDATE_PARAM_DURATION'),
        `${Math.floor(dur / 60)}m`, '120m', 'hardware',
        t('VALIDATE_MSG_GENERAL_DURATION_2')
      ));
    }
  }

  // Cross-modality checks
  const hasBES = enabled.some(m => m.modalityParams.type === 'bes_tacs');
  const hasTDCS = enabled.some(m => m.modalityParams.type === 'tdcs');
  if (hasBES && hasTDCS) {
    issues.push(issue(
      'warning', undefined, 'cross_modality', t('VALIDATE_PARAM_CROSS_MODALITY'),
      'BES + tDCS', 'Separate electrode paths', 'hardware',
      t('VALIDATE_MSG_GENERAL_CROSS_MODALITY')
    ));
  }

  // Check TMS with stimulation modalities
  const hasTMS = enabled.some(m => m.modalityParams.type === 'tms');
  const hasClinicalTacs = enabled.some(m => m.modalityParams.type === 'clinical_tacs');
  if (hasTMS && (hasBES || hasTDCS || hasClinicalTacs)) {
    issues.push(issue(
      'warning', undefined, 'cross_modality_tms', t('VALIDATE_PARAM_CROSS_MODALITY'),
      'TMS + electrical stim', 'Sequential recommended', 'hardware',
      t('VALIDATE_MSG_GENERAL_CROSS_MODALITY_TMS')
    ));
  }

  // Per-modality validation
  for (const block of enabled) {
    validateModality(block, resolvedLimits, issues);
  }

  return makeResult(issues);
}

// ─── Entry-level validation ────────────────────────────────────────────────────

export function validateEntry(
  entry: NPProtocolEntry,
  resolvedLimits: NPLimitsSet,
  allProtocols?: NPProtocolEntry[]
): NPValidationResult {
  if (entry.kind === 'single') {
    return validateProtocol(entry.protocol, resolvedLimits);
  }

  if (entry.kind === 'composite') {
    const issues: NPValidationIssue[] = [];

    if (entry.composite.layers.length === 0) {
      issues.push(issue(
        'error', undefined, 'layers', t('VALIDATE_PARAM_LAYERS'),
        '0', '≥1', 'hardware',
        t('VALIDATE_MSG_GENERAL_LAYERS')
      ));
    }

    for (const layer of entry.composite.layers) {
      const ref = allProtocols?.find(
        p => p.kind === 'single' && p.protocol.name === layer.protocolName
      );
      if (!ref && allProtocols) {
        issues.push(issue(
          'error', undefined, 'layer_ref', t('VALIDATE_PARAM_LAYER_REFERENCE'),
          layer.protocolName, 'Known protocol', 'hardware',
          t('VALIDATE_MSG_GENERAL_LAYER_REF', { 0: layer.protocolName })
        ));
      } else if (ref && ref.kind === 'single') {
        const sub = validateProtocol(ref.protocol, resolvedLimits);
        issues.push(...sub.issues.map(i => ({
          ...i,
          id: crypto.randomUUID(),
          message: t('VALIDATE_LAYER_PREFIX', { 0: layer.protocolName, 1: i.message }),
        })));
      }

      if (layer.intensityScale < 0 || layer.intensityScale > 1) {
        issues.push(issue(
          'error', undefined, 'layer_intensity_scale', t('VALIDATE_PARAM_LAYER_INTENSITY_SCALE'),
          `${layer.intensityScale}`, '0–1', 'hardware',
          t('VALIDATE_MSG_GENERAL_LAYER_INTENSITY_SCALE', { 0: layer.protocolName, 1: layer.intensityScale })
        ));
      }
    }

    return makeResult(issues);
  }

  return makeResult([]);
}
