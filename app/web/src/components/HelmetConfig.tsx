// Helmet configuration — NP-CFG-UI-001.
//
// The screen used to program the helmet. Three panes:
//
//   1. Inventory   — which modules are fitted (simulated until firmware ships
//                    the inventory characteristic).
//   2. Protocols   — predefined protocols, optionally filtered by condition.
//                    Only protocols the fitted helmet can actually run are
//                    selectable; a disabled one states exactly which sockets
//                    need which module.
//   3. Zones       — the NPPS-defined zones, plus an editor for new ones.
//
// Every zone reference in this UI resolves to a zone defined in a .npps file.
// There is no path here that produces a bare socket list masquerading as a zone.

import { useMemo, useState } from 'react';
import {
  NPSimulatedInventoryProvider,
  type NPInventoryPreset,
  type NPHelmetInventory,
} from '../lib/helmetInventory';
import {
  evaluateProtocol,
  zonesForModality,
  coverageLabel,
  coverageFraction,
  describeShortfall,
  MODALITY_REQUIREMENTS,
  type NPEligibility,
} from '../lib/protocolEligibility';
import { NP_SOCKETS } from '../lib/socketMap.generated';
import { socketRangeLabel, toSocketSet, unionSockets } from '../lib/socketSet';
import { getPredefinedNamespace } from '../lib/predefinedProtocols';
import { serializeZone } from '../lib/nppsSerializer';
import { ConditionChips } from './ConditionLinkDialog';
import { SocketPicker } from './SocketPicker';
import {
  MODALITY_META,
  entryName,
  type NPProtocolEntry,
  type NPZoneDefinition,
  type NPModalityTypeId,
  type NPConditionDefinition,
} from '../types/protocol';

const PRESETS: Array<{ id: NPInventoryPreset; label: string }> = [
  { id: 'full-t1', label: 'T1 — fully fitted' },
  { id: 'full-t2', label: 'T2 — fully fitted' },
  { id: 'pbm-only', label: 'PBM modules only' },
  { id: 'eeg-only', label: 'EEG modules only' },
  { id: 'partial-frontal', label: 'Frontal band only' },
  { id: 'none', label: 'No helmet connected' },
];

const NO_CONDITIONS: ReadonlyMap<string, NPConditionDefinition> = new Map();

