#!/usr/bin/env bun
/**
 * pbm-optical-psf.ts — How sharp a spatial boundary can a scalp PBM module produce?
 *
 * Answers the question left open by PR #210: the "~14% of a midline module's energy
 * lands contralateral" figure was pure geometry — it assumed light goes straight down.
 * It does not. Tissue is a strong scatterer, so a module's footprint at cortex is much
 * wider and much softer-edged than its aperture at the scalp, and BOTH conclusions that
 * were inferred from that (partial-module masking is ineffective; a narrower zone is only
 * partially effective) need a number rather than an assertion.
 *
 * Method: weighted-photon Monte Carlo transport (MCML formulation — Wang, Jacques &
 * Zheng 1995) through a four-layer plane-parallel head model, scoring fluence in
 * cylindrical (r, z) bins for a pencil beam at the origin. Because the geometry is
 * radially symmetric the result is a point-spread function: the response to any extended
 * source is that source convolved with this PSF. The hex module response is then obtained
 * by convolving the PSF with a uniform regular-hexagon aperture.
 *
 * Two reductions are used, both exact given a radially symmetric PSF:
 *   - line profile through the module centre  → FWHM and 10–90% edge width
 *   - 1-D marginal (integrated over y)        → left/right energy split, which for a
 *     radially symmetric kernel depends only on the module's distance from the midline,
 *     so the socket's row (y) drops out entirely.
 *
 * ── Scope and honest limits ──────────────────────────────────────────────────
 *
 * PLANE-PARALLEL. The real vault is curved (R_s ~65–130 mm per NP-HEX-ZM-001 §3). Over
 * the ±40 mm that dominates the answer, the sagitta of an 87 mm sphere is ~9 mm, so the
 * slab is a local approximation. Curvature makes the true lateral spread slightly WIDER
 * in arc-length terms than modelled here, so the contralateral fractions below are, if
 * anything, mild under-estimates. This is not a substitute for a curved-mesh MC or a
 * phantom measurement; it is the first quantitative bound the repo has.
 *
 * OPTICAL PROPERTIES ARE LITERATURE VALUES, NOT MEASURED. See LAYERS below for the
 * per-layer basis. A ±30% sweep on reduced scattering is run automatically so the
 * reader can see how much the answer depends on that choice.
 *
 * Run:  bun scripts/pbm-optical-psf.ts            (default 2e6 photons)
 *       bun scripts/pbm-optical-psf.ts --photons 1e7
 *       bun scripts/pbm-optical-psf.ts --json out.json
 */

// ─── Tissue model ──────────────────────────────────────────────────────────────

interface Layer {
  name: string;
  thickness: number; // mm
  mua: number;       // absorption coefficient, mm^-1
  musp: number;      // REDUCED scattering coefficient, mm^-1
  g: number;         // Henyey-Greenstein anisotropy
  n: number;         // refractive index
}

/**
 * Adult head at 1064 nm. Thicknesses are adult-typical over lateral prefrontal cortex;
 * mua/musp are mid-range literature values for 1064 nm (Bashkatov et al. 2005 for skin
 * and cranial bone; Yaroslavsky et al. 2002 for brain; CSF from Custo et al. 2006).
 * 1064 nm sits in the low-absorption window: lower haemoglobin absorption than 810 nm,
 * water absorption still small, and reduced scattering falls roughly as lambda^-b, so
 * musp at 1064 is below the more commonly quoted 800 nm values.
 *
 * These are NOT measured on NeurOne hardware. Treat the absolute depth-dose with
 * caution; the LATERAL result is far more robust, because lateral spread is governed by
 * musp and the transport mean free path rather than by absolute attenuation.
 */
