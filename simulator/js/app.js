/**
 * NeurOne Helmet Simulator — main entry point.
 *
 * Wires together the Three.js scene, HelmetModel, SessionEngine, and UIManager.
 * Runs the RAF animation loop.
 */

import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

import { HelmetModel }  from './helmet.js';
import { SessionEngine } from './session.js';
import { UIManager }    from './ui.js';
import { PROTOCOLS, loadLibrary } from './vendor/npps-runtime.js';
import { DeviceAPI }    from './api.js';
import { SIM_VERSION }  from './version.js';

// ─────────────────────────────────────────────────────────────────────────────

class NeurOneSimulator {
  constructor() {
    this._canvas = document.getElementById('main-canvas');
    this._wrap   = document.getElementById('viewport-wrap');

    this._initRenderer();
    this._initScene();
    this._initHelmet();
    this._initSession();
    this._initUI();
    this._initAPI();
    this._setVersionBadge();

    window.addEventListener('resize', () => this._onResize());
    this._onResize();

    this._lastRaf = 0;
    this._prevRunning = false;
    this._raf();
  }

  // ─── Three.js setup ────────────────────────────────────────────────────────

  _initRenderer() {
    this._renderer = new THREE.WebGLRenderer({
      canvas:    this._canvas,
      antialias: true,
      alpha:     false,
    });
    this._renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this._renderer.shadowMap.enabled = true;
    this._renderer.shadowMap.type    = THREE.PCFSoftShadowMap;
    this._renderer.toneMapping        = THREE.ACESFilmicToneMapping;
    this._renderer.toneMappingExposure = 1.1;
    this._renderer.outputColorSpace    = THREE.SRGBColorSpace;
  }

  _initScene() {
    this._scene = new THREE.Scene();
    this._scene.background = new THREE.Color(0x050510);
    this._scene.fog = new THREE.FogExp2(0x050510, 0.06);

    // Camera
    this._camera = new THREE.PerspectiveCamera(42, 1, 0.05, 60);
    this._camera.position.set(0, 1.2, 4.5);
    this._camera.lookAt(0, 0.3, 0);

    // Orbit controls
    this._controls = new OrbitControls(this._camera, this._canvas);
    this._controls.target.set(0, 0.3, 0);
    this._controls.minDistance = 2.0;
    this._controls.maxDistance = 9.0;
    this._controls.maxPolarAngle = Math.PI * 0.88;
    this._controls.enableDamping = true;
    this._controls.dampingFactor = 0.08;
    this._controls.update();

    // Lights
    const ambient = new THREE.AmbientLight(0x0d1530, 3.0);
    this._scene.add(ambient);

    const keyLight = new THREE.DirectionalLight(0xffffff, 2.5);
    keyLight.position.set(3, 6, 4);
    keyLight.castShadow = true;
    keyLight.shadow.mapSize.set(1024, 1024);
    keyLight.shadow.camera.near = 0.5;
    keyLight.shadow.camera.far  = 20;
    keyLight.shadow.camera.left = keyLight.shadow.camera.bottom = -3;
    keyLight.shadow.camera.right = keyLight.shadow.camera.top   =  3;
    this._scene.add(keyLight);

    const rimLight = new THREE.DirectionalLight(0x2255ff, 0.9);
    rimLight.position.set(-4, 2, -3);
    this._scene.add(rimLight);

    const fillLight = new THREE.DirectionalLight(0x331155, 0.5);
    fillLight.position.set(0, -2, 3);
    this._scene.add(fillLight);

    // Subtle environment (fake IBL via hemisphere)
    const hemi = new THREE.HemisphereLight(0x111133, 0x080818, 0.8);
    this._scene.add(hemi);

    // Floor plane (receives shadows, reflects scene)
    const floorG = new THREE.CircleGeometry(3.5, 64);
    const floorM = new THREE.MeshStandardMaterial({
      color: 0x07081a, roughness: 0.9, metalness: 0.1,
    });
    const floor = new THREE.Mesh(floorG, floorM);
    floor.rotation.x = -Math.PI / 2;
    floor.position.y = -1.25;
    floor.receiveShadow = true;
    this._scene.add(floor);

    // Grid overlay on floor
    const grid = new THREE.GridHelper(8, 24, 0x111133, 0x0d0e28);
    grid.position.y = -1.24;
    this._scene.add(grid);

    // Subtle glow plane under helmet (bounce light)
    const glowG = new THREE.PlaneGeometry(2.5, 2.5);
    const glowM = new THREE.MeshStandardMaterial({
      color: 0x001133, transparent: true, opacity: 0.15, depthWrite: false,
    });
    const glow = new THREE.Mesh(glowG, glowM);
    glow.rotation.x = -Math.PI / 2;
    glow.position.y = -1.23;
    this._scene.add(glow);
    this._bouncePlane = { mesh: glow, material: glowM };
  }

