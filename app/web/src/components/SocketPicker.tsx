// Graphical socket picker — NP-CFG-UI-001.
//
// Uses the unrolled-helmet map drawn in the Module redesign study
// (widget `hexagonal_zone_module_helmet_layout`, PR #207 / NP-HEX-ZM-001):
// front at the top, occiput at the bottom, dashed midline, L/R hemispheres,
// hexagonal tiles, ear/audio and neck zones marked as excluded, and each tile
// labelled with its socket (major) address.
//
// Carried over verbatim from that widget: the unrolled ellipse framing, the
// FRONT/BACK and L/R annotations, the dashed midline, the excluded ear/audio
// arcs and neck-attach label, and the `S-NN` address labels.
//
// ── Tiles are coloured by ZONE, not by lobe (ZONE-1, 2026-07-30) ─────────────
//
// The picker used to colour tiles from a `lobe` field on the generated socket
// map, against a four-entry palette. That field is gone: nothing in this repo
// defines, derives or hardcodes a brain lobe, and zones are defined only in
// protocols/predefined/00-zones.npps.
//
// Colouring from the zone namespace instead is strictly better for the operator,
// which is the SMART-1 research-flexibility rationale: the picker now shows
// EVERY zone the helmet has loaded — the shipped ones, plus any user-authored or
// research zone in any .npps file in the protocol tree. Under the old model a
// research zone was invisible here no matter how it was authored, because it was
// not one of four hardcoded lobes.
//
// A socket usually belongs to several zones at once ("All" contains everything,
// and membership is inclusive by PR #210). Its colour comes from the SMALLEST
// zone containing it — the most specific thing true of that socket — with ties
// broken by name so the map is stable across reloads. The tooltip lists every
// zone the socket is in, so the broader memberships stay discoverable.
//
// NOT carried over: that widget generated its own cells by scanning a hex
// lattice inside the ellipse. Tile positions here come from
// socketMap.generated.ts. Nothing here hardcodes a count; NP_SOCKETS is the
// source.
//
// Positions remain PROVISIONAL pending shell CAD — see the note in
// socketMap.generated.ts. Selection and eligibility never depend on them.

import { useMemo } from 'react';
import { NP_SOCKETS, type NPSocketGeometry } from '../lib/socketMap.generated';
import { NP_SOCKET_COUNT } from '../lib/socketSet';
import type { NPHelmetInventory, NPElementType } from '../lib/helmetInventory';
import type { NPZoneDefinition } from '../types/protocol';

interface SocketPickerProps {
  selected: ReadonlySet<number>;
  onToggle: (socketId: number) => void;
  inventory: NPHelmetInventory | null;
  /**
   * Every zone in the loaded namespace — shipped, user-authored and research
   * zones alike. Drives the tile colouring and the legend. An empty map is
   * legitimate (the namespace loads asynchronously); the map then renders
   * uncoloured rather than failing.
   */
  zones: ReadonlyMap<string, NPZoneDefinition>;
  /**
   * When set, sockets whose fitted module cannot supply these elements are
   * marked — they stay selectable, since a zone may legitimately span sockets
   * the user intends to re-fit later.
   */
  requiredElements?: readonly NPElementType[][];
}

/**
 * Qualitative palette, assigned to zones in display order. The first four are
 * the Module redesign layout widget's original swatches, kept so the shipped
 * zones look familiar; the rest extend it far enough that a protocol tree with
 * many user zones still reads. It cycles if a helmet somehow loads more zones
 * than there are entries — repeating a hue is a cosmetic collision, not a
 * correctness problem, and the legend disambiguates by name.
 */
const ZONE_PALETTE = [
  '#3b82f6', '#f59e0b', '#10b981', '#8b5cf6',
  '#ef4444', '#06b6d4', '#ec4899', '#84cc16',
  '#f97316', '#6366f1', '#14b8a6', '#a855f7',
];

/** Colour used when a socket is in no loaded zone at all. */
const UNZONED_COLOR = '#94a3b8';

// Unit-lattice → SVG. The map is pointy-top hex packing: horizontal pitch 1
// unit, vertical pitch sqrt(3)/2, which is what socketMap.generated.ts emits.
const SCALE = 46;
const HEX_R = SCALE * 0.5774;  // circumradius for a 1-unit-wide pointy-top hex
const PAD = 54;