const LAYERS: Layer[] = [
  { name: "scalp",        thickness: 6,   mua: 0.015, musp: 0.65, g: 0.9, n: 1.4 },
  { name: "skull",        thickness: 7,   mua: 0.012, musp: 0.75, g: 0.9, n: 1.4 },
  // CSF mua is floored at the pure-water value at 1064 nm (~0.14 cm^-1 = 0.0144 mm^-1).
  // CSF is >98% water, so it cannot absorb less than water does. An earlier revision of
  // this table carried 0.003, which is the water figure at ~800 nm — a wavelength error.
  { name: "CSF",          thickness: 2,   mua: 0.0144, musp: 0.03, g: 0.9, n: 1.4 },
  { name: "gray matter",  thickness: 45,  mua: 0.020, musp: 0.75, g: 0.9, n: 1.4 },
];

const N_OUTSIDE = 1.0;

/** Depth of the gray-matter surface = sum of the three layers above it. */
const CORTEX_MM = LAYERS.slice(0, 3).reduce((s, l) => s + l.thickness, 0); // 15 mm

// ─── Grid ──────────────────────────────────────────────────────────────────────

const DR = 0.5;   // mm, radial bin
const NR = 300;   // → 150 mm radial extent
const DZ = 0.5;   // mm, depth bin
const NZ = 120;   // → 60 mm depth extent

const TOTAL_THICKNESS = LAYERS.reduce((s, l) => s + l.thickness, 0);

// ─── Monte Carlo ───────────────────────────────────────────────────────────────

/** xorshift128+ — deterministic, seedable, fast enough that the RNG is not the bottleneck. */
function makeRng(seed: number) {
  let s0 = seed >>> 0 || 1;
  let s1 = (seed * 2654435761) >>> 0 || 2;
  let s2 = 0x9e3779b9;
  let s3 = 0x85ebca6b;
  return function rng(): number {
    const t = s1 << 9;
    s2 ^= s0; s3 ^= s1; s1 ^= s2; s0 ^= s3; s2 ^= t;
    s3 = (s3 << 11) | (s3 >>> 21);
    // Map to (0,1); never return exactly 0, which would break log().
    return ((s0 + s3) >>> 0) * 2.3283064365386963e-10 + 1e-12;
  };
}

/** Which layer contains depth z. Returns -1 above the surface / below the model. */
function layerAt(z: number): number {
  let top = 0;
  for (let i = 0; i < LAYERS.length; i++) {
    const bottom = top + LAYERS[i].thickness;
    if (z < bottom) return i;
    top = bottom;
  }
  return -1;
}

/** Depth of the top of layer i. */
const LAYER_TOP: number[] = (() => {
  const tops: number[] = [];
  let t = 0;
  for (const l of LAYERS) { tops.push(t); t += l.thickness; }
  return tops;
})();

const MIN_WEIGHT = 1e-4;
const ROULETTE_CHANCE = 10;

export interface PsfResult {
  /** absorbed[iz][ir] converted to fluence (arbitrary units, per unit incident energy). */
  fluence: Float64Array[];
  photons: number;
  /** Fraction of launched weight that escaped back out of the scalp surface. */
  diffuseReflectance: number;
}

/**
 * Launch `photons` pencil-beam photons at (r=0, z=0) travelling +z, score absorption.
 * Standard MCML: specular reflection at entry, Henyey-Greenstein scattering, step
 * splitting at layer boundaries with Fresnel at internal interfaces, Russian roulette.
 */
