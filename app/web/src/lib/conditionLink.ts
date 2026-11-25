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

import type { NPConditionDefinition } from '../../../../common/types/protocol';
import { t } from '../../../../common/lib/i18n';

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
 * Loopback, link-local, mDNS, RFC1918, CGNAT, multicast, and single-label
 * intranet hosts. A crafted registry must not be able to aim the system browser
 * at a service bound to the user's own machine or LAN.
 *
 * A numeric host is accepted only in canonical dotted-quad form: browsers read
 * `127.1`, `0x7f.0.0.1`, `0177.0.0.1` and `2130706433` as 127.0.0.1, so a host
 * whose last label is a number and which is not a canonical quad is refused.
 * IPv6 literals are parsed to eight groups so an embedded IPv4 address is
 * classified as IPv4; an unparseable literal or a zone identifier is refused
 * (NP-PRIV-ANALYSIS-007 L14, OI-PRIV-66).
 */
export function isLocalOrPrivateHost(hostname: string): boolean {
  const host = hostname.toLowerCase();

  if (host.includes(':')) {
    const groups = parseIPv6(host);
    return groups === null || !isPublicIPv6(groups);
  }

  // One trailing dot is the DNS root and names the same host.
  const name = host.endsWith('.') ? host.slice(0, -1) : host;
  if (name === '') return true;
  const labels = name.split('.');
  if (labels.some((l) => l === '')) return true;

  if (name === 'localhost' || name.endsWith('.localhost')) return true;
  if (name.endsWith('.local')) return true;

  if (isNumericLabel(labels[labels.length - 1])) {
    const octets = parseCanonicalIPv4(labels);
    return octets === null || isPrivateIPv4(octets);
  }

  // A single-label name like `wiki` or `intranet` is not a public FQDN; it
  // resolves through the host's DNS search suffix, which lands on an intranet
  // machine. Public condition links are always dotted.
  return labels.length === 1;
}

/** All decimal digits, or `0x` and hex digits: a label a browser reads as a number. */
function isNumericLabel(label: string): boolean {
  if (label.startsWith('0x')) return /^[0-9a-f]*$/.test(label.slice(2));
  return /^[0-9]*$/.test(label);
}

/** Exactly four decimal octets, 0–255, with no leading zero. */
function parseCanonicalIPv4(labels: string[]): number[] | null {
  if (labels.length !== 4) return null;
  const octets: number[] = [];
  for (const part of labels) {
    if (!/^\d{1,3}$/.test(part)) return null;
    if (part.length > 1 && part[0] === '0') return null;
    const n = Number(part);
    if (n > 255) return null;
    octets.push(n);
  }
  return octets;
}

function isPrivateIPv4([a, b]: number[]): boolean {
  if (a === 127 || a === 0) return true;              // loopback, "this network"
  if (a === 10) return true;                          // RFC1918 10/8
  if (a === 100 && b >= 64 && b <= 127) return true;  // RFC6598 shared address space (CGNAT)
  if (a === 192 && b === 168) return true;            // RFC1918 192.168/16
  if (a === 172 && b >= 16 && b <= 31) return true;   // RFC1918 172.16/12
  if (a === 169 && b === 254) return true;            // link-local
  if (a >= 224) return true;                          // multicast, reserved, broadcast
  return false;
}

/**
 * RFC 4291 text form to eight 16-bit groups, or null. At most one `::`; a
 * dotted IPv4 tail only as the last element; no zone identifier.
 */
