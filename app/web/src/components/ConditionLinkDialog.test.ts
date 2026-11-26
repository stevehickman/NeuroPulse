// The "leaving the app" confirmation shows the whole address it will open, not only the host
// (REQ-SW03-38; NP-COND-LINK-001 §4; NP-PRIV-ANALYSIS-006 F-16). Rendered to static markup: the
// dialog is a pure function of the condition it is given, and no DOM is needed to read it.

import { describe, it, expect } from 'vitest';
import { createElement } from 'react';
import { renderToStaticMarkup } from 'react-dom/server';
import { ConditionLinkDialog } from './ConditionLinkDialog';
import { evaluateConditionLink, type NPResolvedCondition } from '../lib/conditionLink';

function render(link: string): string {
  const condition: NPResolvedCondition = { name: 'Migraine', definition: null, verdict: evaluateConditionLink(link) };
  return renderToStaticMarkup(createElement(ConditionLinkDialog, { condition, onClose: () => {} }));
}

describe('condition link confirmation', () => {
  // @verifies REQ-SW03-38
  it('shows the host and the whole address, so a query string is seen before leaving', () => {
    const link = 'https://en.wikipedia.org/wiki/Migraine?utm_source=np&ref=a1b2c3#Signs';
    const html = render(link);
    expect(html).toContain('<span class="condition-destination-host">en.wikipedia.org</span>');
    // React escapes the ampersand in text; the address is otherwise as given.
    expect(html).toContain(
      '<span class="condition-destination-url">https://en.wikipedia.org/wiki/Migraine?utm_source=np&amp;ref=a1b2c3#Signs</span>',
    );
  });

  // @verifies REQ-SW03-38
  it('shows no address for a link the policy refused', () => {
    const html = render('javascript:alert(1)');
    expect(html).not.toContain('condition-destination-url');
    expect(html).not.toContain('javascript:');
  });
});