function runMonteCarlo(photons: number, seed: number, muspScale = 1, csfMm?: number): PsfResult {
  const rng = makeRng(seed);

  // Sub-arachnoid CSF is a low-scattering light pipe (musp 0.03) and its thickness varies
  // 1–5 mm across subjects and sulcal position, so it is swept independently of musp.
  const L: Layer[] = LAYERS.map(l =>
    l.name === "CSF" && csfMm !== undefined ? { ...l, thickness: csfMm } : l,
  );
  const tops: number[] = [];
  { let t = 0; for (const l of L) { tops.push(t); t += l.thickness; } }
  const totalThickness = L.reduce((s, l) => s + l.thickness, 0);
  const layerOf = (z: number) => {
    for (let i = 0; i < L.length; i++) if (z < tops[i] + L[i].thickness) return i;
    return -1;
  };

  // Per-layer derived coefficients, with the sweep factor applied to scattering only.
  const mus = L.map(l => (l.musp * muspScale) / (1 - l.g));
  const mut = L.map((l, i) => l.mua + mus[i]);
  const albedo = L.map((l, i) => mus[i] / mut[i]);

  const absorbed: Float64Array[] = Array.from({ length: NZ }, () => new Float64Array(NR));

  // Specular reflection at the air/scalp interface (normal incidence).
  const rSpec = ((N_OUTSIDE - LAYERS[0].n) / (N_OUTSIDE + LAYERS[0].n)) ** 2;
  let escaped = 0;

  for (let p = 0; p < photons; p++) {
    let x = 0, y = 0, z = 0;
    let ux = 0, uy = 0, uz = 1;
    let w = 1 - rSpec;
    let li = 0;

    // Photons enter perpendicular; the pencil beam is the definition of the PSF.
    for (;;) {
      // --- step size ---
      let s = -Math.log(rng()) / mut[li];

      // --- move, splitting the step at layer boundaries ---
      for (;;) {
        const top = tops[li];
        const bottom = top + L[li].thickness;
        let dBoundary = Infinity;
        if (uz > 0) dBoundary = (bottom - z) / uz;
        else if (uz < 0) dBoundary = (top - z) / uz;

        if (dBoundary >= s) {
          x += ux * s; y += uy * s; z += uz * s;
          break;
        }

        // Advance to the interface.
        x += ux * dBoundary; y += uy * dBoundary; z += uz * dBoundary;
        s -= dBoundary;

        const nextLi = uz > 0 ? li + 1 : li - 1;
        const nHere = L[li].n;
        const nNext = nextLi < 0 ? N_OUTSIDE
          : nextLi >= L.length ? LAYERS[L.length - 1].n
          : L[nextLi].n;

        if (nHere !== nNext) {
          // Fresnel at the interface. Only the air interface differs here, but the
          // branch is kept general so per-layer n can be varied without a rewrite.
          const cosI = Math.abs(uz);
          const sinI = Math.sqrt(Math.max(0, 1 - cosI * cosI));
          const sinT = (nHere / nNext) * sinI;
          let R: number;
          if (sinT >= 1) {
            R = 1; // total internal reflection
          } else {
            const cosT = Math.sqrt(Math.max(0, 1 - sinT * sinT));
            const rs = ((nHere * cosI - nNext * cosT) / (nHere * cosI + nNext * cosT)) ** 2;
            const rp = ((nHere * cosT - nNext * cosI) / (nHere * cosT + nNext * cosI)) ** 2;
            R = 0.5 * (rs + rp);
          }
          if (rng() <= R) {
            uz = -uz; // reflect, stay in this layer, continue the remaining step
            continue;
          }
        }

        if (nextLi < 0) { escaped += w; w = 0; break; }          // out through the scalp
        if (nextLi >= L.length) { w = 0; break; }           // out the bottom (deep, negligible)
        li = nextLi;
        s *= mut[li - (uz > 0 ? 1 : -1)] / mut[li];              // rescale remaining path
      }

      if (w <= 0) break;
      if (z < 0 || z >= totalThickness) break;

      // --- absorb ---
      const dw = w * (1 - albedo[li]);
      w -= dw;
      const r = Math.sqrt(x * x + y * y);
      const ir = Math.floor(r / DR);
      const iz = Math.floor(z / DZ);
      if (ir < NR && iz < NZ && iz >= 0) absorbed[iz][ir] += dw;

      // --- scatter (Henyey-Greenstein) ---
      const g = L[li].g;
      let cost: number;
      if (g === 0) {
        cost = 2 * rng() - 1;
      } else {
        const t = (1 - g * g) / (1 - g + 2 * g * rng());
        cost = (1 + g * g - t * t) / (2 * g);
      }
      const sint = Math.sqrt(Math.max(0, 1 - cost * cost));
      const psi = 2 * Math.PI * rng();
      const cosp = Math.cos(psi), sinp = Math.sin(psi);

      let nux: number, nuy: number, nuz: number;
      if (Math.abs(uz) > 0.99999) {
        nux = sint * cosp;
        nuy = sint * sinp;
        nuz = cost * Math.sign(uz);
      } else {
        const denom = Math.sqrt(1 - uz * uz);
        nux = (sint * (ux * uz * cosp - uy * sinp)) / denom + ux * cost;
        nuy = (sint * (uy * uz * cosp + ux * sinp)) / denom + uy * cost;
        nuz = -sint * cosp * denom + uz * cost;
      }
      ux = nux; uy = nuy; uz = nuz;

      // --- Russian roulette ---
      if (w < MIN_WEIGHT) {
        if (rng() <= 1 / ROULETTE_CHANCE) w *= ROULETTE_CHANCE;
        else break;
      }
    }
  }

  // Convert absorbed weight to fluence: phi = A / (mua * V), V = 2*pi*r*dr*dz.
  const fluence: Float64Array[] = Array.from({ length: NZ }, () => new Float64Array(NR));
  for (let iz = 0; iz < NZ; iz++) {
    const z = (iz + 0.5) * DZ;
    const li = layerOf(z);
    if (li < 0) continue;
    const mua = L[li].mua;
    for (let ir = 0; ir < NR; ir++) {
      const r = (ir + 0.5) * DR;
      const vol = 2 * Math.PI * r * DR * DZ;
      fluence[iz][ir] = absorbed[iz][ir] / (mua * vol * photons);
    }
  }

  return { fluence, photons, diffuseReflectance: escaped / photons };
}

