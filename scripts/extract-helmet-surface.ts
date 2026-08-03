/**
 * Extract the helmet interior surface from the LiDAR scan.
 *
 * STAGE 1 OF 2 — segmentation and frame recovery. This runs and its results are
 * cross-checked below. It does NOT yet emit socket coordinates; see "What still
 * blocks stage 2" at the bottom.
 *
 * Principal direction 2026-07-31: the ellipsoid in sync-socket-map.ts is a
 * convenient DESCRIPTION, not a fact. cad/HelmetScan.3dm is the reference of
 * record until exact CAD exists. This script is the first half of moving onto it.
 *
 * ── What the scan actually is ────────────────────────────────────────────────
 *
 * A raw Scaniverse capture in metres: 98,003 vertices, 181,944 faces, one mesh
 * named "mesh", including the surroundings it was captured with. It is NOT a
 * segmented helmet. Recovering one takes three steps, in this order:
 *
 *   1. Strip the three dominant planes (table and walls). The first alone is
 *      44,713 points — nearly half the scan.
 *   2. Grid flood-fill the remainder; the helmet is then the largest cluster by
 *      a wide margin (41,884 points; the next is 31).
 *   3. Fit a sphere to that cluster. The bowl axis is centre -> centroid, which
 *      is a far better estimator than mean-unit-vector or PCA: a bowl's points
 *      all lie on one side of its own centre, and PCA finds the footprint plane
 *      rather than the axis.
 *
 * ── Cross-checks (all independent of the numbers they are checked against) ───
 *
 *   sphere fit R          118.7 mm
 *   rim -> crown span     ~162 mm    vs 157 mm documented (NP-HEX-ZM-001 §3.4)
 *   rim span, long axis   277 mm     vs 267 mm documented interior length
 *   rim span, lateral     180 mm     vs 229 mm documented interior breadth  <-- SEE BELOW
 *
 * Two of three agree closely, which is what makes the frame trustworthy. The
 * third does not, and the reason matters.
 *
 * ── What still blocks stage 2 ────────────────────────────────────────────────
 *
 * (a) THE SCAN HAS HOLES EXACTLY WHERE THE LATERAL SOCKETS ARE. Binning the
 *     lower shell by azimuth gives ~800-1800 points per 15° bin around the front
 *     and back, but n=98 at -90° and n=145 at +90° — the two ear-adjacent
 *     regions are all but unscanned (self-occlusion; the helmet was captured
 *     sitting inverted). Socket coordinates there would be interpolated across a
 *     ~40° gap and presented as measured. That is worse than an honest ellipsoid,
 *     and it is also the region the active-surface boundary (ACT-1) is decided
 *     in, so it is not a region to guess in.
 *
 * (b) FRONT vs BACK IS STILL UNRESOLVED. The agreed method was to find the Boa
 *     dial, which sits at the rear. It is not detectable here: the azimuthal
 *     radial profile is near-symmetric (median in-plane radius 124 mm at 0°
 *     against 125-128 mm at ±180°) with no protrusion at either end. That is
 *     consistent with this being the INTERIOR capture, which is what it was
 *     confirmed to be — the dial is an exterior feature and simply is not in this
 *     surface. So the dial cannot disambiguate an interior scan, and the long
 *     axis remains a line without a direction. Reversing it mirrors every socket
 *     front to back: a wrong-site error.
 *
 * Neither is solvable by being more clever with this file. (a) needs a rescan
 * with lateral coverage, or an explicit decision to interpolate the ear regions.
 * (b) needs the exterior scan registered to this one, a fiducial, or someone to
 * state the forward direction.
 */

import rhino3dm from "rhino3dm";
import { readFileSync, writeFileSync } from "fs";
const rh = await rhino3dm();
const doc = rh.File3dm.fromByteArray(new Uint8Array(readFileSync("cad/HelmetScan.3dm")));
const m: any = doc.objects().get(0).geometry();
const vs = m.vertices(); const n = vs.count;
let P: number[][] = new Array(n);
for (let i=0;i<n;i++){ const v = vs.get(i) as unknown as number[]; P[i]=[v[0]*1000,v[1]*1000,v[2]*1000]; }
const sub=(a:number[],b:number[])=>[a[0]-b[0],a[1]-b[1],a[2]-b[2]];
const cross=(a:number[],b:number[])=>[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]];
const dot=(a:number[],b:number[])=>a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
const norm=(a:number[])=>{const l=Math.hypot(...a);return a.map(v=>v/l);};

