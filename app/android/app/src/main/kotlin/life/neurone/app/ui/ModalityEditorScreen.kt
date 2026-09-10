package life.neurone.app.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.NPAudioEntrainmentParams
import life.neurone.core.protocol.NPBESTacsParams
import life.neurone.core.protocol.NPEEGNeurofeedbackParams
import life.neurone.core.protocol.NPPBMTarget
import life.neurone.core.protocol.NPModalityParams
import life.neurone.core.protocol.NPModalityType
import life.neurone.core.protocol.NPPBMIntranasalParams
import life.neurone.core.protocol.NPPBMTranscranialParams
import life.neurone.core.protocol.NPProtocolDefinition
import life.neurone.core.protocol.NPProtocolEntry
import life.neurone.core.protocol.NPProtocolLibrary
import life.neurone.core.protocol.NPProtocolModality
import life.neurone.core.protocol.NPTDCSParams
import life.neurone.core.protocol.NPVNSHRVParams
import life.neurone.core.protocol.NPVisualStimParams
import life.neurone.core.protocol.NPZoneRegistry
import androidx.compose.ui.res.stringResource
import life.neurone.app.R

// Port of iOS ModalityEditorView — the deep per-modality parameter editor. Each enabled
// modality is an expandable card with an enable toggle and typed controls; T1 modalities have
// full widget editing here (T2/accessory show a note to edit via the script editor). Add a
// modality from the picker, tune params, Save writes the updated protocol to the library.

@Composable
fun ModalityEditorScreen(
    library: NPProtocolLibrary,
    existing: NPProtocolDefinition,
    onSaved: () -> Unit,
    onCancel: () -> Unit,
    modifier: Modifier = Modifier,
) {
    var modalities by remember { mutableStateOf(existing.modalities) }
    var expanded by remember { mutableStateOf<Int?>(0) }

    fun updateAt(index: Int, transform: (NPProtocolModality) -> NPProtocolModality) {
        modalities = modalities.mapIndexed { i, m -> if (i == index) transform(m) else m }
    }

    Column(modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp)) {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onCancel) { Text(stringResource(R.string.common_cancel)) }
            Text(stringResource(R.string.and_modality_edit_modalities), style = MaterialTheme.typography.titleMedium)
            Button(onClick = {
                library.save(NPProtocolEntry.Single(existing.copy(modalities = modalities)))
                onSaved()
            }) { Text(stringResource(R.string.protocol_composer_save)) }
        }

        Spacer(Modifier.height(12.dp))
        modalities.forEachIndexed { index, modality ->
            ModalityCard(
                modality = modality,
                isExpanded = expanded == index,
                onToggleExpand = { expanded = if (expanded == index) null else index },
                onChange = { updateAt(index) { _ -> it } },
                onRemove = {
                    modalities = modalities.filterIndexed { i, _ -> i != index }
                    expanded = null
                },
            )
            Spacer(Modifier.height(8.dp))
        }

        Spacer(Modifier.height(8.dp))
        AddModalityButton(onAdd = { type ->
            defaultModality(type)?.let { modalities = modalities + it; expanded = modalities.lastIndex }
        })
    }
}

@Composable
private fun ModalityCard(
    modality: NPProtocolModality,
    isExpanded: Boolean,
    onToggleExpand: () -> Unit,
    onChange: (NPProtocolModality) -> Unit,
    onRemove: () -> Unit,
) {
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(12.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Switch(checked = modality.enabled, onCheckedChange = { onChange(modality.copy(enabled = it)) })
                Spacer(Modifier.height(0.dp))
                Text(
                    modalityLabel(modality.modalityType),
                    style = MaterialTheme.typography.titleSmall,
                    modifier = Modifier.padding(start = 8.dp).clickable { onToggleExpand() },
                )
                Spacer(Modifier.weight(1f))
                TextButton(onClick = onToggleExpand) { Text(if (isExpanded) stringResource(R.string.and_modality_hide) else stringResource(R.string.ui_edit)) }
                TextButton(onClick = onRemove) { Text(stringResource(R.string.web_remove)) }
            }
            if (isExpanded) {
                Spacer(Modifier.height(8.dp))
                ParamControls(modality) { onChange(modality.copy(params = it)) }
            }
        }
    }
}

