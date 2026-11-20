import Foundation

/// The clinician limits whose value is outside the range its field states, as `modality.field` keys. A set with any
/// is refused when it is saved or imported, never changed: the limit a clinician typed is the limit they meant, and
/// holding it inside its range would save one they did not choose (`NP-SW-001` SWR-UI-01 (f)). An absent limit is
/// "no limit" and never fails; a value that is not a finite number always does. The ranges are the generated
/// `NPBounds.Limit` table, the same one the limit fields read and the web and Compose editors check.
enum NPLimitRangeCheck {
    private struct Entry {
        let key: String
        let bound: NPBound
        let value: (NPLimitsSet) -> Double?
    }

    private static let entries: [Entry] = [
        Entry(
            key: "pbmTranscranial.maxIrradianceMWcm2",
            bound: NPBounds.Limit.PbmTranscranial.maxIrradianceMWcm2,
            value: { $0.pbmTranscranial?.maxIrradianceMWcm2 }
        ),
        Entry(
            key: "pbmTranscranial.maxFrequencyHz",
            bound: NPBounds.Limit.PbmTranscranial.maxFrequencyHz,
            value: { $0.pbmTranscranial?.maxFrequencyHz }
        ),
        Entry(
            key: "pbmTranscranial.maxDutyCyclePercent",
            bound: NPBounds.Limit.PbmTranscranial.maxDutyCyclePercent,
            value: { $0.pbmTranscranial?.maxDutyCyclePercent.map { Double($0) } }
        ),
        Entry(
            key: "pbmTranscranial.maxSessionDoseJCm2",
            bound: NPBounds.Limit.PbmTranscranial.maxSessionDoseJCm2,
            value: { $0.pbmTranscranial?.maxSessionDoseJCm2 }
        ),
        Entry(
            key: "pbmTranscranial.maxDailyDoseJCm2",
            bound: NPBounds.Limit.PbmTranscranial.maxDailyDoseJCm2,
            value: { $0.pbmTranscranial?.maxDailyDoseJCm2 }
        ),
        Entry(
            key: "pbmIntranasal.maxIrradianceMWcm2",
            bound: NPBounds.Limit.PbmIntranasal.maxIrradianceMWcm2,
            value: { $0.pbmIntranasal?.maxIrradianceMWcm2 }
        ),
        Entry(
            key: "pbmIntranasal.maxSessionDoseJCm2",
            bound: NPBounds.Limit.PbmIntranasal.maxSessionDoseJCm2,
            value: { $0.pbmIntranasal?.maxSessionDoseJCm2 }
        ),
        Entry(
            key: "besTacs.maxIntensityMilliamps",
            bound: NPBounds.Limit.BesTacs.maxIntensityMilliamps,
            value: { $0.besTacs?.maxIntensityMilliamps }
        ),
        Entry(
            key: "besTacs.maxFrequencyHz",
            bound: NPBounds.Limit.BesTacs.maxFrequencyHz,
            value: { $0.besTacs?.maxFrequencyHz }
        ),
        Entry(
            key: "besTacs.minFrequencyHz",
            bound: NPBounds.Limit.BesTacs.minFrequencyHz,
            value: { $0.besTacs?.minFrequencyHz }
        ),
        Entry(
            key: "besTacs.maxSessionsPerDay",
            bound: NPBounds.Limit.BesTacs.maxSessionsPerDay,
            value: { $0.besTacs?.maxSessionsPerDay.map { Double($0) } }
        ),
        Entry(
            key: "tdcs.maxIntensityMilliamps",
            bound: NPBounds.Limit.Tdcs.maxIntensityMilliamps,
            value: { $0.tdcs?.maxIntensityMilliamps }
        ),
        Entry(
            key: "tdcs.maxSessionsPerDay",
            bound: NPBounds.Limit.Tdcs.maxSessionsPerDay,
            value: { $0.tdcs?.maxSessionsPerDay.map { Double($0) } }
        ),
        Entry(
            key: "vnsHrv.maxIntensityMilliamps",
            bound: NPBounds.Limit.VnsHrv.maxIntensityMilliamps,
            value: { $0.vnsHrv?.maxIntensityMilliamps }
        ),
        Entry(
            key: "vnsHrv.maxFrequencyHz",
            bound: NPBounds.Limit.VnsHrv.maxFrequencyHz,
            value: { $0.vnsHrv?.maxFrequencyHz }
        ),
        Entry(
            key: "audioEntrainment.maxVolumeDb",
            bound: NPBounds.Limit.AudioEntrainment.maxVolumeDb,
            value: { $0.audioEntrainment?.maxVolumeDb }
        ),
        Entry(
            key: "audioEntrainment.maxBinauralBeatsHz",
            bound: NPBounds.Limit.AudioEntrainment.maxBinauralBeatsHz,
            value: { $0.audioEntrainment?.maxBinauralBeatsHz }
        ),
        Entry(
            key: "audioEntrainment.maxIsochronicTonesHz",
            bound: NPBounds.Limit.AudioEntrainment.maxIsochronicTonesHz,
            value: { $0.audioEntrainment?.maxIsochronicTonesHz }
        ),
        Entry(
            key: "visualStimulation.maxFrequencyHz",
            bound: NPBounds.Limit.VisualStimulation.maxFrequencyHz,
            value: { $0.visualStimulation?.maxFrequencyHz }
        ),
        Entry(
            key: "visualStimulation.minFrequencyHz",
            bound: NPBounds.Limit.VisualStimulation.minFrequencyHz,
            value: { $0.visualStimulation?.minFrequencyHz }
        ),
        Entry(
            key: "tms.maxIntensityPercentMT",
            bound: NPBounds.Limit.Tms.maxIntensityPercentMT,
            value: { $0.tms?.maxIntensityPercentMT.map { Double($0) } }
        ),
        Entry(
            key: "tms.maxPulsesPerSession",
            bound: NPBounds.Limit.Tms.maxPulsesPerSession,
            value: { $0.tms?.maxPulsesPerSession.map { Double($0) } }
        ),
        Entry(
            key: "tms.maxPulsesPerDay",
            bound: NPBounds.Limit.Tms.maxPulsesPerDay,
            value: { $0.tms?.maxPulsesPerDay.map { Double($0) } }
        ),
        Entry(
            key: "tms.maxSessionsPerWeek",
            bound: NPBounds.Limit.Tms.maxSessionsPerWeek,
            value: { $0.tms?.maxSessionsPerWeek.map { Double($0) } }
        ),
        Entry(
            key: "pbmDeep1170nm.maxIntensityMWcm2",
            bound: NPBounds.Limit.PbmDeep1170nm.maxIntensityMWcm2,
            value: { $0.pbmDeep1170nm?.maxIntensityMWcm2 }
        ),
        Entry(
            key: "clinicalTacs.maxIntensityMilliamps",
            bound: NPBounds.Limit.ClinicalTacs.maxIntensityMilliamps,
            value: { $0.clinicalTacs?.maxIntensityMilliamps }
        ),
        Entry(
            key: "hdTdcs.maxIntensityMilliamps",
            bound: NPBounds.Limit.HdTdcs.maxIntensityMilliamps,
            value: { $0.hdTdcs?.maxIntensityMilliamps }
        ),
        Entry(
            key: "cervicalVns.maxIntensityMilliamps",
            bound: NPBounds.Limit.CervicalVns.maxIntensityMilliamps,
            value: { $0.cervicalVns?.maxIntensityMilliamps }
        ),
        Entry(
            key: "vibrotactile40hz.maxIntensityG",
            bound: NPBounds.Limit.Vibrotactile40hz.maxIntensityG,
            value: { $0.vibrotactile40hz?.maxIntensityG }
        )
    ]

    static func outOfRange(_ limits: NPLimitsSet) -> [String] {
        entries.compactMap { entry in
            guard let v = entry.value(limits) else { return nil }
            return v.isFinite && entry.bound.range.contains(v) ? nil : entry.key
        }
    }
}
