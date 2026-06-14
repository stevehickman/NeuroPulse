import SwiftUI

// MARK: - ModalityEditorRow

struct ModalityEditorRow: View {
    @Binding var modality: NPProtocolModality
    let isExpanded: Bool
    let onToggleExpand: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Header
            Button(action: onToggleExpand) {
                HStack(spacing: 10) {
                    Toggle("", isOn: $modality.enabled)
                        .labelsHidden()
                        .scaleEffect(0.85)

                    Image(systemName: modality.modalityType.systemImage)
                        .foregroundColor(modality.enabled ? .accentColor : .secondary)
                        .frame(width: 20)

                    VStack(alignment: .leading, spacing: 1) {
                        Text(modality.modalityType.displayName)
                            .font(.subheadline.weight(.medium))
                            .foregroundColor(modality.enabled ? .primary : .secondary)
                        Text(modality.modalityType.consumerName)
                            .font(.caption2)
                            .foregroundColor(.secondary)
                    }

                    Spacer()

                    if modality.modalityType.tier == .t2 {
                        Text("T2")
                            .font(.caption2.bold())
                            .padding(.horizontal, 5).padding(.vertical, 2)
                            .background(Color.blue.opacity(0.15))
                            .foregroundColor(.blue)
                            .clipShape(Capsule())
                    } else if modality.modalityType.tier == .accessory {
                        Text("Provisional")
                            .font(.caption2)
                            .padding(.horizontal, 5).padding(.vertical, 2)
                            .background(Color.orange.opacity(0.15))
                            .foregroundColor(.orange)
                            .clipShape(Capsule())
                    }

                    Image(systemName: isExpanded ? "chevron.up" : "chevron.down")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .padding(.vertical, 4)

            // Expanded params
            if isExpanded {
                VStack(alignment: .leading, spacing: 0) {
                    Divider().padding(.leading, 30)
                    paramControls
                        .padding(.leading, 30)
                        .padding(.vertical, 8)
                    Divider().padding(.leading, 30)
                    IntervalConfigView(interval: $modality.interval)
                        .padding(.leading, 30)
                        .padding(.top, 8)
                }
            }
        }
    }

    @ViewBuilder
    private var paramControls: some View {
        switch modality.params {
        case .pbmTranscranial(let p):
            PBMTranscranialParamsView(params: Binding(
                get: { p },
                set: { modality.params = .pbmTranscranial($0) }
            ))
        case .pbmIntranasal(let p):
            PBMIntranasalParamsView(params: Binding(
                get: { p },
                set: { modality.params = .pbmIntranasal($0) }
            ))
        case .eegNeurofeedback(let p):
            EEGNeurofeedbackParamsView(params: Binding(
                get: { p },
                set: { modality.params = .eegNeurofeedback($0) }
            ))
        case .besTacs(let p):
            BESTacsParamsView(params: Binding(
                get: { p },
                set: { modality.params = .besTacs($0) }
            ))
        case .tdcs(let p):
            TDCSParamsView(params: Binding(
                get: { p },
                set: { modality.params = .tdcs($0) }
            ))
        case .vnsHRV(let p):
            VNSHRVParamsView(params: Binding(
                get: { p },
                set: { modality.params = .vnsHRV($0) }
            ))
        case .audioEntrainment(let p):
            AudioEntrainmentParamsView(params: Binding(
                get: { p },
                set: { modality.params = .audioEntrainment($0) }
            ))
        case .visualStimulation(let p):
            VisualStimParamsView(params: Binding(
                get: { p },
                set: { modality.params = .visualStimulation($0) }
            ))
        case .qeeg21ch(let p):
            T2ParamsView(label: "21-Channel qEEG") {
                qEEG21chParamsView(params: Binding(
                    get: { p },
                    set: { modality.params = .qeeg21ch($0) }
                ))
            }
        case .tms(let p):
            T2ParamsView(label: "TMS") {
                TMSParamsView(params: Binding(
                    get: { p },
                    set: { modality.params = .tms($0) }
                ))
            }
        case .pbmDeep1170nm(let p):
            T2ParamsView(label: "Deep PBM 1170nm") {
                DeepPBMParamsView(params: Binding(
                    get: { p },
                    set: { modality.params = .pbmDeep1170nm($0) }
                ))
            }
        case .clinicalTacs(let p):
            T2ParamsView(label: "Clinical tACS") {
                ClinicalTacsParamsView(params: Binding(
                    get: { p },
                    set: { modality.params = .clinicalTacs($0) }
                ))
            }
        case .hdTdcs(let p):
            T2ParamsView(label: "HD-tDCS") {
                HDTdcsParamsView(params: Binding(
                    get: { p },
                    set: { modality.params = .hdTdcs($0) }
                ))
            }
        case .cervicalVns(let p):
            T2ParamsView(label: "Cervical VNS") {
                CervicalVnsParamsView(params: Binding(
                    get: { p },
                    set: { modality.params = .cervicalVns($0) }
                ))
            }
        case .vibrotactile40hz(let p):
            VibrotactileParamsView(params: Binding(
                get: { p },
                set: { modality.params = .vibrotactile40hz($0) }
            ))
        }
    }
}