@Composable
private fun ParamControls(modality: NPProtocolModality, onParams: (NPModalityParams) -> Unit) {
    when (val p = modality.params) {
        is NPModalityParams.PbmTranscranial -> PbmTranscranial(p.params) { onParams(NPModalityParams.PbmTranscranial(it)) }
        is NPModalityParams.PbmIntranasal -> PbmIntranasal(p.params) { onParams(NPModalityParams.PbmIntranasal(it)) }
        is NPModalityParams.EegNeurofeedback -> Eeg(p.params) { onParams(NPModalityParams.EegNeurofeedback(it)) }
        is NPModalityParams.BesTacs -> Bes(p.params) { onParams(NPModalityParams.BesTacs(it)) }
        is NPModalityParams.Tdcs -> Tdcs(p.params) { onParams(NPModalityParams.Tdcs(it)) }
        is NPModalityParams.VnsHRV -> Vns(p.params) { onParams(NPModalityParams.VnsHRV(it)) }
        is NPModalityParams.AudioEntrainment -> Audio(p.params) { onParams(NPModalityParams.AudioEntrainment(it)) }
        is NPModalityParams.VisualStimulation -> Visual(p.params) { onParams(NPModalityParams.VisualStimulation(it)) }
        else -> Text(
            stringResource(R.string.and_modality_edit_this_modality_s_parameters_in_the_scrip),
            style = MaterialTheme.typography.bodySmall,
        )
    }
}

// ── Per-modality param editors (T1) ─────────────────────────────────────────

@Composable
private fun PbmTranscranial(p: NPPBMTranscranialParams, on: (NPPBMTranscranialParams) -> Unit) {
    NumField(stringResource(R.string.and_modality_intensity), p.intensityPercent) { on(p.copy(intensityPercent = it)) }
    NumField(stringResource(R.string.and_modality_frequency_hz_0_cw), p.frequencyHz) { on(p.copy(frequencyHz = it)) }
    IntField(stringResource(R.string.and_modality_duty_cycle), p.dutyCyclePercent) { on(p.copy(dutyCyclePercent = it)) }
    // A zone is a named set of modules, not one of five fixed slots. The picker
    // offers the authored zone names plus clinician_selected; multi-zone targets
    // are authored in .npps until the multi-select picker lands (NP-CFG-UI-001).
    ZoneDropdown(p.target) { on(p.copy(target = it)) }
    EnumDropdown(stringResource(R.string.modality_wavelength), p.wavelength, NPPBMTranscranialParams.Wavelength.entries, { it.rawValue }) { on(p.copy(wavelength = it)) }
}

@Composable
private fun ZoneDropdown(target: NPPBMTarget, on: (NPPBMTarget) -> Unit) {
    val clinicianLabel = stringResource(R.string.pbm_target_clinician_selected)
    val options = NPZoneRegistry.zoneNames + clinicianLabel
    val current = when (target) {
        is NPPBMTarget.Named -> target.zoneNames.firstOrNull() ?: NPZoneRegistry.zoneNames.first()
        is NPPBMTarget.ClinicianSelected -> clinicianLabel
    }
    EnumDropdown(stringResource(R.string.and_modality_zone), current, options) { picked ->
        on(
            if (picked == clinicianLabel) NPPBMTarget.ClinicianSelected
            else NPPBMTarget.Named(listOf(picked)),
        )
    }
}

@Composable
private fun PbmIntranasal(p: NPPBMIntranasalParams, on: (NPPBMIntranasalParams) -> Unit) {
    NumField(stringResource(R.string.and_modality_intensity), p.intensityPercent) { on(p.copy(intensityPercent = it)) }
    NumField(stringResource(R.string.and_modality_frequency_hz), p.frequencyHz) { on(p.copy(frequencyHz = it)) }
    IntField(stringResource(R.string.and_modality_duty_cycle), p.dutyCyclePercent) { on(p.copy(dutyCyclePercent = it)) }
}

@Composable
private fun Eeg(p: NPEEGNeurofeedbackParams, on: (NPEEGNeurofeedbackParams) -> Unit) {
    EnumDropdown(stringResource(R.string.modality_band), p.band, NPEEGNeurofeedbackParams.EEGBand.entries, { it.rawValue }) { on(p.copy(band = it)) }
    EnumDropdown(stringResource(R.string.modality_channels), p.channels, NPEEGNeurofeedbackParams.ChannelSelection.entries) { on(p.copy(channels = it)) }
    ToggleRow(stringResource(R.string.and_modality_closed_loop), p.closedLoopEnabled) { on(p.copy(closedLoopEnabled = it)) }
}

@Composable
private fun Bes(p: NPBESTacsParams, on: (NPBESTacsParams) -> Unit) {
    NumField(stringResource(R.string.and_modality_frequency_hz), p.frequencyHz) { on(p.copy(frequencyHz = it)) }
    NumField(stringResource(R.string.and_modality_intensity_ma), p.intensityMilliamps) { on(p.copy(intensityMilliamps = it)) }
    EnumDropdown(stringResource(R.string.modality_waveform), p.waveform, NPBESTacsParams.Waveform.entries, { it.rawValue }) { on(p.copy(waveform = it)) }
}