// ─── Aperture: one regular hexagon, flat-to-flat width W ───────────────────────

/**
 * Chord length of a regular hexagon at offset u from centre, for a hexagon of
 * flat-to-flat width W oriented with flats facing left/right (the packing used by
 * the socket row structure: horizontal neighbour spacing = W).
 *
 * With inradius r = W/2 and circumradius a = W/sqrt(3), the horizontal extent is +/- r
 * and the vertical chord runs from the full vertex span 2a at u=0 down to a at |u|=r.
 */
function hexChord(u: number, W: number): number {
  const r = W / 2;
  const a = W / Math.sqrt(3); // circumradius
  const au = Math.abs(u);
  if (au >= r) return 0;
  // Linear taper from 2a at centre to a at the flat edges.
  return 2 * a - (au / r) * a;
}

/**
 * Chord of the IPSILATERAL HALF of the same hexagon — the aperture you would get if
 * partial-module inclusion (option (c), minor-address subsetting) existed and you lit
 * only the elements on the right of the module's centre line. This is the probe that
 * decides whether sub-module masking can buy anything the optics does not immediately
 * give back.
 */
function halfHexChord(u: number, W: number): number {
  return u < 0 ? 0 : hexChord(u, W);
}

/** Area of a regular hexagon of flat-to-flat width W (mm^2). */
function hexArea(W: number): number {
  return (Math.sqrt(3) / 2) * W * W;
}

// ─── Reductions ────────────────────────────────────────────────────────────────

/** Interpolate the radial PSF at depth z for arbitrary radius. */
function psfAt(fluence: Float64Array[], z: number): (r: number) => number {
  const iz = Math.min(NZ - 1, Math.max(0, Math.floor(z / DZ)));
  const row = fluence[iz];
  return (r: number) => {
    const fr = r / DR - 0.5;
    if (fr <= 0) return row[0];
    const i = Math.floor(fr);
    if (i >= NR - 1) return 0;
    const t = fr - i;
    return row[i] * (1 - t) + row[i + 1] * t;
  };
}