/** Pointy-top hexagon: vertices at 30°, 90°, ... */
function hexPoints(cx: number, cy: number, r: number): string {
  const pts: string[] = [];
  for (let i = 0; i < 6; i++) {
    const a = (Math.PI / 180) * (60 * i + 30);
    pts.push(`${(cx + r * Math.cos(a)).toFixed(1)},${(cy + r * Math.sin(a)).toFixed(1)}`);
  }
  return pts.join(' ');
}

export interface ZoneGrouping {
  /** Colour per zone name, for the legend and the tiles. */
  colorOf: ReadonlyMap<string, string>;
  /** Most specific zone containing each socket, or undefined if it is in none. */
  primaryOf: ReadonlyMap<number, string>;
  /** Every zone containing each socket, smallest first — for the tooltip. */
  membershipOf: ReadonlyMap<number, string[]>;
  /** Zones that are the primary zone of at least one socket, in display order. */
  legend: string[];
}

/**
 * Assign each socket the smallest zone that contains it.
 *
 * Smallest wins because it is the most specific true statement about the socket:
 * every socket is in "All", so colouring by any larger zone would paint the
 * whole helmet one colour. Ties break on name so the map does not shuffle
 * between reloads when two zones have equal size.
 */
export function groupByZone(zones: ReadonlyMap<string, NPZoneDefinition>): ZoneGrouping {
  const byName = [...zones.values()].sort((a, b) => a.name.localeCompare(b.name));

  const colorOf = new Map<string, string>();
  byName.forEach((z, i) => colorOf.set(z.name, ZONE_PALETTE[i % ZONE_PALETTE.length]));

  // Smallest-first, then by name — one pass, and the first zone to claim a
  // socket is by construction its most specific one.
  const bySpecificity = [...byName].sort(
    (a, b) => a.sockets.length - b.sockets.length || a.name.localeCompare(b.name),
  );

  const primaryOf = new Map<number, string>();
  const membershipOf = new Map<number, string[]>();
  for (const zone of bySpecificity) {
    for (const id of zone.sockets) {
      if (!primaryOf.has(id)) primaryOf.set(id, zone.name);
      membershipOf.set(id, [...(membershipOf.get(id) ?? []), zone.name]);
    }
  }

  const claimed = new Set(primaryOf.values());
  const legend = byName.map(z => z.name).filter(n => claimed.has(n));

  return { colorOf, primaryOf, membershipOf, legend };
}