export function HelmetConfig({ entries }: { entries: NPProtocolEntry[] }) {
  const [preset, setPreset] = useState<NPInventoryPreset>('full-t1');
  const [conditionFilter, setConditionFilter] = useState<string>('');
  const [selectedProtocol, setSelectedProtocol] = useState<string | null>(null);

  // Operator-chosen sockets for `clinician_selected` targets, keyed by protocol
  // name. Deliberately session-only: a perilesional target belongs to one
  // patient, so it must never persist into the next person's session.
  const [targeting, setTargeting] = useState<Record<string, number[]>>({});

  const namespace = getPredefinedNamespace();
  const zones = namespace?.zones ?? new Map<string, NPZoneDefinition>();
  const conditionRegistry = namespace?.conditions ?? NO_CONDITIONS;

  const inventory = useMemo(
    () => new NPSimulatedInventoryProvider(preset).getInventory(),
    [preset],
  );

  // Conditions actually referenced by at least one protocol — filtering by a
  // condition no protocol treats would only ever produce an empty list.
  const referencedConditions = useMemo(() => {
    const names = new Set<string>();
    for (const e of entries) {
      const list = e.kind === 'single' ? e.protocol.conditions : e.composite.conditions;
      for (const c of list ?? []) names.add(c);
    }
    return [...names].sort();
  }, [entries]);

  const evaluated = useMemo(() => {
    return entries.map(entry => {
      const conditions =
        (entry.kind === 'single' ? entry.protocol.conditions : entry.composite.conditions) ?? [];

      const chosen = targeting[entryName(entry)];
      const perModality = new Map<NPModalityTypeId, readonly number[]>();
      if (entry.kind === 'single' && chosen) {
        for (const m of entry.protocol.modalities) {
          perModality.set(m.modalityParams.type, chosen);
        }
      }

      const eligibility: NPEligibility =
        entry.kind === 'single' && inventory
          ? evaluateProtocol(entry.protocol, inventory, zones, perModality)
          : {
              eligible: !!inventory, degraded: false,
              requiresTargeting: [], clinicianTargeted: [],
              shortfalls: [], summary: inventory ? '' : 'No helmet connected',
            };

      return { entry, conditions, eligibility };
    });
  }, [entries, inventory, zones, targeting]);

  const visible = conditionFilter
    ? evaluated.filter(e => e.conditions.includes(conditionFilter))
    : evaluated;

  return (
    <div className="config-screen">
      <InventoryPane preset={preset} onPreset={setPreset} inventory={inventory} />

      <section className="config-pane">
        <header className="config-pane-header">
          <h2>Protocols</h2>
          <select
            className="condition-filter"
            value={conditionFilter}
            onChange={e => setConditionFilter(e.target.value)}
          >
            <option value="">All conditions</option>
            {referencedConditions.map(c => (
              <option key={c} value={c}>{c}</option>
            ))}
          </select>
        </header>

        {visible.length === 0 && (
          <p className="config-empty">No protocols treat {conditionFilter}.</p>
        )}

        <div className="protocol-rows">
          {visible.map(({ entry, conditions, eligibility }) => (
            <ProtocolRow
              key={entryName(entry)}
              entry={entry}
              conditions={conditions}
              conditionRegistry={conditionRegistry}
              eligibility={eligibility}
              inventory={inventory}
              targetedSockets={targeting[entryName(entry)] ?? []}
              // Toggle by id, resolved against `prev` inside the setter. Passing
              // a precomputed array would read a stale `targetedSockets` when
              // several clicks land in one React batch, so only the last would
              // survive.
              onToggleSocket={id =>
                setTargeting(prev => {
                  const name = entryName(entry);
                  const current = prev[name] ?? [];
                  return {
                    ...prev,
                    [name]: current.includes(id)
                      ? current.filter(s => s !== id)
                      : [...current, id],
                  };
                })
              }
              expanded={selectedProtocol === entryName(entry)}
              onSelect={() =>
                setSelectedProtocol(
                  selectedProtocol === entryName(entry) ? null : entryName(entry),
                )
              }
            />
          ))}
        </div>
      </section>

      <ZonePane zones={zones} inventory={inventory} />
    </div>
  );
}

// ─── Inventory ─────────────────────────────────────────────────────────────────

function InventoryPane({
  preset, onPreset, inventory,
}: {
  preset: NPInventoryPreset;
  onPreset: (p: NPInventoryPreset) => void;
  inventory: NPHelmetInventory | null;
}) {
  const fitted = inventory?.occupiedSockets.length ?? 0;

  return (
    <section className="config-pane">
      <header className="config-pane-header">
        <h2>Helmet</h2>
        <select value={preset} onChange={e => onPreset(e.target.value as NPInventoryPreset)}>
          {PRESETS.map(p => <option key={p.id} value={p.id}>{p.label}</option>)}
        </select>
      </header>

      <p className="config-note">
        {fitted} of {NP_SOCKETS.length} sockets fitted.{' '}
        <span className="config-provisional">
          Inventory is simulated — firmware does not yet expose a per-socket
          characteristic.
        </span>
      </p>
    </section>
  );
}

// ─── Protocol row ──────────────────────────────────────────────────────────────

