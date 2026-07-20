// Condition chips + the "leaving the app" confirmation — NP-COND-LINK-001 §4.
//
// Selecting a condition never navigates directly. The destination host is a
// third party that would learn this user looked up a specific condition, so the
// user gets to see the host and decide. Nothing here is logged or reported:
// condition views are UHDR-class by the §5.1 boundary rule, and the cleanest
// way to honour that is to not record them at all.

import { useState } from 'react';
import {
  resolveConditions,
  linkBlockedMessage,
  openConditionLink,
  type NPResolvedCondition,
} from '../lib/conditionLink';
import type { NPConditionDefinition } from '../types/protocol';

interface ConditionChipsProps {
  conditions: readonly string[] | undefined;
  registry: ReadonlyMap<string, NPConditionDefinition>;
}

export function ConditionChips({ conditions, registry }: ConditionChipsProps) {
  const [selected, setSelected] = useState<NPResolvedCondition | null>(null);

  if (!conditions || conditions.length === 0) return null;
  const resolved = resolveConditions(conditions, registry);

  return (
    <>
      <div className="condition-chips">
        {resolved.map(c => (
          <button
            key={c.name}
            type="button"
            className={`condition-chip${c.verdict.allowed ? '' : ' blocked'}`}
            onClick={() => setSelected(c)}
            title={
              c.verdict.allowed
                ? `Open definition on ${c.verdict.host}`
                : linkBlockedMessage(c.verdict.reason)
            }
          >
            {c.name}
            {c.verdict.allowed && <span className="condition-chip-icon" aria-hidden="true">↗</span>}
          </button>
        ))}
      </div>

      {selected && (
        <ConditionLinkDialog condition={selected} onClose={() => setSelected(null)} />
      )}
    </>
  );
}

interface ConditionLinkDialogProps {
  condition: NPResolvedCondition;
  onClose: () => void;
}

export function ConditionLinkDialog({ condition, onClose }: ConditionLinkDialogProps) {
  const { name, definition, verdict } = condition;

  const confirm = () => {
    if (openConditionLink(verdict)) onClose();
  };

  return (
    <div
      className="modal-overlay"
      onClick={onClose}
      role="presentation"
    >
      <div
        className="modal condition-modal"
        onClick={e => e.stopPropagation()}
        role="dialog"
        aria-modal="true"
        aria-labelledby="condition-dialog-title"
      >
        <div className="modal-header">
          <span id="condition-dialog-title" className="modal-title">{name}</span>
          {definition?.code && <span className="tag-chip">ICD-11 {definition.code}</span>}
        </div>

        <div className="condition-modal-body">
          {definition?.description && (
            <p className="condition-description">{definition.description}</p>
          )}

          {verdict.allowed ? (
            <>
              <p className="condition-leaving">
                This opens an external site in your browser. NeurOne does not
                record which conditions you look up.
              </p>
              <div className="condition-destination">
                <span className="condition-destination-label">Destination</span>
                <span className="condition-destination-host">{verdict.host}</span>
              </div>
            </>
          ) : (
            <p className="condition-blocked">
              {linkBlockedMessage(verdict.reason)}
              {!definition && ' This condition is not in the registry.'}
            </p>
          )}
        </div>

        <div className="condition-modal-actions">
          <button type="button" className="btn-secondary" onClick={onClose}>
            {verdict.allowed ? 'Cancel' : 'Close'}
          </button>
          {verdict.allowed && (
            <button type="button" className="btn-primary" onClick={confirm}>
              Open in browser
            </button>
          )}
        </div>
      </div>
    </div>
  );
}
