import { useState, useEffect } from 'react';
import {
  NPProtocolEntry,
  NPModalityTypeId,
  MODALITY_META,
  modalityName,
  entryId,
  entryName,
  entryDescription,
  entryTags,
  entryIsPredefined,
  entryIsReadOnly,
} from '../types/protocol';
import { protocolLibrary, ProtocolAvailability } from '../lib/protocolLibrary';
import { useProtocolContext } from '../App';
import { limitsStore } from '../lib/limitsStore';
import { validateEntry } from '../lib/protocolValidator';
import { NPValidationResult } from '../types/limits';
import { LimitsSettings } from './LimitsSettings';
import { ConditionChips } from './ConditionLinkDialog';
import { getPredefinedNamespace } from '../lib/predefinedProtocols';
import type { NPConditionDefinition } from '../types/protocol';
import { t, tPlural } from '../lib/i18n';

// Stable identity so ConditionChips doesn't see a new Map every render.
const NO_CONDITIONS: ReadonlyMap<string, NPConditionDefinition> = new Map();

// ─── Props ─────────────────────────────────────────────────────────────────────

interface ProtocolMenuProps {
  onEdit: (entry: NPProtocolEntry) => void;
  onNewProtocol: () => void;
  onOpenComposer: () => void;
}

// ─── Confirm dialog ────────────────────────────────────────────────────────────

interface ConfirmState {
  message: string;
  onConfirm: () => void;
}

// ─── Protocol Menu ─────────────────────────────────────────────────────────────