function ProtocolRow({
  entry, conditions, conditionRegistry, eligibility, inventory,
  targetedSockets, onToggleSocket, expanded, onSelect,
}: {
  entry: NPProtocolEntry;
  conditions: string[];
  conditionRegistry: ReadonlyMap<string, NPConditionDefinition>;
  eligibility: NPEligibility;
  inventory: NPHelmetInventory | null;
  targetedSockets: number[];
  onToggleSocket: (socketId: number) => void;
  expanded: boolean;
  onSelect: () => void;
}) {
  const needsTargeting = eligibility.requiresTargeting.length > 0;
  // The panel stays for the whole session once a protocol uses clinician
  // targeting — the operator must be able to review and revise a perilesional
  // site, not have it vanish the moment the first socket is clicked.
  const usesTargeting = eligibility.clinicianTargeted.length > 0;
  const disabled = !eligibility.eligible;

  return (
    <div
      className={`protocol-row${disabled ? ' disabled' : ''}${eligibility.degraded ? ' degraded' : ''}${needsTargeting ? ' targeting' : ''}`}
    >
      <button type="button" className="protocol-row-main" onClick={onSelect} aria-expanded={expanded}>
        <span className="protocol-row-name">{entryName(entry)}</span>
        {needsTargeting && <span className="protocol-row-badge targeting">Needs targeting</span>}
        {disabled && !needsTargeting && <span className="protocol-row-badge blocked">Unavailable</span>}
        {eligibility.degraded && <span className="protocol-row-badge partial">Partial coverage</span>}
        {!disabled && !eligibility.degraded && <span className="protocol-row-badge ok">Ready</span>}
      </button>

      {/* Patient-specific target: the operator must choose the sockets, and the
          protocol cannot run until they do (NP-CFG-UI-001). */}
      {usesTargeting && (
        <div className="targeting-panel">
          <p className="targeting-note">
            {needsTargeting
              ? eligibility.summary
              : `Patient-specific target — ${targetedSockets.length} socket${targetedSockets.length === 1 ? '' : 's'} selected for this session.`}
          </p>
          {expanded ? (
            <>
              <SocketPicker
                selected={new Set(unionSockets(targetedSockets))}
                onToggle={onToggleSocket}
                inventory={inventory}
                requiredElements={
                  MODALITY_REQUIREMENTS[eligibility.clinicianTargeted[0]]?.requires
                }
              />
              <p className="config-note">
                {targetedSockets.length} socket{targetedSockets.length === 1 ? '' : 's'} selected
                {targetedSockets.length > 0 && ` — ${unionSockets(targetedSockets).join(', ')}`}
              </p>
            </>
          ) : (
            <p className="config-note">Open this protocol to select the target sockets.</p>
          )}
        </div>
      )}

      {conditions.length > 0 && (
        <ConditionChips conditions={conditions} registry={conditionRegistry} />
      )}

      {/* The requirement that shaped this screen: never just "unavailable". */}
      {disabled && !needsTargeting && (
        <p className="protocol-row-reason">{eligibility.summary}</p>
      )}

      {expanded && eligibility.shortfalls.length > 0 && (
        <div className="shortfall-detail">
          {eligibility.shortfalls.map(s => (
            <div key={s.modality} className="shortfall-modality">
              <div className="shortfall-modality-name">
                {MODALITY_META[s.modality]?.displayName ?? s.modality}
              </div>

              {s.unresolvedZones.map(z => (
                <p key={z} className="shortfall-line unresolved">
                  Zone “{z}” is not defined in any .npps file.
                </p>
              ))}

              {s.coverage.map(c => (
                <p key={c.zoneName} className="shortfall-line">
                  {c.zoneName}: <strong>{coverageLabel(c)}</strong>
                  {c.satisfied.length === 0 && ' — no compatible module fitted'}
                </p>
              ))}

              {s.sockets.slice(0, 12).map(sock => (
                <p key={sock.socketId} className="shortfall-line socket">
                  {describeShortfall(sock)}
                </p>
              ))}
              {s.sockets.length > 12 && (
                <p className="shortfall-line muted">+{s.sockets.length - 12} more sockets</p>
              )}
            </div>
          ))}
        </div>
      )}
    </div>
  );
}

// ─── Zones ─────────────────────────────────────────────────────────────────────

function ZonePane({
  zones, inventory,
}: {
  zones: ReadonlyMap<string, NPZoneDefinition>;
  inventory: NPHelmetInventory | null;
}) {
  const [modality, setModality] = useState<NPModalityTypeId>('pbm_transcranial');
  const [editing, setEditing] = useState(false);

  const socketBased = (Object.keys(MODALITY_REQUIREMENTS) as NPModalityTypeId[])
    .filter(m => MODALITY_REQUIREMENTS[m].socketBased);

  // "Only the zones where that modality is currently installed."
  const offered = inventory ? zonesForModality(zones.values(), modality, inventory) : [];

  return (
    <section className="config-pane">
      <header className="config-pane-header">
        <h2>Zones</h2>
        <select value={modality} onChange={e => setModality(e.target.value as NPModalityTypeId)}>
          {socketBased.map(m => (
            <option key={m} value={m}>{MODALITY_META[m]?.displayName ?? m}</option>
          ))}
        </select>
      </header>

      <label className="config-label" htmlFor="zone-select">
        Zones supporting {MODALITY_META[modality]?.displayName ?? modality}
      </label>
      <select id="zone-select" className="zone-select" size={6}>
        {offered.length === 0 && <option disabled>No zone has this modality fitted</option>}
        {offered.map(c => (
          <option key={c.zoneName} value={c.zoneName}>
            {c.zoneName} — {coverageLabel(c)}
            {coverageFraction(c) < 1 ? ' ⚠ partial' : ''}
          </option>
        ))}
      </select>

      <button type="button" className="btn-secondary" onClick={() => setEditing(v => !v)}>
        {editing ? 'Cancel' : '+ Define new zone'}
      </button>

      {editing && (
        <ZoneEditor
          existingNames={new Set(zones.keys())}
          inventory={inventory}
          modality={modality}
          onDone={() => setEditing(false)}
        />
      )}
    </section>
  );
}

