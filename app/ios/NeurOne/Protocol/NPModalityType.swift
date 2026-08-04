import Foundation
import SwiftUI

// MARK: - Device Tier

enum NPModalityTier: String, Codable, Equatable {
    case t1
    case t2
    case accessory
}

// MARK: - NPModalityType

enum NPModalityType: String, CaseIterable, Codable, Identifiable {

    // T1 — 8 modalities
    case pbmTranscranial    = "pbm_transcranial"
    case pbmIntranasal      = "pbm_intranasal"
    case eegNeurofeedback   = "eeg_neurofeedback"
    case besTacs            = "bes_tacs"
    case tdcs               = "tdcs"
    case vnsHRV             = "vns_hrv"
    case audioEntrainment   = "audio_entrainment"
    case visualStimulation  = "visual_stimulation"

    // T2 additions
    case qeeg21ch           = "qeeg_21ch"
    case tms                = "tms"
    case pbmDeep1170nm      = "pbm_deep_1170nm"
    case clinicalTacs       = "clinical_tacs"
    case hdTdcs             = "hd_tdcs"
    case cervicalVns        = "cervical_vns"

    // Accessory
    case vibrotactile40hz   = "vibrotactile_40hz"

    // MARK: Identifiable
    var id: String { rawValue }

    // MARK: Display name
    var displayName: String {
        switch self {
        case .pbmTranscranial:   return "Transcranial PBM"
        case .pbmIntranasal:     return "Intranasal PBM"
        case .eegNeurofeedback:  return "EEG Neurofeedback"
        case .besTacs:           return "Brainwave Entrainment Stimulation"
        case .tdcs:              return "Cortical Priming Stimulation"
        case .vnsHRV:            return "VNS + HRV Biofeedback"
        case .audioEntrainment:  return "Neural Audio Entrainment"
        case .visualStimulation: return "Visual Stimulation"
        case .qeeg21ch:          return "21-Channel qEEG"
        case .tms:               return "TMS"
        case .pbmDeep1170nm:     return "Deep PBM (1170nm)"
        case .clinicalTacs:      return "Clinical tACS"
        case .hdTdcs:            return "sLORETA-Guided HD-tDCS"
        case .cervicalVns:       return "Cervical VNS"
        case .vibrotactile40hz:  return "40Hz Vibrotactile"
        }
    }

    // MARK: Consumer-facing regulatory-safe name
    var consumerName: String {
        switch self {
        case .pbmTranscranial:   return "Transcranial Photobiomodulation"
        case .pbmIntranasal:     return "Intranasal Photobiomodulation"
        case .eegNeurofeedback:  return "EEG Neurofeedback"
        case .besTacs:           return "Brainwave Entrainment Stimulation"
        case .tdcs:              return "Cortical Priming Stimulation"
        case .vnsHRV:            return "VNS + HRV Biofeedback"
        case .audioEntrainment:  return "Neural Audio Entrainment"
        case .visualStimulation: return "Visual Stimulation"
        case .qeeg21ch:          return "Clinical EEG (21-Channel)"
        case .tms:               return "Transcranial Magnetic Stimulation"
        case .pbmDeep1170nm:     return "Deep Photobiomodulation (1170nm)"
        case .clinicalTacs:      return "Clinical Alternating Stimulation"
        case .hdTdcs:            return "Precision-Targeted Cortical Priming"
        case .cervicalVns:       return "Cervical Vagus Nerve Stimulation"
        case .vibrotactile40hz:  return "40Hz Vibrotactile Entrainment"
        }
    }

    // MARK: Short description
    var shortDescription: String {
        switch self {
        case .pbmTranscranial:
            return "660nm + 810nm LED arrays tiled across the socket lattice; zone-targeted;"
                + " pulsed or CW; real-time J/cm² dose metering."
        case .pbmIntranasal:
            return "Bilateral Y-probe; 660nm + 810nm; depth-stop rings; authenticated hygiene sleeves."
        case .eegNeurofeedback:
            return "8-channel 24-bit semi-dry EEG; closed-loop frequency adaptation; 500 Hz sample rate."
        case .besTacs:
            return "0.5–40Hz charge-balanced biphasic; ≤1mA; per-electrode impedance monitoring."
        case .tdcs:
            return "0.1–2mA DC; 30s hardware-enforced ramp; 40µC/cm² safety MCU limit."
        case .vnsHRV:
            return "Auricular clip CN X; PPG HRV; resonance breathing pacer; four evidence-based protocols."
        case .audioEntrainment:
            return "Binaural beats + isochronic tones; over-ear planar + bone conduction; EEG-adaptive."
        case .visualStimulation:
            return "108 micro-LEDs/lens; binocular, EMDR, retinal PBM (Mode F); 0.5–100Hz."
        case .qeeg21ch:
            return "Full 10-20 + FC3/FC4 + Oz + A1/A2; wet gel; sLORETA source imaging."
        case .tms:
            return "Focal figure-8 coil; 0.1–0.5T; rTMS + TBS; TMS-gated EMF cancellation."
        case .pbmDeep1170nm:
            return "Laser diodes; 35–40mm subcortical depth; TEC stabilized; ≤1,000mW/cm²."
        case .clinicalTacs:
            return "≤4mA; 16-channel arbitrary waveform; clinical-grade precision."
        case .hdTdcs:
            return "4×1 ring montage; sLORETA-guided targeting; ~3–5× focality vs standard tDCS."
        case .cervicalVns:
            return "Neck-worn tcVNS; carotid sheath; cardiac rhythm interlock via safety MCU."
        case .vibrotactile40hz:
            return "Mastoid LRA pad; 40Hz ± 0.5Hz locked; 0.6–1.2G; hub-powered."
        }
    }

    // MARK: SF Symbol
    var systemImage: String {
        switch self {
        case .pbmTranscranial:   return "sun.max.fill"
        case .pbmIntranasal:     return "nose"
        case .eegNeurofeedback:  return "brain.head.profile"
        case .besTacs:           return "waveform.path"
        case .tdcs:              return "bolt.fill"
        case .vnsHRV:            return "ear.fill"
        case .audioEntrainment:  return "music.note"
        case .visualStimulation: return "eye.fill"
        case .qeeg21ch:          return "brain"
        case .tms:               return "magnet.fill"
        case .pbmDeep1170nm:     return "laser.burst"
        case .clinicalTacs:      return "waveform"
        case .hdTdcs:            return "scope"
        case .cervicalVns:       return "person.bust"
        case .vibrotactile40hz:  return "waveform.and.magnifyingglass"
        }
    }

    // MARK: Tier
    var tier: NPModalityTier {
        switch self {
        case .pbmTranscranial, .pbmIntranasal, .eegNeurofeedback,
             .besTacs, .tdcs, .vnsHRV, .audioEntrainment, .visualStimulation:
            return .t1
        case .qeeg21ch, .tms, .pbmDeep1170nm, .clinicalTacs, .hdTdcs, .cervicalVns:
            return .t2
        case .vibrotactile40hz:
            return .accessory
        }
    }

    // MARK: Tier sets
    static var t1Set: Set<NPModalityType> {
        Set([.pbmTranscranial, .pbmIntranasal, .eegNeurofeedback,
             .besTacs, .tdcs, .vnsHRV, .audioEntrainment, .visualStimulation])
    }

    static var t2Set: Set<NPModalityType> {
        t1Set.union([.qeeg21ch, .tms, .pbmDeep1170nm, .clinicalTacs, .hdTdcs, .cervicalVns])
    }

    static var fullSet: Set<NPModalityType> {
        t2Set.union([.vibrotactile40hz])
    }
}
