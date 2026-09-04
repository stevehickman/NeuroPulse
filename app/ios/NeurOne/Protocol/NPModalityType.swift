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
    // These three properties are a SEPARATE key family from the web table's
    // MODALITY_<id>_NAME / _CONSUMER / _DESC: 35 of the 45 strings differ in
    // wording between the two surfaces today (see the PR body). The keys hold
    // this app's current text verbatim so nothing on screen changes; collapsing
    // the two families is a copy decision, not a refactor.
    var displayName: String {
        switch self {
        case .pbmTranscranial: return String(localized: "MODALITY_PBM_TRANSCRANIAL_DETAIL_NAME")
        case .pbmIntranasal: return String(localized: "MODALITY_PBM_INTRANASAL_DETAIL_NAME")
        case .eegNeurofeedback: return String(localized: "MODALITY_EEG_NEUROFEEDBACK_DETAIL_NAME")
        case .besTacs: return String(localized: "MODALITY_BES_TACS_DETAIL_NAME")
        case .tdcs: return String(localized: "MODALITY_TDCS_DETAIL_NAME")
        case .vnsHRV: return String(localized: "MODALITY_VNS_HRV_DETAIL_NAME")
        case .audioEntrainment: return String(localized: "MODALITY_AUDIO_ENTRAINMENT_DETAIL_NAME")
        case .visualStimulation: return String(localized: "MODALITY_VISUAL_STIMULATION_DETAIL_NAME")
        case .qeeg21ch: return String(localized: "MODALITY_QEEG_21CH_DETAIL_NAME")
        case .tms: return String(localized: "MODALITY_TMS_DETAIL_NAME")
        case .pbmDeep1170nm: return String(localized: "MODALITY_PBM_DEEP_1170NM_DETAIL_NAME")
        case .clinicalTacs: return String(localized: "MODALITY_CLINICAL_TACS_DETAIL_NAME")
        case .hdTdcs: return String(localized: "MODALITY_HD_TDCS_DETAIL_NAME")
        case .cervicalVns: return String(localized: "MODALITY_CERVICAL_VNS_DETAIL_NAME")
        case .vibrotactile40hz: return String(localized: "MODALITY_VIBROTACTILE_40HZ_DETAIL_NAME")
        }
    }

    // MARK: Consumer-facing regulatory-safe name
    var consumerName: String {
        switch self {
        case .pbmTranscranial: return String(localized: "MODALITY_PBM_TRANSCRANIAL_DETAIL_CONSUMER")
        case .pbmIntranasal: return String(localized: "MODALITY_PBM_INTRANASAL_DETAIL_CONSUMER")
        case .eegNeurofeedback: return String(localized: "MODALITY_EEG_NEUROFEEDBACK_DETAIL_CONSUMER")
        case .besTacs: return String(localized: "MODALITY_BES_TACS_DETAIL_CONSUMER")
        case .tdcs: return String(localized: "MODALITY_TDCS_DETAIL_CONSUMER")
        case .vnsHRV: return String(localized: "MODALITY_VNS_HRV_DETAIL_CONSUMER")
        case .audioEntrainment: return String(localized: "MODALITY_AUDIO_ENTRAINMENT_DETAIL_CONSUMER")
        case .visualStimulation: return String(localized: "MODALITY_VISUAL_STIMULATION_DETAIL_CONSUMER")
        case .qeeg21ch: return String(localized: "MODALITY_QEEG_21CH_DETAIL_CONSUMER")
        case .tms: return String(localized: "MODALITY_TMS_DETAIL_CONSUMER")
        case .pbmDeep1170nm: return String(localized: "MODALITY_PBM_DEEP_1170NM_DETAIL_CONSUMER")
        case .clinicalTacs: return String(localized: "MODALITY_CLINICAL_TACS_DETAIL_CONSUMER")
        case .hdTdcs: return String(localized: "MODALITY_HD_TDCS_DETAIL_CONSUMER")
        case .cervicalVns: return String(localized: "MODALITY_CERVICAL_VNS_DETAIL_CONSUMER")
        case .vibrotactile40hz: return String(localized: "MODALITY_VIBROTACTILE_40HZ_DETAIL_CONSUMER")
        }
    }

    // MARK: Short description
    var shortDescription: String {
        switch self {
        case .pbmTranscranial: return String(localized: "MODALITY_PBM_TRANSCRANIAL_DETAIL_SUMMARY")
        case .pbmIntranasal: return String(localized: "MODALITY_PBM_INTRANASAL_DETAIL_SUMMARY")
        case .eegNeurofeedback: return String(localized: "MODALITY_EEG_NEUROFEEDBACK_DETAIL_SUMMARY")
        case .besTacs: return String(localized: "MODALITY_BES_TACS_DETAIL_SUMMARY")
        case .tdcs: return String(localized: "MODALITY_TDCS_DETAIL_SUMMARY")
        case .vnsHRV: return String(localized: "MODALITY_VNS_HRV_DETAIL_SUMMARY")
        case .audioEntrainment: return String(localized: "MODALITY_AUDIO_ENTRAINMENT_DETAIL_SUMMARY")
        case .visualStimulation: return String(localized: "MODALITY_VISUAL_STIMULATION_DETAIL_SUMMARY")
        case .qeeg21ch: return String(localized: "MODALITY_QEEG_21CH_DETAIL_SUMMARY")
        case .tms: return String(localized: "MODALITY_TMS_DETAIL_SUMMARY")
        case .pbmDeep1170nm: return String(localized: "MODALITY_PBM_DEEP_1170NM_DETAIL_SUMMARY")
        case .clinicalTacs: return String(localized: "MODALITY_CLINICAL_TACS_DETAIL_SUMMARY")
        case .hdTdcs: return String(localized: "MODALITY_HD_TDCS_DETAIL_SUMMARY")
        case .cervicalVns: return String(localized: "MODALITY_CERVICAL_VNS_DETAIL_SUMMARY")
        case .vibrotactile40hz: return String(localized: "MODALITY_VIBROTACTILE_40HZ_DETAIL_SUMMARY")
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
