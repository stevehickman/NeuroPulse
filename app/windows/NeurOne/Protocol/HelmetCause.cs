// HelmetCause.cs — what a helmet's upload report means to a person (SWR-UP-02, SWR-HV-08, REQ-UPG-01).
//
// Windows has no hub transport yet, so nothing reads UPLOAD_CHECK here; this is the seam the transport will fill, matching
// iOS (`UploadVerdict`) and Android (`UploadVerdict`). Nothing is decided here: the reasons the helmet can refuse a protocol
// for, and how they are grouped by what the person can do next, are the shared core's (`helmetCause` in
// common/npps-core/src/library.rs), so Windows says what every other app says. The screen maps a cause to a locale key and
// compares no reason codes.

using System.Text.Json.Nodes;

namespace NeurOne.Protocol;

/// The core's reading of a helmet upload report. `RefusalKind` is the library's `helmetRefusal` (`tier`, `hardware_version`
/// or null), `NotReady` is a refusal only because the helmet had not yet heard its tier or hardware version (sending the same
/// protocol again can succeed), and `Cause` is what to say: `tier`, `hardware_version`, `not_ready`, `signature`,
/// `wrong_device`, `format_version`, `damaged`, `malformed`, `session_active`, `boot_refused`, `other`, `unmatched`, or null
/// for an accepted upload and for no answer.
sealed record HelmetCauseAnswer(string? RefusalKind, bool NotReady, string? Cause);

static class HelmetCause
{
    /// `status` is `accepted`, `refused` or null; `reason` is the refusal's `np_hub_status_t` as a positive number;
    /// `sentHash` is given when the screen is about the upload whose bytes hash to it, and then a final report for other bytes
    /// is `unmatched` whatever it said.
    public static HelmetCauseAnswer Of(string? status, int? reason, string? helmetHash = null, string? sentHash = null)
    {
        var request = new JsonObject { ["op"] = "helmetCause" };
        if (status is not null) request["status"] = status;
        if (reason is not null) request["reason"] = reason;
        if (helmetHash is not null) request["helmetHash"] = helmetHash;
        if (sentHash is not null) request["sentHash"] = sentHash;
        var answer = NppsCore.Library(request);
        return new HelmetCauseAnswer(
            (string?)answer["refusalKind"], (bool?)answer["notReady"] == true, (string?)answer["cause"]);
    }
}
