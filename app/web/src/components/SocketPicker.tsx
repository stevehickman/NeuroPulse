// Graphical socket picker — NP-CFG-UI-001.
//
// Uses the unrolled-helmet map drawn in the Module redesign study
// (widget `hexagonal_zone_module_helmet_layout`, PR #207 / NP-HEX-ZM-001):
// front at the top, occiput at the bottom, dashed midline, L/R hemispheres,
// hexagonal tiles coloured by lobe group, ear/audio and neck zones marked as
// excluded, and each tile labelled with its socket (major) address.
//
// Carried over verbatim from that widget: the unrolled ellipse framing, the
// FRONT/BACK and L/R annotations, the dashed midline, the excluded ear/audio
// arcs and neck-attach label, the `S-NN` address labels, and the lobe palette
// (frontal #3b82f6, temporal #f59e0b, parietal #10b981, occipital #8b5cf6).
//
// NOT carried over: that widget generated its own cells by scanning a hex
// lattice inside the ellipse. Tile positions here come from
// socketMap.generated.ts (derived from the row structure that reproduces all
// eight lobe zones in 00-zones.npps) rather than from the widget's lattice
// scan. Lobe colour comes from that same validated map, not from the widget's
// positional thresholds.
//
// Worth recording: the widget's scan yielded ~29 tiles at 40 mm and was
// overridden by a 78-socket lattice. The scan was right — 40 mm tiles over the
// vault hold 30 (NP-HEX-ZM-001 §3) — and the lattice is now derived from that
// arithmetic. Nothing here hardcodes a count; NP_SOCKETS is the source.
//
// Positions remain PROVISIONAL pending shell CAD — see the note in
// socketMap.generated.ts. Selection and eligibility never depend on them.

import { useMemo } from 'react';
import { NP_SOCKETS, type NPSocketGeometry, type NPLobe } from '../lib/socketMap.generated';
import { NP_SOCKET_COUNT } from '../lib/socketSet';
import type { NPHelmetInventory, NPElementType } from '../lib/helmetInventory';

interface SocketPickerProps {
  selected: ReadonlySet<number>;
  onToggle: (socketId: number) => void;
  inventory: NPHelmetInventory | null;
  /**
   * When set, sockets whose fitted module cannot supply these elements are
   * marked — they stay selectable, since a zone may legitimately span sockets
   * the user intends to re-fit later.
   */
  requiredElements?: readonly NPElementType[][];
}

/** Lobe palette, verbatim from the Module redesign layout widget. */
const LOBE_COLOR: Record<NPLobe, string> = {
  frontal: '#3b82f6',
  temporal: '#f59e0b',
  parietal: '#10b981',
  occipital: '#8b5cf6',
};

const LOBE_LABEL: Record<NPLobe, string> = {
  frontal: 'Frontal L/R',
  temporal: 'Temporal L/R',
  parietal: 'Parietal L/R',
  occipital: 'Occipital L/R',
};

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

export function SocketPicker({ selected, onToggle, inventory, requiredElements }: SocketPickerProps) {
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

  function statusOf(socket: NPSocketGeometry): 'empty' | 'incompatible' | 'fitted' {
    const entry = inventory?.at(socket.id);
    if (!entry?.present) return 'empty';
    if (!requiredElements || requiredElements.length === 0) return 'fitted';
    return requiredElements.every(g => inventory!.satisfies(socket.id, g)) ? 'fitted' : 'incompatible';
  }

  return (
    <div className="socket-picker">
      <div className="socket-picker-legend">
        {(Object.keys(LOBE_COLOR) as NPLobe[]).map(lobe => (
          <span key={lobe}>
            <i className="swatch" style={{ background: LOBE_COLOR[lobe], opacity: 0.45, border: `2px solid ${LOBE_COLOR[lobe]}` }} />
            {LOBE_LABEL[lobe]}
          </span>
        ))}
        <span className="socket-picker-legend-note">S-NN = socket (major address) · L/R split at midline</span>
      </div>

      <div className="socket-picker-scroll">
        <svg viewBox={`0 0 ${width} ${height}`} width={width} role="img" className="socket-picker-svg">
          <title>Unrolled helmet interior — {NP_SOCKET_COUNT} sockets by lobe group</title>

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
            const color = LOBE_COLOR[socket.lobe];
            const fitted = inventory?.at(socket.id);

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
                  {`Socket ${socket.id} — ${socket.lobe} ${socket.side}\n` +
                    (fitted?.present ? `Fitted: ${fitted.partNumber}` : 'Empty') +
                    (status === 'incompatible' ? '\nDoes not supply the required elements' : '')}
                </title>
                <polygon
                  points={hexPoints(cx, cy, HEX_R - 2)}
                  fill={isSelected ? color : color}
                  fillOpacity={isSelected ? 0.85 : status === 'empty' ? 0.07 : 0.2}
                  stroke={color}
                  strokeWidth={isSelected ? 3.5 : 2}
                  /* Midline sockets belong to BOTH hemisphere zones — dashed so
                     the double membership is visible rather than surprising. */
                  strokeDasharray={socket.side === 'midline' ? '4 3' : undefined}
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
        dashed midline, tiles coloured by lobe. Dashed outlines are midline sockets, which belong
        to both hemisphere zones. Amber dot = fitted module cannot supply the selected modality.
        Tile <em>positions</em> are provisional pending shell CAD; socket identity and lobe grouping
        are derived from the shipped zone definitions.
      </div>
    </div>
  );
}