for (let round=0; round<3; round++){
  let best={cnt:0,nrm:[0,0,1],d:0};
  for(let it=0;it<3000;it++){
    const p=()=>P[(Math.random()*P.length)|0];
    const a=p(),b=p(),c=p();
    let nr=cross(sub(b,a),sub(c,a)); const l=Math.hypot(...nr); if(l<1e-6)continue;
    nr=nr.map(v=>v/l); const d=dot(nr,a);
    let cnt=0; for(let k=0;k<P.length;k+=5) if(Math.abs(dot(nr,P[k])-d)<8) cnt++;
    if(cnt>best.cnt) best={cnt:cnt*5,nrm:nr,d};
  }
  P = P.filter(p=>Math.abs(dot(best.nrm,p)-best.d)>=8);
}
const CELL=10;
const key=(p:number[])=>`${Math.floor(p[0]/CELL)},${Math.floor(p[1]/CELL)},${Math.floor(p[2]/CELL)}`;
const cells=new Map<string,number[]>();
P.forEach((p,i)=>{const k=key(p); if(!cells.has(k))cells.set(k,[]); cells.get(k)!.push(i);});
const seen=new Set<string>(); let best:number[]=[];
for(const k0 of cells.keys()){ if(seen.has(k0))continue;
  const stack=[k0]; seen.add(k0); const acc:number[]=[];
  while(stack.length){ const k=stack.pop()!; acc.push(...cells.get(k)!);
    const [a,b,c]=k.split(",").map(Number);
    for(let dx=-1;dx<=1;dx++)for(let dy=-1;dy<=1;dy++)for(let dz=-1;dz<=1;dz++){
      const nk=`${a+dx},${b+dy},${c+dz}`; if(cells.has(nk)&&!seen.has(nk)){seen.add(nk);stack.push(nk);} } }
  if(acc.length>best.length)best=acc; }
const H = best.map(i=>P[i]);
console.log("helmet points:", H.length);

// Bowl axis: PCA smallest-variance is NOT it; use the one-sidedness of unit vectors.
const C0=[0,1,2].map(k=>H.reduce((s,p)=>s+p[k],0)/H.length);
const mean=[0,0,0];
for(const p of H){const d=sub(p,C0);const l=Math.hypot(...d);for(let k=0;k<3;k++)mean[k]+=d[k]/l/H.length;}
const up = norm(mean);   // centroid -> crown
console.log("up (centroid->crown) =", up.map(v=>v.toFixed(3)).join(", "));

// Height along axis; rim = low end.
const h = H.map(p=>dot(sub(p,C0),up));
const hs=[...h].sort((a,b)=>a-b);
const hRim=hs[Math.floor(0.01*hs.length)], hCrown=hs[Math.floor(0.995*hs.length)];
console.log(`rim h=${hRim.toFixed(1)}  crown h=${hCrown.toFixed(1)}  span=${(hCrown-hRim).toFixed(1)} mm`);

// Origin = centre of the rim plane, on the bowl axis.
const rimPts = H.filter((_,i)=>h[i] < hRim+12);
const Crim=[0,1,2].map(k=>rimPts.reduce((s,p)=>s+p[k],0)/rimPts.length);
const ORIGIN = Crim.map((v,k)=>v - up[k]*dot(sub(Crim,Crim),up));
console.log("rim centre:", ORIGIN.map(v=>v.toFixed(1)).join(", "), " rim pts:", rimPts.length);

// In-plane axes: PCA of rim points projected into the rim plane -> long axis = fore/aft.
const proj=(p:number[])=>{const d=sub(p,ORIGIN); const a=dot(d,up); return sub(d,up.map(v=>v*a));};
const R2=rimPts.map(proj);
const cov=[[0,0],[0,0]];
let e1=norm(sub(R2[0],[0,0,0])); // seed
// build orthonormal in-plane basis
let tmp = Math.abs(up[0])<0.9 ? [1,0,0] : [0,1,0];
let ax = norm(cross(up,tmp)); let ay = norm(cross(up,ax));
for(const q of R2){ const a=dot(q,ax), b=dot(q,ay);
  cov[0][0]+=a*a; cov[0][1]+=a*b; cov[1][0]+=a*b; cov[1][1]+=b*b; }
const th=0.5*Math.atan2(2*cov[0][1], cov[0][0]-cov[1][1]);
const long = norm(ax.map((v,k)=>v*Math.cos(th)+ay[k]*Math.sin(th)));
const shortA = norm(cross(up,long));
const spanAlong=(v:number[])=>{const t=rimPts.map(p=>dot(sub(p,ORIGIN),v)).sort((a,b)=>a-b);
  return t[t.length-1]-t[0];};
console.log(`rim span along long axis = ${spanAlong(long).toFixed(0)} mm, short axis = ${spanAlong(shortA).toFixed(0)} mm`);

// Which end of the long axis is the DIAL? Look for material sticking out beyond the
// shell near the rim, on each end.
for (const sgn of [1,-1]){
  const dir = long.map(v=>v*sgn);
  const near = H.filter(p=>{ const d=sub(p,ORIGIN); return dot(d,dir) > 0.55*spanAlong(long)/2 && dot(d,up) < hRim+70-hRim; });
  const radial = near.map(p=>{const d=sub(p,ORIGIN); const a=dot(d,up); const q=sub(d,up.map(v=>v*a)); return Math.hypot(...q);});
  radial.sort((a,b)=>a-b);
  const p50=radial[Math.floor(0.5*radial.length)]??0, p99=radial[Math.floor(0.99*radial.length)]??0;
  console.log(`  long ${sgn>0?"+":"-"} end: n=${near.length} radial p50=${p50.toFixed(1)} p99=${p99.toFixed(1)} protrusion=${(p99-p50).toFixed(1)} mm`);
}
writeFileSync("/private/tmp/claude-502/-Users-stevehickman-Developer-GitHub-NeuroPulse--claude-worktrees-objective-wright-83508d/fd04e7da-1316-4e4a-9008-885b1a667993/scratchpad/helmet_pts.json",
  JSON.stringify({origin:ORIGIN, up, long, shortA, points:H}));
console.log("saved helmet_pts.json");
