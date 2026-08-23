#!/usr/bin/env bun
/**
 * pod-pattern-coverage.ts — how well can a pod pattern reach an arbitrary scalp point?
 *
 * Answers OI-EEGNET-25 (NP-HW-EEGNET-001 §1.10.5). NP-HW-EEGNET-001 §1.10.4 bounds
 * pod count by ANGULAR quantisation on one tile: worst case 2*r*sin(pi/2N) for a target
 * sitting between two pods at ring radius r. That formula has two defects. It assumes the
 * target sits AT radius r (ignoring radial mismatch), and it assumes a target is served
 * only by its own tile's pods, when sockets are 40 mm apart and pods reach ~14 mm.
 *
 * The montage-independent quantity is the lattice COVERING RADIUS: for every point on the
 * lattice-covered scalp, the distance to the nearest pod position available anywhere on the
 * lattice — i.e. assuming you may place the electrode tile at whichever socket suits best.
 * That needs no 10-20 coordinates, so unlike a montage-specific fit it does not wait on REG-1.
 *
 * Geometry: hardware/np_socket_map.json (80 sockets, mm, INTERIM ellipsoid — PROVISIONAL
 * pending REG-1/ACT-1, so absolute residuals inherit that status; the RANKING of patterns
 * is far more robust than the absolute numbers).
 *
 * Tangent frames come from a plane fit to each socket's 7 nearest neighbours, so no
 * ellipsoid parameters are assumed. Pods are placed in that tangent plane; over a <=14.5 mm
 * offset on a ~100 mm-radius surface the flat-plane error is ~1 mm (r^2/2R).
 *
 * Sanity check built in: a single centre pod (today's T1-B) must give a covering radius of
 * ~half the 40 mm socket pitch, and must be consistent with §1.3's 18 mm Oz defect.
 *
 * Run: bun scripts/pod-pattern-coverage.ts
 */
type V = [number, number, number];
const sub = (a: V, b: V): V => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const add = (a: V, b: V): V => [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
const mul = (a: V, k: number): V => [a[0] * k, a[1] * k, a[2] * k];
const dot = (a: V, b: V) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const cross = (a: V, b: V): V => [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]];
const norm = (a: V) => Math.sqrt(dot(a, a));
const unit = (a: V): V => mul(a, 1 / norm(a));

const raw = JSON.parse(await Bun.file("hardware/np_socket_map.json").text());
const P: V[] = raw.sockets.map((s: any) => [s.xMm, s.yMm, s.zMm] as V);
const n = P.length;

/** Orthonormal tangent basis at each socket, from a plane fit to its k nearest neighbours. */
function frames(k = 7): [V, V][] {
  return P.map((p, i) => {
    const idx = P.map((q, j) => [norm(sub(q, p)), j] as [number, number])
      .sort((a, b) => a[0] - b[0]).slice(0, k).map((t) => t[1]);
    const c = idx.reduce((acc, j) => add(acc, P[j]), [0, 0, 0] as V);
    const mean = mul(c, 1 / idx.length);
    // normal = eigenvector of smallest eigenvalue; power-iterate on (trace*I - M)
    const M = [[0,0,0],[0,0,0],[0,0,0]];
    for (const j of idx) { const d = sub(P[j], mean);
      for (let a = 0; a < 3; a++) for (let b = 0; b < 3; b++) M[a][b] += d[a] * d[b]; }
    const tr = M[0][0] + M[1][1] + M[2][2];
    const A = M.map((row, a) => row.map((v, b) => (a === b ? tr - v : -v)));
    let v: V = [1, 1, 1];
    for (let it = 0; it < 200; it++) {
      const w: V = [ A[0][0]*v[0]+A[0][1]*v[1]+A[0][2]*v[2],
                     A[1][0]*v[0]+A[1][1]*v[1]+A[1][2]*v[2],
                     A[2][0]*v[0]+A[2][1]*v[1]+A[2][2]*v[2] ];
      v = unit(w);
    }
    const nrm = v;                                  // surface normal
    let seed: V = Math.abs(nrm[0]) < 0.9 ? [1,0,0] : [0,1,0];
    const u = unit(cross(nrm, seed));
    return [u, cross(nrm, u)] as [V, V];
  });
}
const F = frames();

/** All pod positions on the lattice for a pattern: optional centre pod + m on a ring of radius r. */
function pods(centre: boolean, m: number, r: number): V[] {
  const out: V[] = [];
  for (let i = 0; i < n; i++) {
    const [u, v] = F[i];
    if (centre) out.push(P[i]);
    for (let j = 0; j < m; j++) {
      const a = (2 * Math.PI * j) / m;
      out.push(add(P[i], add(mul(u, r * Math.cos(a)), mul(v, r * Math.sin(a)))));
    }
  }
  return out;
}

/** Dense sample of the scalp the lattice covers: each socket's own 20 mm-inradius tile face. */
function samples(step = 2.5, reach = 20): V[] {
  const out: V[] = [];
  for (let i = 0; i < n; i++) {
    const [u, v] = F[i];
    for (let a = -reach; a <= reach + 1e-9; a += step)
      for (let b = -reach; b <= reach + 1e-9; b += step)
        if (a * a + b * b <= reach * reach)
          out.push(add(P[i], add(mul(u, a), mul(v, b))));
  }
  return out;
}
const T = samples();

function cover(pd: V[], T: V[]) {
  const d: number[] = [];
  for (const t of T) {
    let best = Infinity;
    for (const q of pd) {
      const dx = t[0]-q[0], dy = t[1]-q[1], dz = t[2]-q[2];
      const s = dx*dx + dy*dy + dz*dz;
      if (s < best) best = s;
    }
    d.push(Math.sqrt(best));
  }
  d.sort((a, b) => a - b);
  return { worst: d[d.length - 1], p95: d[Math.floor(0.95 * d.length)] };
}

console.log(`${n} sockets, ${T.length} sampled scalp points (7-NN tangent frames)\n`);
const s0 = cover(pods(true, 0, 0), T);
console.log(`SANITY — centre pod only (today's T1-B): worst ${s0.worst.toFixed(1)} mm, p95 ${s0.p95.toFixed(1)} mm`);
console.log(`         expected ~20 mm = half the 40 mm socket pitch; §1.3's Oz defect is 18 mm\n`);
console.log(" pattern      pods   best r    worst    p95   emitters   meets ±10 worst?");
console.log("---------------------------------------------------------------------------");
for (const [centre, m] of [[false,4],[true,3],[false,5],[true,4],[false,6],[true,5],[false,8],[true,7]] as [boolean,number][]) {
  const N = (centre ? 1 : 0) + m;
  let best = { worst: Infinity, p95: 0, r: 0 };
  for (let r = 6; r <= 14.5 + 1e-9; r += 0.5) {
    const c = cover(pods(centre, m, r), T);
    if (c.worst < best.worst) best = { ...c, r };
  }
  const lab = (centre ? `c+ring${m}` : `ring${m}`).padEnd(11);
  const em = 91 - 1 - 7 * N;
  console.log(` ${lab} ${String(N).padStart(4)}  ${best.r.toFixed(1).padStart(6)}mm  ${best.worst.toFixed(1).padStart(6)}mm ${best.p95.toFixed(1).padStart(6)}   ${String(em).padStart(2)}/90      ${best.worst <= 10 ? "YES" : "no"}`);
}