@Composable
private fun Tdcs(p: NPTDCSParams, on: (NPTDCSParams) -> Unit) {
    NumField(stringResource(R.string.and_modality_intensity_ma), p.intensityMilliamps) { on(p.copy(intensityMilliamps = it)) }
    // OI-CHARGE-04: the pad geometry the 40 µC/cm² ceiling divides by. The pairs below
    // name 10-20 sites and say nothing about pad size, and the device enforces against
    // whatever is declared here, so it has to be authored — editable, not a summary line.
    NumField(stringResource(R.string.modality_tdcs_electrode_area_label), p.electrodeAreaCm2) { on(p.copy(electrodeAreaCm2 = it)) }
    Text(stringResource(R.string.modality_tdcs_electrode_area_help), style = MaterialTheme.typography.bodySmall)
    Text(stringResource(R.string.and_modality_ramp_0_s_hardware_enforced, p.rampSeconds), style = MaterialTheme.typography.bodySmall)
    Text(stringResource(R.string.and_modality_electrode_pairs_0, p.electrodePairs.joinToString("; ") { it.joinToString("–") }), style = MaterialTheme.typography.bodySmall)
}

@Composable
private fun Vns(p: NPVNSHRVParams, on: (NPVNSHRVParams) -> Unit) {
    NumField(stringResource(R.string.and_modality_frequency_hz), p.frequencyHz) { on(p.copy(frequencyHz = it)) }
    NumField(stringResource(R.string.and_modality_intensity_ma), p.intensityMilliamps) { on(p.copy(intensityMilliamps = it)) }
    EnumDropdown(stringResource(R.string.and_modality_hrv_protocol), p.hrvProtocol, NPVNSHRVParams.HRVProtocol.entries, { it.rawValue }) { on(p.copy(hrvProtocol = it)) }
    NumField(stringResource(R.string.and_modality_breathing_rate_breaths_min), p.resonanceBreathingRate) { on(p.copy(resonanceBreathingRate = it)) }
}

@Composable
private fun Audio(p: NPAudioEntrainmentParams, on: (NPAudioEntrainmentParams) -> Unit) {
    NumField(stringResource(R.string.and_modality_binaural_beat_hz), p.binauralBeatsHz ?: 0.0) { on(p.copy(binauralBeatsHz = it.takeIf { v -> v > 0 })) }
    NumField(stringResource(R.string.and_modality_isochronic_tone_hz), p.isochronicTonesHz ?: 0.0) { on(p.copy(isochronicTonesHz = it.takeIf { v -> v > 0 })) }
    NumField(stringResource(R.string.and_modality_volume), p.volumePercent) { on(p.copy(volumePercent = it)) }
    ToggleRow(stringResource(R.string.and_modality_eeg_adaptive), p.eegAdaptive) { on(p.copy(eegAdaptive = it)) }
    ToggleRow(stringResource(R.string.and_modality_bone_conduction_pacer), p.boneConductionPacer) { on(p.copy(boneConductionPacer = it)) }
}

@Composable
private fun Visual(p: NPVisualStimParams, on: (NPVisualStimParams) -> Unit) {
    NumField(stringResource(R.string.and_modality_frequency_hz), p.frequencyHz) { on(p.copy(frequencyHz = it)) }
    EnumDropdown(stringResource(R.string.validate_param_mode), p.mode, NPVisualStimParams.VisualMode.entries, { it.rawValue }) { on(p.copy(mode = it)) }
    NumField(stringResource(R.string.and_modality_emdr_cadence_hz), p.emdrCadenceHz) { on(p.copy(emdrCadenceHz = it)) }
    ToggleRow(stringResource(R.string.and_modality_mode_f_invisible_nir), p.enableModeF) { on(p.copy(enableModeF = it)) }
}

// ── Add-modality picker ─────────────────────────────────────────────────────

private val T1_TYPES = listOf(
    NPModalityType.PBM_TRANSCRANIAL, NPModalityType.PBM_INTRANASAL, NPModalityType.EEG_NEUROFEEDBACK,
    NPModalityType.BES_TACS, NPModalityType.TDCS, NPModalityType.VNS_HRV,
    NPModalityType.AUDIO_ENTRAINMENT, NPModalityType.VISUAL_STIMULATION,
)

