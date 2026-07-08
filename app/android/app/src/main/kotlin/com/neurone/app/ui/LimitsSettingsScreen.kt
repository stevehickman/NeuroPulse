package com.neurone.app.ui

import androidx.compose.foundation.layout.Arrangement
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
import com.neurone.core.protocol.NPBESTacsLimits
import com.neurone.core.protocol.NPLimitsSet
import com.neurone.core.protocol.NPLimitsStore
import com.neurone.core.protocol.NPPBMTranscranialLimits
import com.neurone.core.protocol.NPTDCSLimits
import com.neurone.core.protocol.NPVNSHRVLimits
import com.neurone.core.protocol.NPVisualStimLimits

// Port of iOS LimitsSettingsView (global tier). Edits the safety-relevant global dosage caps —
// they layer under the firmware hardware ceilings (NPHardwareLimits, which can never be
// exceeded) and are enforced by the validator. Blank field = no configured cap for that value.
// Full per-helmet / per-individual tiers and the remaining fields are a follow-up (OI-AND-LIMITS-01).

@Composable
fun LimitsSettingsScreen(
    store: NPLimitsStore,
    onDone: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val existing = store.globalLimits

    var pbmIntensity by remember { mutableStateOf(existing?.pbmTranscranial?.maxIntensityPercent.toField()) }
    var pbmDose by remember { mutableStateOf(existing?.pbmTranscranial?.maxSessionDoseJCm2.toField()) }
    var besMa by remember { mutableStateOf(existing?.besTacs?.maxIntensityMilliamps.toField()) }
    var tdcsMa by remember { mutableStateOf(existing?.tdcs?.maxIntensityMilliamps.toField()) }
    var vnsMa by remember { mutableStateOf(existing?.vnsHrv?.maxIntensityMilliamps.toField()) }
    var blockHighRisk by remember { mutableStateOf(existing?.visualStimulation?.blockHighRiskRange ?: false) }

    fun save() {
        // Preserve any fields this simplified editor doesn't expose; overwrite the ones it does.
        val base = existing ?: NPLimitsSet(name = "Global")
        val updated = base.copy(
            name = "Global",
            pbmTranscranial = (base.pbmTranscranial ?: NPPBMTranscranialLimits()).copy(
                maxIntensityPercent = pbmIntensity.toDoubleOrNull(),
                maxSessionDoseJCm2 = pbmDose.toDoubleOrNull(),
            ),
            besTacs = (base.besTacs ?: NPBESTacsLimits()).copy(maxIntensityMilliamps = besMa.toDoubleOrNull()),
            tdcs = (base.tdcs ?: NPTDCSLimits()).copy(maxIntensityMilliamps = tdcsMa.toDoubleOrNull()),
            vnsHrv = (base.vnsHrv ?: NPVNSHRVLimits()).copy(maxIntensityMilliamps = vnsMa.toDoubleOrNull()),
            visualStimulation = (base.visualStimulation ?: NPVisualStimLimits()).copy(blockHighRiskRange = blockHighRisk),
        )
        store.saveGlobalLimits(updated)
        onDone()
    }

    Column(modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp)) {
        Row(
            Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            TextButton(onClick = onDone) { Text("Cancel") }
            Text("Dosage limits", style = MaterialTheme.typography.titleMedium)
            Button(onClick = { save() }) { Text("Save") }
        }
        Spacer(Modifier.height(8.dp))
        Text(
            "Global caps layer under the firmware hardware limits, which can never be exceeded. " +
                "Leave a field blank for no configured cap.",
            style = MaterialTheme.typography.bodySmall,
        )
        Spacer(Modifier.height(12.dp))

        NumberField("PBM max intensity (%)", pbmIntensity) { pbmIntensity = it }
        NumberField("PBM max session dose (J/cm²)", pbmDose) { pbmDose = it }
        NumberField("BES/tACS max current (mA)", besMa) { besMa = it }
        NumberField("tDCS max current (mA)", tdcsMa) { tdcsMa = it }
        NumberField("VNS max current (mA)", vnsMa) { vnsMa = it }

        Spacer(Modifier.height(8.dp))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            Text("Block 3–30 Hz photoparoxysmal range", Modifier.weight(1f))
            Switch(checked = blockHighRisk, onCheckedChange = { blockHighRisk = it })
        }

        Spacer(Modifier.height(16.dp))
        OutlinedButton(onClick = { store.clearGlobalLimits(); onDone() }, modifier = Modifier.fillMaxWidth()) {
            Text("Clear all global limits")
        }
    }
}

@Composable
private fun NumberField(label: String, value: String, onChange: (String) -> Unit) {
    OutlinedTextField(
        value = value,
        onValueChange = { onChange(it.filter { c -> c.isDigit() || c == '.' }) },
        label = { Text(label) },
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
    )
}

private fun Double?.toField(): String = this?.let { if (it == it.toLong().toDouble()) it.toLong().toString() else it.toString() } ?: ""