export function ProtocolMenu({ onEdit, onNewProtocol, onOpenComposer }: ProtocolMenuProps) {
  useProtocolContext(); // subscribe to version changes for re-render
  const [search, setSearch] = useState('');
  const [filter, setFilter] = useState<'all' | 'predefined' | 'mine'>('all');
  const [confirm, setConfirm] = useState<ConfirmState | null>(null);
  const [showLimits, setShowLimits] = useState(false);
  const [, setLimitsVersion] = useState(0);
  const [, setLibraryVersion] = useState(0);

  // Subscribe to limits store changes to trigger re-render when limits change
  useEffect(() => {
    const handler = () => setLimitsVersion(v => v + 1);
    limitsStore.addEventListener('change', handler);
    return () => limitsStore.removeEventListener('change', handler);
  }, []);

  // Subscribe to protocolLibrary changes (e.g. when predefined protocols finish loading)
  useEffect(() => {
    return protocolLibrary.subscribe(() => setLibraryVersion(v => v + 1));
  }, []);

  const resolvedLimits = limitsStore.resolvedLimits;
  const activeProfile = limitsStore.activeProfile;
  const allProtocols = protocolLibrary.allProtocols;

  function filterEntries(entries: NPProtocolEntry[]): NPProtocolEntry[] {
    const q = search.trim().toLowerCase();
    return entries.filter(e => {
      if (filter === 'predefined' && !entryIsPredefined(e)) return false;
      if (filter === 'mine' && entryIsPredefined(e)) return false;
      if (!q) return true;
      const name = entryName(e).toLowerCase();
      const desc = entryDescription(e).toLowerCase();
      const tags = entryTags(e).join(' ').toLowerCase();
      return name.includes(q) || desc.includes(q) || tags.includes(q);
    });
  }

  const predefined = filterEntries(allProtocols.filter(e => entryIsPredefined(e)));
  const userOwned = filterEntries(allProtocols.filter(e => !entryIsPredefined(e)));

  function handleDuplicate(entry: NPProtocolEntry) {
    const baseName = entryName(entry);
    const newName = t('WEB_PROTOCOL_COPY_SUFFIX', { 0: baseName });
    const dup = protocolLibrary.duplicateProtocol(entry, newName);
    protocolLibrary.saveUserProtocol(dup);
  }

  function handleDelete(entry: NPProtocolEntry) {
    setConfirm({
      message: t('WEB_CONFIRM_DELETE_PROTOCOL', { 0: entryName(entry) }),
      onConfirm: () => {
        protocolLibrary.deleteUserProtocol(entryId(entry));
        setConfirm(null);
      },
    });
  }

  const isLoadingPredefined = !protocolLibrary.isLoaded;
  const showPredefined = filter !== 'mine' && predefined.length > 0;
  const showUser = filter !== 'predefined' && userOwned.length > 0;
  const showEmpty = predefined.length === 0 && userOwned.length === 0 && !isLoadingPredefined;

  return (
    <div className="protocol-menu">
      <div className="protocol-menu-toolbar">
        <div className="search-input-wrap">
          <span className="search-icon">🔍</span>
          <input
            className="search-input"
            type="text"
            placeholder={t('WEB_SEARCH_PROTOCOLS')}
            value={search}
            onChange={e => setSearch(e.target.value)}
          />
        </div>

        <div className="filter-tabs">
          {(['all', 'predefined', 'mine'] as const).map(f => (
            <button
              key={f}
              className={`filter-tab${filter === f ? ' active' : ''}`}
              onClick={() => setFilter(f)}
            >
              {f === 'all'
                ? t('WEB_FILTER_ALL')
                : f === 'predefined'
                  ? t('WEB_FILTER_PREDEFINED')
                  : t('WEB_FILTER_MINE')}
            </button>
          ))}
        </div>

        <div className="protocol-menu-actions">
          {activeProfile && (
            <span
              style={{
                fontSize: 11,
                padding: '3px 8px',
                background: 'var(--accent)',
                color: '#000',
                borderRadius: 10,
                fontWeight: 700,
                cursor: 'pointer',
              }}
              title={t('WEB_ACTIVE_PROFILE_TOOLTIP', { 0: activeProfile.name })}
              onClick={() => setShowLimits(true)}
            >
              {activeProfile.name}
            </span>
          )}
          <button
            className="btn btn-secondary"
            onClick={() => setShowLimits(true)}
            title={t('WEB_LIMITS_TOOLTIP')}
            style={{ fontSize: 13 }}
          >
            {t('WEB_LIMITS')}
          </button>
          <button className="btn btn-secondary" onClick={onOpenComposer}>
            {t('WEB_COMPOSE')}
          </button>
          <button className="btn btn-primary" onClick={onNewProtocol}>
            {t('WEB_NEW_PROTOCOL_BUTTON')}
          </button>
        </div>
      </div>

      <div className="protocol-list">
        {isLoadingPredefined && filter !== 'mine' && (
          <div className="empty-state" style={{ opacity: 0.6 }}>
            <div className="empty-state-icon">⏳</div>
            <div className="empty-state-title">{t('WEB_LOADING_PROTOCOLS')}</div>
          </div>
        )}

        {showEmpty && (
          <div className="empty-state">
            <div className="empty-state-icon">📋</div>
            <div className="empty-state-title">{t('WEB_NO_PROTOCOLS_FOUND')}</div>
            <div className="empty-state-text">
              {search
                ? t('WEB_NO_PROTOCOLS_MATCH', { 0: search })
                : t('WEB_NO_PROTOCOLS_CREATE_FIRST')}
            </div>
          </div>
        )}

        {showPredefined && (
          <div className="protocol-section">
            <div className="protocol-section-header">{t('WEB_FILTER_PREDEFINED')}</div>
            <div className="protocol-cards">
              {predefined.map(entry => (
                <ProtocolCard
                  key={entryId(entry)}
                  entry={entry}
                  availability={protocolLibrary.checkAvailability(entry)}
                  validation={validateEntry(entry, resolvedLimits, allProtocols)}
                  onEdit={() => onEdit(entry)}
                  onDuplicate={() => handleDuplicate(entry)}
                  onDelete={null}
                  canEdit={protocolLibrary.canEdit(entry)}
                />
              ))}
            </div>
          </div>
        )}

        {showUser && (
          <div className="protocol-section">
            <div className="protocol-section-header">{t('WEB_FILTER_MINE')}</div>
            <div className="protocol-cards">
              {userOwned.map(entry => (
                <ProtocolCard
                  key={entryId(entry)}
                  entry={entry}
                  availability={protocolLibrary.checkAvailability(entry)}
                  validation={validateEntry(entry, resolvedLimits, allProtocols)}
                  onEdit={() => onEdit(entry)}
                  onDuplicate={() => handleDuplicate(entry)}
                  onDelete={protocolLibrary.canDelete(entry) ? () => handleDelete(entry) : null}
                  canEdit={protocolLibrary.canEdit(entry)}
                />
              ))}
            </div>
          </div>
        )}
      </div>

      {confirm && (
        <ConfirmDialog
          message={confirm.message}
          onConfirm={confirm.onConfirm}
          onCancel={() => setConfirm(null)}
        />
      )}

      {showLimits && (
        <LimitsSettings onClose={() => setShowLimits(false)} />
      )}
    </div>
  );
}

// ─── Protocol Card ─────────────────────────────────────────────────────────────

interface ProtocolCardProps {
  entry: NPProtocolEntry;
  availability: ProtocolAvailability;
  validation: NPValidationResult;
  onEdit: () => void;
  onDuplicate: () => void;
  onDelete: (() => void) | null;
  canEdit?: boolean;
}