  _initHelmet() {
    this._helmet = new HelmetModel(this._scene);
  }

  _initSession() {
    this._session = new SessionEngine();
  }

  _initAPI() {
    this._api = new DeviceAPI({
      onSessionStart: (descriptor) => {
        const id = descriptor.protocolId;
        if (id && PROTOCOLS[id]) {
          this._session.load(id);
          this._ui.populateTimeline(PROTOCOLS[id]);
          this._session.start();
        }
        // Apply per-zone overrides if the descriptor includes them
        if (Array.isArray(descriptor.zones)) {
          descriptor.zones.forEach(({ zoneId, config }) => {
            if (zoneId && config) this._helmet.configureZone(zoneId, config);
          });
        }
      },
      onSessionPause: () => {
        this._session.pause();
      },
      onSessionStop: () => {
        this._session.stop();
      },
      onZoneConfig: (zoneId, config) => {
        this._helmet.configureZone(zoneId, config);
      },
      onAccessoryChange: (name, visible) => {
        this._helmet.setAccessory(name, visible);
      },
    });
  }

  _initUI() {
    this._ui = new UIManager({
      onZoneChange: (id, cfg) => {
        this._helmet.configureZone(id, cfg);
      },
      onAccessoryChange: (name, visible) => {
        this._helmet.setAccessory(name, visible);
      },
      onProtocolSelect: (id) => {
        this._session.load(id);
        const p = PROTOCOLS[id];
        if (p) this._ui.populateTimeline(p);
      },
      onStart: () => {
        if (this._session.start()) {
          // nop — session runs on tick
        }
      },
      onPause: () => {
        this._session.pause();
      },
      onStop: () => {
        this._session.stop();
      },
      onViewChange: (view) => {
        this._setView(view);
      },
      onDisplayModeChange: (mode) => {
        this._helmet.setDisplayMode(mode);
      },
    });
  }

  _setVersionBadge() {
    const el = document.getElementById('ver-badge');
    if (el) el.textContent = `v${SIM_VERSION}`;
  }

  // ─── camera presets ────────────────────────────────────────────────────────

  _setView(view) {
    const presets = {
      front: new THREE.Vector3(0,   1.2,  4.5),
      side:  new THREE.Vector3(4.5, 1.2,  0),
      top:   new THREE.Vector3(0,   5.0,  0.001),
      // PBM modules are on the concave INNER (scalp-facing) surface and emit
      // inward (NP-HEX-ZM-001 §3.4) — from outside, the opaque CFRP shell
      // hides them entirely, same as the real hardware. The shell is an open
      // bowl (no bottom cap, see _buildShell's polar extent), so looking up
      // through that opening from below is the only angle that shows the
      // modules lit.
      inside: new THREE.Vector3(0, -0.85, 0.55),
      orbit: new THREE.Vector3(2.8, 2.2,  3.2),
    };
    const lookAt = {
      inside: new THREE.Vector3(0, 0.15, -0.1),
    };
    const pos = presets[view];
    if (!pos) return;

    // Simple lerp-to animation via flags (handled in _raf)
    this._viewTarget = pos.clone();
    this._lookAtTarget = (lookAt[view] ?? new THREE.Vector3(0, 0.3, 0)).clone();
    this._controls.enabled = (view === 'orbit');
  }

  // ─── animation loop ────────────────────────────────────────────────────────

  _raf() {
    requestAnimationFrame(t => {
      const dt = (t - this._lastRaf) / 1000;
      this._lastRaf = t;
      this._tick(t / 1000, dt);
      this._raf();
    });
  }

