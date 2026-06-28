/**
 * Wavelength-to-color mapping for the NeuroPulse helmet simulator.
 *
 * Design principle: longer actual wavelengths are represented by longer
 * apparent visible wavelengths (toward the red end of the visible spectrum).
 * Visible 660 nm is shown as its actual color. All NIR wavelengths are
 * false-colored, with the convention that the progression
 * red → orange → amber → gold tracks 660 → 808 → 1064 → 1170 nm.
 *
 * The EEG, BES, tDCS, VNS and audio modalities each get a distinct
 * non-overlapping hue so users can identify them at a glance on the helmet.
 */

export const WL = {
  '660nm': {
    label: '660 nm — Red',
    description: 'Visible red. Shown as actual color.',
    hex: '#FF2200',
    glowHex: '#FF5533',
    threeHex: 0xFF2200,
    glowThreeHex: 0xFF5533,
    isVisible: true,
    nm: 660,
  },
  '808nm': {
    label: '808 nm — NIR',
    description: 'Near-infrared (invisible). False color: orange.',
    hex: '#FF7200',
    glowHex: '#FF9944',
    threeHex: 0xFF7200,
    glowThreeHex: 0xFF9944,
    isVisible: false,
    nm: 808,
  },
  '830nm': {
    label: '830 nm — NIR',
    description: 'Near-infrared (invisible). False color: deep orange.',
    hex: '#FF8C00',
    glowHex: '#FFAA44',
    threeHex: 0xFF8C00,
    glowThreeHex: 0xFFAA44,
    isVisible: false,
    nm: 830,
  },
  '1064nm': {
    label: '1064 nm — NIR-II',
    description: 'NIR-II (invisible). False color: amber.',
    hex: '#FFBB00',
    glowHex: '#FFD044',
    threeHex: 0xFFBB00,
    glowThreeHex: 0xFFD044,
    isVisible: false,
    nm: 1064,
  },
  '1170nm': {
    label: '1170 nm — Deep NIR (T2)',
    description: 'Deep near-infrared (invisible, T2 only). False color: gold.',
    hex: '#FFE500',
    glowHex: '#FFF066',
    threeHex: 0xFFE500,
    glowThreeHex: 0xFFF066,
    isVisible: false,
    nm: 1170,
  },
};

/** Per-modality accent colors (used in UI panels and indicators). */
export const MODALITY_COLORS = {
  pbm:      { hex: '#FF5500', three: 0xFF5500, label: 'PBM' },
  eeg:      { hex: '#00D4FF', three: 0x00D4FF, label: 'EEG' },
  bes:      { hex: '#AA44FF', three: 0xAA44FF, label: 'BES / tACS' },
  tdcs:     { hex: '#FF44AA', three: 0xFF44AA, label: 'tDCS' },
  vns:      { hex: '#44FFAA', three: 0x44FFAA, label: 'VNS + HRV' },
  audio:    { hex: '#4488FF', three: 0x4488FF, label: 'Audio' },
  visual:   { hex: '#FF44FF', three: 0xFF44FF, label: 'Visual' },
  tms:      { hex: '#33AAFF', three: 0x33AAFF, label: 'TMS' },
  hd_tdcs:  { hex: '#FF88CC', three: 0xFF88CC, label: 'HD-tDCS' },
  haptic:   { hex: '#FFFF44', three: 0xFFFF44, label: 'Haptic (40 Hz)' },
};

/** EEG electrode impedance status colors. */
export const IMPEDANCE_COLORS = {
  good:     { hex: '#00FF44', three: 0x00FF44 }, // < 10 kΩ
  marginal: { hex: '#FFAA00', three: 0xFFAA00 }, // 10–25 kΩ
  poor:     { hex: '#FF2200', three: 0xFF2200 }, // > 25 kΩ
  off:      { hex: '#223344', three: 0x223344 }, // inactive
};
