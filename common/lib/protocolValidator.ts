/**
 * Protocol validation, for the web app and the simulator: a thin layer over the shared NPPS core
 * (common/npps-core, OI-NPPS-CORE-01), loaded as WebAssembly (nppsCore.ts).
 *
 * This file used to be an 800-line hand-written validator, one of three that drifted (the iOS and Android ones
 * divided charge by a sum of electrode areas the safety MCU never uses, and attributed limits differently). There
 * is one validator now, and it is in Rust. The core returns no text: each issue carries locale keys and
 * positional arguments, and this file resolves them with `t()`, so the wording is the app's and the finding is the
 * core's.
 *
 * In a browser, `await initNppsCore()` (nppsCore.ts) once before the first call; Node needs nothing.
 */

import type { NPProtocolDefinition, NPProtocolEntry } from '../types/protocol';
import type {
  LimitSource,
  NPLimitsSet,
  NPValidationIssue,
  NPValidationResult,
} from '../types/limits';
import { t } from './i18n';
import { nppsValidate, type NppsMessage } from './nppsCore';
import { coreEntry } from './nppsSerializer';

/** Locale key per limit source, for the "(global)" suffix on a limit value. */
const LIMIT_SOURCE_KEY: Record<LimitSource, string> = {
  hardware: 'VALIDATE_SOURCE_HARDWARE',
  global: 'VALIDATE_SOURCE_GLOBAL',
  helmet: 'VALIDATE_SOURCE_HELMET',
  individual: 'VALIDATE_SOURCE_INDIVIDUAL',
};

/** A core message as text: a plain string stays, a key is resolved with its arguments (themselves messages). */
function text(m: NppsMessage): string {
  if (typeof m === 'string') return m;
  const args: Record<number, string> = {};
  (m.args ?? []).forEach((a, i) => { args[i] = text(a); });
  return t(m.key, args);
}

export function makeResult(issues: NPValidationIssue[]): NPValidationResult {
  return {
    issues,
    isValid: !issues.some(i => i.severity === 'error'),
    hasWarnings: issues.some(i => i.severity === 'warning'),
    errors: issues.filter(i => i.severity === 'error'),
    warnings: issues.filter(i => i.severity === 'warning'),
  };
}

/**
 * Validate one entry against the resolved limits. `allProtocols` is the library a composite's layers resolve
 * against; leave it out to skip the layer-reference checks.
 */
export function validateEntry(
  entry: NPProtocolEntry,
  resolvedLimits: NPLimitsSet,
  allProtocols?: NPProtocolEntry[],
): NPValidationResult {
  const result = nppsValidate({
    entry: coreEntry(entry),
    limits: resolvedLimits,
    allProtocols: allProtocols ? allProtocols.map(coreEntry) : null,
  });
  return makeResult(result.issues.map((i): NPValidationIssue => ({
    id: crypto.randomUUID(),
    severity: i.severity,
    modality: i.modality,
    parameterKey: i.parameterKey,
    parameterDisplayName: text(i.parameterName),
    actualValueDescription: text(i.actualValueDescription),
    limitValueDescription: t('VALIDATE_LIMIT_WITH_SOURCE', {
      0: text(i.limitValueDescription),
      1: t(LIMIT_SOURCE_KEY[i.limitSource]),
    }),
    limitSource: i.limitSource,
    message: text(i.message),
  })));
}

/** Validate a single protocol against the resolved limits. */
export function validateProtocol(definition: NPProtocolDefinition, resolvedLimits: NPLimitsSet): NPValidationResult {
  return validateEntry({ kind: 'single', protocol: definition }, resolvedLimits);
}
