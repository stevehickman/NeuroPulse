using System;
using System.Diagnostics;

namespace NeurOne.Protocol;

/// <summary>
/// Opens an approved condition link in the user's default browser.
/// </summary>
/// <remarks>
/// An interface rather than a static call so the eventual Windows shell can
/// bind a view-model to it, and so tests can assert the confirmation flow
/// without launching a browser. See NP-COND-LINK-001 §4.
/// </remarks>
public interface IExternalBrowser
{
    /// <summary>
    /// Open <paramref name="verdict"/>'s URL, or do nothing if the verdict is
    /// not an allowed one.
    /// </summary>
    /// <returns><c>true</c> when the browser was launched.</returns>
    bool Open(NPLinkVerdict verdict);
}

/// <summary>
/// Hands the URL to the shell, which routes it to the registered https handler.
/// </summary>
public sealed class ShellExternalBrowser : IExternalBrowser
{
    public bool Open(NPLinkVerdict verdict)
    {
        // Defence in depth: the UI is expected to offer Open only for an allowed
        // verdict, but this is the single point where a condition link reaches
        // the OS, so it re-checks rather than trusting its caller. A verdict is
        // only ever constructed as allowed by NPConditionLinkPolicy.Evaluate.
        if (!verdict.Allowed || string.IsNullOrEmpty(verdict.Url)) return false;

        try
        {
            // UseShellExecute routes through the default browser. It is safe here
            // *because* the policy has already constrained the string to an https
            // URL on a public host — without that check the shell would launch
            // whatever app claimed the scheme.
            using var process = Process.Start(new ProcessStartInfo
            {
                FileName = verdict.Url,
                UseShellExecute = true,
            });
            return true;
        }
        catch (Exception ex) when (ex is System.ComponentModel.Win32Exception or InvalidOperationException)
        {
            // No registered https handler, or the shell refused. Nothing to fall
            // back to, and nothing worth logging — which conditions a user looks
            // up is UHDR-class (NP-COND-LINK-001 §2), so this failure stays local.
            return false;
        }
    }
}