function parseIPv6(host: string): number[] | null {
  if (host.includes('%')) return null;
  const gap = host.indexOf('::');
  if (gap !== -1 && host.indexOf('::', gap + 1) !== -1) return null;

  const head = gap === -1 ? host : host.slice(0, gap);
  const tail = gap === -1 ? '' : host.slice(gap + 2);
  const headParts = head === '' ? [] : head.split(':');
  const tailParts = tail === '' ? [] : tail.split(':');

  const headGroups = ipv6Groups(headParts, gap === -1);
  const tailGroups = ipv6Groups(tailParts, gap !== -1);
  if (headGroups === null || tailGroups === null) return null;
  const total = headGroups.length + tailGroups.length;
  if (gap === -1 && total !== 8) return null;
  if (gap !== -1 && total > 7) return null;

  const groups = new Array<number>(8).fill(0);
  headGroups.forEach((g, i) => { groups[i] = g; });
  tailGroups.forEach((g, i) => { groups[8 - tailGroups.length + i] = g; });
  return groups;
}

function ipv6Groups(parts: string[], dottedLastAllowed: boolean): number[] | null {
  const out: number[] = [];
  for (let i = 0; i < parts.length; i++) {
    const part = parts[i];
    if (part.includes('.')) {
      if (!dottedLastAllowed || i !== parts.length - 1) return null;
      const o = parseCanonicalIPv4(part.split('.'));
      if (o === null) return null;
      out.push((o[0] << 8) | o[1], (o[2] << 8) | o[3]);
    } else {
      if (!/^[0-9a-f]{1,4}$/.test(part)) return null;
      out.push(parseInt(part, 16));
    }
  }
  return out;
}

function embeddedIPv4(hi: number, lo: number): number[] {
  return [hi >> 8, hi & 0xff, lo >> 8, lo & 0xff];
}

function isPublicIPv6(g: number[]): boolean {
  const zeroTo = (n: number) => g.slice(0, n).every((x) => x === 0);
  // ::, ::1, and the deprecated IPv4-compatible ::a.b.c.d.
  if (zeroTo(6)) return false;
  // ::ffff:a.b.c.d (IPv4-mapped) and ::ffff:0:a.b.c.d (IPv4-translated).
  if (zeroTo(5) && g[5] === 0xffff) return !isPrivateIPv4(embeddedIPv4(g[6], g[7]));
  if (zeroTo(4) && g[4] === 0xffff && g[5] === 0) return !isPrivateIPv4(embeddedIPv4(g[6], g[7]));
  // 64:ff9b::/96 NAT64 well-known prefix; 64:ff9b:1::/48 is local-use.
  if (g[0] === 0x64 && g[1] === 0xff9b) {
    if (g[2] === 1) return false;
    if (g[2] === 0 && g[3] === 0 && g[4] === 0 && g[5] === 0) {
      return !isPrivateIPv4(embeddedIPv4(g[6], g[7]));
    }
  }
  // 2002::/16 6to4 carries an IPv4 address in groups 1–2.
  if (g[0] === 0x2002) return !isPrivateIPv4(embeddedIPv4(g[1], g[2]));
  if ((g[0] & 0xff80) === 0xfe80) return false;  // fe80::/10 link-local, fec0::/10 site-local
  if ((g[0] & 0xfe00) === 0xfc00) return false;  // fc00::/7 unique-local
  if ((g[0] & 0xff00) === 0xff00) return false;  // ff00::/8 multicast
  return true;
}

// ─── UI-facing helpers ─────────────────────────────────────────────────────────

/** Human-readable explanation for a blocked link, shown in place of Open. */
export function linkBlockedMessage(reason: NPLinkReason): string {
  switch (reason) {
    case 'ok':
      return '';
    case 'not_absolute':
      return t('UI_CONDITION_BLOCKED_NOT_ABSOLUTE');
    case 'scheme_not_https':
      return t('UI_CONDITION_BLOCKED_NOT_HTTPS');
    case 'embedded_credentials':
      return t('UI_CONDITION_BLOCKED_CREDENTIALS');
    case 'missing_host':
      return t('UI_CONDITION_BLOCKED_MISSING_HOST');
    case 'local_or_private_host':
      return t('UI_CONDITION_BLOCKED_PRIVATE_HOST');
    case 'malformed':
      return t('UI_CONDITION_BLOCKED_MALFORMED');
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