// MARK: - T2 wrapper

struct T2ParamsView<Content: View>: View {
    let label: String
    @ViewBuilder let content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label("Requires T2 Hardware", systemImage: "exclamationmark.triangle")
                .font(.caption)
                .foregroundColor(.orange)
            content
        }
    }
}

// MARK: - PBM Transcranial

struct PBMTranscranialParamsView: View {
    @Binding var params: NPPBMTranscranialParams

    private let freqPresets: [(String, Double)] = [
        ("CW", 0), ("2Hz", 2), ("6Hz", 6), ("10Hz", 10), ("20Hz", 20), ("40Hz", 40)
    ]

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            // Zone selection
            VStack(alignment: .leading, spacing: 4) {
                Text("Zones").font(.caption).foregroundColor(.secondary)
                Picker("Zones", selection: $params.zones) {
                    ForEach(NPPBMTranscranialParams.ZoneSelection.allCases) { sel in
                        Text(sel.displayName).tag(sel)
                    }
                }
                .pickerStyle(.menu)
            }

            // Wavelength
            VStack(alignment: .leading, spacing: 4) {
                Text("Wavelength").font(.caption).foregroundColor(.secondary)
                Picker("Wavelength", selection: $params.wavelength) {
                    ForEach(NPPBMTranscranialParams.Wavelength.allCases) { wl in
                        Text(wl.displayName + (wl.requiresSmartModule ? " ★" : "")).tag(wl)
                    }
                }
                .pickerStyle(.menu)
                if params.wavelength.requiresSmartModule {
                    Text("★ Smart Module required")
                        .font(.caption2)
                        .foregroundColor(.orange)
                }
            }

            // Intensity
            SliderRow(
                label: "Intensity",
                value: $params.intensityPercent,
                range: 0...100,
                format: { "\(Int($0))%" }
            )

            // Frequency
            VStack(alignment: .leading, spacing: 4) {
                Text("Frequency").font(.caption).foregroundColor(.secondary)
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 6) {
                        ForEach(freqPresets, id: \.0) { preset in
                            Button {
                                params.frequencyHz = preset.1
                            } label: {
                                Text(preset.0)
                                    .font(.caption)
                                    .padding(.horizontal, 10).padding(.vertical, 5)
                                    .background(params.frequencyHz == preset.1 ? Color.accentColor : Color(.systemGray5))
                                    .foregroundColor(params.frequencyHz == preset.1 ? .white : .primary)
                                    .clipShape(Capsule())
                            }
                            .buttonStyle(.plain)
                        }
                    }
                }
            }

            // Duty cycle — only when pulsed
            if params.frequencyHz > 0 {
                Stepper(
                    value: $params.dutyCyclePercent,
                    in: 5...25,
                    step: 5
                ) {
                    HStack {
                        Text("Duty Cycle")
                            .font(.caption)
                            .foregroundColor(.secondary)
                        Spacer()
                        Text("\(params.dutyCyclePercent)%")
                            .font(.caption)
                    }
                }
            }
        }
    }
}

// MARK: - PBM Intranasal

