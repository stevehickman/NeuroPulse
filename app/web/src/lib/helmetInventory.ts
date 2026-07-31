// Helmet inventory — what module is actually plugged into each socket.
//
// This is the data the configuration UI needs to answer "can this protocol run
// on THIS helmet, and if not, what is missing where". Firmware already models
// it (np_module_map.h: socket_id -> { module UID, health, element types }) and
// caches it in NVRAM, but NO GATT characteristic exposes it yet — the only
// hardware-facing characteristic is the 5-byte legacy `zoneModuleStatus`, which
// can describe five anatomical zone modules and nothing finer.
//
// So the inventory arrives through a provider interface. A simulated provider
// drives the UI today (mirroring the existing device-tier simulator); a real
// provider drops in unchanged once firmware ships the characteristic. Nothing
// above this file knows which one it is talking to.
//
// Privacy: module UIDs are COMPONENT identifiers, device-linked, SHDR class —
// the same class as accessory authentication. No user biology passes through
// here, and nothing in this file is UHDR (see np_module_map.h "Privacy").

import { NP_SOCKETS, NP_ROW_WIDTHS, type NPSocketGeometry } from './socketMap.generated';
import { isValidSocketId, socketRangeLabel } from './socketSet';

/**
 * Rows the `partial-anterior` preset populates: the front half of the lattice,
 * rounded down. Expressed against NP_ROW_WIDTHS rather than as a literal so a
 * re-cut lattice keeps meaning "the front half" instead of some stale row count.
 */
const ANTERIOR_ROW_COUNT = Math.floor(NP_ROW_WIDTHS.length / 2);

// ─── Element types ─────────────────────────────────────────────────────────────

/**
 * Mirrors firmware `np_elem_type_t` (np_module_map.h). Enum values there are
 * frozen once written to NVRAM — append only, never renumber — so this union
 * must stay in the same order when it gains members.
 */
export type NPElementType =
  | 'led_660'
  | 'led_808'
  | 'led_1064'
  | 'led_1170'
  | 'eeg_electrode'
  | 'tes_electrode'
  | 'vns_contact'
  | 'ntc'
  | 'pd_forward'
  | 'pd_back'
  | 'ir_prox'
  | 'hall'
  | 'dual_electrode';

/** Wire values, matching the firmware enum ordinals exactly. */
export const ELEMENT_TYPE_ORDINAL: Record<NPElementType, number> = {
  led_660: 1,
  led_808: 2,
  led_1064: 3,
  led_1170: 4,
  eeg_electrode: 5,
  tes_electrode: 6,
  vns_contact: 7,
  ntc: 8,
  pd_forward: 9,
  pd_back: 10,
  ir_prox: 11,
  hall: 12,
  dual_electrode: 13,
};

export const ELEMENT_TYPE_LABEL: Record<NPElementType, string> = {
  led_660: '660nm LED',
  led_808: '808nm LED',
  led_1064: '1064nm LED',
  led_1170: '1170nm laser',
  eeg_electrode: 'EEG electrode',
  tes_electrode: 'tES electrode',
  vns_contact: 'VNS contact',
  ntc: 'thermistor',
  pd_forward: 'forward photodiode',
  pd_back: 'scalp photodiode',
  ir_prox: 'IR proximity sensor',
  hall: 'Hall sensor',
  dual_electrode: 'dual EEG/tES electrode',
};

// ─── Module types ──────────────────────────────────────────────────────────────

/**
 * A module part number and the elements it provides. This is what the UI names
 * when it says "socket 34 needs a ZM-PBM-DUAL".
 */
export interface NPModuleType {
  partNumber: string;
  displayName: string;
  elements: NPElementType[];
}

export const MODULE_TYPES: Record<string, NPModuleType> = {
  'ZM-PBM-DUAL': {
    partNumber: 'ZM-PBM-DUAL',
    displayName: 'PBM dual-wavelength',
    elements: ['led_660', 'led_808', 'ntc', 'pd_forward', 'pd_back'],
  },
  'ZM-PBM-TRI': {
    partNumber: 'ZM-PBM-TRI',
    displayName: 'PBM tri-wavelength (1064nm smart)',
    elements: ['led_660', 'led_808', 'led_1064', 'ntc', 'pd_forward', 'pd_back'],
  },
  'ZM-PBM-DEEP': {
    partNumber: 'ZM-PBM-DEEP',
    displayName: 'Deep PBM 1170nm',
    elements: ['led_1170', 'ntc', 'pd_forward'],
  },
  'ZM-EEG': {
    partNumber: 'ZM-EEG',
    displayName: 'EEG electrode',
    elements: ['eeg_electrode'],
  },
  'ZM-TES': {
    partNumber: 'ZM-TES',
    displayName: 'tES electrode',
    elements: ['tes_electrode'],
  },
  'ZM-DUAL-EL': {
    partNumber: 'ZM-DUAL-EL',
    displayName: 'Dual EEG/tES electrode (Ag/AgCl)',
    elements: ['dual_electrode'],
  },
  'ZM-COMBO': {
    partNumber: 'ZM-COMBO',
    displayName: 'PBM + dual electrode combo',
    elements: ['led_660', 'led_808', 'dual_electrode', 'ntc', 'pd_forward'],
  },
  'ZM-COMBO-TRI': {
    partNumber: 'ZM-COMBO-TRI',
    displayName: 'Tri-wavelength PBM + dual electrode combo',
    elements: ['led_660', 'led_808', 'led_1064', 'dual_electrode', 'ntc', 'pd_forward', 'pd_back'],
  },
};