/**
 * Line profile through the module centre at depth z: sum the PSF over aperture pixels.
 * Returns f(x) sampled on `xs`.
 */
function lineProfile(psf: (r: number) => number, W: number, xs: number[]): number[] {
  const step = 0.5; // mm aperture sampling
  const r = W / 2;
  const cells: Array<[number, number]> = [];
  for (let ax = -r; ax <= r; ax += step) {
    const half = hexChord(ax, W) / 2;
    for (let ay = -half; ay <= half; ay += step) cells.push([ax, ay]);
  }
  const norm = cells.length;
  return xs.map(x => {
    let acc = 0;
    for (const [ax, ay] of cells) {
      const dx = x - ax;
      acc += psf(Math.sqrt(dx * dx + ay * ay));
    }
    return acc / norm;
  });
}

/**
 * 1-D marginal of the module response at depth z: P(x) = integral over y.
 * Built as chord(u) convolved with K(x) = 2 * integral_0^inf PSF(sqrt(x^2+y^2)) dy.
 * Exact for a radially symmetric PSF, and independent of the module's row position.
 */
function marginalProfile(
  psf: (r: number) => number,
  W: number,
  xs: number[],
  chord: (u: number, W: number) => number = hexChord,
): number[] {
  const YMAX = 150, DY = 0.5;
  const K = (x: number) => {
    let acc = 0;
    for (let y = DY / 2; y < YMAX; y += DY) acc += psf(Math.sqrt(x * x + y * y));
    return 2 * acc * DY;
  };
  // Cache keyed at 1/8 mm. Every argument reaching this function is x - u where x is a
  // multiple of 0.5 and u is an odd multiple of 0.125, so x - u is always an exact
  // multiple of 0.125 and the key is lossless. Keying at 1/4 mm instead would put every
  // argument exactly on a .5 tie, and Math.round breaks ties toward +inf, which
  // translated the whole profile by a systematic +0.125 mm rather than rounding it.
  const kCache = new Map<number, number>();
  const kOf = (x: number) => {
    const key = Math.round(x * 8);
    let v = kCache.get(key);
    if (v === undefined) { v = K(key / 8); kCache.set(key, v); }
    return v;
  };

  const DU = 0.25;
  const r = W / 2;
  return xs.map(x => {
    let acc = 0;
    for (let u = -r + DU / 2; u < r; u += DU) acc += chord(u, W) * kOf(x - u) * DU;
    return acc;
  });
}

/** Width at which the profile falls to `frac` of peak, by linear interpolation. */
function widthAtFraction(xs: number[], ys: number[], frac: number): number {
  const peak = Math.max(...ys);
  const target = peak * frac;
  const mid = Math.floor(xs.length / 2);
  for (let i = mid; i < xs.length - 1; i++) {
    if (ys[i] >= target && ys[i + 1] < target) {
      const t = (ys[i] - target) / (ys[i] - ys[i + 1]);
      return 2 * (xs[i] + t * (xs[i + 1] - xs[i]));
    }
  }
  return NaN;
}

/** Half-width from centre at which the profile falls to `frac` of peak. */
function halfWidthAtFraction(xs: number[], ys: number[], frac: number): number {
  const w = widthAtFraction(xs, ys, frac);
  return w / 2;
}

/**
 * Fraction of the module's energy at this depth landing at x < 0 (contralateral),
 * for a module whose centre sits at x = d.
 */
