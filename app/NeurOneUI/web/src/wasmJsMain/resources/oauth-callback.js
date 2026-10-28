// Hands the address this window was sent to (it carries the sign-in's answer) to the app's page, and closes the window (SWR-MG-19).
// A BroadcastChannel is used and not window.opener: a provider's sign-in page can set Cross-Origin-Opener-Policy, which severs the
// opener, and a channel reaches only pages of this same origin. The address is then taken out of this window's history.
(function () {
  var channel = new BroadcastChannel('neurone-oauth');
  channel.postMessage({ url: location.href });
  channel.close();
  history.replaceState(null, '', location.pathname);
  window.close();
})();