struct PBMIntranasalParamsView: View {
    @Binding var params: NPPBMIntranasalParams

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SliderRow(label: "Intensity", value: $params.intensityPercent, range: 0...100, format: { "\(Int($0))%" })
            SliderRow(label: "Frequency (Hz)", value: $params.frequencyHz, range: 0...40, format: { "\(Int($0))Hz" })
            if params.frequencyHz > 0 {
                Stepper(value: $params.dutyCyclePercent, in: 5...25, step: 5) {
                    HStack {
                        Text("Duty Cycle").font(.caption).foregroundColor(.secondary)
                        Spacer()
                        Text("\(params.dutyCyclePercent)%").font(.caption)
                    }
                }
            }
        }
    }
}

// MARK: - EEG Neurofeedback

struct EEGNeurofeedbackParamsView: View {
    @Binding var params: NPEEGNeurofeedbackParams

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            VStack(alignment: .leading, spacing: 4) {
                Text("Band").font(.caption).foregroundColor(.secondary)
                Picker("Band", selection: $params.band) {
                    ForEach(NPEEGNeurofeedbackParams.EEGBand.allCases) { band in
                        Text(band.displayName).tag(band)
                    }
                }
                .pickerStyle(.menu)
                Text("\(params.band.hzRange.lowerBound, specifier: "%.1f")–\(params.band.hzRange.upperBound, specifier: "%.1f") Hz")
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }

            VStack(alignment: .leading, spacing: 4) {
                Text("Channels").font(.caption).foregroundColor(.secondary)
                Picker("Channels", selection: $params.channels) {
                    ForEach(NPEEGNeurofeedbackParams.ChannelSelection.allCases) { ch in
                        Text(ch.displayName).tag(ch)
                    }
                }
                .pickerStyle(.menu)
            }

            Toggle("Closed-Loop EEG Adaptation", isOn: $params.closedLoopEnabled)
                .font(.caption)
        }
    }
}

// MARK: - BES / tACS

struct BESTacsParamsView: View {
    @Binding var params: NPBESTacsParams

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SliderRow(
                label: "Frequency (Hz)",
                value: $params.frequencyHz,
                range: 0.5...40,
                format: { String(format: "%.1f Hz", $0) }
            )
            SliderRow(
                label: "Intensity (mA)",
                value: $params.intensityMilliamps,
                range: 0...1,
                format: { String(format: "%.2f mA", $0) }
            )
            VStack(alignment: .leading, spacing: 4) {
                Text("Waveform").font(.caption).foregroundColor(.secondary)
                Picker("Waveform", selection: $params.waveform) {
                    ForEach(NPBESTacsParams.Waveform.allCases) { wf in
                        Text(wf.displayName).tag(wf)
                    }
                }
                .pickerStyle(.segmented)
            }
        }
    }
}

// MARK: - tDCS

struct TDCSParamsView: View {
    @Binding var params: NPTDCSParams
    @State private var pairsText: String = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SliderRow(
                label: "Intensity (mA)",
                value: $params.intensityMilliamps,
                range: 0.1...2.0,
                format: { String(format: "%.2f mA", $0) }
            )
            HStack {
                Text("Ramp (30s)").font(.caption).foregroundColor(.secondary)
                Spacer()
                Text("Hardware-enforced")
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
            VStack(alignment: .leading, spacing: 4) {
                Text("Electrode Pairs (e.g. Fp1,P3)").font(.caption).foregroundColor(.secondary)
                ForEach(Array(params.electrodePairs.enumerated()), id: \.offset) { idx, pair in
                    HStack {
                        Text(pair.joined(separator: " → "))
                            .font(.caption)
                        Spacer()
                        Button {
                            params.electrodePairs.remove(at: idx)
                        } label: {
                            Image(systemName: "minus.circle.fill")
                                .foregroundColor(.red)
                        }
                        .buttonStyle(.plain)
                    }
                }
                HStack {
                    TextField("Anode,Cathode", text: $pairsText)
                        .font(.caption)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                    Button("Add") {
                        let parts = pairsText.split(separator: ",").map { String($0).trimmingCharacters(in: .whitespaces) }
                        if parts.count >= 2 {
                            params.electrodePairs.append(parts)
                        }
                        pairsText = ""
                    }
                    .font(.caption)
                    .disabled(pairsText.isEmpty)
                }
            }
        }
        .onAppear { pairsText = "" }
    }
}

