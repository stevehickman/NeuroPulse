/// <reference types="node" />
import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  evaluateConditionLink,
  resolveConditions,
  linkBlockedMessage,
  type NPLinkReason,
} from './conditionLink';
import { parseNPPSFile, buildNamespace } from './nppsParser';

const __fixturesDir = join(
  dirname(fileURLToPath(import.meta.url)),
  '..', '..', '..', '..', 'npps', 'fixtures',
);

interface LinkVector {
  name: string;
  url: string;
  verdict: 'allow' | 'block';
  reason: NPLinkReason;
  host?: string;
}

const vectors: LinkVector[] = JSON.parse(
  readFileSync(join(__fixturesDir, 'condition_links.json'), 'utf-8'),
).vectors;

// ─── Shared cross-platform vectors ─────────────────────────────────────────────
//
// These are the same vectors the Swift, Kotlin, and C# suites run. A change here
// is a change to the contract in NP-COND-LINK-001 §3 and must land in all four.

describe('condition link policy — shared vectors', () => {
  it('loads the shared vector file', () => {
    expect(vectors.length).toBeGreaterThan(25);
  });

  for (const v of vectors) {
    it(`${v.verdict}: ${v.name}`, () => {
      const got = evaluateConditionLink(v.url);
      expect(got.allowed).toBe(v.verdict === 'allow');
      expect(got.reason).toBe(v.reason);
      if (v.host !== undefined) expect(got.host).toBe(v.host);
    });
  }

  it('never returns a url on a blocked verdict', () => {
    for (const v of vectors.filter(x => x.verdict === 'block')) {
      expect(evaluateConditionLink(v.url).url).toBeUndefined();
    }
  });

  it('returns a trimmed, openable url on every allowed verdict', () => {
    for (const v of vectors.filter(x => x.verdict === 'allow')) {
      const got = evaluateConditionLink(v.url);
      expect(got.url).toBe(v.url.trim());
    }
  });
});

// ─── The shipped registry ──────────────────────────────────────────────────────

describe('shipped condition registry', () => {
  const registrySource = readFileSync(
    join(__fixturesDir, '..', '..', 'protocols', 'predefined', '00-conditions.npps'),
    'utf-8',
  );

  const { conditions } = parseNPPSFile(registrySource);

  it('parses every condition in the registry', () => {
    expect(conditions.length).toBeGreaterThan(20);
  });

  it('every shipped condition link passes policy', () => {
    const rejected = conditions
      .map(c => ({ name: c.name, verdict: evaluateConditionLink(c.link) }))
      .filter(r => !r.verdict.allowed);

    expect(rejected).toEqual([]);
  });

  it('every shipped condition link is https on a public host', () => {
    for (const c of conditions) {
      const v = evaluateConditionLink(c.link);
      expect(v.host).toBeTruthy();
      expect(c.link.startsWith('https://')).toBe(true);
    }
  });
});

// ─── Resolution against a namespace ────────────────────────────────────────────

describe('resolveConditions', () => {
  const namespace = buildNamespace([
    parseNPPSFile(`
      condition "Major Depressive Disorder" {
        link: "https://en.wikipedia.org/wiki/Major_depressive_disorder"
        code: "6A70"
      }
      condition "Bad Link" { link: "javascript:alert(1)" }
    `),
  ]).namespace;

  it('resolves a known condition to an allowed verdict', () => {
    const [r] = resolveConditions(['Major Depressive Disorder'], namespace.conditions);
    expect(r.definition?.code).toBe('6A70');
    expect(r.verdict.allowed).toBe(true);
    expect(r.verdict.host).toBe('en.wikipedia.org');
  });

  it('lists an unknown condition rather than dropping it', () => {
    const [r] = resolveConditions(['Not In Registry'], namespace.conditions);
    expect(r.name).toBe('Not In Registry');
    expect(r.definition).toBeNull();
    expect(r.verdict.allowed).toBe(false);
  });

  it('lists a condition whose link fails policy, with the block reason', () => {
    const [r] = resolveConditions(['Bad Link'], namespace.conditions);
    expect(r.definition).not.toBeNull();
    expect(r.verdict.allowed).toBe(false);
    expect(r.verdict.reason).toBe('scheme_not_https');
  });

  it('preserves order and arity', () => {
    const rs = resolveConditions(
      ['Major Depressive Disorder', 'Not In Registry', 'Bad Link'],
      namespace.conditions,
    );
    expect(rs.map(r => r.name)).toEqual([
      'Major Depressive Disorder', 'Not In Registry', 'Bad Link',
    ]);
  });
});

describe('linkBlockedMessage', () => {
  it('gives a non-empty message for every blocking reason', () => {
    const reasons = [...new Set(vectors.filter(v => v.verdict === 'block').map(v => v.reason))];
    for (const r of reasons) {
      expect(linkBlockedMessage(r).length).toBeGreaterThan(0);
    }
  });

  it('gives an empty message for ok', () => {
    expect(linkBlockedMessage('ok')).toBe('');
  });
});