function contralateralFraction(xs: number[], marg: number[], d: number): number {
  const dx = xs[1] - xs[0];
  let total = 0, left = 0;
  for (let i = 0; i < xs.length; i++) {
    const v = marg[i] * dx;
    total += v;
    // Each sample owns the interval [x - dx/2, x + dx/2]. The bin straddling the midline
    // is split in proportion rather than assigned wholesale — for a module centred ON the
    // midline that bin is the largest in the profile, and giving it entirely to one side
    // biased the answer by half a bin (0.63 pp), breaking the exact 50% symmetry result.
    const lo = xs[i] + d - dx / 2;
    const hi = xs[i] + d + dx / 2;
    if (hi <= 0) left += v;
    else if (lo < 0) left += v * ((0 - lo) / dx);
  }
  return left / total;
}

/**
 * Edge-spread function width — the response to a HALF-PLANE source, which is the
 * aperture-independent boundary sharpness of the tissue kernel itself and therefore the
 * real spatial resolution floor.
 *
 * Measuring the 10–90% edge of a finite module instead conflates two things: a 40 mm
 * aperture is too narrow for its plateau to saturate, which depresses the normalising
 * peak, and the far edge's tail lifts the 10% level. Both compress the apparent width and
 * make the floor look sharper than it is.
 *
 * ESF(x) = integral_0^inf K(x-u) du = integral_{-inf}^{x} K(t) dt — simply the cumulative
 * of the line-spread function, so it is computed exactly with no convolution.
 */
function esfWidths(psf: (r: number) => number): { w10_90: number; w25_75: number } {
  const YMAX = 200, DY = 0.5, DX = 0.125, XMAX = 200;
  const K = (x: number) => {
    let acc = 0;
    for (let y = DY / 2; y < YMAX; y += DY) acc += psf(Math.sqrt(x * x + y * y));
    return 2 * acc * DY;
  };

  const xs: number[] = [], cum: number[] = [];
  let run = 0;
  for (let x = -XMAX; x <= XMAX; x += DX) {
    run += K(x) * DX;
    xs.push(x);
    cum.push(run);
  }
  const total = run;

  const crossing = (frac: number) => {
    const target = frac * total;
    for (let i = 0; i < cum.length - 1; i++) {
      if (cum[i] <= target && cum[i + 1] > target) {
        const t = (target - cum[i]) / (cum[i + 1] - cum[i]);
        return xs[i] + t * DX;
      }
    }
    return NaN;
  };

  return {
    w10_90: crossing(0.9) - crossing(0.1),
    w25_75: crossing(0.75) - crossing(0.25),
  };
}

// ─── Socket geometry (frontal rows only — this is where clinical-03 lives) ─────

/** Socket id → horizontal offset in units of hex pitch, derived from the row structure. */
const FRONTAL_X: Record<number, number> = (() => {
  const widths = [1, 2, 3, 4, 5, 4];
  const out: Record<number, number> = {};
  let id = 1;
  for (const w of widths) {
    for (let col = 0; col < w; col++) out[id++] = col - (w - 1) / 2;
  }
  return out;
})();

const FRONTAL_RIGHT = [1, 3, 5, 6, 9, 10, 13, 14, 15, 18, 19];
const FRONTAL_RIGHT_STRICT = [3, 6, 9, 10, 14, 15, 18, 19];

// ─── Main ──────────────────────────────────────────────────────────────────────

function parseArg(flag: string): string | undefined {
  const i = process.argv.indexOf(flag);
  return i >= 0 ? process.argv[i + 1] : undefined;
}

const PHOTONS = Number(parseArg("--photons") ?? 2e6);
const W_DESIGN = 40;   // mm, NP-HEX-ZM-001 §3 design point
const W_SMALL = 34;    // mm, smallest width in the doc's workable table

const xs: number[] = [];
for (let x = -150; x <= 150; x += 0.5) xs.push(Number(x.toFixed(2)));

