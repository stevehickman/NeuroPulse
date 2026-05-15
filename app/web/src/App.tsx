import { createContext, useContext, useEffect, useState, useCallback } from 'react';
import { NPProtocolEntry, NPCompositeProtocol } from './types/protocol';
import { protocolLibrary, DeviceTier } from './lib/protocolLibrary';
import { ProtocolMenu } from './components/ProtocolMenu';
import { ProtocolEditor } from './components/ProtocolEditor';
import { ProtocolComposer } from './components/ProtocolComposer';

// ─── Protocol Context ─────────────────────────────────────────────────────────

interface ProtocolContextValue {
  version: number;
  refresh: () => void;
}

export const ProtocolContext = createContext<ProtocolContextValue>({
  version: 0,
  refresh: () => {},
});

export function useProtocolContext() {
  return useContext(ProtocolContext);
}

// ─── App View State ────────────────────────────────────────────────────────────

type AppView =
  | { view: 'menu' }
  | { view: 'editor'; entry?: NPProtocolEntry }
  | { view: 'composer'; composite?: NPCompositeProtocol };

// ─── App ──────────────────────────────────────────────────────────────────────

export default function App() {
  const [version, setVersion] = useState(0);
  const [appView, setAppView] = useState<AppView>({ view: 'menu' });
  const [deviceTier, setDeviceTierState] = useState<DeviceTier>('t1');
  const [has1064, setHas1064] = useState(false);

  const refresh = useCallback(() => setVersion(v => v + 1), []);

  // Subscribe to library changes
  useEffect(() => {
    const unsub = protocolLibrary.subscribe(refresh);
    return unsub;
  }, [refresh]);

  // Init device tier
  useEffect(() => {
    protocolLibrary.updateDeviceTier(deviceTier, has1064);
  }, [deviceTier, has1064]);

  function handleDeviceTierChange(tier: DeviceTier) {
    setDeviceTierState(tier);
    protocolLibrary.updateDeviceTier(tier, has1064);
    refresh();
  }

  function handleHas1064Change(val: boolean) {
    setHas1064(val);
    protocolLibrary.updateDeviceTier(deviceTier, val);
    refresh();
  }

  function handleSaveEntry(entry: NPProtocolEntry) {
    // If it's predefined, duplicate it first
    if (entry.kind === 'single' && entry.protocol.isPredefined) {
      const dup = protocolLibrary.duplicateProtocol(entry, entry.protocol.name + ' (copy)');
      protocolLibrary.saveUserProtocol(dup);
    } else if (entry.kind === 'composite' && entry.composite.isPredefined) {
      const dup = protocolLibrary.duplicateProtocol(entry, entry.composite.name + ' (copy)');
      protocolLibrary.saveUserProtocol(dup);
    } else {
      protocolLibrary.saveUserProtocol(entry);
    }
    setAppView({ view: 'menu' });
  }

  function handleSaveComposite(composite: NPCompositeProtocol) {
    protocolLibrary.saveUserProtocol({ kind: 'composite', composite });
    setAppView({ view: 'menu' });
  }

  function handleEditEntry(entry: NPProtocolEntry) {
    if (entry.kind === 'composite') {
      setAppView({ view: 'composer', composite: entry.composite });
    } else {
      setAppView({ view: 'editor', entry });
    }
  }

  function handleNewProtocol() {
    setAppView({ view: 'editor', entry: undefined });
  }

  function handleOpenComposer() {
    setAppView({ view: 'composer', composite: undefined });
  }

  const sidebarItems = [
    { id: 'menu', label: 'Protocol Library', icon: '📋' },
    { id: 'editor', label: 'Protocol Editor', icon: '✏️' },
    { id: 'composer', label: 'Composer', icon: '🎛️' },
  ] as const;

  const activeNavId =
    appView.view === 'menu' ? 'menu' :
    appView.view === 'editor' ? 'editor' : 'composer';

  return (
    <ProtocolContext.Provider value={{ version, refresh }}>
      <div className="app-header">
        <span className="app-header-logo">NeuroPulse</span>
        <span className="app-header-subtitle">Protocol Library</span>
        <div className="app-header-device">
          <DeviceTierControl
            tier={deviceTier}
            has1064={has1064}
            onChangeTier={handleDeviceTierChange}
            onChangeHas1064={handleHas1064Change}
          />
        </div>
      </div>

      <div className="app-body">
        <nav className="sidebar">
          <div className="sidebar-section">Navigation</div>
          <div className="sidebar-nav">
            {sidebarItems.map(item => (
              <button
                key={item.id}
                className={`sidebar-item${activeNavId === item.id ? ' active' : ''}`}
                onClick={() => {
                  if (item.id === 'menu') setAppView({ view: 'menu' });
                  else if (item.id === 'editor') setAppView({ view: 'editor' });
                  else setAppView({ view: 'composer' });
                }}
              >
                <span className="sidebar-item-icon">{item.icon}</span>
                {item.label}
              </button>
            ))}
          </div>
        </nav>

        <main className="main-content">
          {appView.view === 'menu' && (
            <ProtocolMenu
              onEdit={handleEditEntry}
              onNewProtocol={handleNewProtocol}
              onOpenComposer={handleOpenComposer}
            />
          )}
          {appView.view === 'editor' && (
            <ProtocolEditor
              existing={appView.entry}
              onSave={handleSaveEntry}
              onCancel={() => setAppView({ view: 'menu' })}
            />
          )}
          {appView.view === 'composer' && (
            <ProtocolComposer
              existing={appView.composite}
              onSave={handleSaveComposite}
              onCancel={() => setAppView({ view: 'menu' })}
            />
          )}
        </main>
      </div>
    </ProtocolContext.Provider>
  );
}

// ─── Device Tier Control ───────────────────────────────────────────────────────

function DeviceTierControl({
  tier,
  has1064,
  onChangeTier,
  onChangeHas1064,
}: {
  tier: DeviceTier;
  has1064: boolean;
  onChangeTier: (t: DeviceTier) => void;
  onChangeHas1064: (v: boolean) => void;
}) {
  return (
    <div className="device-tier-select">
      <span style={{ color: 'var(--text-muted)', fontSize: 12 }}>Simulate:</span>
      <select
        value={tier}
        onChange={e => onChangeTier(e.target.value as DeviceTier)}
        style={{
          background: 'var(--bg-input)',
          border: '1px solid var(--border)',
          borderRadius: 'var(--radius-sm)',
          color: 'var(--text-primary)',
          fontSize: 12,
          padding: '4px 8px',
          cursor: 'pointer',
          outline: 'none',
        }}
      >
        <option value="none">No Device</option>
        <option value="t1">T1 Device</option>
        <option value="t2">T2 Device</option>
      </select>
      <span
        className={`tier-badge ${tier}`}
        title={tier === 'none' ? 'No device' : tier === 't1' ? 'Home tier' : 'Pro tier'}
      >
        {tier === 'none' ? 'OFFLINE' : tier === 't1' ? 'T1' : 'T2'}
      </span>
      {tier !== 'none' && (
        <label style={{ display: 'flex', alignItems: 'center', gap: 4, fontSize: 12, color: 'var(--text-secondary)', cursor: 'pointer' }}>
          <input
            type="checkbox"
            checked={has1064}
            onChange={e => onChangeHas1064(e.target.checked)}
            style={{ accentColor: 'var(--accent)' }}
          />
          1064nm
        </label>
      )}
    </div>
  );
}