// MARK: - VNS + HRV

struct VNSHRVParamsView: View {
    @Binding var params: NPVNSHRVParams

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SliderRow(label: "Frequency (Hz)", value: $params.frequencyHz, range: 1...25, format: { "\(Int($0)) Hz" })
            SliderRow(label: "Intensity (mA)", value: $params.intensityMilliamps, range: 0...2, format: { String(format: "%.2f mA", $0) })

            VStack(alignment: .leading, spacing: 4) {
                Text("HRV Protocol").font(.caption).foregroundColor(.secondary)
                Picker("HRV Protocol", selection: $params.hrvProtocol) {
                    ForEach(NPVNSHRVParams.HRVProtocol.allCases) { p in
                        Text(p.displayName).tag(p)
                    }
                }
                .pickerStyle(.menu)
            }

            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("Breathing Rate").font(.caption).foregroundColor(.secondary)
                    Spacer()
                    Text(String(format: "%.1f breaths/min", params.resonanceBreathingRate))
                        .font(.caption)
                }
                Slider(value: $params.resonanceBreathingRate, in: 4...7, step: 0.5)
            }
        }
    }
}

// MARK: - Audio Entrainment

struct AudioEntrainmentParamsView: View {
    @Binding var params: NPAudioEntrainmentParams
    @State private var binauralEnabled: Bool = false
    @State private var isochronicEnabled: Bool = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            // Binaural beats
            VStack(alignment: .leading, spacing: 4) {
                Toggle("Binaural Beats", isOn: Binding(
                    get: { params.binauralBeatsHz != nil },
                    set: { params.binauralBeatsHz = $0 ? 20 : nil }
                ))
                .font(.caption)
                if params.binauralBeatsHz != nil {
                    SliderRow(
                        label: "Binaural Hz",
                        value: Binding(
                            get: { params.binauralBeatsHz ?? 20 },
                            set: { params.binauralBeatsHz = $0 }
                        ),
                        range: 0.5...40,
                        format: { String(format: "%.1f Hz", $0) }
                    )
                }
            }

            // Isochronic tones
            VStack(alignment: .leading, spacing: 4) {
                Toggle("Isochronic Tones", isOn: Binding(
                    get: { params.isochronicTonesHz != nil },
                    set: { params.isochronicTonesHz = $0 ? 40 : nil }
                ))
                .font(.caption)
                if params.isochronicTonesHz != nil {
                    SliderRow(
                        label: "Isochronic Hz",
                        value: Binding(
                            get: { params.isochronicTonesHz ?? 40 },
                            set: { params.isochronicTonesHz = $0 }
                        ),
                        range: 0.5...40,
                        format: { String(format: "%.1f Hz", $0) }
                    )
                }
            }

            // Noise
            VStack(alignment: .leading, spacing: 4) {
                Text("Noise").font(.caption).foregroundColor(.secondary)
                Picker("Noise", selection: Binding(
                    get: { params.noiseType?.rawValue ?? "none" },
                    set: { val in
                        if val == "none" { params.noiseType = nil }
                        else { params.noiseType = NPAudioEntrainmentParams.NoiseType(rawValue: val) }
                    }
                )) {
                    Text("None").tag("none")
                    ForEach(NPAudioEntrainmentParams.NoiseType.allCases) { nt in
                        Text(nt.displayName).tag(nt.rawValue)
                    }
                }
                .pickerStyle(.segmented)
            }

            SliderRow(label: "Volume", value: $params.volumePercent, range: 0...100, format: { "\(Int($0))%" })
            Toggle("EEG-Adaptive Frequency", isOn: $params.eegAdaptive).font(.caption)
            Toggle("Bone Conduction Breathing Pacer", isOn: $params.boneConductionPacer).font(.caption)
        }
    }
}

// MARK: - Visual Stimulation