function ProtocolCard({ entry, availability, validation, onEdit, onDuplicate, onDelete, canEdit = true }: ProtocolCardProps) {
  const name = entryName(entry);
  const description = entryDescription(entry);
  const tags = entryTags(entry);
  const isComposite = entry.kind === 'composite';
  const isReadOnly = entryIsReadOnly(entry);
  const conditionRegistry = getPredefinedNamespace()?.conditions ?? NO_CONDITIONS;

  // Get modality icons for single protocols
  const modalityIcons: Array<{ icon: string; label: string }> = [];
  if (entry.kind === 'single') {
    const seen = new Set<NPModalityTypeId>();
    for (const m of entry.protocol.modalities) {
      if (!seen.has(m.modalityParams.type)) {
        seen.add(m.modalityParams.type);
        const meta = MODALITY_META[m.modalityParams.type];
        if (meta) modalityIcons.push({ icon: meta.icon, label: t(meta.displayNameKey) });
      }
    }
  }

  const deviceTier = protocolLibrary.deviceTier;
  const availLabel = deviceTier === 'none'
    ? t('WEB_AVAIL_NO_DEVICE')
    : availability.available
      ? t('WEB_AVAIL_AVAILABLE')
      : t('WEB_AVAIL_UNAVAILABLE');

  const availClass = deviceTier === 'none'
    ? 'no-device'
    : availability.available ? 'available' : 'unavailable';

  const missingTooltip = availability.missingModalities && availability.missingModalities.length > 0
    ? t('WEB_AVAIL_MISSING', {
        0: availability.missingModalities.map(m => modalityName(m)).join(', '),
      })
    : availability.reason ?? '';

  // Validation border color
  const hasErrors = validation.errors.length > 0;
  const hasWarnings = validation.warnings.length > 0;
  const validationBorderColor = hasErrors
    ? 'var(--error, #ef4444)'
    : hasWarnings
      ? 'var(--warning, #f59e0b)'
      : undefined;

  return (
    <div
      className={`protocol-card${!availability.available && deviceTier !== 'none' ? ' unavailable' : ''}`}
      style={validationBorderColor ? { borderLeft: `3px solid ${validationBorderColor}` } : undefined}
    >
      <div className="card-main">
        <div className="card-header">
          <div className="card-title">
            {name}
            {isReadOnly && (
              <span
                title={t('WEB_READ_ONLY_TOOLTIP')}
                style={{ opacity: 0.55, fontSize: '0.8em', marginLeft: 6, verticalAlign: 'middle' }}
              >
                🔒
              </span>
            )}
          </div>
          {isComposite && (
            <span className="composite-badge">
              {t('WEB_COMPOSITE_LAYERS_BADGE', { 0: entry.composite.layers.length })}
            </span>
          )}
        </div>
        <div className="card-description">{description}</div>
        <ConditionChips
          conditions={entry.kind === 'single' ? entry.protocol.conditions : entry.composite.conditions}
          registry={conditionRegistry}
        />
        <div className="card-footer">
          {tags.slice(0, 4).map(tag => (
            <span key={tag} className="tag-chip">{tag}</span>
          ))}
          {tags.length > 4 && (
            <span className="tag-chip">+{tags.length - 4}</span>
          )}
          {modalityIcons.length > 0 && (
            <div className="modality-icons" style={{ marginLeft: 'auto' }}>
              {modalityIcons.slice(0, 6).map((m, i) => (
                <span key={i} className="modality-icon" title={m.label}>{m.icon}</span>
              ))}
              {modalityIcons.length > 6 && (
                <span style={{ fontSize: 12, color: 'var(--text-muted)' }}>+{modalityIcons.length - 6}</span>
              )}
            </div>
          )}
        </div>
      </div>

      <div className="card-actions">
        <div className="tooltip-wrap">
          <span
            className={`availability-badge ${availClass}`}
          >
            {availability.available ? '✓' : '✗'} {availLabel}
          </span>
          {missingTooltip && <span className="tooltip">{missingTooltip}</span>}
        </div>

        {/* Validation badge */}
        {(hasErrors || hasWarnings) && (
          <ValidationBadge validation={validation} />
        )}

        <button className="btn btn-secondary btn-sm" onClick={onEdit}>
          {canEdit ? t('WEB_EDIT') : t('WEB_VIEW')}
        </button>
        <button className="btn btn-ghost btn-sm" onClick={onDuplicate} title={t('WEB_DUPLICATE_TOOLTIP')}>
          {t('WEB_COPY')}
        </button>
        {onDelete && canEdit && (
          <button className="btn btn-danger btn-sm" onClick={onDelete}>
            {t('WEB_DELETE')}
          </button>
        )}
      </div>
    </div>
  );
}