/** Cheapest module(s) that would supply `element`, for the "what's needed" text. */
export function modulesProviding(element: NPElementType): NPModuleType[] {
  return Object.values(MODULE_TYPES).filter(m => m.elements.includes(element));
}

// ─── Inventory ─────────────────────────────────────────────────────────────────

export interface NPSocketInventory {
  socketId: number;
  /** False when the socket is empty. */
  present: boolean;
  /** Module part number, when a recognised module is fitted. */
  partNumber?: string;
  /** 64-bit module UID as hex. SHDR-class component identifier. */
  moduleUid?: string;
  /** Elements this module actually provides. Empty when the socket is empty. */
  elements: NPElementType[];
}

export class NPHelmetInventory {
  private readonly bySocket: Map<number, NPSocketInventory>;

  constructor(sockets: NPSocketInventory[]) {
    this.bySocket = new Map(sockets.map(s => [s.socketId, s]));
  }

  at(socketId: number): NPSocketInventory | undefined {
    return this.bySocket.get(socketId);
  }

  /** True when the socket holds a module providing `element`. */
  provides(socketId: number, element: NPElementType): boolean {
    const s = this.bySocket.get(socketId);
    return !!s?.present && s.elements.includes(element);
  }

  /**
   * True when the socket satisfies the requirement — any ONE of `anyOf`.
   * Requirements are expressed as alternatives because a dual Ag/AgCl electrode
   * satisfies both an EEG and a tES need on its own.
   */
  satisfies(socketId: number, anyOf: readonly NPElementType[]): boolean {
    return anyOf.some(e => this.provides(socketId, e));
  }

  get occupiedSockets(): number[] {
    return [...this.bySocket.values()].filter(s => s.present).map(s => s.socketId).sort((a, b) => a - b);
  }

  get allSockets(): NPSocketInventory[] {
    return [...this.bySocket.values()].sort((a, b) => a.socketId - b.socketId);
  }
}

// ─── Provider ──────────────────────────────────────────────────────────────────

export interface NPInventoryProvider {
  /** Current inventory, or null when no helmet is connected. */
  getInventory(): NPHelmetInventory | null;
}

/**
 * Fills sockets from a preset so the UI is exercisable before firmware ships the
 * inventory characteristic. Presets deliberately include partially-populated
 * helmets — partial coverage is the interesting case for the zone picker.
 */
export type NPInventoryPreset =
  | 'none'
  | 'full-t1'
  | 'full-t2'
  | 'pbm-only'
  | 'eeg-only'
  | 'partial-anterior';

export class NPSimulatedInventoryProvider implements NPInventoryProvider {
  constructor(private preset: NPInventoryPreset = 'full-t1') {}

  setPreset(preset: NPInventoryPreset): void {
    this.preset = preset;
  }

  getPreset(): NPInventoryPreset {
    return this.preset;
  }

  getInventory(): NPHelmetInventory | null {
    if (this.preset === 'none') return null;

    const fit = (geo: NPSocketGeometry): string | null => {
      switch (this.preset) {
        // "Fully fitted" must mean every T1 modality runs everywhere, so the
        // combo part (both wavelengths + a dual-rated electrode) goes in every
        // socket. An earlier every-Nth-socket pattern looked plausible but left
        // whole zones without electrodes — temporal sockets are all ≡ 2 (mod 3)
        // — which made a "fully fitted" helmet report protocols as unavailable.
        case 'full-t1':
          return 'ZM-COMBO';
        case 'full-t2':
          return 'ZM-COMBO-TRI';
        case 'pbm-only':
          return 'ZM-PBM-DUAL';
        case 'eeg-only':
          return 'ZM-EEG';
        case 'partial-anterior':
          // Only the front half of the lattice rows is populated — the case that
          // should surface coverage warnings on rear zones. Defined on the ROW
          // structure, which is a hardware fact this module already imports;
          // nothing here knows or asks which zones those rows fall in.
          return geo.row < ANTERIOR_ROW_COUNT ? 'ZM-COMBO' : null;
        default:
          return null;
      }
    };

    return new NPHelmetInventory(
      NP_SOCKETS.map(geo => {
        const partNumber = fit(geo);
        if (!partNumber) {
          return { socketId: geo.id, present: false, elements: [] };
        }
        return {
          socketId: geo.id,
          present: true,
          partNumber,
          // Deterministic pseudo-UID so the simulator is reproducible across reloads.
          moduleUid: `SIM${geo.id.toString(16).padStart(4, '0').toUpperCase()}`,
          elements: MODULE_TYPES[partNumber].elements,
        };
      }),
    );
  }
}

/** Guard for socket ids arriving from user input or a .npps file. */
export function assertValidSocket(id: number): void {
  if (!isValidSocketId(id)) {
    throw new Error(
      `socket ${id} is not a valid socket on this helmet (${socketRangeLabel()})`,
    );
  }
}