struct VisualStimParamsView: View {
    @Binding var params: NPVisualStimParams

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SliderRow(
                label: "Frequency (Hz)",
                value: $params.frequencyHz,
                range: 0.5...100,
                format: { String(format: "%.1f Hz", $0) }
            )

            VStack(alignment: .leading, spacing: 4) {
                Text("Mode").font(.caption).foregroundColor(.secondary)
                Picker("Mode", selection: $params.mode) {
                    ForEach(NPVisualStimParams.VisualMode.allCases) { m in
                        Text(m.displayName).tag(m)
                    }
                }
                .pickerStyle(.menu)
            }

            if params.mode == .emdr {
                SliderRow(
                    label: "EMDR Cadence (Hz)",
                    value: $params.emdrCadenceHz,
                    range: 0.5...4,
                    format: { String(format: "%.1f Hz", $0) }
                )
            }

            if params.mode == .retinalPBM || params.mode == .modeF {
                Toggle("Enable Mode F (Invisible NIR)", isOn: $params.enableModeF)
                    .font(.caption)
                if params.enableModeF {
                    Text("808–830nm retinal PBM during normal-looking wear. Regulatory opinion required before clinical claims.")
                        .font(.caption2)
                        .foregroundColor(.orange)
                }
            }
        }
    }
}

// MARK: - T2: 21-ch qEEG

struct qEEG21chParamsView: View {
    @Binding var params: NPqEEG21chParams

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            VStack(alignment: .leading, spacing: 4) {
                Text("Montage").font(.caption).foregroundColor(.secondary)
                Picker("Montage", selection: $params.montage) {
                    ForEach(NPqEEG21chParams.Montage.allCases) { m in
                        Text(m.displayName).tag(m)
                    }
                }
                .pickerStyle(.menu)
            }
            Toggle("sLORETA Source Imaging", isOn: $params.sloretaEnabled).font(.caption)
            VStack(alignment: .leading, spacing: 4) {
                Text("Reference").font(.caption).foregroundColor(.secondary)
                Picker("Reference", selection: $params.reference) {
                    ForEach(NPqEEG21chParams.Reference.allCases) { r in
                        Text(r.displayName).tag(r)
                    }
                }
                .pickerStyle(.menu)
            }
        }
    }
}

// MARK: - T2: TMS

struct TMSParamsView: View {
    @Binding var params: NPTMSParams

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            VStack(alignment: .leading, spacing: 4) {
                Text("Protocol").font(.caption).foregroundColor(.secondary)
                Picker("Protocol", selection: $params.tmsProtocol) {
                    ForEach(NPTMSParams.TMSProtocol.allCases) { p in
                        Text(p.displayName).tag(p)
                    }
                }
                .pickerStyle(.menu)
            }
            SliderRow(label: "Frequency (Hz)", value: $params.frequencyHz, range: 1...20, format: { "\(Int($0)) Hz" })
            // swiftlint:disable:next line_length
            SliderRow(label: "Intensity (%MT)", value: Binding(get: { Double(params.intensityPercentMT) }, set: { params.intensityPercentMT = Int($0) }), range: 60...120, format: { "\(Int($0))%MT" })
            VStack(alignment: .leading, spacing: 4) {
                Text("Target").font(.caption).foregroundColor(.secondary)
                Picker("Target", selection: $params.target) {
                    ForEach(NPTMSParams.TMSTarget.allCases) { t in
                        Text(t.displayName).tag(t)
                    }
                }
                .pickerStyle(.menu)
            }
            Stepper(value: $params.pulseCount, in: 100...3000, step: 100) {
                HStack {
                    Text("Pulse Count").font(.caption).foregroundColor(.secondary)
                    Spacer()
                    Text("\(params.pulseCount)").font(.caption)
                }
            }
        }
    }
}

// MARK: - T2: Deep PBM 1170nm

