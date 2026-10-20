package life.neurone.shared.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import life.neurone.core.protocol.NPAudioEntrainmentLimits
import life.neurone.core.protocol.NPBESTacsLimits
import life.neurone.core.protocol.NPCervicalVnsLimits
import life.neurone.core.protocol.NPClinicalTacsLimits
import life.neurone.core.protocol.NPDeepPBMLimits
import life.neurone.core.protocol.NPEEGNeurofeedbackLimits
import life.neurone.core.protocol.NPEEGNeurofeedbackParams
import life.neurone.core.protocol.NPHDTdcsLimits
import life.neurone.core.protocol.NPHDTdcsParams
import life.neurone.core.protocol.NPLimitsSet
import life.neurone.core.protocol.NPPBMIntranasalLimits
import life.neurone.core.protocol.NPPBMTranscranialLimits
import life.neurone.core.protocol.NPTDCSLimits
import life.neurone.core.protocol.NPTMSLimits
import life.neurone.core.protocol.NPVNSHRVLimits
import life.neurone.core.protocol.NPVNSHRVParams
import life.neurone.core.protocol.NPVibrotactileLimits
import life.neurone.core.protocol.NPVisualStimLimits
import life.neurone.core.protocol.NPVisualStimParams
import life.neurone.shared.resources.*
import org.jetbrains.compose.resources.StringResource
import org.jetbrains.compose.resources.stringResource

// Port of the iOS tier editor (LimitsSettingsView, the Form and its field views): every modality's caps,
// each section switched on to create its block, each field optional. A blank field is "no configured cap".
// The sections are a table (below), so a modality's fields are declared once. The ranges are the iOS editor's
// input bounds; they are not the hardware ceilings, which the validator enforces whatever is typed here.

/** One input in a section: how to read it from, and write it into, the working set. */
private sealed interface LimitField {
    class Num(
        val label: StringResource, val unit: String, val range: ClosedFloatingPointRange<Double>,
        val get: (NPLimitsSet) -> Double?, val set: (NPLimitsSet, Double?) -> NPLimitsSet,
    ) : LimitField
    class Whole(
        val label: StringResource, val unit: String, val range: IntRange,
        val get: (NPLimitsSet) -> Int?, val set: (NPLimitsSet, Int?) -> NPLimitsSet,
    ) : LimitField
    class Duration(
        val label: StringResource, val get: (NPLimitsSet) -> Int?, val set: (NPLimitsSet, Int?) -> NPLimitsSet,
    ) : LimitField
    class Flag(
        val label: StringResource, val get: (NPLimitsSet) -> Boolean?, val set: (NPLimitsSet, Boolean?) -> NPLimitsSet,
    ) : LimitField
    class Allowed(
        val label: StringResource, val options: List<Pair<String, StringResource>>,
        val get: (NPLimitsSet) -> List<String>?, val set: (NPLimitsSet, List<String>?) -> NPLimitsSet,
    ) : LimitField
    class Note(val text: StringResource) : LimitField
}

private class LimitSection(
    val title: StringResource,
    val present: (NPLimitsSet) -> Boolean,
    val enable: (NPLimitsSet, Boolean) -> NPLimitsSet,
    val fields: List<LimitField>,
)

private val MAX_SESSION_DURATION = Res.string.web_lim_max_session_duration

