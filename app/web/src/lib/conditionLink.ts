// Condition external-link policy — NP-COND-LINK-001 §3.
//
// A condition `link` arrives from a parsed .npps file, and .npps files are
// user-authorable. Handing an arbitrary string to window.open is an injection
// surface, so every link is run through `evaluateConditionLink` before the UI
// offers to open it. Behaviour is pinned by npps/fixtures/condition_links.json,
// which the Swift, Kotlin, and C# ports are tested against too — change the
// vectors and all four implementations must follow.
//
// Scheme and authority are extracted by hand rather than by handing the string
// to the platform URL parser first. Those parsers normalise aggressively and
// they do not agree with each other: WHATWG turns `https:///wiki/Stroke` into
// host `wiki` (silently promoting a path segment to a hostname), while
// Foundation and java.net report an empty host for the same input. Any policy
// built on top of that divergence would be a different policy on each platform.
// The scan below is the same five steps in all four languages.

import type { NPConditionDefinition } from '../types/protocol';

export type NPLinkReason =
  | 'ok'
  | 'not_absolute'
  | 'scheme_not_https'
  | 'embedded_credentials'
  | 'missing_host'
  | 'local_or_private_host'
  | 'malformed';

export interface NPLinkVerdict {
  allowed: boolean;
  reason: NPLinkReason;
  /** Destination host, present only when allowed. Shown in the confirmation UI. */
  host?: string;
  /** Whitespace-trimmed URL to hand to the browser, present only when allowed. */
  url?: string;
}

const block = (reason: NPLinkReason): NPLinkVerdict => ({ allowed: false, reason });

/** RFC 3986 scheme: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) ":" */
const SCHEME = /^([a-zA-Z][a-zA-Z0-9+.-]*):/;

/**
 * Any ASCII control character or space, anywhere in the string. Checked BEFORE
 * parsing: URL parsers silently strip tabs and newlines and percent-encode
 * spaces, so a post-parse check would let them through.
 * Written as a code-point scan rather than a regex so the source stays free of
 * literal control characters, and so the Swift/Kotlin/C# ports read identically.
 */
function hasControlOrSpace(s: string): boolean {
  for (let i = 0; i < s.length; i++) {
    const c = s.charCodeAt(i);
    if (c <= 0x20 || c === 0x7f) return true;
  }
  return false;
}

