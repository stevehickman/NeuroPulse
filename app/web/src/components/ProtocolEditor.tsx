import React, { useState, useCallback, useEffect } from 'react';
import {
  NPProtocolEntry,
  NPProtocolDefinition,
  NPProtocolModality,
  NPModalityTypeId,
  NPTimingMode,
  defaultParams,
  MODALITY_META,
} from '../types/protocol';
import { parseNPPS, NPPSParseError } from '../lib/nppsParser';
import { serializeNPPS } from '../lib/nppsSerializer';
import { ModalityEditorBlock, ModalityPicker } from './ModalityEditor';
import { ScriptEditor } from './ScriptEditor';

interface ProtocolEditorProps {
  existing?: NPProtocolEntry;
  onSave: (entry: NPProtocolEntry) => void;
  onCancel: () => void;
}

type EditorTab = 'visual' | 'script';

function blankProtocol(): NPProtocolDefinition {
  const now = new Date().toISOString();
  return {
    id: crypto.randomUUID(),
    name: '',
    description: '',
    author: 'NeuroPulse',
    version: '1.0',
    tags: [],
    createdAt: now,
    modifiedAt: now,
    isPredefined: false,
    timingMode: { type: 'duration', seconds: 1200 },
    modalities: [],
  };
}

export function ProtocolEditor({ existing, onSave, onCancel }: ProtocolEditorProps) {
  const isNew = !existing || (existing.kind === 'single' && existing.protocol.isPredefined);

  const [tab, setTab] = useState<EditorTab>('visual');
  const [protocol, setProtocol] = useState<NPProtocolDefinition>(() => {
    if (!existing) return blankProtocol();
    if (existing.kind === 'single') {
      const p = existing.protocol;
      if (p.isPredefined) {
        // Editing predefined: create a copy
        const now = new Date().toISOString();
        return { ...p, id: crypto.randomUUID(), isPredefined: false, createdAt: now, modifiedAt: now,
          name: p.name + ' (copy)',
          modalities: p.modalities.map(m => ({ ...m, id: crypto.randomUUID() })) };
      }
      return { ...p };
    }
    return blankProtocol();
  });

  const [scriptText, setScriptText] = useState('');
  const [scriptError, setScriptError] = useState<string | undefined>();
  const [showPicker, setShowPicker] = useState(false);
  const [tagInput, setTagInput] = useState('');
  const [saveError, setSaveError] = useState('');

  // Sync visual → script when switching to script tab
  useEffect(() => {
    if (tab === 'script') {
      const entry: NPProtocolEntry = { kind: 'single', protocol };
      setScriptText(serializeNPPS([entry]));
      setScriptError(undefined);
    }
  }, [tab]);

  function handleScriptChange(text: string) {
    setScriptText(text);
    try {
      const entries = parseNPPS(text);
      if (entries.length > 0 && entries[0].kind === 'single') {
        const parsed = entries[0].protocol;
        // Preserve id, createdAt, isPredefined from current state
        setProtocol(prev => ({
          ...parsed,
          id: prev.id,
          createdAt: prev.createdAt,
          isPredefined: false,
          modifiedAt: new Date().toISOString(),
        }));
        setScriptError(undefined);
      } else if (entries.length === 0) {
        setScriptError(undefined);
      }
    } catch (e) {
      if (e instanceof NPPSParseError) {
        setScriptError(e.message);
      } else {
        setScriptError(String(e));
      }
    }
  }

  function handleFormat() {
    try {
      const entry: NPProtocolEntry = { kind: 'single', protocol };
      setScriptText(serializeNPPS([entry]));
      setScriptError(undefined);
    } catch {}
  }

  function handleSwitchToVisual() {
    // If script is valid, parse it first
    if (!scriptError && scriptText.trim()) {
      try {
        const entries = parseNPPS(scriptText);
        if (entries.length > 0 && entries[0].kind === 'single') {
          const parsed = entries[0].protocol;
          setProtocol(prev => ({
            ...parsed,
            id: prev.id,
            createdAt: prev.createdAt,
            isPredefined: false,
            modifiedAt: new Date().toISOString(),
          }));
        }
      } catch {}
    }
    setTab('visual');
  }

  function handleTabChange(newTab: EditorTab) {
    if (newTab === 'script') setTab('script');
    else handleSwitchToVisual();
  }

  function updateProtocol(updates: Partial<NPProtocolDefinition>) {
    setProtocol(prev => ({ ...prev, ...updates, modifiedAt: new Date().toISOString() }));
  }

  function handleAddModality(typeId: NPModalityTypeId) {
    const newModality: NPProtocolModality = {
      id: crypto.randomUUID(),
      modalityParams: { type: typeId, params: defaultParams(typeId) } as NPProtocolModality['modalityParams'],
      interval: { intervalOnSeconds: 0, intervalOffSeconds: 0 },
      enabled: true,
    };
    updateProtocol({ modalities: [...protocol.modalities, newModality] });
  }

  function handleModalityChange(index: number, updated: NPProtocolModality) {
    const next = [...protocol.modalities];
    next[index] = updated;
    updateProtocol({ modalities: next });
  }

  function handleModalityDelete(index: number) {
    updateProtocol({ modalities: protocol.modalities.filter((_, i) => i !== index) });
  }

  function handleAddTag(tag: string) {
    const t = tag.trim().toLowerCase().replace(/\s+/g, '-');
    if (t && !protocol.tags.includes(t)) {
      updateProtocol({ tags: [...protocol.tags, t] });
    }
    setTagInput('');
  }

  function handleRemoveTag(tag: string) {
    updateProtocol({ tags: protocol.tags.filter(t => t !== tag) });
  }

  function handleSave() {
    if (!protocol.name.trim()) {
      setSaveError('Protocol name is required.');
      return;
    }
    setSaveError('');
    const entry: NPProtocolEntry = { kind: 'single', protocol: { ...protocol, modifiedAt: new Date().toISOString() } };
    onSave(entry);
  }

  const timingType = protocol.timingMode.type;
  const timingValue = protocol.timingMode.type === 'duration'
    ? protocol.timingMode.seconds
    : protocol.timingMode.count;

  return (
    <div className="protocol-editor">
      <div className="editor-header">
        <button className="btn btn-ghost btn-sm" onClick={onCancel}>← Back</button>
        <div className="editor-title">
          {existing && !isNew ? `Editing: ${existing.kind === 'single' ? existing.protocol.name : ''}` : 'New Protocol'}
        </div>
        <div className="editor-tabs">
          <button className={`editor-tab${tab === 'visual' ? ' active' : ''}`} onClick={() => handleTabChange('visual')}>
            Visual
          </button>
          <button className={`editor-tab${tab === 'script' ? ' active' : ''}`} onClick={() => handleTabChange('script')}>
            Script
          </button>
        </div>
      </div>

      {tab === 'visual' ? (
        <div className="editor-body">
          {/* Metadata */}
          <div className="form-section">
            <div className="form-section-title">Identity</div>
            <div className="form-row">
              <div className="form-field full">
                <label className="form-label">Name *</label>
                <input
                  className="form-input"
                  value={protocol.name}
                  onChange={e => updateProtocol({ name: e.target.value })}
                  placeholder="Protocol name"
                />
              </div>
              <div className="form-field full">
                <label className="form-label">Description</label>
                <textarea
                  className="form-textarea"
                  value={protocol.description}
                  onChange={e => updateProtocol({ description: e.target.value })}
                  placeholder="What is this protocol for?"
                />
              </div>
            </div>
            <div className="form-row">
              <div className="form-field">
                <label className="form-label">Author</label>
                <input className="form-input" value={protocol.author} onChange={e => updateProtocol({ author: e.target.value })} />
              </div>
              <div className="form-field">
                <label className="form-label">Version</label>
                <input className="form-input" value={protocol.version} onChange={e => updateProtocol({ version: e.target.value })} style={{ maxWidth: 100 }} />
              </div>
            </div>
            <div className="form-field full">
              <label className="form-label">Tags</label>
              <div className="tags-editor">
                {protocol.tags.map(tag => (
                  <span key={tag} className="tag-pill">
                    {tag}
                    <button className="tag-remove" onClick={() => handleRemoveTag(tag)}>×</button>
                  </span>
                ))}
                <input
                  className="tag-add-input"
                  value={tagInput}
                  onChange={e => setTagInput(e.target.value)}
                  onKeyDown={e => {
                    if (e.key === 'Enter' || e.key === ',') { e.preventDefault(); handleAddTag(tagInput); }
                    if (e.key === 'Backspace' && !tagInput && protocol.tags.length > 0) {
                      handleRemoveTag(protocol.tags[protocol.tags.length - 1]);
                    }
                  }}
                  onBlur={() => { if (tagInput) handleAddTag(tagInput); }}
                  placeholder="Add tag…"
                />
              </div>
            </div>
          </div>

          {/* Timing */}
          <div className="form-section">
            <div className="form-section-title">Session Timing</div>
            <div className="form-row">
              <div className="form-field">
                <label className="form-label">Mode</label>
                <div className="radio-group">
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="timing"
                      value="duration"
                      checked={timingType === 'duration'}
                      onChange={() => updateProtocol({ timingMode: { type: 'duration', seconds: timingValue } })}
                    />
                    <span>Duration (seconds)</span>
                  </label>
                  <label className="radio-label">
                    <input
                      type="radio"
                      name="timing"
                      value="interval_count"
                      checked={timingType === 'interval_count'}
                      onChange={() => updateProtocol({ timingMode: { type: 'interval_count', count: timingValue } })}
                    />
                    <span>Interval count</span>
                  </label>
                </div>
              </div>
              <div className="form-field" style={{ maxWidth: 180 }}>
                <label className="form-label">
                  {timingType === 'duration' ? 'Duration (sec)' : 'Repeat count'}
                </label>
                <input
                  type="number"
                  className="form-input"
                  min={1}
                  value={timingValue}
                  onChange={e => {
                    const v = Number(e.target.value);
                    if (timingType === 'duration') {
                      updateProtocol({ timingMode: { type: 'duration', seconds: v } });
                    } else {
                      updateProtocol({ timingMode: { type: 'interval_count', count: v } });
                    }
                  }}
                />
                {timingType === 'duration' && (
                  <span className="form-hint">
                    = {Math.floor(timingValue / 60)}m {timingValue % 60}s
                  </span>
                )}
              </div>
            </div>
          </div>

          {/* Modalities */}
          <div className="form-section">
            <div className="form-section-title" style={{ marginBottom: 10 }}>
              Modalities ({protocol.modalities.length})
            </div>
            <div className="modality-list">
              {protocol.modalities.length === 0 && (
                <div className="empty-state" style={{ padding: '30px 0' }}>
                  <div className="empty-state-icon">🧠</div>
                  <div className="empty-state-text">No modalities added yet. Click "Add Modality" to begin.</div>
                </div>
              )}
              {protocol.modalities.map((m, i) => (
                <ModalityEditorBlock
                  key={m.id}
                  modality={m}
                  onChange={updated => handleModalityChange(i, updated)}
                  onDelete={() => handleModalityDelete(i)}
                />
              ))}
            </div>
            <button className="btn btn-secondary" style={{ marginTop: 10 }} onClick={() => setShowPicker(true)}>
              + Add Modality
            </button>
          </div>
        </div>
      ) : (
        <ScriptEditor
          value={scriptText}
          onChange={handleScriptChange}
          error={scriptError}
          onFormat={handleFormat}
        />
      )}

      <div className="editor-footer">
        {saveError && <span style={{ color: 'var(--error)', fontSize: 13, flex: 1 }}>{saveError}</span>}
        <button className="btn btn-secondary" onClick={onCancel}>Cancel</button>
        <button className="btn btn-primary" onClick={handleSave}>
          {isNew ? 'Create Protocol' : 'Save Changes'}
        </button>
      </div>

      {showPicker && (
        <ModalityPicker
          onSelect={handleAddModality}
          onClose={() => setShowPicker(false)}
        />
      )}
    </div>
  );
}