export function SocketPicker({
  selected, onToggle, inventory, zones, requiredElements,
}: SocketPickerProps) {
  const { width, height, cells, midX } = useMemo(() => {
    const xs = NP_SOCKETS.map(s => s.x);
    const minX = Math.min(...xs);
    const maxX = Math.max(...xs);
    const maxY = Math.max(...NP_SOCKETS.map(s => s.y));

    const cells = NP_SOCKETS.map(s => ({
      socket: s,
      cx: PAD + (s.x - minX) * SCALE,
      cy: PAD + s.y * SCALE,
    }));

    return {
      width: (maxX - minX) * SCALE + PAD * 2,
      height: maxY * SCALE + PAD * 2,
      midX: PAD + (0 - minX) * SCALE,
      cells,
    };
  }, []);

  const { colorOf, primaryOf, membershipOf, legend } = useMemo(
    () => groupByZone(zones),
    [zones],
  );

  function colorFor(socketId: number): string {
    const primary = primaryOf.get(socketId);
    return primary ? colorOf.get(primary)! : UNZONED_COLOR;
  }

  function statusOf(socket: NPSocketGeometry): 'empty' | 'incompatible' | 'fitted' {
    const entry = inventory?.at(socket.id);
    if (!entry?.present) return 'empty';
    if (!requiredElements || requiredElements.length === 0) return 'fitted';
    return requiredElements.every(g => inventory!.satisfies(socket.id, g)) ? 'fitted' : 'incompatible';
  }

  return (
    <div className="socket-picker">
      <div className="socket-picker-legend">
        {legend.map(name => (
          <span key={name}>
            <i
              className="swatch"
              style={{
                background: colorOf.get(name),
                opacity: 0.45,
                border: `2px solid ${colorOf.get(name)}`,
              }}
            />
            {name}
          </span>
        ))}
        <span className="socket-picker-legend-note">
          S-NN = socket (major address) · tile colour = smallest zone containing it
        </span>
      </div>

      <div className="socket-picker-scroll">
        <svg viewBox={`0 0 ${width} ${height}`} width={width} role="img" className="socket-picker-svg">
          <title>Unrolled helmet interior — {NP_SOCKET_COUNT} sockets by zone</title>

          {/* Unrolled-map framing, as in the Module redesign widget. */}
          <text x={midX} y={20} textAnchor="middle" className="map-axis">FRONT</text>
          <text x={midX} y={height - 10} textAnchor="middle" className="map-axis">BACK (occiput)</text>
          <line
            x1={midX} y1={30} x2={midX} y2={height - 26}
            className="map-midline" strokeDasharray="5 5"
          />
          <text x={PAD - 34} y={height / 2} textAnchor="middle" className="map-side">L</text>
          <text x={width - PAD + 34} y={height / 2} textAnchor="middle" className="map-side">R</text>

          {/* Ear/audio zones are excluded from tiling — the widget marked these
              with dashed arcs; the socket map simply has no sockets there. */}
          <text x={PAD - 34} y={height / 2 + 22} textAnchor="middle" className="map-excluded">ear /</text>
          <text x={PAD - 34} y={height / 2 + 34} textAnchor="middle" className="map-excluded">audio</text>
          <text x={width - PAD + 34} y={height / 2 + 22} textAnchor="middle" className="map-excluded">ear /</text>
          <text x={width - PAD + 34} y={height / 2 + 34} textAnchor="middle" className="map-excluded">audio</text>

          {cells.map(({ socket, cx, cy }) => {
            const isSelected = selected.has(socket.id);
            const status = statusOf(socket);
            const color = colorFor(socket.id);
            const fitted = inventory?.at(socket.id);
            const memberOf = membershipOf.get(socket.id) ?? [];
            const straddlesMidline =
              memberOf.some(z => /\bLeft\b/.test(z)) &&
              memberOf.some(z => /\bRight\b/.test(z));

            return (
              <g
                key={socket.id}
                className={`map-hex ${status}${isSelected ? ' selected' : ''}`}
                onClick={() => onToggle(socket.id)}
                role="button"
                tabIndex={0}
                aria-pressed={isSelected}
                onKeyDown={e => {
                  if (e.key === 'Enter' || e.key === ' ') {
                    e.preventDefault();
                    onToggle(socket.id);
                  }
                }}
              >
                <title>
                  {`Socket ${socket.id}\n` +
                    `Zones: ${memberOf.length > 0 ? memberOf.join(', ') : 'none'}\n` +
                    (fitted?.present ? `Fitted: ${fitted.partNumber}` : 'Empty') +
                    (status === 'incompatible' ? '\nDoes not supply the required elements' : '')}
                </title>
                <polygon
                  points={hexPoints(cx, cy, HEX_R - 2)}
                  fill={color}
                  fillOpacity={isSelected ? 0.85 : status === 'empty' ? 0.07 : 0.2}
                  stroke={color}
                  strokeWidth={isSelected ? 3.5 : 2}
                  /* Midline sockets sit on the centre column and so are listed in
                     both the left- and right-side zone they belong to — dashed so
                     the double membership is visible rather than surprising.
                     Read from the AUTHORED zone membership, not from a `side`
                     field: laterality is a property of the zone file, not of the
                     hardware, and the helmet reports neither lobe nor side
                     (ZONE-1). Socket 2 is in "Frontal Left" AND "Frontal Right";
                     socket 3 is only in the Right zones. */
                  strokeDasharray={straddlesMidline ? '4 3' : undefined}
                />
                {status === 'incompatible' && (
                  <circle cx={cx} cy={cy - HEX_R * 0.52} r={3.2} fill="#f59e0b" />
                )}
                <text x={cx} y={cy + 3.5} textAnchor="middle" className="map-hex-label">
                  {`S-${socket.id.toString().padStart(2, '0')}`}
                </text>
              </g>
            );
          })}
        </svg>
      </div>

      <div className="socket-picker-caption">
        Unrolled interior map from the Module redesign study — front at top, occiput at bottom,
        dashed midline. Each tile is coloured by the smallest zone that contains it, from the
        zones loaded out of the .npps protocol tree, so user-defined and research zones appear
        here alongside the shipped ones; hover a tile for its full zone membership. Dashed
        outlines are midline sockets, which the zone file lists in both the left- and right-side
        zone they belong to. Amber dot = fitted module cannot supply the selected modality.
        Tile <em>positions</em> are provisional pending shell CAD; socket identity is from the
        generated lattice and zone membership is from the zone file.
      </div>
    </div>
  );
}