private fun defaultModality(type: NPModalityType): NPProtocolModality? {
    val params: NPModalityParams = when (type) {
        NPModalityType.PBM_TRANSCRANIAL -> NPModalityParams.PbmTranscranial(NPPBMTranscranialParams())
        NPModalityType.PBM_INTRANASAL -> NPModalityParams.PbmIntranasal(NPPBMIntranasalParams())
        NPModalityType.EEG_NEUROFEEDBACK -> NPModalityParams.EegNeurofeedback(NPEEGNeurofeedbackParams())
        NPModalityType.BES_TACS -> NPModalityParams.BesTacs(NPBESTacsParams())
        NPModalityType.TDCS -> NPModalityParams.Tdcs(NPTDCSParams())
        NPModalityType.VNS_HRV -> NPModalityParams.VnsHRV(NPVNSHRVParams())
        NPModalityType.AUDIO_ENTRAINMENT -> NPModalityParams.AudioEntrainment(NPAudioEntrainmentParams())
        NPModalityType.VISUAL_STIMULATION -> NPModalityParams.VisualStimulation(NPVisualStimParams())
        else -> return null
    }
    return NPProtocolModality(params = params)
}

@Composable
private fun AddModalityButton(onAdd: (NPModalityType) -> Unit) {
    var open by remember { mutableStateOf(false) }
    Box {
        OutlinedButton(onClick = { open = true }, modifier = Modifier.fillMaxWidth()) { Text(stringResource(R.string.and_modality_add_modality)) }
        DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            for (type in T1_TYPES) {
                DropdownMenuItem(text = { Text(modalityLabel(type)) }, onClick = { onAdd(type); open = false })
            }
        }
    }
}

// ── Reusable controls ───────────────────────────────────────────────────────

@Composable
private fun NumField(label: String, value: Double, onChange: (Double) -> Unit) {
    var text by remember { mutableStateOf(fmt(value)) }
    OutlinedTextField(
        value = text,
        onValueChange = { raw ->
            text = raw.filter { it.isDigit() || it == '.' }
            text.toDoubleOrNull()?.let(onChange)
        },
        label = { Text(label) },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
        modifier = Modifier.fillMaxWidth().padding(vertical = 3.dp),
    )
}

@Composable
private fun IntField(label: String, value: Int, onChange: (Int) -> Unit) {
    var text by remember { mutableStateOf(value.toString()) }
    OutlinedTextField(
        value = text,
        onValueChange = { raw ->
            text = raw.filter { it.isDigit() }
            text.toIntOrNull()?.let(onChange)
        },
        label = { Text(label) },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
        modifier = Modifier.fillMaxWidth().padding(vertical = 3.dp),
    )
}

@Composable
private fun ToggleRow(label: String, checked: Boolean, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(1f))
        Switch(checked = checked, onCheckedChange = onChange)
    }
}

@Composable
private fun <T> EnumDropdown(
    label: String,
    current: T,
    options: List<T>,
    display: (T) -> String = { it.toString() },
    onSelect: (T) -> Unit,
) {
    var open by remember { mutableStateOf(false) }
    Row(Modifier.fillMaxWidth().padding(vertical = 3.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
        Box {
            OutlinedButton(onClick = { open = true }) { Text(display(current)) }
            DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
                for (o in options) {
                    DropdownMenuItem(text = { Text(display(o)) }, onClick = { onSelect(o); open = false })
                }
            }
        }
    }
}

private fun fmt(v: Double): String = if (v == v.toLong().toDouble()) v.toLong().toString() else v.toString()

@Composable
private fun modalityLabel(type: NPModalityType): String = stringResource(
    when (type) {
        NPModalityType.PBM_TRANSCRANIAL -> R.string.modality_pbm_transcranial_name
        NPModalityType.PBM_INTRANASAL -> R.string.modality_pbm_intranasal_name
        NPModalityType.EEG_NEUROFEEDBACK -> R.string.modality_eeg_neurofeedback_name
        NPModalityType.BES_TACS -> R.string.modality_bes_tacs_name
        NPModalityType.TDCS -> R.string.modality_tdcs_name
        NPModalityType.VNS_HRV -> R.string.modality_vns_hrv_name
        NPModalityType.AUDIO_ENTRAINMENT -> R.string.modality_audio_entrainment_name
        NPModalityType.VISUAL_STIMULATION -> R.string.modality_visual_stimulation_name
        NPModalityType.QEEG_21CH -> R.string.modality_qeeg_21ch_name
        NPModalityType.TMS -> R.string.modality_tms_name
        NPModalityType.PBM_DEEP_1170NM -> R.string.modality_pbm_deep_1170nm_name
        NPModalityType.CLINICAL_TACS -> R.string.modality_clinical_tacs_name
        NPModalityType.HD_TDCS -> R.string.modality_hd_tdcs_name
        NPModalityType.CERVICAL_VNS -> R.string.modality_cervical_vns_name
        NPModalityType.VIBROTACTILE_40HZ -> R.string.modality_vibrotactile_40hz_name
    },
)