private val eegBands = listOf(
    "delta" to Res.string.eeg_band_delta, "theta" to Res.string.eeg_band_theta, "alpha" to Res.string.eeg_band_alpha,
    "beta" to Res.string.eeg_band_beta, "gamma" to Res.string.eeg_band_gamma,
    "alphaTheta" to Res.string.eeg_band_alphatheta, "gammaTheta" to Res.string.eeg_band_gammatheta,
)
private val hrvProtocols = listOf(
    "standalone" to Res.string.hrv_protocol_standalone, "tavnsSync" to Res.string.hrv_protocol_tavnssync,
    "eegBiofeedback" to Res.string.hrv_protocol_eegbiofeedback, "combinedPBM" to Res.string.hrv_protocol_combinedpbm,
)
private val visualModes = listOf(
    "binocular" to Res.string.visual_mode_binocular, "emdr" to Res.string.visual_mode_emdr,
    "retinal_pbm" to Res.string.visual_mode_retinalpbm, "mode_f" to Res.string.visual_mode_modef,
)
private val hdMontages = listOf(
    "ring_4x1" to Res.string.hdtdcs_montage_ring4x1, "bilateral_4x1" to Res.string.hdtdcs_montage_bilateral4x1,
    "standard_2_electrode" to Res.string.hdtdcs_montage_standard2el,
)