struct DeepPBMParamsView: View {
    @Binding var params: NPDeepPBM1170Params

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SliderRow(label: "Intensity (mW/cm²)", value: $params.intensityMWcm2, range: 100...1000, format: { "\(Int($0)) mW/cm²" })
            SliderRow(label: "Frequency (Hz)", value: $params.frequencyHz, range: 0...40, format: { "\(Int($0)) Hz" })
            if params.frequencyHz > 0 {
                Stepper(value: $params.dutyCyclePercent, in: 5...25, step: 5) {
                    HStack {
                        Text("Duty Cycle").font(.caption).foregroundColor(.secondary)
                        Spacer()
                        Text("\(params.dutyCyclePercent)%").font(.caption)
                    }
                }
            }
        }
    }
}

// MARK: - T2: Clinical tACS

struct ClinicalTacsParamsView: View {
    @Binding var params: NPClinicalTacsParams

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SliderRow(label: "Frequency (Hz)", value: $params.frequencyHz, range: 0.5...40, format: { String(format: "%.1f Hz", $0) })
            SliderRow(label: "Intensity (mA)", value: $params.intensityMilliamps, range: 0...4, format: { String(format: "%.2f mA", $0) })
            Stepper(value: $params.channelCount, in: 2...16, step: 2) {
                HStack {
                    Text("Channels").font(.caption).foregroundColor(.secondary)
                    Spacer()
                    Text("\(params.channelCount)").font(.caption)
                }
            }
        }
    }
}

// MARK: - T2: HD-tDCS

struct HDTdcsParamsView: View {
    @Binding var params: NPHDTdcsParams

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            VStack(alignment: .leading, spacing: 4) {
                Text("Target").font(.caption).foregroundColor(.secondary)
                Picker("Target", selection: $params.target) {
                    ForEach(NPTMSParams.TMSTarget.allCases) { t in
                        Text(t.displayName).tag(t)
                    }
                }
                .pickerStyle(.menu)
            }
            VStack(alignment: .leading, spacing: 4) {
                Text("Montage").font(.caption).foregroundColor(.secondary)
                Picker("Montage", selection: $params.montage) {
                    ForEach(NPHDTdcsParams.Montage.allCases) { m in
                        Text(m.displayName).tag(m)
                    }
                }
                .pickerStyle(.menu)
            }
            // swiftlint:disable:next line_length
            SliderRow(label: "Intensity (mA)", value: $params.intensityMilliamps, range: 0.5...2.0, format: { String(format: "%.2f mA", $0) })
        }
    }
}

// MARK: - T2: Cervical VNS

struct CervicalVnsParamsView: View {
    @Binding var params: NPCervicalVnsParams

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            SliderRow(label: "Frequency (Hz)", value: $params.frequencyHz, range: 1...25, format: { "\(Int($0)) Hz" })
            SliderRow(label: "Intensity (mA)", value: $params.intensityMilliamps, range: 0...2, format: { String(format: "%.2f mA", $0) })
            Label("Cardiac rhythm interlock is always active — enforced by safety MCU.", systemImage: "heart.fill")
                .font(.caption2)
                .foregroundColor(.secondary)
        }
    }
}

// MARK: - Vibrotactile 40Hz

struct VibrotactileParamsView: View {
    @Binding var params: NPVibrotactileParams

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Text("Frequency").font(.caption).foregroundColor(.secondary)
                Spacer()
                Text("40Hz (locked)")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            SliderRow(label: "Intensity (G)", value: $params.intensityG, range: 0.6...1.2, format: { String(format: "%.2f G", $0) })
            Toggle("Sync to Audio Session", isOn: $params.syncToAudio).font(.caption)
            Toggle("Sync to Visual Session", isOn: $params.syncToVisual).font(.caption)
            Label("Provisional — pending HOPE Phase 3 results (mid-2026).", systemImage: "clock.badge.questionmark")
                .font(.caption2)
                .foregroundColor(.orange)
        }
    }
}

// MARK: - Interval Config View

struct IntervalConfigView: View {
    @Binding var interval: NPIntervalConfig

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Toggle("Run in Intervals", isOn: Binding(
                get: { !interval.isContinuous },
                set: { on in
                    if on {
                        interval = NPIntervalConfig(intervalOnSeconds: 120, intervalOffSeconds: 60, repeatCount: nil)
                    } else {
                        interval = .continuous
                    }
                }
            ))
            .font(.caption)

