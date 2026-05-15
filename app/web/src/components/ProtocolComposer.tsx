import { useState, useMemo } from 'react';
import {
  NPCompositeProtocol,
  NPCompositeLayer,
  NPProtocolEntry,
  entryIsPredefined,
} from '../types/protocol';
import { protocolLibrary } from '../lib/protocolLibrary';
import { serializeNPPS } from '../lib/nppsSerializer';
import { ScriptEditor } from './ScriptEditor';

interface ProtocolComposerProps {
  existing?: NPCompositeProtocol;
  onSave: (composite: NPCompositeProtocol) => void;
  onCancel: () => void;
}

type ComposerTab = 'visual' | 'script';

const LAYER_COLORS = [
  '#4a9eff', '#a855f7', '#22c55e', '#f59e0b',
  '#ef4444', '#06b6d4', '#ec4899', '#84cc16',
];

function blankComposite(): NPCompositeProtocol {
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
    layers: [],
    conflictResolution: 'merge',
  };
}

export function ProtocolComposer({ existing, onSave, onCancel }: ProtocolComposerProps) {
  const [tab, setTab] = useState<ComposerTab>('visual');
  const [composite, setComposite] = useState<NPCompositeProtocol>(() => {
    if (!existing) return blankComposite();
    if (existing.isPredefined) {
      const now = new Date().toISOString();
      return {
        ...existing,
        id: crypto.randomUUID(),
        name: existing.name + ' (copy)',
        isPredefined: false,
        createdAt: now,
        modifiedAt: now,
        layers: existing.layers.map(l => ({ ...l, id: crypto.randomUUID() })),
      };
    }
    return { ...existing };
  });
  const [showProtocolPicker, setShowProtocolPicker] = useState(false);
  const [tagInput, setTagInput] = useState('');
  const [saveError, setSaveError] = useState('');

  // Available single protocols to add as layers
  const availableSingleProtocols = protocolLibrary.allProtocols.filter(e => e.kind === 'single');

  function updateComposite(updates: Partial<NPCompositeProtocol>) {
    setComposite(prev => ({ ...prev, ...updates, modifiedAt: new Date().toISOString() }));
  }

  function addLayer(entry: NPProtocolEntry) {
    if (entry.kind !== 'single') return;
    const name = entry.protocol.name;
    // Find last layer end time
    const maxEnd = composite.layers.reduce((acc, l) => {
      const end = l.startOffsetSeconds + (l.durationSeconds ?? 1200);
      return Math.max(acc, end);
    }, 0);
    const newLayer: NPCompositeLayer = {
      id: crypto.randomUUID(),
      protocolName: name,
      startOffsetSeconds: maxEnd,
      durationSeconds: 1200,
      intensityScale: 1.0,
    };
    updateComposite({ layers: [...composite.layers, newLayer] });
    setShowProtocolPicker(false);
  }

  function updateLayer(id: string, updates: Partial<NPCompositeLayer>) {
    updateComposite({
      layers: composite.layers.map(l => l.id === id ? { ...l, ...updates } : l),
    });
  }

  function deleteLayer(id: string) {
    updateComposite({ layers: composite.layers.filter(l => l.id !== id) });
  }

  function handleAddTag(tag: string) {
    const t = tag.trim().toLowerCase().replace(/\s+/g, '-');
    if (t && !composite.tags.includes(t)) {
      updateComposite({ tags: [...composite.tags, t] });
    }
    setTagInput('');
  }

  function handleRemoveTag(tag: string) {
    updateComposite({ tags: composite.tags.filter(t => t !== tag) });
  }

  function handleSave() {
    if (!composite.name.trim()) { setSaveError('Composite name is required.'); return; }
    if (composite.layers.length === 0) { setSaveError('Add at least one layer.'); return; }
    setSaveError('');
    onSave({ ...composite, modifiedAt: new Date().toISOString() });
  }

  const scriptText = useMemo(() => {
    const entry: NPProtocolEntry = { kind: 'composite', composite };
    return serializeNPPS([entry]);
  }, [composite]);

  // Timeline data
  const maxTime = useMemo(() => {
    if (composite.layers.length === 0) return 1200;
    return composite.layers.reduce((acc, l) => {
      return Math.max(acc, l.startOffsetSeconds + (l.durationSeconds ?? 1200));
    }, 0);
  }, [composite.layers]);

  return (
    <div className="composer">
      <div className="editor-header">
        <button className="btn btn-ghost btn-sm" onClick={onCancel}>← Back</button>
        <div className="editor-title">
          {existing && !existing.isPredefined ? `Editing: ${existing.name}` : 'Composite Protocol'}
        </div>
        <div className="composer-tabs">
          <button className={`composer-tab${tab === 'visual' ? ' active' : ''}`} onClick={() => setTab('visual')}>Visual</button>
          <button className={`composer-tab${tab === 'script' ? ' active' : ''}`} onClick={() => setTab('script')}>Script Preview</button>
        </div>
      </div>

      {tab === 'visual' ? (
        <div className="composer-body">
          {/* Identity */}
          <div className="form-section">
            <div className="form-section-title">Identity</div>
            <div className="form-row">
              <div className="form-field full">
                <label className="form-label">Name *</label>
                <input className="form-input" value={composite.name} onChange={e => updateComposite({ name: e.target.value })} placeholder="Composite name" />
              </div>
              <div className="form-field full">
                <label className="form-label">Description</label>
                <textarea className="form-textarea" value={composite.description} onChange={e => updateComposite({ description: e.target.value })} placeholder="What is this composite for?" />
              </div>
            </div>
            <div className="form-row">
              <div className="form-field">
                <label className="form-label">Author</label>
                <input className="form-input" value={composite.author} onChange={e => updateComposite({ author: e.target.value })} />
              </div>
              <div className="form-field">
                <label className="form-label">Version</label>
                <input className="form-input" value={composite.version} onChange={e => updateComposite({ version: e.target.value })} style={{ maxWidth: 100 }} />
              </div>
              <div className="form-field">
                <label className="form-label">Conflict Resolution</label>
                <select className="form-select" value={composite.conflictResolution} onChange={e => updateComposite({ conflictResolution: e.target.value as NPCompositeProtocol['conflictResolution'] })}>
                  <option value="merge">Merge (simultaneous)</option>
                  <option value="sequential">Sequential</option>
                  <option value="override">Override</option>
                </select>
              </div>
            </div>
            <div className="form-field full">
              <label className="form-label">Tags</label>
              <div className="tags-editor">
                {composite.tags.map(tag => (
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
                    if (e.key === 'Backspace' && !tagInput && composite.tags.length > 0) handleRemoveTag(composite.tags[composite.tags.length - 1]);
                  }}
                  onBlur={() => { if (tagInput) handleAddTag(tagInput); }}
                  placeholder="Add tag…"
                />
              </div>
            </div>
          </div>

          {/* Timeline visualization */}
          <div className="timeline-container">
            <div className="timeline-header-bar">
              Timeline — {composite.layers.length} layer{composite.layers.length !== 1 ? 's' : ''} · Total: {Math.round(maxTime / 60)}m
            </div>
            <div className="timeline-svg-wrap">
              {composite.layers.length === 0 ? (
                <div className="empty-state" style={{ padding: '24px' }}>
                  <div className="empty-state-text">Add layers to see the timeline.</div>
                </div>
              ) : (
                <TimelineSVG layers={composite.layers} maxTime={maxTime} />
              )}
            </div>
          </div>

          {/* Layers */}
          <div className="form-section">
            <div className="form-section-title">Layers ({composite.layers.length})</div>
            <div className="layer-list">
              {composite.layers.length === 0 && (
                <div className="empty-state" style={{ padding: '20px 0' }}>
                  <div className="empty-state-text">No layers added. Click "Add Layer" to start composing.</div>
                </div>
              )}
              {composite.layers.map((layer, i) => (
                <div key={layer.id} className="layer-row">
                  <div className="layer-color-dot" style={{ background: LAYER_COLORS[i % LAYER_COLORS.length] }} />
                  <span className="layer-name">{layer.protocolName}</span>
                  <div className="layer-fields">
                    <div>
                      <div className="layer-field-label">Start (sec)</div>
                      <input
                        className="layer-field-input"
                        type="number"
                        min={0}
                        value={layer.startOffsetSeconds}
                        onChange={e => updateLayer(layer.id, { startOffsetSeconds: Number(e.target.value) })}
                      />
                    </div>
                    <div>
                      <div className="layer-field-label">Duration (sec)</div>
                      <input
                        className="layer-field-input"
                        type="number"
                        min={60}
                        value={layer.durationSeconds ?? ''}
                        placeholder="Full"
                        onChange={e => updateLayer(layer.id, { durationSeconds: e.target.value ? Number(e.target.value) : undefined })}
                      />
                    </div>
                    <div>
                      <div className="layer-field-label">Intensity</div>
                      <input
                        className="layer-field-input"
                        type="number"
                        min={0.1}
                        max={2.0}
                        step={0.05}
                        value={layer.intensityScale}
                        onChange={e => updateLayer(layer.id, { intensityScale: Number(e.target.value) })}
                      />
                    </div>
                  </div>
                  <button className="btn btn-danger btn-sm" onClick={() => deleteLayer(layer.id)}>Remove</button>
                </div>
              ))}
            </div>
            <button className="btn btn-secondary" style={{ marginTop: 10 }} onClick={() => setShowProtocolPicker(true)}>
              + Add Layer
            </button>
          </div>
        </div>
      ) : (
        <ScriptEditor
          value={scriptText}
          onChange={() => {}}
          onFormat={() => {}}
          error={undefined}
        />
      )}

      <div className="editor-footer">
        {saveError && <span style={{ color: 'var(--error)', fontSize: 13, flex: 1 }}>{saveError}</span>}
        <button className="btn btn-secondary" onClick={onCancel}>Cancel</button>
        <button className="btn btn-primary" onClick={handleSave}>
          {existing && !existing.isPredefined ? 'Save Changes' : 'Create Composite'}
        </button>
      </div>

      {showProtocolPicker && (
        <ProtocolPickerModal
          protocols={availableSingleProtocols}
          onSelect={addLayer}
          onClose={() => setShowProtocolPicker(false)}
        />
      )}
    </div>
  );
}

// ─── Timeline SVG ─────────────────────────────────────────────────────────────

interface TimelineSVGProps {
  layers: NPCompositeLayer[];
  maxTime: number;
}

function TimelineSVG({ layers, maxTime }: TimelineSVGProps) {
  const WIDTH = 700;
  const ROW_HEIGHT = 36;
  const LABEL_WIDTH = 140;
  const AXIS_HEIGHT = 24;
  const PADDING = 8;
  const DRAW_WIDTH = WIDTH - LABEL_WIDTH - PADDING * 2;

  const totalHeight = layers.length * ROW_HEIGHT + AXIS_HEIGHT + PADDING;

  function xFor(sec: number): number {
    return LABEL_WIDTH + (sec / maxTime) * DRAW_WIDTH;
  }

  // Time axis ticks: ~ every 2 minutes
  const tickInterval = maxTime <= 600 ? 60 : maxTime <= 1800 ? 120 : maxTime <= 3600 ? 300 : 600;
  const ticks: number[] = [];
  for (let t = 0; t <= maxTime; t += tickInterval) ticks.push(t);

  return (
    <svg
      width={WIDTH}
      height={totalHeight}
      style={{ display: 'block', overflow: 'visible' }}
    >
      {/* Background */}
      <rect x={0} y={0} width={WIDTH} height={totalHeight} fill="transparent" />

      {/* Axis */}
      <line x1={LABEL_WIDTH} y1={AXIS_HEIGHT - 4} x2={WIDTH - PADDING} y2={AXIS_HEIGHT - 4} stroke="var(--border)" strokeWidth={1} />

      {ticks.map(t => {
        const x = xFor(t);
        return (
          <g key={t}>
            <line x1={x} y1={AXIS_HEIGHT - 8} x2={x} y2={totalHeight} stroke="var(--border)" strokeWidth={0.5} strokeDasharray="3 3" />
            <text x={x} y={AXIS_HEIGHT - 10} fill="var(--text-muted)" fontSize={10} textAnchor="middle">
              {t >= 60 ? `${Math.round(t / 60)}m` : `${t}s`}
            </text>
          </g>
        );
      })}

      {/* Layers */}
      {layers.map((layer, i) => {
        const y = AXIS_HEIGHT + i * ROW_HEIGHT;
        const x = xFor(layer.startOffsetSeconds);
        const dur = layer.durationSeconds ?? (maxTime - layer.startOffsetSeconds);
        const w = Math.max((dur / maxTime) * DRAW_WIDTH, 4);
        const color = LAYER_COLORS[i % LAYER_COLORS.length];
        const label = layer.protocolName.length > 18 ? layer.protocolName.slice(0, 17) + '…' : layer.protocolName;

        return (
          <g key={layer.id}>
            {/* Row label */}
            <text
              x={LABEL_WIDTH - 6}
              y={y + ROW_HEIGHT / 2 + 4}
              fill="var(--text-secondary)"
              fontSize={11}
              textAnchor="end"
              fontWeight="500"
            >
              {i + 1}. {layer.protocolName.length > 14 ? layer.protocolName.slice(0, 13) + '…' : layer.protocolName}
            </text>

            {/* Bar */}
            <rect
              x={x}
              y={y + 4}
              width={w}
              height={ROW_HEIGHT - 8}
              rx={4}
              fill={color}
              fillOpacity={0.85}
            />

            {/* Bar label (if wide enough) */}
            {w > 50 && (
              <text
                x={x + Math.min(w / 2, 60)}
                y={y + ROW_HEIGHT / 2 + 4}
                fill="white"
                fontSize={10}
                fontWeight="600"
                textAnchor="middle"
              >
                {Math.round((dur / 60))}m
                {layer.intensityScale !== 1.0 ? ` ×${layer.intensityScale}` : ''}
              </text>
            )}
          </g>
        );
      })}
    </svg>
  );
}

// ─── Protocol Picker Modal ─────────────────────────────────────────────────────

interface ProtocolPickerModalProps {
  protocols: NPProtocolEntry[];
  onSelect: (entry: NPProtocolEntry) => void;
  onClose: () => void;
}

function ProtocolPickerModal({ protocols, onSelect, onClose }: ProtocolPickerModalProps) {
  const [selected, setSelected] = useState<string | null>(null);

  const predefined = protocols.filter(e => entryIsPredefined(e));
  const user = protocols.filter(e => !entryIsPredefined(e));

  function handleAdd() {
    const found = protocols.find(e => e.kind === 'single' && e.protocol.id === selected);
    if (found) { onSelect(found); }
  }

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div className="modal" onClick={e => e.stopPropagation()}>
        <div className="modal-header">
          <span className="modal-title">Add Layer</span>
          <button className="modal-close" onClick={onClose}>×</button>
        </div>
        <div className="modal-body">
          {predefined.length > 0 && (
            <div>
              <div className="picker-group-title">Predefined Protocols</div>
              {predefined.map(entry => {
                if (entry.kind !== 'single') return null;
                const id = entry.protocol.id;
                return (
                  <div
                    key={id}
                    className={`picker-item${selected === id ? ' selected' : ''}`}
                    onClick={() => setSelected(id)}
                  >
                    <div className="picker-info">
                      <div className="picker-name">{entry.protocol.name}</div>
                      <div className="picker-desc">{entry.protocol.description.slice(0, 70)}{entry.protocol.description.length > 70 ? '…' : ''}</div>
                    </div>
                  </div>
                );
              })}
            </div>
          )}
          {user.length > 0 && (
            <div>
              <div className="picker-group-title">My Protocols</div>
              {user.map(entry => {
                if (entry.kind !== 'single') return null;
                const id = entry.protocol.id;
                return (
                  <div
                    key={id}
                    className={`picker-item${selected === id ? ' selected' : ''}`}
                    onClick={() => setSelected(id)}
                  >
                    <div className="picker-info">
                      <div className="picker-name">{entry.protocol.name}</div>
                      <div className="picker-desc">{entry.protocol.description.slice(0, 70)}{entry.protocol.description.length > 70 ? '…' : ''}</div>
                    </div>
                  </div>
                );
              })}
            </div>
          )}
          {protocols.length === 0 && (
            <div className="empty-state" style={{ padding: 20 }}>
              <div className="empty-state-text">No protocols available to add as layers.</div>
            </div>
          )}
        </div>
        <div className="modal-footer">
          <button className="btn btn-secondary" onClick={onClose}>Cancel</button>
          <button className="btn btn-primary" disabled={!selected} onClick={handleAdd}>
            Add Layer
          </button>
        </div>
      </div>
    </div>
  );
}