// ─── Validation Badge ─────────────────────────────────────────────────────────

function ValidationBadge({ validation }: { validation: NPValidationResult }) {
  const [open, setOpen] = useState(false);
  const hasErrors = validation.errors.length > 0;
  const count = hasErrors ? validation.errors.length : validation.warnings.length;
  const label = hasErrors
    ? tPlural('WEB_VALIDATION_ERROR_COUNT', count)
    : tPlural('WEB_VALIDATION_WARNING_COUNT', count);

  const badgeColor = hasErrors
    ? 'var(--error, #ef4444)'
    : 'var(--warning, #f59e0b)';
  const badgeBg = hasErrors
    ? 'rgba(239,68,68,0.15)'
    : 'rgba(245,158,11,0.15)';

  const allIssues = hasErrors ? validation.errors : validation.warnings;

  return (
    <div className="tooltip-wrap" style={{ position: 'relative' }}>
      <button
        onClick={e => { e.stopPropagation(); setOpen(o => !o); }}
        style={{
          background: badgeBg,
          border: `1px solid ${badgeColor}`,
          borderRadius: 10,
          color: badgeColor,
          fontSize: 10,
          fontWeight: 700,
          padding: '2px 7px',
          cursor: 'pointer',
          whiteSpace: 'nowrap',
        }}
      >
        {t('WEB_VALIDATION_BADGE', { 0: label })}
      </button>
      {open && (
        <div
          onClick={e => e.stopPropagation()}
          style={{
            position: 'absolute',
            bottom: '100%',
            right: 0,
            marginBottom: 6,
            width: 320,
            background: 'var(--bg-primary)',
            border: '1px solid var(--border)',
            borderRadius: 6,
            boxShadow: '0 4px 20px rgba(0,0,0,0.4)',
            zIndex: 1000,
            padding: 12,
            maxHeight: 280,
            overflow: 'auto',
          }}
        >
          <div style={{ fontWeight: 700, fontSize: 12, marginBottom: 8, color: badgeColor }}>
            {hasErrors ? t('WEB_VALIDATION_ERRORS_TITLE') : t('WEB_VALIDATION_WARNINGS_TITLE')}
          </div>
          {allIssues.map(issue => (
            <div key={issue.id} style={{ marginBottom: 8, borderBottom: '1px solid var(--border)', paddingBottom: 6 }}>
              <div style={{ fontSize: 11, fontWeight: 600, color: badgeColor }}>
                {issue.modality ? t('WEB_VALIDATION_ISSUE_MODALITY', { 0: issue.modality }) : ''}
                {issue.parameterDisplayName}
              </div>
              <div style={{ fontSize: 11, color: 'var(--text-secondary)', marginTop: 2 }}>
                {issue.message}
              </div>
              <div style={{ fontSize: 10, color: 'var(--text-muted)', marginTop: 2 }}>
                {t('WEB_VALIDATION_ACTUAL_LIMIT', {
                  0: issue.actualValueDescription,
                  1: issue.limitValueDescription,
                })}
              </div>
            </div>
          ))}
          <button
            onClick={() => setOpen(false)}
            style={{
              marginTop: 4,
              background: 'none',
              border: 'none',
              color: 'var(--text-muted)',
              fontSize: 11,
              cursor: 'pointer',
              padding: 0,
            }}
          >
            {t('WEB_DISMISS')}
          </button>
        </div>
      )}
    </div>
  );
}

// ─── Confirm Dialog ────────────────────────────────────────────────────────────

function ConfirmDialog({ message, onConfirm, onCancel }: {
  message: string;
  onConfirm: () => void;
  onCancel: () => void;
}) {
  return (
    <div className="modal-overlay" onClick={onCancel}>
      <div className="modal confirm-dialog" onClick={e => e.stopPropagation()}>
        <div className="modal-header">
          <span className="modal-title">{t('WEB_CONFIRM_TITLE')}</span>
          <button className="modal-close" onClick={onCancel}>×</button>
        </div>
        <div className="modal-body">
          {message}
        </div>
        <div className="modal-footer">
          <button className="btn btn-secondary" onClick={onCancel}>{t('COMMON_CANCEL')}</button>
          <button className="btn btn-danger" onClick={onConfirm}>{t('WEB_DELETE')}</button>
        </div>
      </div>
    </div>
  );
}