            if !interval.isContinuous {
                Stepper(
                    value: Binding(
                        get: { interval.intervalOnSeconds / 60 },
                        set: { interval.intervalOnSeconds = $0 * 60 }
                    ),
                    in: 1...120
                ) {
                    HStack {
                        Text("On-time").font(.caption).foregroundColor(.secondary)
                        Spacer()
                        Text(formatTime(interval.intervalOnSeconds)).font(.caption)
                    }
                }

                Stepper(
                    value: Binding(
                        get: { interval.intervalOffSeconds / 60 },
                        set: { interval.intervalOffSeconds = $0 * 60 }
                    ),
                    in: 0...120
                ) {
                    HStack {
                        Text("Off-time").font(.caption).foregroundColor(.secondary)
                        Spacer()
                        Text(formatTime(interval.intervalOffSeconds)).font(.caption)
                    }
                }

                HStack {
                    Text("Repeat").font(.caption).foregroundColor(.secondary)
                    Spacer()
                    Picker("Repeat", selection: Binding(
                        get: { interval.repeatCount == nil ? "until_end" : "custom" },
                        set: { val in
                            interval.repeatCount = val == "until_end" ? nil : 5
                        }
                    )) {
                        Text("Until End").tag("until_end")
                        Text("Custom Count").tag("custom")
                    }
                    .pickerStyle(.menu)
                    .font(.caption)
                }

                if interval.repeatCount != nil {
                    Stepper(
                        value: Binding(
                            get: { interval.repeatCount ?? 1 },
                            set: { interval.repeatCount = $0 }
                        ),
                        in: 1...1000
                    ) {
                        HStack {
                            Text("Count").font(.caption).foregroundColor(.secondary)
                            Spacer()
                            Text("\(interval.repeatCount ?? 1)×").font(.caption)
                        }
                    }
                }
            }
        }
        .padding(.bottom, 4)
    }

    private func formatTime(_ s: Int) -> String {
        if s < 60 { return "\(s)s" }
        return "\(s / 60)m"
    }
}

// MARK: - Reusable Slider Row

struct SliderRow: View {
    let label: String
    @Binding var value: Double
    let range: ClosedRange<Double>
    let format: (Double) -> String

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            HStack {
                Text(label)
                    .font(.caption)
                    .foregroundColor(.secondary)
                Spacer()
                Text(format(value))
                    .font(.caption.monospacedDigit())
            }
            Slider(value: $value, in: range)
        }
    }
}

// MARK: - Modality Picker Sheet

struct ModalityPickerSheet: View {
    @Environment(\.dismiss) private var dismiss
    let existingTypes: Set<NPModalityType>
    let onAdd: (NPModalityType) -> Void

    var body: some View {
        NavigationStack {
            List {
                modalitySection(title: "T1 Modalities", tier: .t1)
                modalitySection(title: "T2 Modalities (Requires T2 hardware)", tier: .t2)
                modalitySection(title: "Accessories (Provisional)", tier: .accessory)
            }
            .navigationTitle("Add Modality")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { dismiss() }
                }
            }
        }
    }

    @ViewBuilder
    private func modalitySection(title: String, tier: NPModalityTier) -> some View {
        Section(title) {
            ForEach(NPModalityType.allCases.filter { $0.tier == tier }) { type in
                let isAdded = existingTypes.contains(type)
                Button {
                    if !isAdded {
                        onAdd(type)
                        dismiss()
                    }
                } label: {
                    HStack(spacing: 12) {
                        Image(systemName: type.systemImage)
                            .foregroundColor(isAdded ? .secondary : .accentColor)
                            .frame(width: 24)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(type.displayName)
                                .font(.subheadline)
                                .foregroundColor(isAdded ? .secondary : .primary)
                            Text(type.shortDescription)
                                .font(.caption2)
                                .foregroundColor(.secondary)
                                .lineLimit(2)
                        }
                        Spacer()
                        if isAdded {
                            Image(systemName: "checkmark")
                                .foregroundColor(.secondary)
                                .font(.caption)
                        }
                    }
                }
                .disabled(isAdded)
            }
        }
    }
}
