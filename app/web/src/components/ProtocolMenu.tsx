import React, { useState, useCallback } from 'react';
import {
  NPProtocolEntry,
  NPModalityTypeId,
  MODALITY_META,
  entryId,
  entryName,
  entryDescription,
  entryTags,
  entryIsPredefined,
} from '../types/protocol';
import { protocolLibrary, ProtocolAvailability } from '../lib/protocolLibrary';
import { useProtocolContext } from '../App';

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
  const { version, refresh } = useProtocolContext();
  const [search, setSearch] = useState('');
  const [filter, setFilter] = useState<'all' | 'predefined' | 'mine'>('all');
  const [confirm, setConfirm] = useState<ConfirmState | null>(null);

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
    const newName = `${baseName} (copy)`;
    const dup = protocolLibrary.duplicateProtocol(entry, newName);
    protocolLibrary.saveUserProtocol(dup);
  }

  function handleDelete(entry: NPProtocolEntry) {
    setConfirm({
      message: `Delete "${entryName(entry)}"? This cannot be undone.`,
      onConfirm: () => {
        protocolLibrary.deleteUserProtocol(entryId(entry));
        setConfirm(null);
      },
    });
  }

  const showPredefined = filter !== 'mine' && predefined.length > 0;
  const showUser = filter !== 'predefined' && userOwned.length > 0;
  const showEmpty = predefined.length === 0 && userOwned.length === 0;

  return (
    <div className="protocol-menu">
      <div className="protocol-menu-toolbar">
        <div className="search-input-wrap">
          <span className="search-icon">🔍</span>
          <input
            className="search-input"
            type="text"
            placeholder="Search protocols..."
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
              {f === 'all' ? 'All' : f === 'predefined' ? 'Predefined' : 'My Protocols'}
            </button>
          ))}
        </div>

        <div className="protocol-menu-actions">
          <button className="btn btn-secondary" onClick={onOpenComposer}>
            🎛️ Compose
          </button>
          <button className="btn btn-primary" onClick={onNewProtocol}>
            + New Protocol
          </button>
        </div>
      </div>

      <div className="protocol-list">
        {showEmpty && (
          <div className="empty-state">
            <div className="empty-state-icon">📋</div>
            <div className="empty-state-title">No protocols found</div>
            <div className="empty-state-text">
              {search ? `No protocols match "${search}". Try a different search term.` : 'Create your first protocol using the button above.'}
            </div>
          </div>
        )}

        {showPredefined && (
          <div className="protocol-section">
            <div className="protocol-section-header">Predefined</div>
            <div className="protocol-cards">
              {predefined.map(entry => (
                <ProtocolCard
                  key={entryId(entry)}
                  entry={entry}
                  availability={protocolLibrary.checkAvailability(entry)}
                  onEdit={() => onEdit(entry)}
                  onDuplicate={() => handleDuplicate(entry)}
                  onDelete={null}
                />
              ))}
            </div>
          </div>
        )}

        {showUser && (
          <div className="protocol-section">
            <div className="protocol-section-header">My Protocols</div>
            <div className="protocol-cards">
              {userOwned.map(entry => (
                <ProtocolCard
                  key={entryId(entry)}
                  entry={entry}
                  availability={protocolLibrary.checkAvailability(entry)}
                  onEdit={() => onEdit(entry)}
                  onDuplicate={() => handleDuplicate(entry)}
                  onDelete={() => handleDelete(entry)}
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
    </div>
  );
}

// ─── Protocol Card ─────────────────────────────────────────────────────────────

interface ProtocolCardProps {
  entry: NPProtocolEntry;
  availability: ProtocolAvailability;
  onEdit: () => void;
  onDuplicate: () => void;
  onDelete: (() => void) | null;
}

function ProtocolCard({ entry, availability, onEdit, onDuplicate, onDelete }: ProtocolCardProps) {
  const name = entryName(entry);
  const description = entryDescription(entry);
  const tags = entryTags(entry);
  const isComposite = entry.kind === 'composite';

  // Get modality icons for single protocols
  const modalityIcons: Array<{ icon: string; label: string }> = [];
  if (entry.kind === 'single') {
    const seen = new Set<NPModalityTypeId>();
    for (const m of entry.protocol.modalities) {
      if (!seen.has(m.modalityParams.type)) {
        seen.add(m.modalityParams.type);
        const meta = MODALITY_META[m.modalityParams.type];
        if (meta) modalityIcons.push({ icon: meta.icon, label: meta.displayName });
      }
    }
  }

  const deviceTier = protocolLibrary.deviceTier;
  const availLabel = deviceTier === 'none'
    ? 'No device'
    : availability.available
      ? 'Available'
      : 'Unavailable';

  const availClass = deviceTier === 'none'
    ? 'no-device'
    : availability.available ? 'available' : 'unavailable';

  const missingTooltip = availability.missingModalities && availability.missingModalities.length > 0
    ? `Missing: ${availability.missingModalities.map(m => MODALITY_META[m]?.displayName ?? m).join(', ')}`
    : availability.reason ?? '';

  return (
    <div className={`protocol-card${!availability.available && deviceTier !== 'none' ? ' unavailable' : ''}`}>
      <div className="card-main">
        <div className="card-header">
          <div className="card-title">{name}</div>
          {isComposite && (
            <span className="composite-badge">
              ⊞ {entry.composite.layers.length} layers
            </span>
          )}
        </div>
        <div className="card-description">{description}</div>
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
        <button className="btn btn-secondary btn-sm" onClick={onEdit}>
          {onDelete ? 'Edit' : 'View'}
        </button>
        <button className="btn btn-ghost btn-sm" onClick={onDuplicate}>
          Copy
        </button>
        {onDelete && (
          <button className="btn btn-danger btn-sm" onClick={onDelete}>
            Delete
          </button>
        )}
      </div>
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
          <span className="modal-title">Confirm</span>
          <button className="modal-close" onClick={onCancel}>×</button>
        </div>
        <div className="modal-body">
          {message}
        </div>
        <div className="modal-footer">
          <button className="btn btn-secondary" onClick={onCancel}>Cancel</button>
          <button className="btn btn-danger" onClick={onConfirm}>Delete</button>
        </div>
      </div>
    </div>
  );
}
