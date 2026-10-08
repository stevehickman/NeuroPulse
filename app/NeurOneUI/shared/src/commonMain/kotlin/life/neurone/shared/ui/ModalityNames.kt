package life.neurone.shared.ui

import life.neurone.core.protocol.NPModalityType
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.StringResource

/** The one display name per modality (`MODALITY_<ID>_NAME`, CLAUDE.md §17). */
internal fun modalityNameRes(type: NPModalityType): StringResource =
    when (type) {
        NPModalityType.PBM_TRANSCRANIAL -> Res.string.modality_pbm_transcranial_name
        NPModalityType.PBM_INTRANASAL -> Res.string.modality_pbm_intranasal_name
        NPModalityType.EEG_NEUROFEEDBACK -> Res.string.modality_eeg_neurofeedback_name
        NPModalityType.BES_TACS -> Res.string.modality_bes_tacs_name
        NPModalityType.TDCS -> Res.string.modality_tdcs_name
        NPModalityType.VNS_HRV -> Res.string.modality_vns_hrv_name
        NPModalityType.AUDIO_ENTRAINMENT -> Res.string.modality_audio_entrainment_name
        NPModalityType.VISUAL_STIMULATION -> Res.string.modality_visual_stimulation_name
        NPModalityType.QEEG_21CH -> Res.string.modality_qeeg_21ch_name
        NPModalityType.TMS -> Res.string.modality_tms_name
        NPModalityType.PBM_DEEP_1170NM -> Res.string.modality_pbm_deep_1170nm_name
        NPModalityType.CLINICAL_TACS -> Res.string.modality_clinical_tacs_name
        NPModalityType.HD_TDCS -> Res.string.modality_hd_tdcs_name
        NPModalityType.CERVICAL_VNS -> Res.string.modality_cervical_vns_name
        NPModalityType.VIBROTACTILE_40HZ -> Res.string.modality_vibrotactile_40hz_name
    }
