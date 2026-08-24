import { useState, useEffect } from 'react';
import {
  NPLimitsSet,
  NPIndividualProfile,
  LimitLevel,
} from '../types/limits';
import { serializeNPPSLimits } from '../lib/nppsSerializer';
import { parseNPPSLimits } from '../lib/nppsParser';
import { limitsStore } from '../lib/limitsStore';
import { NPHardwareLimits } from '../lib/hardwareLimits';

// ─── Props ─────────────────────────────────────────────────────────────────────

interface LimitsSettingsProps {
  onClose: () => void;
}

// ─── Tabs ─────────────────────────────────────────────────────────────────────

type SettingsTab = 'global' | 'helmet' | 'individual';

// ─── Helper: blank limits set ─────────────────────────────────────────────────

function blankLimitsSet(level: LimitLevel, name: string): NPLimitsSet {
  const now = new Date().toISOString();
  return {
    id: crypto.randomUUID(),
    name,
    description: '',
    createdAt: now,
    modifiedAt: now,
    level,
  };
}

// ─── Main Component ───────────────────────────────────────────────────────────

export function LimitsSettings({ onClose }: LimitsSettingsProps) {
  const [tab, setTab] = useState<SettingsTab>('global');
  const [, setVersion] = useState(0);

  // Subscribe to store changes
  useEffect(() => {
    const handler = () => setVersion(v => v + 1);
    limitsStore.addEventListener('change', handler);
    return () => limitsStore.removeEventListener('change', handler);
  }, []);

  // Editor modal state
  const [editingLimits, setEditingLimits] = useState<NPLimitsSet | null>(null);
  const [editingContext, setEditingContext] = useState<
    | { type: 'global' }
    | { type: 'helmet'; helmetId: string }
    | { type: 'individual'; profileId: string }
    | null
  >(null);

  // Profile modal state
  const [showNewProfile, setShowNewProfile] = useState(false);
  const [profileName, setProfileName] = useState('');
  const [profileNotes, setProfileNotes] = useState('');
  const [profileDob, setProfileDob] = useState('');

  // New helmet state
  const [showNewHelmet, setShowNewHelmet] = useState(false);
  const [newHelmetSerial, setNewHelmetSerial] = useState('');

  // Delete confirmation
  const [confirmDelete, setConfirmDelete] = useState<{
    message: string;
    onConfirm: () => void;
  } | null>(null);

  function openGlobalEditor() {
    setEditingLimits(limitsStore.globalLimits ?? blankLimitsSet('global', 'Global Limits'));
    setEditingContext({ type: 'global' });
  }

  function openHelmetEditor(helmetId: string) {
    const existing = limitsStore.helmetLimits.get(helmetId);
    setEditingLimits(existing ?? blankLimitsSet('helmet', `Helmet ${helmetId}`));
    setEditingContext({ type: 'helmet', helmetId });
  }

  function openIndividualEditor(profileId: string) {
    const existing = limitsStore.individualLimits.get(profileId);
    setEditingLimits(existing ?? blankLimitsSet('individual', 'Individual Limits'));
    setEditingContext({ type: 'individual', profileId });
  }

  function saveEditing(updated: NPLimitsSet) {
    if (!editingContext) return;
    if (editingContext.type === 'global') {
      limitsStore.saveGlobalLimits(updated);
    } else if (editingContext.type === 'helmet') {
      limitsStore.saveHelmetLimits(updated, editingContext.helmetId);
    } else if (editingContext.type === 'individual') {
      limitsStore.saveIndividualLimits(updated, editingContext.profileId);
    }
    setEditingLimits(null);
    setEditingContext(null);
  }

  function handleSaveProfile() {
    if (!profileName.trim()) return;
    const profile: NPIndividualProfile = {
      id: crypto.randomUUID(),
      name: profileName.trim(),
      notes: profileNotes.trim(),
      dateOfBirth: profileDob || undefined,
      createdAt: new Date().toISOString(),
    };
    limitsStore.saveProfile(profile);
    setShowNewProfile(false);
    setProfileName('');
    setProfileNotes('');
    setProfileDob('');
  }

  function handleAddHelmet() {
    const serial = newHelmetSerial.trim();
    if (!serial) return;
    limitsStore.saveHelmetLimits(blankLimitsSet('helmet', `Helmet ${serial}`), serial);
    setShowNewHelmet(false);
    setNewHelmetSerial('');
  }

  function handleDeleteProfile(id: string, name: string) {
    setConfirmDelete({
      message: `Delete profile "${name}"? This will also remove their individual limits. This cannot be undone.`,
      onConfirm: () => {
        limitsStore.deleteProfile(id);
        setConfirmDelete(null);
      },
    });
  }

  function handleDeleteHelmet(helmetId: string) {
    setConfirmDelete({
      message: `Remove limits configuration for helmet "${helmetId}"?`,
      onConfirm: () => {
        limitsStore.deleteHelmetLimits(helmetId);
        setConfirmDelete(null);
      },
    });
  }

  const chain = limitsStore.resolutionChainDescription;

  // If editing, show the editor
  if (editingLimits && editingContext) {
    return (
      <div className="modal-overlay">
        <div
          className="modal"
          style={{ width: '90vw', maxWidth: 820, maxHeight: '90vh', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}
          onClick={e => e.stopPropagation()}
        >
          <div className="modal-header">
            <span className="modal-title">Edit Limits — {editingLimits.name}</span>
            <button className="modal-close" onClick={() => { setEditingLimits(null); setEditingContext(null); }}>×</button>
          </div>
          <LimitsEditor
            initial={editingLimits}
            onSave={saveEditing}
            onCancel={() => { setEditingLimits(null); setEditingContext(null); }}
            onExport={l => limitsStore.downloadLimitsFile(l)}
          />
        </div>
      </div>
    );
  }

  return (
    <div className="modal-overlay" onClick={onClose}>
      <div
        className="modal"
        style={{ width: '80vw', maxWidth: 700, maxHeight: '85vh', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}
        onClick={e => e.stopPropagation()}
      >
        <div className="modal-header">
          <span className="modal-title">Dosage Limits</span>
          <button className="modal-close" onClick={onClose}>×</button>
        </div>

        {/* Active resolution chain banner */}
        <div style={{
          padding: '8px 20px',
          background: 'var(--bg-secondary)',
          borderBottom: '1px solid var(--border)',
          fontSize: 12,
          color: 'var(--text-secondary)',
        }}>
          <span style={{ color: 'var(--text-muted)' }}>Active: </span>
          <span style={{ color: 'var(--accent)' }}>{chain}</span>
        </div>

        {/* Tab bar */}
        <div style={{ display: 'flex', borderBottom: '1px solid var(--border)', background: 'var(--bg-secondary)' }}>
          {(['global', 'helmet', 'individual'] as SettingsTab[]).map(t => (
            <button
              key={t}
              onClick={() => setTab(t)}
              style={{
                padding: '10px 20px',
                background: 'none',
                border: 'none',
                borderBottom: tab === t ? '2px solid var(--accent)' : '2px solid transparent',
                color: tab === t ? 'var(--accent)' : 'var(--text-secondary)',
                cursor: 'pointer',
                fontSize: 13,
                fontWeight: tab === t ? 600 : 400,
                textTransform: 'capitalize',
              }}
            >
              {t === 'global' ? 'Global' : t === 'helmet' ? 'Helmet' : 'Individual'}
            </button>
          ))}
        </div>

        {/* Tab content */}
        <div style={{ flex: 1, overflow: 'auto', padding: 20 }}>
          {tab === 'global' && (
            <GlobalTab
              onEdit={openGlobalEditor}
              onImport={() => {
                const input = document.createElement('input');
                input.type = 'file';
                input.accept = '.npps,.txt';
                input.onchange = async () => {
                  const file = input.files?.[0];
                  if (!file) return;
                  const text = await file.text();
                  try {
                    const imported = limitsStore.importLimitsFromNPPS(text);
                    limitsStore.saveGlobalLimits(imported);
                  } catch (e) {
                    alert(`Import failed: ${(e as Error).message}`);
                  }
                };
                input.click();
              }}
              onExport={() => {
                const g = limitsStore.globalLimits;
                if (g) limitsStore.downloadLimitsFile(g);
              }}
              onClear={() => limitsStore.clearGlobalLimits()}
            />
          )}

          {tab === 'helmet' && (
            <HelmetTab
              helmetLimits={limitsStore.helmetLimits}
              activeHelmet={limitsStore.activeHelmetSerial}
              showNewHelmet={showNewHelmet}
              newHelmetSerial={newHelmetSerial}
              onSetActive={s => limitsStore.setActiveHelmet(s === limitsStore.activeHelmetSerial ? undefined : s)}
              onEdit={openHelmetEditor}
              onDelete={handleDeleteHelmet}
              onShowNew={() => setShowNewHelmet(true)}
              onCancelNew={() => { setShowNewHelmet(false); setNewHelmetSerial(''); }}
              onSerialChange={setNewHelmetSerial}
              onAddHelmet={handleAddHelmet}
            />
          )}

          {tab === 'individual' && (
            <IndividualTab
              profiles={limitsStore.profiles}
              individualLimits={limitsStore.individualLimits}
              activeProfileId={limitsStore.activeProfileId}
              showNewProfile={showNewProfile}
              profileName={profileName}
              profileNotes={profileNotes}
              profileDob={profileDob}
              onSetActive={id => limitsStore.setActiveProfile(id === limitsStore.activeProfileId ? undefined : id)}
              onEditLimits={openIndividualEditor}
              onDelete={handleDeleteProfile}
              onShowNew={() => setShowNewProfile(true)}
              onCancelNew={() => { setShowNewProfile(false); setProfileName(''); setProfileNotes(''); setProfileDob(''); }}
              onNameChange={setProfileName}
              onNotesChange={setProfileNotes}
              onDobChange={setProfileDob}
              onSaveProfile={handleSaveProfile}
            />
          )}
        </div>

        {confirmDelete && (
          <div className="modal-overlay" onClick={() => setConfirmDelete(null)}>
            <div className="modal confirm-dialog" onClick={e => e.stopPropagation()}>
              <div className="modal-header">
                <span className="modal-title">Confirm</span>
                <button className="modal-close" onClick={() => setConfirmDelete(null)}>×</button>
              </div>
              <div className="modal-body">{confirmDelete.message}</div>
              <div className="modal-footer">
                <button className="btn btn-secondary" onClick={() => setConfirmDelete(null)}>Cancel</button>
                <button className="btn btn-danger" onClick={confirmDelete.onConfirm}>Delete</button>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

// ─── Global Tab ───────────────────────────────────────────────────────────────

function GlobalTab({
  onEdit,
  onImport,
  onExport,
  onClear,
}: {
  onEdit: () => void;
  onImport: () => void;
  onExport: () => void;
  onClear: () => void;
}) {
  const g = limitsStore.globalLimits;

  return (
    <div>
      <div style={{ marginBottom: 16 }}>
        <div style={{ fontSize: 13, color: 'var(--text-secondary)', marginBottom: 12 }}>
          Global limits apply to all sessions on all devices unless overridden by helmet or individual limits.
        </div>
        <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
          <button className="btn btn-primary" onClick={onEdit}>Edit Global Limits</button>
          <button className="btn btn-secondary" onClick={onImport}>Import .npps</button>
          {g && (
            <>
              <button className="btn btn-secondary" onClick={onExport}>Export .npps</button>
              <button className="btn btn-danger" onClick={onClear}>Clear</button>
            </>
          )}
        </div>
      </div>

      {!g ? (
        <div style={{
          padding: '24px',
          textAlign: 'center',
          color: 'var(--text-muted)',
          border: '1px dashed var(--border)',
          borderRadius: 8,
        }}>
          No global limits configured — hardware limits only apply.
        </div>
      ) : (
        <div>
          <div style={{ fontWeight: 600, marginBottom: 8, fontSize: 13 }}>{g.name}</div>
          {g.description && (
            <div style={{ fontSize: 12, color: 'var(--text-secondary)', marginBottom: 8 }}>{g.description}</div>
          )}
          <LimitsSummary limits={g} />
        </div>
      )}
    </div>
  );
}

// ─── Helmet Tab ───────────────────────────────────────────────────────────────

function HelmetTab({
  helmetLimits,
  activeHelmet,
  showNewHelmet,
  newHelmetSerial,
  onSetActive,
  onEdit,
  onDelete,
  onShowNew,
  onCancelNew,
  onSerialChange,
  onAddHelmet,
}: {
  helmetLimits: ReadonlyMap<string, NPLimitsSet>;
  activeHelmet: string | undefined;
  showNewHelmet: boolean;
  newHelmetSerial: string;
  onSetActive: (s: string) => void;
  onEdit: (s: string) => void;
  onDelete: (s: string) => void;
  onShowNew: () => void;
  onCancelNew: () => void;
  onSerialChange: (s: string) => void;
  onAddHelmet: () => void;
}) {
  const helmets = Array.from(helmetLimits.entries());

  return (
    <div>
      <div style={{ fontSize: 13, color: 'var(--text-secondary)', marginBottom: 12 }}>
        Helmet limits are tied to a specific device serial number. They override global limits for that device.
      </div>
      <button className="btn btn-primary" style={{ marginBottom: 16 }} onClick={onShowNew}>
        + Add Helmet
      </button>

      {showNewHelmet && (
        <div style={{
          padding: 12,
          background: 'var(--bg-secondary)',
          border: '1px solid var(--border)',
          borderRadius: 6,
          marginBottom: 16,
          display: 'flex',
          gap: 8,
          alignItems: 'center',
        }}>
          <input
            type="text"
            placeholder="Helmet serial number (e.g. NP-001)"
            value={newHelmetSerial}
            onChange={e => onSerialChange(e.target.value)}
            onKeyDown={e => { if (e.key === 'Enter') onAddHelmet(); if (e.key === 'Escape') onCancelNew(); }}
            style={{
              flex: 1,
              background: 'var(--bg-input)',
              border: '1px solid var(--border)',
              borderRadius: 4,
              color: 'var(--text-primary)',
              padding: '6px 10px',
              fontSize: 13,
              outline: 'none',
            }}
            autoFocus
          />
          <button className="btn btn-primary btn-sm" onClick={onAddHelmet}>Add</button>
          <button className="btn btn-secondary btn-sm" onClick={onCancelNew}>Cancel</button>
        </div>
      )}

      {helmets.length === 0 && !showNewHelmet && (
        <div style={{
          padding: '24px',
          textAlign: 'center',
          color: 'var(--text-muted)',
          border: '1px dashed var(--border)',
          borderRadius: 8,
        }}>
          No helmet limits configured.
        </div>
      )}

      {helmets.map(([serial, limits]) => (
        <div
          key={serial}
          style={{
            padding: 12,
            background: activeHelmet === serial ? 'var(--bg-hover)' : 'var(--bg-secondary)',
            border: `1px solid ${activeHelmet === serial ? 'var(--accent)' : 'var(--border)'}`,
            borderRadius: 6,
            marginBottom: 8,
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 8 }}>
            <span style={{ fontWeight: 600, fontSize: 13 }}>{serial}</span>
            {activeHelmet === serial && (
              <span style={{
                fontSize: 10,
                padding: '2px 6px',
                background: 'var(--accent)',
                color: '#000',
                borderRadius: 10,
                fontWeight: 700,
              }}>ACTIVE</span>
            )}
            <div style={{ marginLeft: 'auto', display: 'flex', gap: 6 }}>
              <button
                className="btn btn-secondary btn-sm"
                onClick={() => onSetActive(serial)}
              >
                {activeHelmet === serial ? 'Deactivate' : 'Set Active'}
              </button>
              <button className="btn btn-secondary btn-sm" onClick={() => onEdit(serial)}>Edit</button>
              <button className="btn btn-danger btn-sm" onClick={() => onDelete(serial)}>Remove</button>
            </div>
          </div>
          <LimitsSummary limits={limits} />
        </div>
      ))}
    </div>
  );
}

// ─── Individual Tab ───────────────────────────────────────────────────────────

function IndividualTab({
  profiles,
  individualLimits,
  activeProfileId,
  showNewProfile,
  profileName,
  profileNotes,
  profileDob,
  onSetActive,
  onEditLimits,
  onDelete,
  onShowNew,
  onCancelNew,
  onNameChange,
  onNotesChange,
  onDobChange,
  onSaveProfile,
}: {
  profiles: readonly NPIndividualProfile[];
  individualLimits: ReadonlyMap<string, NPLimitsSet>;
  activeProfileId: string | undefined;
  showNewProfile: boolean;
  profileName: string;
  profileNotes: string;
  profileDob: string;
  onSetActive: (id: string) => void;
  onEditLimits: (profileId: string) => void;
  onDelete: (id: string, name: string) => void;
  onShowNew: () => void;
  onCancelNew: () => void;
  onNameChange: (s: string) => void;
  onNotesChange: (s: string) => void;
  onDobChange: (s: string) => void;
  onSaveProfile: () => void;
}) {
  // Active profile first
  const sorted = [...profiles].sort((a, b) => {
    if (a.id === activeProfileId) return -1;
    if (b.id === activeProfileId) return 1;
    return a.name.localeCompare(b.name);
  });

  return (
    <div>
      <div style={{ fontSize: 13, color: 'var(--text-secondary)', marginBottom: 12 }}>
        Individual profiles override global and helmet limits for a specific person. Only one profile is active at a time.
      </div>
      <button className="btn btn-primary" style={{ marginBottom: 16 }} onClick={onShowNew}>
        + New Profile
      </button>

      {showNewProfile && (
        <div style={{
          padding: 14,
          background: 'var(--bg-secondary)',
          border: '1px solid var(--border)',
          borderRadius: 6,
          marginBottom: 16,
        }}>
          <div style={{ fontWeight: 600, fontSize: 13, marginBottom: 10 }}>New Profile</div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
            <input
              type="text"
              placeholder="Name *"
              value={profileName}
              onChange={e => onNameChange(e.target.value)}
              style={inputStyle}
              autoFocus
            />
            <input
              type="date"
              placeholder="Date of birth (optional)"
              value={profileDob}
              onChange={e => onDobChange(e.target.value)}
              style={inputStyle}
            />
            <textarea
              placeholder="Notes (optional)"
              value={profileNotes}
              onChange={e => onNotesChange(e.target.value)}
              rows={2}
              style={{ ...inputStyle, resize: 'vertical' }}
            />
            <div style={{ display: 'flex', gap: 8 }}>
              <button className="btn btn-primary btn-sm" onClick={onSaveProfile} disabled={!profileName.trim()}>
                Create Profile
              </button>
              <button className="btn btn-secondary btn-sm" onClick={onCancelNew}>Cancel</button>
            </div>
          </div>
        </div>
      )}

      {profiles.length === 0 && !showNewProfile && (
        <div style={{
          padding: '24px',
          textAlign: 'center',
          color: 'var(--text-muted)',
          border: '1px dashed var(--border)',
          borderRadius: 8,
        }}>
          No individual profiles created.
        </div>
      )}

      {sorted.map(profile => {
        const isActive = profile.id === activeProfileId;
        const hasLimits = individualLimits.has(profile.id);
        return (
          <div
            key={profile.id}
            style={{
              padding: 12,
              background: isActive ? 'var(--bg-hover)' : 'var(--bg-secondary)',
              border: `1px solid ${isActive ? 'var(--accent)' : 'var(--border)'}`,
              borderRadius: 6,
              marginBottom: 8,
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
              <span style={{ fontWeight: 600, fontSize: 13 }}>{profile.name}</span>
              {isActive && (
                <span style={{
                  fontSize: 10,
                  padding: '2px 6px',
                  background: 'var(--accent)',
                  color: '#000',
                  borderRadius: 10,
                  fontWeight: 700,
                }}>ACTIVE</span>
              )}
              {profile.dateOfBirth && (
                <span style={{ fontSize: 11, color: 'var(--text-muted)' }}>
                  DOB: {profile.dateOfBirth}
                </span>
              )}
              <div style={{ marginLeft: 'auto', display: 'flex', gap: 6 }}>
                <button className="btn btn-secondary btn-sm" onClick={() => onSetActive(profile.id)}>
                  {isActive ? 'Deactivate' : 'Set Active'}
                </button>
                <button className="btn btn-secondary btn-sm" onClick={() => onEditLimits(profile.id)}>
                  {hasLimits ? 'Edit Limits' : 'Add Limits'}
                </button>
                <button className="btn btn-danger btn-sm" onClick={() => onDelete(profile.id, profile.name)}>
                  Delete
                </button>
              </div>
            </div>
            {profile.notes && (
              <div style={{ fontSize: 11, color: 'var(--text-muted)', marginTop: 4 }}>{profile.notes}</div>
            )}
            {hasLimits && individualLimits.get(profile.id) && (
              <div style={{ marginTop: 8 }}>
                <LimitsSummary limits={individualLimits.get(profile.id)!} compact />
              </div>
            )}
          </div>
        );
      })}
    </div>
  );
}

// ─── LimitsSummary — compact read-only view ────────────────────────────────────

function LimitsSummary({ limits }: { limits: NPLimitsSet; compact?: boolean }) {
  const chips: string[] = [];

  if (limits.pbmTranscranial) {
    const l = limits.pbmTranscranial;
    if (l.maxIntensityPercent != null) chips.push(`PBM ≤${l.maxIntensityPercent}%`);
    if (l.maxFrequencyHz != null) chips.push(`PBM ≤${l.maxFrequencyHz}Hz`);
    if (l.maxDutyCyclePercent != null) chips.push(`PBM duty ≤${l.maxDutyCyclePercent}%`);
  }
  if (limits.besTacs) {
    const l = limits.besTacs;
    if (l.maxIntensityMilliamps != null) chips.push(`BES ≤${l.maxIntensityMilliamps}mA`);
    if (l.maxFrequencyHz != null) chips.push(`BES ≤${l.maxFrequencyHz}Hz`);
  }
  if (limits.tdcs) {
    const l = limits.tdcs;
    if (l.maxIntensityMilliamps != null) chips.push(`tDCS ≤${l.maxIntensityMilliamps}mA`);
  }
  if (limits.vnsHrv) {
    const l = limits.vnsHrv;
    if (l.maxIntensityMilliamps != null) chips.push(`VNS ≤${l.maxIntensityMilliamps}mA`);
  }
  if (limits.visualStimulation) {
    const l = limits.visualStimulation;
    if (l.maxFrequencyHz != null) chips.push(`Visual ≤${l.maxFrequencyHz}Hz`);
    if (l.blockHighRiskRange) chips.push('Block 3–30Hz');
  }
  if (limits.tms) {
    const l = limits.tms;
    if (l.maxIntensityPercentMT != null) chips.push(`TMS ≤${l.maxIntensityPercentMT}%MT`);
    if (l.maxPulsesPerSession != null) chips.push(`TMS ≤${l.maxPulsesPerSession}p/sess`);
  }
  if (limits.audioEntrainment) {
    const l = limits.audioEntrainment;
    if (l.maxVolumePercent != null) chips.push(`Audio ≤${l.maxVolumePercent}%`);
  }
  if (limits.clinicalTacs) {
    const l = limits.clinicalTacs;
    if (l.maxIntensityMilliamps != null) chips.push(`cTACS ≤${l.maxIntensityMilliamps}mA`);
  }
  if (limits.hdTdcs) {
    const l = limits.hdTdcs;
    if (l.maxIntensityMilliamps != null) chips.push(`HD-tDCS ≤${l.maxIntensityMilliamps}mA`);
  }
  if (limits.cervicalVns) {
    const l = limits.cervicalVns;
    if (l.maxIntensityMilliamps != null) chips.push(`cVNS ≤${l.maxIntensityMilliamps}mA`);
  }
  if (limits.eegNeurofeedback) {
    const l = limits.eegNeurofeedback;
    if (l.requireClosedLoop) chips.push('EEG: closed-loop required');
    if (l.allowedBands?.length) chips.push(`Bands: ${l.allowedBands.join(', ')}`);
  }

  if (chips.length === 0) {
    return (
      <div style={{ fontSize: 11, color: 'var(--text-muted)', fontStyle: 'italic' }}>
        No modality limits configured.
      </div>
    );
  }

  return (
    <div style={{ display: 'flex', flexWrap: 'wrap', gap: 4 }}>
      {chips.map((c, i) => (
        <span key={i} style={{
          fontSize: 10,
          padding: '2px 6px',
          background: 'var(--bg-primary)',
          border: '1px solid var(--border)',
          borderRadius: 10,
          color: 'var(--text-secondary)',
        }}>
          {c}
        </span>
      ))}
    </div>
  );
}

// ─── LimitsEditor ─────────────────────────────────────────────────────────────

type EditorTab = 'visual' | 'script';

interface LimitsEditorProps {
  initial: NPLimitsSet;
  onSave: (limits: NPLimitsSet) => void;
  onCancel: () => void;
  onExport: (limits: NPLimitsSet) => void;
}

function LimitsEditor({ initial, onSave, onCancel, onExport }: LimitsEditorProps) {
  const [limits, setLimits] = useState<NPLimitsSet>({ ...initial });
  const [editorTab, setEditorTab] = useState<EditorTab>('visual');
  const [scriptText, setScriptText] = useState('');
  const [scriptError, setScriptError] = useState<string | null>(null);

  // Generate script text from current limits whenever switching to script tab
  function handleSwitchToScript() {
    setScriptText(serializeNPPSLimits(limits));
    setScriptError(null);
    setEditorTab('script');
  }

  function handleScriptChange(text: string) {
    setScriptText(text);
    try {
      const parsed = parseNPPSLimits(text);
      if (parsed) {
        setLimits({ ...parsed, id: limits.id, createdAt: limits.createdAt });
        setScriptError(null);
      } else {
        setScriptError('No limits block found in script.');
      }
    } catch (e) {
      setScriptError((e as Error).message);
    }
  }

  function patch(partial: Partial<NPLimitsSet>) {
    setLimits(l => ({ ...l, ...partial, modifiedAt: new Date().toISOString() }));
  }

  return (
    <>
      {/* Name + description */}
      <div style={{ padding: '12px 20px', borderBottom: '1px solid var(--border)', display: 'flex', gap: 12 }}>
        <input
          type="text"
          value={limits.name}
          onChange={e => patch({ name: e.target.value })}
          placeholder="Limits name"
          style={{ ...inputStyle, flex: 1 }}
        />
        <input
          type="text"
          value={limits.description}
          onChange={e => patch({ description: e.target.value })}
          placeholder="Description (optional)"
          style={{ ...inputStyle, flex: 2 }}
        />
      </div>

      {/* Sub-tabs */}
      <div style={{ display: 'flex', borderBottom: '1px solid var(--border)', background: 'var(--bg-secondary)' }}>
        <button
          style={subTabStyle(editorTab === 'visual')}
          onClick={() => setEditorTab('visual')}
        >
          Visual Editor
        </button>
        <button
          style={subTabStyle(editorTab === 'script')}
          onClick={handleSwitchToScript}
        >
          Script (NPPS)
        </button>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: 20 }}>
        {editorTab === 'visual' ? (
          <VisualLimitsEditor limits={limits} onChange={updated => setLimits(updated)} />
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: 8, height: '100%' }}>
            {scriptError && (
              <div style={{ color: 'var(--error)', fontSize: 12, padding: '4px 8px', background: 'rgba(239,68,68,0.1)', borderRadius: 4 }}>
                {scriptError}
              </div>
            )}
            <textarea
              value={scriptText}
              onChange={e => handleScriptChange(e.target.value)}
              style={{
                flex: 1,
                minHeight: 400,
                background: 'var(--bg-secondary)',
                border: '1px solid var(--border)',
                borderRadius: 4,
                color: 'var(--text-primary)',
                fontFamily: 'monospace',
                fontSize: 12,
                padding: 12,
                resize: 'vertical',
                outline: 'none',
              }}
              spellCheck={false}
            />
          </div>
        )}
      </div>

      {/* Footer */}
      <div style={{
        padding: '12px 20px',
        borderTop: '1px solid var(--border)',
        display: 'flex',
        gap: 8,
        justifyContent: 'flex-end',
      }}>
        <button className="btn btn-secondary btn-sm" onClick={() => onExport(limits)}>Export .npps</button>
        <button className="btn btn-secondary" onClick={onCancel}>Cancel</button>
        <button className="btn btn-primary" onClick={() => onSave(limits)}>Save Limits</button>
      </div>
    </>
  );
}

// ─── Visual Limits Editor ─────────────────────────────────────────────────────

function VisualLimitsEditor({
  limits,
  onChange,
}: {
  limits: NPLimitsSet;
  onChange: (updated: NPLimitsSet) => void;
}) {
  function patchModality<K extends keyof NPLimitsSet>(
    key: K,
    partial: Partial<NonNullable<NPLimitsSet[K]>>
  ) {
    const existing = (limits[key] ?? {}) as object;
    onChange({
      ...limits,
      [key]: { ...existing, ...partial },
      modifiedAt: new Date().toISOString(),
    });
  }

  function clearModality(key: keyof NPLimitsSet) {
    const updated = { ...limits, modifiedAt: new Date().toISOString() };
    delete (updated as Record<string, unknown>)[key];
    onChange(updated);
  }

  function toggleModality(key: keyof NPLimitsSet, defaultVal: object) {
    if (limits[key] != null) {
      clearModality(key);
    } else {
      onChange({ ...limits, [key]: defaultVal, modifiedAt: new Date().toISOString() });
    }
  }

  const hw = NPHardwareLimits;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>

      {/* PBM Transcranial */}
      <ModalitySection
        title="PBM Transcranial"
        enabled={limits.pbmTranscranial != null}
        onToggle={() => toggleModality('pbmTranscranial', {})}
      >
        {limits.pbmTranscranial && (
          <>
            <LimitField
              label="Max Intensity"
              value={limits.pbmTranscranial.maxIntensityPercent}
              unit="%"
              min={0} max={100} step={5}
              hint="0–100%"
              onChange={v => patchModality('pbmTranscranial', { maxIntensityPercent: v })}
              onClear={() => patchModality('pbmTranscranial', { maxIntensityPercent: undefined })}
            />
            <LimitField
              label="Max Frequency"
              value={limits.pbmTranscranial.maxFrequencyHz}
              unit="Hz"
              min={0} max={100} step={1}
              hint="0 = CW"
              onChange={v => patchModality('pbmTranscranial', { maxFrequencyHz: v })}
              onClear={() => patchModality('pbmTranscranial', { maxFrequencyHz: undefined })}
            />
            <LimitField
              label="Max Duty Cycle"
              value={limits.pbmTranscranial.maxDutyCyclePercent}
              unit="%"
              min={0} max={hw.pbmDutyCycleMaxPercent} step={1}
              hint={`≤${hw.pbmDutyCycleMaxPercent}% hardware cap`}
              onChange={v => patchModality('pbmTranscranial', { maxDutyCyclePercent: v })}
              onClear={() => patchModality('pbmTranscranial', { maxDutyCyclePercent: undefined })}
            />
            <LimitField
              label="Max Session Dose"
              value={limits.pbmTranscranial.maxSessionDoseJCm2}
              unit="J/cm²"
              min={0} max={200} step={5}
              onChange={v => patchModality('pbmTranscranial', { maxSessionDoseJCm2: v })}
              onClear={() => patchModality('pbmTranscranial', { maxSessionDoseJCm2: undefined })}
            />
            <LimitField
              label="Max Daily Dose"
              value={limits.pbmTranscranial.maxDailyDoseJCm2}
              unit="J/cm²"
              min={0} max={500} step={10}
              onChange={v => patchModality('pbmTranscranial', { maxDailyDoseJCm2: v })}
              onClear={() => patchModality('pbmTranscranial', { maxDailyDoseJCm2: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* PBM Intranasal */}
      <ModalitySection
        title="PBM Intranasal"
        enabled={limits.pbmIntranasal != null}
        onToggle={() => toggleModality('pbmIntranasal', {})}
      >
        {limits.pbmIntranasal && (
          <>
            <LimitField
              label="Max Intensity"
              value={limits.pbmIntranasal.maxIntensityPercent}
              unit="%"
              min={0} max={100} step={5}
              onChange={v => patchModality('pbmIntranasal', { maxIntensityPercent: v })}
              onClear={() => patchModality('pbmIntranasal', { maxIntensityPercent: undefined })}
            />
            <LimitField
              label="Max Session Duration"
              value={limits.pbmIntranasal.maxSessionDurationSeconds}
              unit="sec"
              min={60} max={3600} step={60}
              onChange={v => patchModality('pbmIntranasal', { maxSessionDurationSeconds: v })}
              onClear={() => patchModality('pbmIntranasal', { maxSessionDurationSeconds: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* EEG Neurofeedback */}
      <ModalitySection
        title="EEG Neurofeedback"
        enabled={limits.eegNeurofeedback != null}
        onToggle={() => toggleModality('eegNeurofeedback', {})}
      >
        {limits.eegNeurofeedback && (
          <>
            <LimitBoolField
              label="Require Closed Loop"
              value={limits.eegNeurofeedback.requireClosedLoop ?? false}
              hint="Protocol must have closed-loop EEG adaptation enabled"
              onChange={v => patchModality('eegNeurofeedback', { requireClosedLoop: v })}
            />
            <LimitWhitelistField
              label="Allowed Bands"
              options={['delta', 'theta', 'alpha', 'beta', 'gamma', 'alpha_theta', 'gamma_theta']}
              value={limits.eegNeurofeedback.allowedBands}
              onChange={v => patchModality('eegNeurofeedback', { allowedBands: v })}
              onClear={() => patchModality('eegNeurofeedback', { allowedBands: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* BES / tACS */}
      <ModalitySection
        title="BES / tACS"
        enabled={limits.besTacs != null}
        onToggle={() => toggleModality('besTacs', {})}
      >
        {limits.besTacs && (
          <>
            <LimitField
              label="Max Intensity"
              value={limits.besTacs.maxIntensityMilliamps}
              unit="mA"
              min={0} max={hw.besTacsMaxMilliamps} step={0.1}
              hint={`Hardware max: ${hw.besTacsMaxMilliamps} mA`}
              onChange={v => patchModality('besTacs', { maxIntensityMilliamps: v })}
              onClear={() => patchModality('besTacs', { maxIntensityMilliamps: undefined })}
            />
            <LimitField
              label="Min Frequency"
              value={limits.besTacs.minFrequencyHz}
              unit="Hz"
              min={hw.besTacsMinHz} max={hw.besTacsMaxHz} step={0.5}
              hint={`Hardware min: ${hw.besTacsMinHz} Hz`}
              onChange={v => patchModality('besTacs', { minFrequencyHz: v })}
              onClear={() => patchModality('besTacs', { minFrequencyHz: undefined })}
            />
            <LimitField
              label="Max Frequency"
              value={limits.besTacs.maxFrequencyHz}
              unit="Hz"
              min={hw.besTacsMinHz} max={hw.besTacsMaxHz} step={0.5}
              hint={`Hardware max: ${hw.besTacsMaxHz} Hz`}
              onChange={v => patchModality('besTacs', { maxFrequencyHz: v })}
              onClear={() => patchModality('besTacs', { maxFrequencyHz: undefined })}
            />
            <LimitField
              label="Max Session Duration"
              value={limits.besTacs.maxSessionDurationSeconds}
              unit="sec"
              min={60} max={7200} step={60}
              onChange={v => patchModality('besTacs', { maxSessionDurationSeconds: v })}
              onClear={() => patchModality('besTacs', { maxSessionDurationSeconds: undefined })}
            />
            <LimitField
              label="Max Sessions/Day"
              value={limits.besTacs.maxSessionsPerDay}
              unit=""
              min={1} max={10} step={1}
              onChange={v => patchModality('besTacs', { maxSessionsPerDay: v })}
              onClear={() => patchModality('besTacs', { maxSessionsPerDay: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* tDCS */}
      <ModalitySection
        title="tDCS"
        enabled={limits.tdcs != null}
        onToggle={() => toggleModality('tdcs', {})}
      >
        {limits.tdcs && (
          <>
            <LimitField
              label="Max Intensity"
              value={limits.tdcs.maxIntensityMilliamps}
              unit="mA"
              min={hw.tdcsMinMilliamps} max={hw.tdcsMaxMilliamps} step={0.1}
              hint={`Hardware: ${hw.tdcsMinMilliamps}–${hw.tdcsMaxMilliamps} mA`}
              onChange={v => patchModality('tdcs', { maxIntensityMilliamps: v })}
              onClear={() => patchModality('tdcs', { maxIntensityMilliamps: undefined })}
            />
            <LimitField
              label="Max Session Duration"
              value={limits.tdcs.maxSessionDurationSeconds}
              unit="sec"
              min={60} max={3600} step={60}
              onChange={v => patchModality('tdcs', { maxSessionDurationSeconds: v })}
              onClear={() => patchModality('tdcs', { maxSessionDurationSeconds: undefined })}
            />
            <LimitField
              label="Max Sessions/Day"
              value={limits.tdcs.maxSessionsPerDay}
              unit=""
              min={1} max={5} step={1}
              onChange={v => patchModality('tdcs', { maxSessionsPerDay: v })}
              onClear={() => patchModality('tdcs', { maxSessionsPerDay: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* VNS + HRV */}
      <ModalitySection
        title="VNS + HRV"
        enabled={limits.vnsHrv != null}
        onToggle={() => toggleModality('vnsHrv', {})}
      >
        {limits.vnsHrv && (
          <>
            <LimitField
              label="Max Intensity"
              value={limits.vnsHrv.maxIntensityMilliamps}
              unit="mA"
              min={0} max={hw.vnsMaxMilliamps} step={0.1}
              hint={`Hardware max: ${hw.vnsMaxMilliamps} mA`}
              onChange={v => patchModality('vnsHrv', { maxIntensityMilliamps: v })}
              onClear={() => patchModality('vnsHrv', { maxIntensityMilliamps: undefined })}
            />
            <LimitField
              label="Max Frequency"
              value={limits.vnsHrv.maxFrequencyHz}
              unit="Hz"
              min={hw.vnsMinHz} max={hw.vnsMaxHz} step={1}
              hint={`Hardware: ${hw.vnsMinHz}–${hw.vnsMaxHz} Hz`}
              onChange={v => patchModality('vnsHrv', { maxFrequencyHz: v })}
              onClear={() => patchModality('vnsHrv', { maxFrequencyHz: undefined })}
            />
            <LimitWhitelistField
              label="Allowed HRV Protocols"
              options={['standalone', 'tavns_sync', 'eeg_biofeedback', 'combined_pbm']}
              value={limits.vnsHrv.allowedProtocols}
              onChange={v => patchModality('vnsHrv', { allowedProtocols: v })}
              onClear={() => patchModality('vnsHrv', { allowedProtocols: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* Audio Entrainment */}
      <ModalitySection
        title="Neural Audio"
        enabled={limits.audioEntrainment != null}
        onToggle={() => toggleModality('audioEntrainment', {})}
      >
        {limits.audioEntrainment && (
          <>
            <LimitField
              label="Max Volume"
              value={limits.audioEntrainment.maxVolumePercent}
              unit="%"
              min={0} max={100} step={5}
              onChange={v => patchModality('audioEntrainment', { maxVolumePercent: v })}
              onClear={() => patchModality('audioEntrainment', { maxVolumePercent: undefined })}
            />
            <LimitField
              label="Max Binaural Beat"
              value={limits.audioEntrainment.maxBinauralBeatsHz}
              unit="Hz"
              min={0.5} max={100} step={0.5}
              onChange={v => patchModality('audioEntrainment', { maxBinauralBeatsHz: v })}
              onClear={() => patchModality('audioEntrainment', { maxBinauralBeatsHz: undefined })}
            />
            <LimitField
              label="Max Isochronic Tone"
              value={limits.audioEntrainment.maxIsochronicTonesHz}
              unit="Hz"
              min={0.5} max={100} step={0.5}
              onChange={v => patchModality('audioEntrainment', { maxIsochronicTonesHz: v })}
              onClear={() => patchModality('audioEntrainment', { maxIsochronicTonesHz: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* Visual Stimulation */}
      <ModalitySection
        title="Visual Stimulation"
        enabled={limits.visualStimulation != null}
        onToggle={() => toggleModality('visualStimulation', {})}
      >
        {limits.visualStimulation && (
          <>
            <LimitField
              label="Min Frequency"
              value={limits.visualStimulation.minFrequencyHz}
              unit="Hz"
              min={0} max={100} step={0.5}
              onChange={v => patchModality('visualStimulation', { minFrequencyHz: v })}
              onClear={() => patchModality('visualStimulation', { minFrequencyHz: undefined })}
            />
            <LimitField
              label="Max Frequency"
              value={limits.visualStimulation.maxFrequencyHz}
              unit="Hz"
              min={0} max={hw.visualMaxHz} step={0.5}
              hint={`Hardware max: ${hw.visualMaxHz} Hz`}
              onChange={v => patchModality('visualStimulation', { maxFrequencyHz: v })}
              onClear={() => patchModality('visualStimulation', { maxFrequencyHz: undefined })}
            />
            <LimitBoolField
              label="Block High-Risk Range (3–30 Hz)"
              value={limits.visualStimulation.blockHighRiskRange ?? false}
              hint="Treat 3–30 Hz as an error instead of a warning (photoparoxysmal zone)"
              onChange={v => patchModality('visualStimulation', { blockHighRiskRange: v })}
            />
            <LimitWhitelistField
              label="Allowed Modes"
              options={['binocular', 'emdr', 'retinal_pbm', 'mode_f']}
              value={limits.visualStimulation.allowedModes}
              onChange={v => patchModality('visualStimulation', { allowedModes: v })}
              onClear={() => patchModality('visualStimulation', { allowedModes: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* TMS */}
      <ModalitySection
        title="TMS (T2)"
        enabled={limits.tms != null}
        onToggle={() => toggleModality('tms', {})}
      >
        {limits.tms && (
          <>
            <LimitField
              label="Max Intensity"
              value={limits.tms.maxIntensityPercentMT}
              unit="% MT"
              min={80} max={200} step={5}
              onChange={v => patchModality('tms', { maxIntensityPercentMT: v })}
              onClear={() => patchModality('tms', { maxIntensityPercentMT: undefined })}
            />
            <LimitField
              label="Max Pulses/Session"
              value={limits.tms.maxPulsesPerSession}
              unit="pulses"
              min={100} max={10000} step={100}
              onChange={v => patchModality('tms', { maxPulsesPerSession: v })}
              onClear={() => patchModality('tms', { maxPulsesPerSession: undefined })}
            />
            <LimitField
              label="Max Pulses/Day"
              value={limits.tms.maxPulsesPerDay}
              unit="pulses"
              min={100} max={30000} step={100}
              onChange={v => patchModality('tms', { maxPulsesPerDay: v })}
              onClear={() => patchModality('tms', { maxPulsesPerDay: undefined })}
            />
            <LimitField
              label="Max Sessions/Week"
              value={limits.tms.maxSessionsPerWeek}
              unit=""
              min={1} max={7} step={1}
              onChange={v => patchModality('tms', { maxSessionsPerWeek: v })}
              onClear={() => patchModality('tms', { maxSessionsPerWeek: undefined })}
            />
            <LimitWhitelistField
              label="Allowed Protocols"
              options={['rTMS', 'TBS', 'iTBS']}
              value={limits.tms.allowedProtocols}
              onChange={v => patchModality('tms', { allowedProtocols: v })}
              onClear={() => patchModality('tms', { allowedProtocols: undefined })}
            />
            <LimitWhitelistField
              label="Allowed Targets"
              options={['DLPFC_L', 'DLPFC_R', 'VLPFC_L', 'ACC', 'MPFC', 'M1_L', 'M1_R']}
              value={limits.tms.allowedTargets}
              onChange={v => patchModality('tms', { allowedTargets: v })}
              onClear={() => patchModality('tms', { allowedTargets: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* Clinical tACS */}
      <ModalitySection
        title="Clinical tACS (T2)"
        enabled={limits.clinicalTacs != null}
        onToggle={() => toggleModality('clinicalTacs', {})}
      >
        {limits.clinicalTacs && (
          <>
            <LimitField
              label="Max Intensity"
              value={limits.clinicalTacs.maxIntensityMilliamps}
              unit="mA"
              min={0} max={hw.clinicalTacsMaxMilliamps} step={0.5}
              hint={`Hardware max: ${hw.clinicalTacsMaxMilliamps} mA`}
              onChange={v => patchModality('clinicalTacs', { maxIntensityMilliamps: v })}
              onClear={() => patchModality('clinicalTacs', { maxIntensityMilliamps: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* HD-tDCS */}
      <ModalitySection
        title="HD-tDCS (T2)"
        enabled={limits.hdTdcs != null}
        onToggle={() => toggleModality('hdTdcs', {})}
      >
        {limits.hdTdcs && (
          <>
            <LimitField
              label="Max Intensity/Electrode"
              value={limits.hdTdcs.maxIntensityMilliamps}
              unit="mA"
              min={0} max={hw.hdTdcsMaxMilliampsPerElectrode} step={0.1}
              hint={`Hardware max: ${hw.hdTdcsMaxMilliampsPerElectrode} mA/electrode`}
              onChange={v => patchModality('hdTdcs', { maxIntensityMilliamps: v })}
              onClear={() => patchModality('hdTdcs', { maxIntensityMilliamps: undefined })}
            />
            <LimitWhitelistField
              label="Allowed Montages"
              options={['ring_4x1', 'bilateral_4x1', 'standard_2_electrode']}
              value={limits.hdTdcs.allowedMontages}
              onChange={v => patchModality('hdTdcs', { allowedMontages: v })}
              onClear={() => patchModality('hdTdcs', { allowedMontages: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* Cervical VNS */}
      <ModalitySection
        title="Cervical VNS (T2)"
        enabled={limits.cervicalVns != null}
        onToggle={() => toggleModality('cervicalVns', {})}
      >
        {limits.cervicalVns && (
          <>
            <LimitField
              label="Max Intensity"
              value={limits.cervicalVns.maxIntensityMilliamps}
              unit="mA"
              min={0} max={hw.cervicalVnsMaxMilliamps} step={0.1}
              hint={`Hardware max: ${hw.cervicalVnsMaxMilliamps} mA. Cardiac interlock always active.`}
              onChange={v => patchModality('cervicalVns', { maxIntensityMilliamps: v })}
              onClear={() => patchModality('cervicalVns', { maxIntensityMilliamps: undefined })}
            />
            <LimitField
              label="Max Session Duration"
              value={limits.cervicalVns.maxSessionDurationSeconds}
              unit="sec"
              min={60} max={1800} step={60}
              onChange={v => patchModality('cervicalVns', { maxSessionDurationSeconds: v })}
              onClear={() => patchModality('cervicalVns', { maxSessionDurationSeconds: undefined })}
            />
          </>
        )}
      </ModalitySection>

      {/* 40Hz Vibrotactile */}
      <ModalitySection
        title="40Hz Vibrotactile (Accessory)"
        enabled={limits.vibrotactile40hz != null}
        onToggle={() => toggleModality('vibrotactile40hz', {})}
      >
        {limits.vibrotactile40hz && (
          <>
            <LimitField
              label="Max Intensity"
              value={limits.vibrotactile40hz.maxIntensityG}
              unit="G"
              min={hw.vibrotactileMinG} max={hw.vibrotactileMaxG} step={0.05}
              hint={`Hardware: ${hw.vibrotactileMinG}–${hw.vibrotactileMaxG} G`}
              onChange={v => patchModality('vibrotactile40hz', { maxIntensityG: v })}
              onClear={() => patchModality('vibrotactile40hz', { maxIntensityG: undefined })}
            />
          </>
        )}
      </ModalitySection>
    </div>
  );
}

// ─── Collapsible modality section ─────────────────────────────────────────────

function ModalitySection({
  title,
  enabled,
  onToggle,
  children,
}: {
  title: string;
  enabled: boolean;
  onToggle: () => void;
  children: React.ReactNode;
}) {
  const [open, setOpen] = useState(enabled);

  useEffect(() => {
    if (enabled) setOpen(true);
  }, [enabled]);

  return (
    <div style={{
      border: '1px solid var(--border)',
      borderRadius: 6,
      marginBottom: 4,
      overflow: 'hidden',
    }}>
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          padding: '8px 12px',
          background: enabled ? 'var(--bg-hover)' : 'var(--bg-secondary)',
          cursor: 'pointer',
          userSelect: 'none',
        }}
        onClick={() => enabled && setOpen(o => !o)}
      >
        <label
          style={{ display: 'flex', alignItems: 'center', gap: 8, cursor: 'pointer' }}
          onClick={e => e.stopPropagation()}
        >
          <input
            type="checkbox"
            checked={enabled}
            onChange={onToggle}
            style={{ accentColor: 'var(--accent)' }}
          />
          <span style={{ fontWeight: 600, fontSize: 12 }}>{title}</span>
        </label>
        {enabled && (
          <span style={{ marginLeft: 'auto', fontSize: 10, color: 'var(--text-muted)' }}>
            {open ? '▲' : '▼'}
          </span>
        )}
      </div>
      {enabled && open && (
        <div style={{ padding: '10px 14px', display: 'flex', flexDirection: 'column', gap: 8 }}>
          {children}
        </div>
      )}
    </div>
  );
}

// ─── Limit field components ───────────────────────────────────────────────────

function LimitField({
  label,
  value,
  unit,
  min,
  max,
  step,
  hint,
  onChange,
  onClear,
}: {
  label: string;
  value: number | undefined;
  unit: string;
  min: number;
  max: number;
  step: number;
  hint?: string;
  onChange: (v: number) => void;
  onClear: () => void;
}) {
  const isSet = value !== undefined;
  const displayVal = isSet ? value : min;

  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
      <input
        type="checkbox"
        checked={isSet}
        onChange={() => isSet ? onClear() : onChange(max)}
        style={{ accentColor: 'var(--accent)', flexShrink: 0 }}
      />
      <span style={{ fontSize: 12, color: 'var(--text-secondary)', width: 150, flexShrink: 0 }}>{label}</span>
      {isSet ? (
        <>
          <input
            type="range"
            min={min}
            max={max}
            step={step}
            value={displayVal}
            onChange={e => onChange(parseFloat(e.target.value))}
            style={{ flex: 1, accentColor: 'var(--accent)' }}
          />
          <input
            type="number"
            min={min}
            max={max}
            step={step}
            value={displayVal}
            onChange={e => {
              const v = parseFloat(e.target.value);
              if (!isNaN(v)) onChange(Math.min(max, Math.max(min, v)));
            }}
            style={{
              width: 64,
              background: 'var(--bg-input)',
              border: '1px solid var(--border)',
              borderRadius: 4,
              color: 'var(--text-primary)',
              fontSize: 12,
              padding: '3px 6px',
              outline: 'none',
              textAlign: 'right',
            }}
          />
          <span style={{ fontSize: 11, color: 'var(--text-muted)', width: 40 }}>{unit}</span>
        </>
      ) : (
        <span style={{ fontSize: 11, color: 'var(--text-muted)', fontStyle: 'italic' }}>
          No limit {hint ? `— ${hint}` : ''}
        </span>
      )}
    </div>
  );
}

function LimitBoolField({
  label,
  value,
  hint,
  onChange,
}: {
  label: string;
  value: boolean;
  hint?: string;
  onChange: (v: boolean) => void;
}) {
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
      <input
        type="checkbox"
        checked={value}
        onChange={e => onChange(e.target.checked)}
        style={{ accentColor: 'var(--accent)', flexShrink: 0 }}
      />
      <span style={{ fontSize: 12, color: 'var(--text-secondary)' }}>{label}</span>
      {hint && <span style={{ fontSize: 11, color: 'var(--text-muted)' }}>{hint}</span>}
    </div>
  );
}

function LimitWhitelistField({
  label,
  options,
  value,
  onChange,
  onClear,
}: {
  label: string;
  options: string[];
  value: string[] | undefined;
  onChange: (v: string[]) => void;
  onClear: () => void;
}) {
  const isSet = value !== undefined;
  const selected = value ?? [];

  function toggle(opt: string) {
    if (selected.includes(opt)) {
      const next = selected.filter(s => s !== opt);
      if (next.length === 0) onClear();
      else onChange(next);
    } else {
      onChange([...selected, opt]);
    }
  }

  return (
    <div>
      <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 6 }}>
        <input
          type="checkbox"
          checked={isSet}
          onChange={() => isSet ? onClear() : onChange([...options])}
          style={{ accentColor: 'var(--accent)', flexShrink: 0 }}
        />
        <span style={{ fontSize: 12, color: 'var(--text-secondary)' }}>{label}</span>
        {!isSet && (
          <span style={{ fontSize: 11, color: 'var(--text-muted)', fontStyle: 'italic' }}>No restriction</span>
        )}
      </div>
      {isSet && (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: 6, marginLeft: 24 }}>
          {options.map(opt => (
            <label key={opt} style={{ display: 'flex', alignItems: 'center', gap: 4, cursor: 'pointer' }}>
              <input
                type="checkbox"
                checked={selected.includes(opt)}
                onChange={() => toggle(opt)}
                style={{ accentColor: 'var(--accent)' }}
              />
              <span style={{ fontSize: 11, color: 'var(--text-secondary)' }}>{opt}</span>
            </label>
          ))}
        </div>
      )}
    </div>
  );
}

// ─── Shared styles ────────────────────────────────────────────────────────────

const inputStyle: React.CSSProperties = {
  background: 'var(--bg-input)',
  border: '1px solid var(--border)',
  borderRadius: 4,
  color: 'var(--text-primary)',
  fontSize: 13,
  padding: '6px 10px',
  outline: 'none',
};

function subTabStyle(active: boolean): React.CSSProperties {
  return {
    padding: '8px 16px',
    background: 'none',
    border: 'none',
    borderBottom: active ? '2px solid var(--accent)' : '2px solid transparent',
    color: active ? 'var(--accent)' : 'var(--text-secondary)',
    cursor: 'pointer',
    fontSize: 12,
    fontWeight: active ? 600 : 400,
  };
}