interface Scenario { label: string; muspScale: number; seed: number; csfMm?: number }
const SCENARIOS: Scenario[] = [
  { label: "nominal",         muspScale: 1.0, seed: 20260720 },
  { label: "nominal (seed2)", muspScale: 1.0, seed: 987654321 },
  { label: "musp -30%",       muspScale: 0.7, seed: 20260720 },
  { label: "musp +30%",       muspScale: 1.3, seed: 20260720 },
  // CSF thickness is the sensitive geometric axis, not musp: at musp 0.03 the
  // sub-arachnoid space acts as a low-scattering light pipe, so its thickness moves the
  // lateral spread more than a +/-30% change in scattering does. It varies 1-5 mm across
  // subjects and sulcal position, so both ends of that range are run.
  { label: "CSF 1 mm",        muspScale: 1.0, seed: 20260720, csfMm: 1 },
  { label: "CSF 4 mm",        muspScale: 1.0, seed: 20260720, csfMm: 4 },
];

const report: Record<string, unknown> = {
  model: {
    method: "weighted-photon Monte Carlo (MCML formulation), plane-parallel 4-layer head",
    wavelength_nm: 1064,
    photons: PHOTONS,
    layers: LAYERS,
    cortex_depth_mm: CORTEX_MM,
    module_widths_mm: [W_SMALL, W_DESIGN],
    caveats: [
      "plane-parallel, not curved — lateral spread on a curved vault is slightly wider",
      "optical properties are literature values at 1064 nm, not measured on NeurOne hardware",
      "socket pitch taken as equal to module flat-to-flat width (row structure assumption)",
    ],
  },
  scenarios: [] as unknown[],
};

console.log(`\nPBM optical point-spread function — 1064 nm, ${PHOTONS.toExponential(1)} photons/run`);
console.log(`Head model: ${LAYERS.map(l => `${l.name} ${l.thickness}mm`).join(" / ")}  → cortex at ${CORTEX_MM} mm\n`);