  _tick(wallTime, dt) {
    // Advance session
    this._session.tick();
    const snap = this._session.snapshot(wallTime);

    // WebSocket device API — telemetry and session-complete events
    this._api.maybeSendTelemetry(snap, wallTime);
    if (this._prevRunning && !snap.running && this._session.protocol) {
      this._api.sendSessionComplete(
        this._buildUhdrSummary(snap),
        this._buildShdrSummary(snap)
      );
    }
    this._prevRunning = snap.running;

    // Camera fly-to
    if (this._viewTarget) {
      this._camera.position.lerp(this._viewTarget, 0.05);
      if (this._camera.position.distanceTo(this._viewTarget) < 0.01) {
        this._viewTarget = null;
      }
    }
    if (this._lookAtTarget) {
      this._controls.target.lerp(this._lookAtTarget, 0.05);
      if (this._controls.target.distanceTo(this._lookAtTarget) < 0.01) {
        this._lookAtTarget = null;
      }
    }

    // Bounce plane color reflects active modality
    if (this._bouncePlane) {
      const col = snap.pbm.active ? 0x221100 : (snap.eeg.active ? 0x001122 : 0x001133);
      this._bouncePlane.material.color.setHex(col);
    }

    // Update 3D helmet
    this._helmet.update(wallTime, snap);

    // Update UI panels
    this._ui.updateFromSnapshot(snap);

    // Controls damping
    this._controls.update();

    // Render
    this._renderer.render(this._scene, this._camera);
  }

  _buildUhdrSummary(snap) {
    return {
      protocol:            this._session.protocol?.id ?? null,
      duration_s:          Math.round(snap.elapsed),
      pbm_dose_jcm2:       snap.pbm.dose,
      eeg_nf_score_final:  +snap.eeg.neurofeedbackScore.toFixed(2),
      hrv_rmssd_final_ms:  +snap.vns.rmssd.toFixed(1),
      hrv_coherence_final: +snap.vns.coherence.toFixed(2),
    };
  }

  _buildShdrSummary(snap) {
    const active = [];
    for (const [key, val] of Object.entries(snap)) {
      if (val && typeof val === 'object' && val.active === true) active.push(key);
    }
    return {
      protocol_id:       this._session.protocol?.id ?? null,
      session_count_delta: 1,
      modalities_active: active,
      sim_version:       SIM_VERSION,
    };
  }

  _onResize() {
    const w = this._wrap.clientWidth;
    const h = this._wrap.clientHeight;
    this._camera.aspect = w / h;
    this._camera.updateProjectionMatrix();
    this._renderer.setSize(w, h, false);
  }
}

// ─────────────────────────────────────────────────────────────────────────────

// The protocol library is fetched and parsed HERE, at run time, from
// protocols/predefined/ — never baked into a generated module
// (NP-NPPS-REF-001 §1.6). Everything downstream reads PROTOCOLS and ZONES
// through ES live bindings, so nothing may be constructed until this resolves.
//
// This is why the simulator must be served over HTTP: fetch() is blocked at a
// file:// origin. See HOWTO §3.
window.addEventListener('DOMContentLoaded', async () => {
  try {
    await loadLibrary();
  } catch (err) {
    console.error('[NP-SIM] could not load protocols/predefined/', err);
    const banner = document.createElement('div');
    banner.style.cssText =
      'position:fixed;inset:0;display:flex;align-items:center;justify-content:center;' +
      'padding:2rem;background:#1a1014;color:#ffd9d9;font:14px/1.6 system-ui;' +
      'text-align:center;z-index:9999';
    banner.innerHTML =
      '<div><strong>Could not load the protocol library.</strong><br><br>' +
      'The simulator reads <code>protocols/predefined/</code> over HTTP and cannot ' +
      'run from a <code>file://</code> URL.<br>Serve the repository root and open ' +
      '<code>/simulator/</code> — see HOWTO §3.<br><br>' +
      `<small>${String(err)}</small></div>`;
    document.body.appendChild(banner);
    return;
  }
  window.__sim = new NeurOneSimulator(); // debug handle — inspect from devtools/console
});