/**
 * Define a zone as a name plus a socket list. Addresses are enterable two ways —
 * typed, for someone reading numbers off module labels, and by clicking the
 * layout — and the two views stay in sync because they share one selection set.
 *
 * The output is NPPS source. A zone only becomes real by being written to a
 * .npps file, which keeps the "all zone references resolve to NPPS definitions"
 * invariant true by construction.
 */
function ZoneEditor({
  existingNames, inventory, modality, onDone,
}: {
  existingNames: ReadonlySet<string>;
  inventory: NPHelmetInventory | null;
  modality: NPModalityTypeId;
  onDone: () => void;
}) {
  const [name, setName] = useState('');
  const [selected, setSelected] = useState<ReadonlySet<number>>(new Set());
  const [typed, setTyped] = useState('');

  function applyTyped(text: string) {
    setTyped(text);
    const { sockets } = toSocketSet([...text.matchAll(/\d+/g)].map(m => Number(m[0])));
    setSelected(new Set(sockets));
  }

  // Functional update, not `new Set(selected)`: several toggles can land in one
  // React batch (fast clicks, or a drag across hexes), and reading `selected`
  // from the closure makes each of them start from the same stale set, so only
  // the last one survives.
  function toggle(id: number) {
    setSelected(prev => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id); else next.add(id);
      setTyped(unionSockets(next).join(', '));
      return next;
    });
  }

  const sockets = unionSockets(selected);
  const invalidTyped = toSocketSet(
    [...typed.matchAll(/\d+/g)].map(m => Number(m[0])),
  ).invalid;

  const nameTaken = existingNames.has(name.trim());
  const canSave = name.trim().length > 0 && sockets.length > 0 && !nameTaken;

  const draft: NPZoneDefinition | null = canSave
    ? { name: name.trim(), sockets, description: 'User-defined zone.' }
    : null;

  return (
    <div className="zone-editor">
      <label className="config-label" htmlFor="zone-name">Zone name</label>
      <input
        id="zone-name"
        value={name}
        onChange={e => setName(e.target.value)}
        placeholder="e.g. Left DLPFC"
      />
      {nameTaken && <p className="zone-editor-error">A zone named “{name.trim()}” already exists.</p>}

      <label className="config-label" htmlFor="zone-sockets">Socket addresses</label>
      <input
        id="zone-sockets"
        value={typed}
        onChange={e => applyTyped(e.target.value)}
        placeholder="e.g. 1, 2, 4, 5"
      />
      {invalidTyped.length > 0 && (
        <p className="zone-editor-error">
          Not valid sockets on this helmet ({socketRangeLabel()}): {invalidTyped.join(', ')}
        </p>
      )}

      <SocketPicker
        selected={selected}
        onToggle={toggle}
        inventory={inventory}
        requiredElements={MODALITY_REQUIREMENTS[modality].requires}
      />

      <p className="config-note">{sockets.length} socket{sockets.length === 1 ? '' : 's'} selected</p>

      {draft && (
        <>
          <label className="config-label">NPPS definition — add to a .npps file to use it</label>
          <pre className="zone-editor-output">{serializeZone(draft)}</pre>
        </>
      )}

      <div className="zone-editor-actions">
        <button type="button" className="btn-secondary" onClick={onDone}>Close</button>
        <button
          type="button"
          className="btn-primary"
          disabled={!canSave}
          onClick={() => draft && navigator.clipboard?.writeText(serializeZone(draft))}
        >
          Copy NPPS
        </button>
      </div>
    </div>
  );
}