export function evaluateConditionLink(raw: string): NPLinkVerdict {
  const trimmed = raw.trim();

  if (hasControlOrSpace(trimmed)) return block('malformed');

  // 1. Scheme must be present and must be exactly https. Scheme carries the
  //    whole attack surface: javascript: and data: execute, file: reads local
  //    disk, custom schemes deep-link into other installed apps.
  const schemeMatch = SCHEME.exec(trimmed);
  if (!schemeMatch) return block('not_absolute');
  if (schemeMatch[1].toLowerCase() !== 'https') return block('scheme_not_https');

  // 2. https must be followed by an authority. `https:wiki/Stroke` is a valid
  //    URL but has no host to show or to reach.
  const afterScheme = trimmed.slice(schemeMatch[0].length);
  if (!afterScheme.startsWith('//')) return block('not_absolute');

  // 3. Authority runs to the first path, query, or fragment delimiter.
  const authority = afterScheme.slice(2).split(/[/?#]/, 1)[0];
  if (authority === '') return block('missing_host');

  // 4. `https://en.wikipedia.org@evil.example.com/...` reads as Wikipedia to a
  //    human but resolves to evil.example.com. Reject before showing any host.
  if (authority.includes('@')) return block('embedded_credentials');

  const host = hostFromAuthority(authority);
  if (host === '') return block('missing_host');
  if (isLocalOrPrivateHost(host)) return block('local_or_private_host');

  return { allowed: true, reason: 'ok', host: host.toLowerCase(), url: trimmed };
}

/** Strip the port, and unwrap an IPv6 literal's brackets. */
function hostFromAuthority(authority: string): string {
  if (authority.startsWith('[')) {
    const close = authority.indexOf(']');
    return close === -1 ? '' : authority.slice(1, close);
  }
  const colon = authority.indexOf(':');
  return colon === -1 ? authority : authority.slice(0, colon);
}

/**
 * Loopback, link-local, mDNS, RFC1918, and single-label intranet hosts. A
 * crafted registry must not be able to aim the system browser at a service
 * bound to the user's own machine or LAN.
 */
export function isLocalOrPrivateHost(hostname: string): boolean {
  const host = hostname.toLowerCase();

  if (host === 'localhost' || host.endsWith('.localhost')) return true;
  if (host.endsWith('.local')) return true;

  if (host.includes(':')) return isPrivateIPv6(host);

  const octets = parseIPv4(host);
  if (octets) return isPrivateIPv4(octets);

  // A single-label name like `wiki` or `intranet` is not a public FQDN; it
  // resolves through the host's DNS search suffix, which lands on an intranet
  // machine. Public condition links are always dotted.
  return !host.includes('.');
}

function isPrivateIPv6(host: string): boolean {
  if (host === '::1' || host === '::') return true;
  // fe80::/10 link-local, fc00::/7 unique-local.
  return host.startsWith('fe80:') || host.startsWith('fc') || host.startsWith('fd');
}

function parseIPv4(host: string): number[] | null {
  const parts = host.split('.');
  if (parts.length !== 4) return null;

  const octets: number[] = [];
  for (const part of parts) {
    if (!/^\d{1,3}$/.test(part)) return null;
    const n = Number(part);
    if (n > 255) return null;
    octets.push(n);
  }
  return octets;
}

function isPrivateIPv4([a, b]: number[]): boolean {
  if (a === 127 || a === 0) return true;              // loopback, "this network"
  if (a === 10) return true;                          // RFC1918 10/8
  if (a === 192 && b === 168) return true;            // RFC1918 192.168/16
  if (a === 172 && b >= 16 && b <= 31) return true;   // RFC1918 172.16/12
  if (a === 169 && b === 254) return true;            // link-local
  return false;
}

// ─── UI-facing helpers ─────────────────────────────────────────────────────────

/** Human-readable explanation for a blocked link, shown in place of Open. */
export function linkBlockedMessage(reason: NPLinkReason): string {
  switch (reason) {
    case 'ok':
      return '';
    case 'not_absolute':
      return 'This condition has no full web address.';
    case 'scheme_not_https':
      return 'Only secure https links can be opened.';
    case 'embedded_credentials':
      return 'This link hides its real destination.';
    case 'missing_host':
      return 'This link has no destination site.';
    case 'local_or_private_host':
      return 'This link points to a private address.';
    case 'malformed':
      return 'This link is malformed.';
  }
}

/**
 * A condition paired with the verdict on its link. The UI lists every condition
 * regardless of verdict — a condition whose link fails policy is still valid
 * protocol metadata — and disables the Open action for blocked ones.
 */
export interface NPResolvedCondition {
  name: string;
  definition: NPConditionDefinition | null;
  verdict: NPLinkVerdict;
}

/** Resolve protocol `conditions` names against the loaded namespace. */
export function resolveConditions(
  names: readonly string[],
  registry: ReadonlyMap<string, NPConditionDefinition>
): NPResolvedCondition[] {
  return names.map(name => {
    const definition = registry.get(name) ?? null;
    return {
      name,
      definition,
      verdict: definition
        ? evaluateConditionLink(definition.link)
        : block('not_absolute'),
    };
  });
}

/**
 * Hand an approved URL to the OS browser. `noopener,noreferrer` drops the
 * Referer header so the destination never learns which app page the user came
 * from (NP-COND-LINK-001 §5).
 */
export function openConditionLink(verdict: NPLinkVerdict): boolean {
  if (!verdict.allowed || !verdict.url) return false;
  window.open(verdict.url, '_blank', 'noopener,noreferrer');
  return true;
}
