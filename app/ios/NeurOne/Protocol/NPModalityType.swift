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
    //
    // One name per modality, shared with the web table and the Android UI:
    // MODALITY_<id>_NAME holds the .npps grammar token for this modality with
    // underscores replaced by spaces and each word capitalised. There is no
    // separate consumer-facing name — the .npps token IS the on-screen wording.
    var displayName: String {
        switch self {
        case .pbmTranscranial: return String(localized: "MODALITY_PBM_TRANSCRANIAL_NAME")
        case .pbmIntranasal: return String(localized: "MODALITY_PBM_INTRANASAL_NAME")
        case .eegNeurofeedback: return String(localized: "MODALITY_EEG_NEUROFEEDBACK_NAME")
        case .besTacs: return String(localized: "MODALITY_BES_TACS_NAME")
        case .tdcs: return String(localized: "MODALITY_TDCS_NAME")
        case .vnsHRV: return String(localized: "MODALITY_VNS_HRV_NAME")
        case .audioEntrainment: return String(localized: "MODALITY_AUDIO_ENTRAINMENT_NAME")
        case .visualStimulation: return String(localized: "MODALITY_VISUAL_STIMULATION_NAME")
        case .qeeg21ch: return String(localized: "MODALITY_QEEG_21CH_NAME")
        case .tms: return String(localized: "MODALITY_TMS_NAME")
        case .pbmDeep1170nm: return String(localized: "MODALITY_PBM_DEEP_1170NM_NAME")
        case .clinicalTacs: return String(localized: "MODALITY_CLINICAL_TACS_NAME")
        case .hdTdcs: return String(localized: "MODALITY_HD_TDCS_NAME")
        case .cervicalVns: return String(localized: "MODALITY_CERVICAL_VNS_NAME")
        case .vibrotactile40hz: return String(localized: "MODALITY_VIBROTACTILE_40HZ_NAME")
        }
    }

    // MARK: Short description
    //
    // One description per modality, shared with the web table's
    // MODALITY_META.shortDescriptionKey — the same MODALITY_<id>_DESC key.
    var shortDescription: String {
        switch self {
        case .pbmTranscranial: return String(localized: "MODALITY_PBM_TRANSCRANIAL_DESC")
        case .pbmIntranasal: return String(localized: "MODALITY_PBM_INTRANASAL_DESC")
        case .eegNeurofeedback: return String(localized: "MODALITY_EEG_NEUROFEEDBACK_DESC")
        case .besTacs: return String(localized: "MODALITY_BES_TACS_DESC")
        case .tdcs: return String(localized: "MODALITY_TDCS_DESC")
        case .vnsHRV: return String(localized: "MODALITY_VNS_HRV_DESC")
        case .audioEntrainment: return String(localized: "MODALITY_AUDIO_ENTRAINMENT_DESC")
        case .visualStimulation: return String(localized: "MODALITY_VISUAL_STIMULATION_DESC")
        case .qeeg21ch: return String(localized: "MODALITY_QEEG_21CH_DESC")
        case .tms: return String(localized: "MODALITY_TMS_DESC")
        case .pbmDeep1170nm: return String(localized: "MODALITY_PBM_DEEP_1170NM_DESC")
        case .clinicalTacs: return String(localized: "MODALITY_CLINICAL_TACS_DESC")
        case .hdTdcs: return String(localized: "MODALITY_HD_TDCS_DESC")
        case .cervicalVns: return String(localized: "MODALITY_CERVICAL_VNS_DESC")
        case .vibrotactile40hz: return String(localized: "MODALITY_VIBROTACTILE_40HZ_DESC")
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
