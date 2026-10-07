// The acknowledgement screen for a protocol in the caution zone (docs/reference/safety-zones.md).
//
// A caution is not a refusal: the compiler runs the protocol only when the request lists the id of every caution it is
// in. This dialog is where an author lists them. Each caution is its own checkbox, because an id names one dose and an
// acknowledgement is of that dose, not of the protocol. Nothing is stored: the ids go to `onAcknowledge` for one
// compile and are dropped (an acknowledgement is a decision about the person, CLAUDE.md §5).

import { useState } from 'react';
import { modalityName, type NPModalityTypeId } from '../../../../common/types/protocol';
import { cautionsOf, type ZoneCaution } from '../../../../common/lib/zoneCautions';
import type { NppsZoneBlock } from '../../../../common/lib/nppsCore';
import { t } from '../../../../common/lib/i18n';

interface CautionAckDialogProps {
  protocolName: string;
  zones: readonly NppsZoneBlock[];
  /** Every caution's id, once all are ticked and the author confirms. */
  onAcknowledge: (ackIds: string[]) => void;
  onCancel: () => void;
}

/** Round for display only; the id keeps the exact dose. */
const shown = (n: number): string => String(Math.round(n * 100) / 100);

export function CautionAckDialog({ protocolName, zones, onAcknowledge, onCancel }: CautionAckDialogProps) {
  const cautions = cautionsOf(zones);
  const [ticked, setTicked] = useState<ReadonlySet<string>>(new Set());
  const all = cautions.length > 0 && cautions.every(c => ticked.has(c.ackId));

  const toggle = (id: string) => setTicked(prev => {
    const next = new Set(prev);
    if (next.has(id)) next.delete(id); else next.add(id);
    return next;
  });

  return (
    <div className="modal-overlay" onClick={onCancel}>
      <div
        className="modal"
        role="alertdialog"
        aria-modal="true"
        aria-labelledby="caution-ack-title"
        onClick={e => e.stopPropagation()}
      >
        <div className="modal-header">
          <div className="modal-title" id="caution-ack-title">{t('ZONE_ACK_TITLE')}</div>
        </div>
        <div className="modal-body">
          <p>{t('ZONE_ACK_INTRO', { 0: protocolName })}</p>
          {cautions.map((c: ZoneCaution) => (
            <label key={c.ackId} className="caution-ack-item" style={{ display: 'flex', gap: 10, alignItems: 'flex-start' }}>
              <input
                type="checkbox"
                checked={ticked.has(c.ackId)}
                onChange={() => toggle(c.ackId)}
              />
              <span>
                <strong>
                  {t('ZONE_ACK_ITEM_HEADING', { 0: modalityName(c.modality as NPModalityTypeId), 1: t(c.axisNameKey) })}
                </strong>
                <br />
                {t('ZONE_ACK_ITEM_DOSE', { 0: shown(c.value), 1: c.unit, 2: shown(c.caution) })}
                <br />
                <small>{t('ZONE_ACK_CHECK_LABEL')}</small>
              </span>
            </label>
          ))}
          <p style={{ fontSize: 12, color: 'var(--text-muted)' }}>{t('ZONE_ACK_SAFETY_NOTE')}</p>
          <p style={{ fontSize: 12, color: 'var(--text-muted)' }}>{t('ZONE_ACK_REASK_NOTE')}</p>
          <p style={{ fontSize: 12, color: 'var(--text-muted)' }}>{t('ZONE_ACK_LOCAL_NOTE')}</p>
        </div>
        <div className="modal-header" style={{ borderTop: '1px solid var(--border)', borderBottom: 'none' }}>
          <span style={{ flex: 1, fontSize: 12 }} aria-live="polite">
            {t('ZONE_ACK_PROGRESS', { 0: cautions.filter(c => ticked.has(c.ackId)).length, 1: cautions.length })}
          </span>
          <button className="btn btn-secondary btn-sm" onClick={onCancel}>{t('ZONE_ACK_CANCEL')}</button>
          <button
            className="btn btn-primary btn-sm"
            disabled={!all}
            onClick={() => onAcknowledge(cautions.map(c => c.ackId))}
          >
            {t('ZONE_ACK_CONFIRM')}
          </button>
        </div>
      </div>
    </div>
  );
}
