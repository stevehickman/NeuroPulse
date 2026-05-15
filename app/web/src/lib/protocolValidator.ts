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
    limitValueDescription: `${limitValueDescription} (${limitSource})`,
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
          'error', 'pbm_transcranial', 'dutyCyclePercent', 'Duty Cycle',
          `${p.params.dutyCyclePercent}%`, `${hw.pbmDutyCycleMaxPercent}%`, 'hardware',
          `PBM duty cycle ${p.params.dutyCyclePercent}% exceeds firmware maximum of ${hw.pbmDutyCycleMaxPercent}%.`
        ));
      }
      if (p.params.frequencyHz < 0) {
        issues.push(issue(
          'error', 'pbm_transcranial', 'frequencyHz', 'Frequency',
          `${p.params.frequencyHz} Hz`, '≥0 Hz', 'hardware',
          'PBM frequency cannot be negative.'
        ));
      }
      // Dosage limits
      if (l?.maxIntensityPercent != null && p.params.intensityPercent > l.maxIntensityPercent) {
        issues.push(issue(
          'error', 'pbm_transcranial', 'intensityPercent', 'Intensity',
          `${p.params.intensityPercent}%`, `${l.maxIntensityPercent}%`, 'global',
          `PBM intensity ${p.params.intensityPercent}% exceeds limit of ${l.maxIntensityPercent}%.`
        ));
      }
      if (l?.maxFrequencyHz != null && p.params.frequencyHz > l.maxFrequencyHz) {
        issues.push(issue(
          'error', 'pbm_transcranial', 'frequencyHz', 'Frequency',
          `${p.params.frequencyHz} Hz`, `${l.maxFrequencyHz} Hz`, 'global',
          `PBM frequency ${p.params.frequencyHz} Hz exceeds limit of ${l.maxFrequencyHz} Hz.`
        ));
      }
      if (l?.maxDutyCyclePercent != null && p.params.dutyCyclePercent > l.maxDutyCyclePercent) {
        issues.push(issue(
          'error', 'pbm_transcranial', 'dutyCyclePercent', 'Duty Cycle',
          `${p.params.dutyCyclePercent}%`, `${l.maxDutyCyclePercent}%`, 'global',
          `PBM duty cycle ${p.params.dutyCyclePercent}% exceeds configured limit of ${l.maxDutyCyclePercent}%.`
        ));
      }
      break;
    }

    case 'pbm_intranasal': {
      const l = lim.pbmIntranasal;
      // Hardware
      if (p.params.dutyCyclePercent > hw.pbmDutyCycleMaxPercent) {
        issues.push(issue(
          'error', 'pbm_intranasal', 'dutyCyclePercent', 'Duty Cycle',
          `${p.params.dutyCyclePercent}%`, `${hw.pbmDutyCycleMaxPercent}%`, 'hardware',
          `Intranasal PBM duty cycle ${p.params.dutyCyclePercent}% exceeds firmware maximum of ${hw.pbmDutyCycleMaxPercent}%.`
        ));
      }
      // Dosage
      if (l?.maxIntensityPercent != null && p.params.intensityPercent > l.maxIntensityPercent) {
        issues.push(issue(
          'error', 'pbm_intranasal', 'intensityPercent', 'Intensity',
          `${p.params.intensityPercent}%`, `${l.maxIntensityPercent}%`, 'global',
          `Intranasal PBM intensity ${p.params.intensityPercent}% exceeds limit of ${l.maxIntensityPercent}%.`
        ));
      }
      break;
    }

    case 'eeg_neurofeedback': {
      const l = lim.eegNeurofeedback;
      if (l?.requireClosedLoop && !p.params.closedLoopEnabled) {
        issues.push(issue(
          'error', 'eeg_neurofeedback', 'closedLoopEnabled', 'Closed Loop',
          'Disabled', 'Required', 'global',
          'Closed-loop EEG adaptation is required by the active limits configuration.'
        ));
      }
      if (l?.allowedBands != null && !l.allowedBands.includes(p.params.band)) {
        issues.push(issue(
          'error', 'eeg_neurofeedback', 'band', 'Band',
          p.params.band, l.allowedBands.join('/'), 'global',
          `EEG band '${p.params.band}' is not in the allowed list: ${l.allowedBands.join(', ')}.`
        ));
      }
      break;
    }

    case 'bes_tacs': {
      const l = lim.besTacs;
      // Hardware
      if (p.params.intensityMilliamps > hw.besTacsMaxMilliamps) {
        issues.push(issue(
          'error', 'bes_tacs', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${hw.besTacsMaxMilliamps} mA`, 'hardware',
          `BES intensity ${p.params.intensityMilliamps} mA exceeds hardware maximum of ${hw.besTacsMaxMilliamps} mA.`
        ));
      }
      if (p.params.frequencyHz < hw.besTacsMinHz || p.params.frequencyHz > hw.besTacsMaxHz) {
        issues.push(issue(
          'error', 'bes_tacs', 'frequencyHz', 'Frequency',
          `${p.params.frequencyHz} Hz`, `${hw.besTacsMinHz}–${hw.besTacsMaxHz} Hz`, 'hardware',
          `BES frequency ${p.params.frequencyHz} Hz is outside hardware range ${hw.besTacsMinHz}–${hw.besTacsMaxHz} Hz.`
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'bes_tacs', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          `BES intensity ${p.params.intensityMilliamps} mA exceeds limit of ${l.maxIntensityMilliamps} mA.`
        ));
      }
      if (l?.maxFrequencyHz != null && p.params.frequencyHz > l.maxFrequencyHz) {
        issues.push(issue(
          'error', 'bes_tacs', 'frequencyHz', 'Frequency',
          `${p.params.frequencyHz} Hz`, `${l.maxFrequencyHz} Hz`, 'global',
          `BES frequency ${p.params.frequencyHz} Hz exceeds limit of ${l.maxFrequencyHz} Hz.`
        ));
      }
      if (l?.minFrequencyHz != null && p.params.frequencyHz < l.minFrequencyHz) {
        issues.push(issue(
          'error', 'bes_tacs', 'frequencyHz', 'Frequency',
          `${p.params.frequencyHz} Hz`, `≥${l.minFrequencyHz} Hz`, 'global',
          `BES frequency ${p.params.frequencyHz} Hz is below limit of ${l.minFrequencyHz} Hz.`
        ));
      }
      break;
    }

    case 'tdcs': {
      const l = lim.tdcs;
      // Hardware
      if (p.params.intensityMilliamps < hw.tdcsMinMilliamps || p.params.intensityMilliamps > hw.tdcsMaxMilliamps) {
        issues.push(issue(
          'error', 'tdcs', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${hw.tdcsMinMilliamps}–${hw.tdcsMaxMilliamps} mA`, 'hardware',
          `tDCS intensity ${p.params.intensityMilliamps} mA is outside hardware range ${hw.tdcsMinMilliamps}–${hw.tdcsMaxMilliamps} mA.`
        ));
      }
      if (p.params.electrodePairs.length > hw.tdcsMaxElectrodePairs) {
        issues.push(issue(
          'error', 'tdcs', 'electrodePairs', 'Electrode Pairs',
          `${p.params.electrodePairs.length}`, `≤${hw.tdcsMaxElectrodePairs}`, 'hardware',
          `tDCS has ${p.params.electrodePairs.length} electrode pairs; hardware maximum is ${hw.tdcsMaxElectrodePairs}.`
        ));
      }
      if (p.params.rampSeconds < hw.tdcsRampSeconds) {
        issues.push(issue(
          'error', 'tdcs', 'rampSeconds', 'Ramp Time',
          `${p.params.rampSeconds}s`, `${hw.tdcsRampSeconds}s`, 'hardware',
          `tDCS ramp ${p.params.rampSeconds}s is below the hardware-enforced minimum of ${hw.tdcsRampSeconds}s.`
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'tdcs', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          `tDCS intensity ${p.params.intensityMilliamps} mA exceeds limit of ${l.maxIntensityMilliamps} mA.`
        ));
      }
      break;
    }

    case 'vns_hrv': {
      const l = lim.vnsHrv;
      // Hardware
      if (p.params.intensityMilliamps > hw.vnsMaxMilliamps) {
        issues.push(issue(
          'error', 'vns_hrv', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${hw.vnsMaxMilliamps} mA`, 'hardware',
          `VNS intensity ${p.params.intensityMilliamps} mA exceeds hardware maximum of ${hw.vnsMaxMilliamps} mA.`
        ));
      }
      if (p.params.frequencyHz < hw.vnsMinHz || p.params.frequencyHz > hw.vnsMaxHz) {
        issues.push(issue(
          'error', 'vns_hrv', 'frequencyHz', 'Frequency',
          `${p.params.frequencyHz} Hz`, `${hw.vnsMinHz}–${hw.vnsMaxHz} Hz`, 'hardware',
          `VNS frequency ${p.params.frequencyHz} Hz is outside hardware range ${hw.vnsMinHz}–${hw.vnsMaxHz} Hz.`
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'vns_hrv', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          `VNS intensity ${p.params.intensityMilliamps} mA exceeds limit of ${l.maxIntensityMilliamps} mA.`
        ));
      }
      if (l?.maxFrequencyHz != null && p.params.frequencyHz > l.maxFrequencyHz) {
        issues.push(issue(
          'error', 'vns_hrv', 'frequencyHz', 'Frequency',
          `${p.params.frequencyHz} Hz`, `${l.maxFrequencyHz} Hz`, 'global',
          `VNS frequency ${p.params.frequencyHz} Hz exceeds limit of ${l.maxFrequencyHz} Hz.`
        ));
      }
      if (l?.allowedProtocols != null && !l.allowedProtocols.includes(p.params.hrvProtocol)) {
        issues.push(issue(
          'error', 'vns_hrv', 'hrvProtocol', 'HRV Protocol',
          p.params.hrvProtocol, l.allowedProtocols.join('/'), 'global',
          `HRV protocol '${p.params.hrvProtocol}' is not in the allowed list: ${l.allowedProtocols.join(', ')}.`
        ));
      }
      break;
    }

    case 'audio_entrainment': {
      const l = lim.audioEntrainment;
      if (l?.maxVolumePercent != null && p.params.volumePercent > l.maxVolumePercent) {
        issues.push(issue(
          'error', 'audio_entrainment', 'volumePercent', 'Volume',
          `${p.params.volumePercent}%`, `${l.maxVolumePercent}%`, 'global',
          `Audio volume ${p.params.volumePercent}% exceeds limit of ${l.maxVolumePercent}%.`
        ));
      }
      if (
        l?.maxBinauralBeatsHz != null &&
        p.params.binauralBeatsHz != null &&
        p.params.binauralBeatsHz > l.maxBinauralBeatsHz
      ) {
        issues.push(issue(
          'error', 'audio_entrainment', 'binauralBeatsHz', 'Binaural Beat',
          `${p.params.binauralBeatsHz} Hz`, `${l.maxBinauralBeatsHz} Hz`, 'global',
          `Binaural beat ${p.params.binauralBeatsHz} Hz exceeds limit of ${l.maxBinauralBeatsHz} Hz.`
        ));
      }
      if (
        l?.maxIsochronicTonesHz != null &&
        p.params.isochronicTonesHz != null &&
        p.params.isochronicTonesHz > l.maxIsochronicTonesHz
      ) {
        issues.push(issue(
          'error', 'audio_entrainment', 'isochronicTonesHz', 'Isochronic Tone',
          `${p.params.isochronicTonesHz} Hz`, `${l.maxIsochronicTonesHz} Hz`, 'global',
          `Isochronic tone ${p.params.isochronicTonesHz} Hz exceeds limit of ${l.maxIsochronicTonesHz} Hz.`
        ));
      }
      break;
    }

    case 'visual_stimulation': {
      const l = lim.visualStimulation;
      // Hardware ceiling
      if (p.params.frequencyHz > hw.visualMaxHz) {
        issues.push(issue(
          'error', 'visual_stimulation', 'frequencyHz', 'Frequency',
          `${p.params.frequencyHz} Hz`, `${hw.visualMaxHz} Hz`, 'hardware',
          `Visual frequency ${p.params.frequencyHz} Hz exceeds hardware IEC 62471 MPE ceiling of ${hw.visualMaxHz} Hz.`
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
            'error', 'visual_stimulation', 'frequencyHz', 'Frequency',
            `${p.params.frequencyHz} Hz`,
            `Outside ${hw.visualHighRiskMinHz}–${hw.visualHighRiskMaxHz} Hz`,
            'global',
            `Visual frequency ${p.params.frequencyHz} Hz is in the photoparoxysmal high-risk zone (${hw.visualHighRiskMinHz}–${hw.visualHighRiskMaxHz} Hz). Blocked by configured limit. Clinician unlock required per device firmware.`
          ));
        } else {
          issues.push(issue(
            'warning', 'visual_stimulation', 'frequencyHz', 'Frequency',
            `${p.params.frequencyHz} Hz`,
            `Outside ${hw.visualHighRiskMinHz}–${hw.visualHighRiskMaxHz} Hz`,
            'hardware',
            `Visual frequency ${p.params.frequencyHz} Hz is in the photoparoxysmal high-risk zone (${hw.visualHighRiskMinHz}–${hw.visualHighRiskMaxHz} Hz). Device firmware requires clinician unlock.`
          ));
        }
      }
      // Dosage limits
      if (l?.maxFrequencyHz != null && p.params.frequencyHz > l.maxFrequencyHz) {
        issues.push(issue(
          'error', 'visual_stimulation', 'frequencyHz', 'Frequency',
          `${p.params.frequencyHz} Hz`, `${l.maxFrequencyHz} Hz`, 'global',
          `Visual frequency ${p.params.frequencyHz} Hz exceeds limit of ${l.maxFrequencyHz} Hz.`
        ));
      }
      if (l?.minFrequencyHz != null && p.params.frequencyHz < l.minFrequencyHz) {
        issues.push(issue(
          'error', 'visual_stimulation', 'frequencyHz', 'Frequency',
          `${p.params.frequencyHz} Hz`, `≥${l.minFrequencyHz} Hz`, 'global',
          `Visual frequency ${p.params.frequencyHz} Hz is below limit of ${l.minFrequencyHz} Hz.`
        ));
      }
      if (l?.allowedModes != null && !l.allowedModes.includes(p.params.mode)) {
        issues.push(issue(
          'error', 'visual_stimulation', 'mode', 'Mode',
          p.params.mode, l.allowedModes.join('/'), 'global',
          `Visual mode '${p.params.mode}' is not in the allowed list: ${l.allowedModes.join(', ')}.`
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
          'error', 'tms', 'intensityPercentMT', 'Intensity',
          `${p.params.intensityPercentMT}% MT`, `${l.maxIntensityPercentMT}% MT`, 'global',
          `TMS intensity ${p.params.intensityPercentMT}% MT exceeds limit of ${l.maxIntensityPercentMT}% MT.`
        ));
      }
      if (l?.maxPulsesPerSession != null && p.params.pulseCount > l.maxPulsesPerSession) {
        issues.push(issue(
          'error', 'tms', 'pulseCount', 'Pulses',
          `${p.params.pulseCount}`, `${l.maxPulsesPerSession}`, 'global',
          `TMS pulse count ${p.params.pulseCount} exceeds limit of ${l.maxPulsesPerSession} per session.`
        ));
      }
      if (l?.allowedProtocols != null && !l.allowedProtocols.includes(p.params.tmsProtocol)) {
        issues.push(issue(
          'error', 'tms', 'tmsProtocol', 'Protocol',
          p.params.tmsProtocol, l.allowedProtocols.join('/'), 'global',
          `TMS protocol '${p.params.tmsProtocol}' is not in the allowed list: ${l.allowedProtocols.join(', ')}.`
        ));
      }
      if (l?.allowedTargets != null && !l.allowedTargets.includes(p.params.target)) {
        issues.push(issue(
          'error', 'tms', 'target', 'Target',
          p.params.target, l.allowedTargets.join('/'), 'global',
          `TMS target '${p.params.target}' is not in the allowed list: ${l.allowedTargets.join(', ')}.`
        ));
      }
      break;
    }

    case 'pbm_deep_1170nm': {
      const l = lim.pbmDeep1170nm;
      if (l?.maxIntensityMWcm2 != null && p.params.intensityMWcm2 > l.maxIntensityMWcm2) {
        issues.push(issue(
          'error', 'pbm_deep_1170nm', 'intensityMWcm2', 'Intensity',
          `${p.params.intensityMWcm2} mW/cm²`, `${l.maxIntensityMWcm2} mW/cm²`, 'global',
          `Deep PBM intensity ${p.params.intensityMWcm2} mW/cm² exceeds limit of ${l.maxIntensityMWcm2} mW/cm².`
        ));
      }
      if (p.params.dutyCyclePercent > hw.pbmDutyCycleMaxPercent) {
        issues.push(issue(
          'error', 'pbm_deep_1170nm', 'dutyCyclePercent', 'Duty Cycle',
          `${p.params.dutyCyclePercent}%`, `${hw.pbmDutyCycleMaxPercent}%`, 'hardware',
          `Deep PBM duty cycle ${p.params.dutyCyclePercent}% exceeds firmware maximum of ${hw.pbmDutyCycleMaxPercent}%.`
        ));
      }
      break;
    }

    case 'clinical_tacs': {
      const l = lim.clinicalTacs;
      // Hardware
      if (p.params.intensityMilliamps > hw.clinicalTacsMaxMilliamps) {
        issues.push(issue(
          'error', 'clinical_tacs', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${hw.clinicalTacsMaxMilliamps} mA`, 'hardware',
          `Clinical tACS intensity ${p.params.intensityMilliamps} mA exceeds hardware maximum of ${hw.clinicalTacsMaxMilliamps} mA.`
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'clinical_tacs', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          `Clinical tACS intensity ${p.params.intensityMilliamps} mA exceeds limit of ${l.maxIntensityMilliamps} mA.`
        ));
      }
      break;
    }

    case 'hd_tdcs': {
      const l = lim.hdTdcs;
      // Hardware
      if (p.params.intensityMilliamps > hw.hdTdcsMaxMilliampsPerElectrode) {
        issues.push(issue(
          'error', 'hd_tdcs', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${hw.hdTdcsMaxMilliampsPerElectrode} mA`, 'hardware',
          `HD-tDCS intensity ${p.params.intensityMilliamps} mA/electrode exceeds Bikson lab safety limit of ${hw.hdTdcsMaxMilliampsPerElectrode} mA.`
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'hd_tdcs', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          `HD-tDCS intensity ${p.params.intensityMilliamps} mA exceeds limit of ${l.maxIntensityMilliamps} mA.`
        ));
      }
      if (l?.allowedMontages != null && !l.allowedMontages.includes(p.params.montage)) {
        issues.push(issue(
          'error', 'hd_tdcs', 'montage', 'Montage',
          p.params.montage, l.allowedMontages.join('/'), 'global',
          `HD-tDCS montage '${p.params.montage}' is not in the allowed list: ${l.allowedMontages.join(', ')}.`
        ));
      }
      break;
    }

    case 'cervical_vns': {
      const l = lim.cervicalVns;
      // Hardware — cardiac interlock always active at firmware level
      if (p.params.intensityMilliamps > hw.cervicalVnsMaxMilliamps) {
        issues.push(issue(
          'error', 'cervical_vns', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${hw.cervicalVnsMaxMilliamps} mA`, 'hardware',
          `Cervical VNS intensity ${p.params.intensityMilliamps} mA exceeds hardware maximum. Cardiac interlock is always active regardless.`
        ));
      }
      // Dosage
      if (l?.maxIntensityMilliamps != null && p.params.intensityMilliamps > l.maxIntensityMilliamps) {
        issues.push(issue(
          'error', 'cervical_vns', 'intensityMilliamps', 'Intensity',
          `${p.params.intensityMilliamps} mA`, `${l.maxIntensityMilliamps} mA`, 'global',
          `Cervical VNS intensity ${p.params.intensityMilliamps} mA exceeds limit of ${l.maxIntensityMilliamps} mA.`
        ));
      }
      if (l?.maxSessionDurationSeconds != null) {
        issues.push(issue(
          'warning', 'cervical_vns', 'sessionDuration', 'Session Duration',
          'Configured', `Max ${l.maxSessionDurationSeconds}s`, 'global',
          `Cervical VNS session duration limit is ${l.maxSessionDurationSeconds}s. Verify session timing matches this limit.`
        ));
      }
      break;
    }

    case 'vibrotactile_40hz': {
      const l = lim.vibrotactile40hz;
      // Hardware
      if (p.params.intensityG > hw.vibrotactileMaxG || p.params.intensityG < hw.vibrotactileMinG) {
        issues.push(issue(
          'error', 'vibrotactile_40hz', 'intensityG', 'Intensity',
          `${p.params.intensityG} G`,
          `${hw.vibrotactileMinG}–${hw.vibrotactileMaxG} G`, 'hardware',
          `Vibrotactile intensity ${p.params.intensityG} G is outside hardware range ${hw.vibrotactileMinG}–${hw.vibrotactileMaxG} G.`
        ));
      }
      // Dosage
      if (l?.maxIntensityG != null && p.params.intensityG > l.maxIntensityG) {
        issues.push(issue(
          'error', 'vibrotactile_40hz', 'intensityG', 'Intensity',
          `${p.params.intensityG} G`, `${l.maxIntensityG} G`, 'global',
          `Vibrotactile intensity ${p.params.intensityG} G exceeds limit of ${l.maxIntensityG} G.`
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
      'error', undefined, 'modalities', 'Modalities',
      '0', '≥1', 'hardware',
      'Protocol has no enabled modalities.'
    ));
  }

  if (definition.timingMode.type === 'duration') {
    const dur = definition.timingMode.seconds;
    if (dur < 60) {
      issues.push(issue(
        'warning', undefined, 'duration', 'Duration',
        `${dur}s`, '60s', 'hardware',
        'Session is very short (< 1 minute). Verify this is intentional.'
      ));
    }
    if (dur > 7200) {
      issues.push(issue(
        'warning', undefined, 'duration', 'Duration',
        `${Math.floor(dur / 60)}m`, '120m', 'hardware',
        'Session is very long (> 2 hours). Verify this is intentional.'
      ));
    }
  }

  // Cross-modality checks
  const hasBES = enabled.some(m => m.modalityParams.type === 'bes_tacs');
  const hasTDCS = enabled.some(m => m.modalityParams.type === 'tdcs');
  if (hasBES && hasTDCS) {
    issues.push(issue(
      'warning', undefined, 'cross_modality', 'Cross-modality',
      'BES + tDCS', 'Separate electrode paths', 'hardware',
      'BES and tDCS are active simultaneously — ensure electrode paths are non-overlapping.'
    ));
  }

  // Check TMS with stimulation modalities
  const hasTMS = enabled.some(m => m.modalityParams.type === 'tms');
  const hasClinicalTacs = enabled.some(m => m.modalityParams.type === 'clinical_tacs');
  if (hasTMS && (hasBES || hasTDCS || hasClinicalTacs)) {
    issues.push(issue(
      'warning', undefined, 'cross_modality_tms', 'Cross-modality',
      'TMS + electrical stim', 'Sequential recommended', 'hardware',
      'TMS combined with electrical stimulation (BES/tDCS/clinical tACS) requires careful electrode placement verification.'
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
        'error', undefined, 'layers', 'Layers',
        '0', '≥1', 'hardware',
        'Composite protocol has no layers.'
      ));
    }

    for (const layer of entry.composite.layers) {
      const ref = allProtocols?.find(
        p => p.kind === 'single' && p.protocol.name === layer.protocolName
      );
      if (!ref && allProtocols) {
        issues.push(issue(
          'error', undefined, 'layer_ref', 'Layer Reference',
          layer.protocolName, 'Known protocol', 'hardware',
          `Layer references unknown protocol '${layer.protocolName}'.`
        ));
      } else if (ref && ref.kind === 'single') {
        const sub = validateProtocol(ref.protocol, resolvedLimits);
        issues.push(...sub.issues.map(i => ({
          ...i,
          id: crypto.randomUUID(),
          message: `[${layer.protocolName}] ${i.message}`,
        })));
      }

      if (layer.intensityScale < 0 || layer.intensityScale > 1) {
        issues.push(issue(
          'error', undefined, 'layer_intensity_scale', 'Layer Intensity Scale',
          `${layer.intensityScale}`, '0–1', 'hardware',
          `Layer '${layer.protocolName}' has intensity scale ${layer.intensityScale} outside valid range 0–1.`
        ));
      }
    }

    return makeResult(issues);
  }

  return makeResult([]);
}