private val sections: List<LimitSection> = listOf(
    LimitSection(
        Res.string.web_lim_section_pbm_transcranial,
        { it.pbmTranscranial != null }, { l, on -> l.copy(pbmTranscranial = if (on) NPPBMTranscranialLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_irradiance, "mW/cm²", 0.0..400.0, { it.pbmTranscranial?.maxIrradianceMWcm2 }) { l, v -> l.copy(pbmTranscranial = l.pbmTranscranial?.copy(maxIrradianceMWcm2 = v)) },
            LimitField.Num(Res.string.web_lim_max_frequency, "Hz", 0.0..100.0, { it.pbmTranscranial?.maxFrequencyHz }) { l, v -> l.copy(pbmTranscranial = l.pbmTranscranial?.copy(maxFrequencyHz = v)) },
            LimitField.Whole(Res.string.web_lim_max_duty_cycle, "%", 0..25, { it.pbmTranscranial?.maxDutyCyclePercent }) { l, v -> l.copy(pbmTranscranial = l.pbmTranscranial?.copy(maxDutyCyclePercent = v)) },
            LimitField.Num(Res.string.web_lim_max_session_dose, "J/cm²", 0.0..1000.0, { it.pbmTranscranial?.maxSessionDoseJCm2 }) { l, v -> l.copy(pbmTranscranial = l.pbmTranscranial?.copy(maxSessionDoseJCm2 = v)) },
            LimitField.Num(Res.string.web_lim_max_daily_dose, "J/cm²", 0.0..5000.0, { it.pbmTranscranial?.maxDailyDoseJCm2 }) { l, v -> l.copy(pbmTranscranial = l.pbmTranscranial?.copy(maxDailyDoseJCm2 = v)) },
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_pbm_intranasal,
        { it.pbmIntranasal != null }, { l, on -> l.copy(pbmIntranasal = if (on) NPPBMIntranasalLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_irradiance, "mW/cm²", 0.0..400.0, { it.pbmIntranasal?.maxIrradianceMWcm2 }) { l, v -> l.copy(pbmIntranasal = l.pbmIntranasal?.copy(maxIrradianceMWcm2 = v)) },
            LimitField.Num(Res.string.web_lim_max_session_dose, "J/cm²", 0.0..500.0, { it.pbmIntranasal?.maxSessionDoseJCm2 }) { l, v -> l.copy(pbmIntranasal = l.pbmIntranasal?.copy(maxSessionDoseJCm2 = v)) },
            LimitField.Duration(MAX_SESSION_DURATION, { it.pbmIntranasal?.maxSessionDurationSeconds }) { l, v -> l.copy(pbmIntranasal = l.pbmIntranasal?.copy(maxSessionDurationSeconds = v)) },
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_eeg,
        { it.eegNeurofeedback != null }, { l, on -> l.copy(eegNeurofeedback = if (on) NPEEGNeurofeedbackLimits() else null) },
        listOf(
            LimitField.Flag(Res.string.web_lim_require_closed_loop, { it.eegNeurofeedback?.requireClosedLoop }) { l, v -> l.copy(eegNeurofeedback = l.eegNeurofeedback?.copy(requireClosedLoop = v)) },
            LimitField.Allowed(Res.string.limits_allowed_eeg_bands, eegBands, { it.eegNeurofeedback?.allowedBands }) { l, v -> l.copy(eegNeurofeedback = l.eegNeurofeedback?.copy(allowedBands = v)) },
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_bes_tacs,
        { it.besTacs != null }, { l, on -> l.copy(besTacs = if (on) NPBESTacsLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_intensity, "mA", 0.0..1.0, { it.besTacs?.maxIntensityMilliamps }) { l, v -> l.copy(besTacs = l.besTacs?.copy(maxIntensityMilliamps = v)) },
            LimitField.Num(Res.string.web_lim_max_frequency, "Hz", 0.5..40.0, { it.besTacs?.maxFrequencyHz }) { l, v -> l.copy(besTacs = l.besTacs?.copy(maxFrequencyHz = v)) },
            LimitField.Num(Res.string.web_lim_min_frequency, "Hz", 0.5..40.0, { it.besTacs?.minFrequencyHz }) { l, v -> l.copy(besTacs = l.besTacs?.copy(minFrequencyHz = v)) },
            LimitField.Duration(MAX_SESSION_DURATION, { it.besTacs?.maxSessionDurationSeconds }) { l, v -> l.copy(besTacs = l.besTacs?.copy(maxSessionDurationSeconds = v)) },
            LimitField.Whole(Res.string.web_lim_max_sessions_day, "", 1..10, { it.besTacs?.maxSessionsPerDay }) { l, v -> l.copy(besTacs = l.besTacs?.copy(maxSessionsPerDay = v)) },
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_tdcs,
        { it.tdcs != null }, { l, on -> l.copy(tdcs = if (on) NPTDCSLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_intensity, "mA", 0.1..2.0, { it.tdcs?.maxIntensityMilliamps }) { l, v -> l.copy(tdcs = l.tdcs?.copy(maxIntensityMilliamps = v)) },
            LimitField.Duration(MAX_SESSION_DURATION, { it.tdcs?.maxSessionDurationSeconds }) { l, v -> l.copy(tdcs = l.tdcs?.copy(maxSessionDurationSeconds = v)) },
            LimitField.Whole(Res.string.web_lim_max_sessions_day, "", 1..5, { it.tdcs?.maxSessionsPerDay }) { l, v -> l.copy(tdcs = l.tdcs?.copy(maxSessionsPerDay = v)) },
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_vns_hrv,
        { it.vnsHrv != null }, { l, on -> l.copy(vnsHrv = if (on) NPVNSHRVLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_intensity, "mA", 0.0..2.0, { it.vnsHrv?.maxIntensityMilliamps }) { l, v -> l.copy(vnsHrv = l.vnsHrv?.copy(maxIntensityMilliamps = v)) },
            LimitField.Num(Res.string.web_lim_max_frequency, "Hz", 1.0..25.0, { it.vnsHrv?.maxFrequencyHz }) { l, v -> l.copy(vnsHrv = l.vnsHrv?.copy(maxFrequencyHz = v)) },
            LimitField.Duration(MAX_SESSION_DURATION, { it.vnsHrv?.maxSessionDurationSeconds }) { l, v -> l.copy(vnsHrv = l.vnsHrv?.copy(maxSessionDurationSeconds = v)) },
            LimitField.Allowed(Res.string.ui_lim_allowed_hrv, hrvProtocols, { it.vnsHrv?.allowedProtocols }) { l, v -> l.copy(vnsHrv = l.vnsHrv?.copy(allowedProtocols = v)) },
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_audio,
        { it.audioEntrainment != null }, { l, on -> l.copy(audioEntrainment = if (on) NPAudioEntrainmentLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_volume, "dB SPL", 40.0..90.0, { it.audioEntrainment?.maxVolumeDb }) { l, v -> l.copy(audioEntrainment = l.audioEntrainment?.copy(maxVolumeDb = v)) },
            LimitField.Num(Res.string.limits_max_binaural_beats, "Hz", 0.0..100.0, { it.audioEntrainment?.maxBinauralBeatsHz }) { l, v -> l.copy(audioEntrainment = l.audioEntrainment?.copy(maxBinauralBeatsHz = v)) },
            LimitField.Num(Res.string.limits_max_isochronic_tones, "Hz", 0.0..100.0, { it.audioEntrainment?.maxIsochronicTonesHz }) { l, v -> l.copy(audioEntrainment = l.audioEntrainment?.copy(maxIsochronicTonesHz = v)) },
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_visual,
        { it.visualStimulation != null }, { l, on -> l.copy(visualStimulation = if (on) NPVisualStimLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_frequency, "Hz", 0.0..100.0, { it.visualStimulation?.maxFrequencyHz }) { l, v -> l.copy(visualStimulation = l.visualStimulation?.copy(maxFrequencyHz = v)) },
            LimitField.Num(Res.string.web_lim_min_frequency, "Hz", 0.0..100.0, { it.visualStimulation?.minFrequencyHz }) { l, v -> l.copy(visualStimulation = l.visualStimulation?.copy(minFrequencyHz = v)) },
            LimitField.Flag(Res.string.limits_block_3_30_hz_risk_range, { it.visualStimulation?.blockHighRiskRange }) { l, v -> l.copy(visualStimulation = l.visualStimulation?.copy(blockHighRiskRange = v)) },
            LimitField.Allowed(Res.string.limits_allowed_visual_modes, visualModes, { it.visualStimulation?.allowedModes }) { l, v -> l.copy(visualStimulation = l.visualStimulation?.copy(allowedModes = v)) },
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_tms,
        { it.tms != null }, { l, on -> l.copy(tms = if (on) NPTMSLimits() else null) },
        listOf(
            LimitField.Whole(Res.string.limits_max_intensity_mt, "%MT", 0..150, { it.tms?.maxIntensityPercentMT }) { l, v -> l.copy(tms = l.tms?.copy(maxIntensityPercentMT = v)) },
            LimitField.Whole(Res.string.web_lim_max_pulses_session, "", 0..5000, { it.tms?.maxPulsesPerSession }) { l, v -> l.copy(tms = l.tms?.copy(maxPulsesPerSession = v)) },
            LimitField.Whole(Res.string.web_lim_max_pulses_day, "", 0..10000, { it.tms?.maxPulsesPerDay }) { l, v -> l.copy(tms = l.tms?.copy(maxPulsesPerDay = v)) },
            LimitField.Whole(Res.string.web_lim_max_sessions_week, "", 0..7, { it.tms?.maxSessionsPerWeek }) { l, v -> l.copy(tms = l.tms?.copy(maxSessionsPerWeek = v)) },
        ),
    ),
    LimitSection(
        Res.string.modality_pbm_deep_1170nm_name,
        { it.pbmDeep1170nm != null }, { l, on -> l.copy(pbmDeep1170nm = if (on) NPDeepPBMLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_intensity, "mW/cm²", 0.0..1000.0, { it.pbmDeep1170nm?.maxIntensityMWcm2 }) { l, v -> l.copy(pbmDeep1170nm = l.pbmDeep1170nm?.copy(maxIntensityMWcm2 = v)) },
            LimitField.Duration(MAX_SESSION_DURATION, { it.pbmDeep1170nm?.maxSessionDurationSeconds }) { l, v -> l.copy(pbmDeep1170nm = l.pbmDeep1170nm?.copy(maxSessionDurationSeconds = v)) },
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_clinical_tacs,
        { it.clinicalTacs != null }, { l, on -> l.copy(clinicalTacs = if (on) NPClinicalTacsLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_intensity, "mA", 0.0..4.0, { it.clinicalTacs?.maxIntensityMilliamps }) { l, v -> l.copy(clinicalTacs = l.clinicalTacs?.copy(maxIntensityMilliamps = v)) },
            LimitField.Duration(MAX_SESSION_DURATION, { it.clinicalTacs?.maxSessionDurationSeconds }) { l, v -> l.copy(clinicalTacs = l.clinicalTacs?.copy(maxSessionDurationSeconds = v)) },
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_hd_tdcs,
        { it.hdTdcs != null }, { l, on -> l.copy(hdTdcs = if (on) NPHDTdcsLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_intensity_electrode, "mA", 0.0..2.0, { it.hdTdcs?.maxIntensityMilliamps }) { l, v -> l.copy(hdTdcs = l.hdTdcs?.copy(maxIntensityMilliamps = v)) },
            LimitField.Duration(MAX_SESSION_DURATION, { it.hdTdcs?.maxSessionDurationSeconds }) { l, v -> l.copy(hdTdcs = l.hdTdcs?.copy(maxSessionDurationSeconds = v)) },
            LimitField.Allowed(Res.string.ui_lim_allowed_montages, hdMontages, { it.hdTdcs?.allowedMontages }) { l, v -> l.copy(hdTdcs = l.hdTdcs?.copy(allowedMontages = v)) },
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_cervical_vns,
        { it.cervicalVns != null }, { l, on -> l.copy(cervicalVns = if (on) NPCervicalVnsLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_intensity, "mA", 0.0..2.0, { it.cervicalVns?.maxIntensityMilliamps }) { l, v -> l.copy(cervicalVns = l.cervicalVns?.copy(maxIntensityMilliamps = v)) },
            LimitField.Duration(MAX_SESSION_DURATION, { it.cervicalVns?.maxSessionDurationSeconds }) { l, v -> l.copy(cervicalVns = l.cervicalVns?.copy(maxSessionDurationSeconds = v)) },
            LimitField.Note(Res.string.limits_cardiac_interlock_is_always_enforced_by_the),
        ),
    ),
    LimitSection(
        Res.string.web_lim_section_vibrotactile,
        { it.vibrotactile40hz != null }, { l, on -> l.copy(vibrotactile40hz = if (on) NPVibrotactileLimits() else null) },
        listOf(
            LimitField.Num(Res.string.web_lim_max_intensity, "G", 0.6..1.2, { it.vibrotactile40hz?.maxIntensityG }) { l, v -> l.copy(vibrotactile40hz = l.vibrotactile40hz?.copy(maxIntensityG = v)) },
            LimitField.Duration(MAX_SESSION_DURATION, { it.vibrotactile40hz?.maxSessionDurationSeconds }) { l, v -> l.copy(vibrotactile40hz = l.vibrotactile40hz?.copy(maxSessionDurationSeconds = v)) },
        ),
    ),
)

private enum class EditorMode(val label: StringResource) {
    FIELDS(Res.string.limits_dosage_limits), SCRIPT(Res.string.ui_tab_script),
}

/**
 * The editor for one tier's [initial] limits (null = none yet): the modality sections, or the same limits as
 * an `.npps` script that is parsed as it is typed. [exportScript] and [importScript] are the store's.
 */
@Composable
internal fun LimitsTierEditor(
    initial: NPLimitsSet?,
    name: String,
    clearLabel: String,
    exportScript: (NPLimitsSet) -> String,
    importScript: (String) -> NPLimitsSet,
    onSave: (NPLimitsSet) -> Unit,
    onClear: () -> Unit,
) {
    var working by remember { mutableStateOf(initial ?: NPLimitsSet(name = name)) }
    var mode by remember { mutableStateOf(EditorMode.FIELDS) }
    // A script import replaces every field, so the field state is rebuilt from the new value.
    var generation by remember { mutableStateOf(0) }

    Column {
        TabRow(selectedTabIndex = mode.ordinal) {
            EditorMode.entries.forEach {
                Tab(selected = mode == it, onClick = { mode = it }, text = { Text(stringResource(if (it == EditorMode.FIELDS) Res.string.ui_limits else it.label)) })
            }
        }
        Spacer(Modifier.height(8.dp))
        when (mode) {
            EditorMode.FIELDS -> androidx.compose.runtime.key(generation) {
                sections.forEach { section ->
                    val on = section.present(working)
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                        Text(stringResource(section.title), Modifier.weight(1f), style = MaterialTheme.typography.titleSmall)
                        Switch(checked = on, onCheckedChange = { working = section.enable(working, it) })
                    }
                    if (on) section.fields.forEach { field -> FieldRow(field, working) { working = it } }
                    Spacer(Modifier.height(8.dp))
                }
                if (sections.none { it.present(working) }) {
                    Text(stringResource(Res.string.web_no_modality_limits), style = MaterialTheme.typography.bodySmall)
                }
            }
            EditorMode.SCRIPT -> ScriptPane(
                initialText = remember(generation) { exportScript(working) },
                importScript = importScript,
                onParsed = { working = it.copy(name = working.name, description = working.description); },
            )
        }
        Spacer(Modifier.height(16.dp))
        Button(onClick = { onSave(working.copy(name = name)) }, modifier = Modifier.fillMaxWidth()) {
            Text(stringResource(Res.string.protocol_composer_save))
        }
        Spacer(Modifier.height(8.dp))
        OutlinedButton(onClick = onClear, modifier = Modifier.fillMaxWidth()) { Text(clearLabel) }
    }
}

@Composable
private fun ScriptPane(initialText: String, importScript: (String) -> NPLimitsSet, onParsed: (NPLimitsSet) -> Unit) {
    var text by remember { mutableStateOf(initialText) }
    var error by remember { mutableStateOf<String?>(null) }
    error?.let {
        Text(stringResource(Res.string.limits_script_error) + ": " + it, color = MaterialTheme.colorScheme.error, style = MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(4.dp))
    }
    OutlinedTextField(
        value = text,
        onValueChange = { v ->
            text = v
            // Live, as on iOS: a script that parses replaces the working limits, one that does not leaves them.
            runCatching { importScript(v) }.fold(
                onSuccess = { error = null; onParsed(it) },
                onFailure = { error = it.message ?: it.toString() },
            )
        },
        modifier = Modifier.fillMaxWidth().height(320.dp),
        textStyle = MaterialTheme.typography.bodySmall.copy(fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace),
    )
}

@Composable
private fun FieldRow(field: LimitField, working: NPLimitsSet, update: (NPLimitsSet) -> Unit) {
    when (field) {
        is LimitField.Num -> OptionalNumber(
            stringResource(field.label), field.unit, field.get(working)?.let(::formatNumber), field.range.start, field.range.endInclusive,
            toValue = { it.toDoubleOrNull() }, onChange = { update(field.set(working, it)) },
        )
        is LimitField.Whole -> OptionalNumber(
            stringResource(field.label), field.unit, field.get(working)?.toString(), field.range.first.toDouble(), field.range.last.toDouble(),
            toValue = { it.toIntOrNull()?.toDouble() }, onChange = { update(field.set(working, it?.toInt())) },
        )
        is LimitField.Duration -> OptionalDuration(stringResource(field.label), field.get(working)) { update(field.set(working, it)) }
        is LimitField.Flag -> {
            val v = field.get(working)
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(field.label), Modifier.weight(1f))
                if (v == null) {
                    TextButton(onClick = { update(field.set(working, true)) }) { Text(stringResource(Res.string.limits_set_limit)) }
                } else {
                    Switch(checked = v, onCheckedChange = { update(field.set(working, it)) })
                    TextButton(onClick = { update(field.set(working, null)) }) { Text("✕") }
                }
            }
        }
        is LimitField.Allowed -> {
            val allowed = field.get(working)
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                Text(stringResource(field.label), Modifier.weight(1f))
                if (allowed == null) {
                    TextButton(onClick = { update(field.set(working, field.options.map { it.first })) }) { Text(stringResource(Res.string.limits_restrict)) }
                } else {
                    TextButton(onClick = { update(field.set(working, null)) }) { Text(stringResource(Res.string.limits_allow_all)) }
                }
            }
            allowed?.let { list ->
                field.options.forEach { (raw, label) ->
                    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(
                            checked = raw in list,
                            onCheckedChange = { on -> update(field.set(working, if (on) list + raw else list - raw)) },
                        )
                        Text(stringResource(label), style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }
        is LimitField.Note -> Text(stringResource(field.text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

/**
 * A number that is either unset ("Set limit") or typed. The value is committed as it is typed, and pulled into
 * [min]..[max] when the field loses focus, as on iOS.
 */
@Composable
private fun OptionalNumber(
    label: String, unit: String, current: String?, min: Double, max: Double,
    toValue: (String) -> Double?, onChange: (Double?) -> Unit,
) {
    var text by remember(current == null) { mutableStateOf(current ?: "") }
    var hadFocus by remember { mutableStateOf(false) }
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(1f))
        if (current == null) {
            TextButton(onClick = { val mid = (min + max) / 2; text = formatNumber(mid); onChange(mid) }) {
                Text(stringResource(Res.string.limits_set_limit))
            }
        } else {
            OutlinedTextField(
                value = text,
                onValueChange = { v ->
                    val filtered = v.filter { it.isDigit() || it == '.' }
                    text = filtered
                    toValue(filtered)?.let { onChange(it) }
                },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                singleLine = true,
                modifier = Modifier.width(110.dp).onFocusChanged { f ->
                    if (hadFocus && !f.isFocused) {
                        val clamped = toValue(text)?.coerceIn(min, max)
                        if (clamped != null) { text = formatNumber(clamped); onChange(clamped) }
                    }
                    hadFocus = f.isFocused
                },
            )
            Text(unit, Modifier.padding(horizontal = 4.dp), style = MaterialTheme.typography.bodySmall)
            TextButton(onClick = { text = ""; onChange(null) }) { Text("✕") }
        }
    }
}

@Composable
private fun OptionalDuration(label: String, seconds: Int?, onChange: (Int?) -> Unit) {
    var text by remember(seconds == null) { mutableStateOf(seconds?.let(::formatDuration) ?: "") }
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
        Text(label, Modifier.weight(1f))
        if (seconds == null) {
            TextButton(onClick = { text = formatDuration(DEFAULT_DURATION_SECONDS); onChange(DEFAULT_DURATION_SECONDS) }) {
                Text(stringResource(Res.string.limits_set_limit))
            }
        } else {
            OutlinedTextField(
                value = text,
                onValueChange = { v -> text = v; parseDuration(v)?.let { onChange(it) } },
                singleLine = true,
                placeholder = { Text("20m") },
                modifier = Modifier.width(110.dp),
            )
            TextButton(onClick = { text = ""; onChange(null) }) { Text("✕") }
        }
    }
}

/** Twenty minutes: the value a freshly set duration limit starts at (the iOS editor's). */
internal const val DEFAULT_DURATION_SECONDS = 1200

internal fun formatNumber(v: Double): String = if (v == v.toLong().toDouble()) v.toLong().toString() else v.toString()

internal fun formatDuration(seconds: Int): String = when {
    seconds % 3600 == 0 -> "${seconds / 3600}h"
    seconds % 60 == 0 -> "${seconds / 60}m"
    else -> "${seconds}s"
}

/** `90`, `90s`, `20m`, `1.5h` to seconds; null when it is not a duration. */
internal fun parseDuration(text: String): Int? {
    val t = text.trim()
    if (t.isEmpty()) return null
    val unit = t.last()
    val scale = when (unit) { 'h' -> 3600.0; 'm' -> 60.0; 's' -> 1.0; else -> return t.toDoubleOrNull()?.toInt() }
    return t.dropLast(1).toDoubleOrNull()?.let { (it * scale).toInt() }
}