for (const sc of SCENARIOS) {
  const t0 = Date.now();
  const mc = runMonteCarlo(PHOTONS, sc.seed, sc.muspScale, sc.csfMm);
  // Cortex depth shifts with the CSF sweep.
  const cortexMm = 6 + 7 + (sc.csfMm ?? 2);
  const secs = ((Date.now() - t0) / 1000).toFixed(1);

  const out: Record<string, unknown> = { label: sc.label, muspScale: sc.muspScale, csf_mm: sc.csfMm ?? 2, cortex_mm: cortexMm, seed: sc.seed, runtime_s: Number(secs) };

  console.log(`── ${sc.label}  (musp x${sc.muspScale}, ${secs}s, diffuse reflectance ${(mc.diffuseReflectance * 100).toFixed(1)}%)`);

  for (const [depthLabel, z] of [["scalp surface (0.25 mm)", 0.25], [`cortex (${cortexMm} mm)`, cortexMm + 0.25], [`cortex +5 mm (${cortexMm + 5} mm)`, cortexMm + 5]] as Array<[string, number]>) {
    const psf = psfAt(mc.fluence, z);
    const widths: Record<string, unknown> = {};

    for (const W of [W_SMALL, W_DESIGN]) {
      const line = lineProfile(psf, W, xs);
      const fwhm = widthAtFraction(xs, line, 0.5);
      const w90 = halfWidthAtFraction(xs, line, 0.9);
      const w10 = halfWidthAtFraction(xs, line, 0.1);
      widths[`W${W}`] = {
        fwhm_mm: Number(fwhm.toFixed(1)),
        edge_10_90_mm: Number((w10 - w90).toFixed(1)),
        aperture_mm: W,
        aperture_area_cm2: Number((hexArea(W) / 100).toFixed(2)),
      };
      console.log(
        `   ${depthLabel.padEnd(24)} W=${W}mm  FWHM ${fwhm.toFixed(1).padStart(5)} mm` +
        `   10–90% edge ${(w10 - w90).toFixed(1).padStart(5)} mm`,
      );
    }
    out[depthLabel] = widths;
  }

  // Lateralization: contralateral energy fraction at cortex, per module offset and per zone.
  const psfCortex = psfAt(mc.fluence, cortexMm + 0.25);

  // The aperture-independent resolution floor. This, not a module's own edge width, is
  // the sharpest cortical boundary the tissue permits from any source shape.
  const esf = esfWidths(psfCortex);
  out.esf_cortex = { w10_90_mm: Number(esf.w10_90.toFixed(1)), w25_75_mm: Number(esf.w25_75.toFixed(1)) };
  console.log(
    `   RESOLUTION FLOOR @cortex   edge-spread function  10–90% ${esf.w10_90.toFixed(1)} mm` +
    `   25–75% ${esf.w25_75.toFixed(1)} mm   (aperture-independent)`,
  );
  const lateral: Record<string, unknown> = {};
  for (const W of [W_SMALL, W_DESIGN]) {
    const marg = marginalProfile(psfCortex, W, xs);
    const fOf = (pitchUnits: number) => contralateralFraction(xs, marg, pitchUnits * W);

    const perOffset: Record<string, number> = {};
    for (const d of [0, 0.5, 1, 1.5, 2]) perOffset[`${d} pitch`] = Number((fOf(d) * 100).toFixed(1));

    const zoneFrac = (ids: number[]) =>
      ids.reduce((s, id) => s + fOf(Math.abs(FRONTAL_X[id])), 0) / ids.length;

    const inclusive = zoneFrac(FRONTAL_RIGHT);
    const strict = zoneFrac(FRONTAL_RIGHT_STRICT);

    // Option (c) probe: light only the ipsilateral half of a MIDLINE module.
    // Compare against lighting it whole (~50% contralateral) and against omitting it
    // (0% contralateral — what option (b) actually does).
    const margHalf = marginalProfile(psfCortex, W, xs, halfHexChord);
    const halfMidline = contralateralFraction(xs, margHalf, 0);
    const fullMidline = fOf(0);

    // Free exact self-test: a symmetric aperture centred on the midline must split
    // exactly 50/50, because the PSF is radially symmetric and the hexagon is even in x.
    // Any deviation is a numerical artefact of the reduction, not physics.
    if (Math.abs(fullMidline - 0.5) > 5e-4) {
      console.error(
        `  SYMMETRY CHECK FAILED (W=${W}): midline module should be exactly 50% ` +
        `contralateral, got ${(fullMidline * 100).toFixed(3)}%`,
      );
      process.exitCode = 1;
    }

    lateral[`W${W}`] = {
      contralateral_pct_by_offset: perOffset,
      frontal_right_inclusive_pct: Number((inclusive * 100).toFixed(1)),
      frontal_right_strict_pct: Number((strict * 100).toFixed(1)),
      absolute_reduction_pct_points: Number(((inclusive - strict) * 100).toFixed(1)),
      relative_reduction_pct: Number(((1 - strict / inclusive) * 100).toFixed(0)),
      midline_module_full_contralateral_pct: Number((fullMidline * 100).toFixed(1)),
      midline_module_half_masked_contralateral_pct: Number((halfMidline * 100).toFixed(1)),
      midline_module_omitted_contralateral_pct: 0,
    };

    console.log(
      `   contralateral @cortex  W=${W}mm  midline module ${(fullMidline * 100).toFixed(1)}%` +
      `  |  Frontal Right ${(inclusive * 100).toFixed(1)}% → strict ${(strict * 100).toFixed(1)}%`,
    );
    console.log(
      `   option (c) probe       W=${W}mm  midline module half-masked still spills ` +
      `${(halfMidline * 100).toFixed(1)}% contralateral (whole ${(fullMidline * 100).toFixed(1)}%, omitted 0%)`,
    );
  }
  out.lateralization = lateral;
  (report.scenarios as unknown[]).push(out);
  console.log();
}

const jsonOut = parseArg("--json");
if (jsonOut) {
  await Bun.write(jsonOut, JSON.stringify(report, null, 2) + "\n");
  console.log(`wrote ${jsonOut}`);
}
