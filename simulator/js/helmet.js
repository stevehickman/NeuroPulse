/**
 * HelmetModel — Three.js scene graph for the NeuroPulse helmet.
 *
 * Physical starting point: Neuronic 1070 Light (dome-style, full-scalp coverage).
 * NeuroPulse additions: 5 snap-in LED zones, integrated goggles, over-ear audio
 * cups, occipital hub + Boa dial, spring-decoupled EEG pods, VNS auricular clip.
 *
 * Coordinate frame:  +Y up,  +Z toward viewer (front of helmet),  +X right.
 * Head centre at origin; crown at Y ≈ +1.0.
 */

import * as THREE from 'three';
import { WL, IMPEDANCE_COLORS } from './colors.js';

// ── shell geometry parameters ────────────────────────────────────────────────
const SHELL_R    = 1.02;
const SHELL_SX   = 0.80;   // lateral compression (head is narrower than tall)
const SHELL_SY   = 1.02;   // slight vertical stretch
const SHELL_SZ   = 0.92;   // front-to-back slightly shallower

// ── zone positions in spherical coords (phi = azimuth from +Z, theta = polar from +Y) ─────
// NB: Three.js SphereGeometry theta is measured from +Y pole.
const ZONE_CONFIGS = [
  { id: 'ZM-01', name: 'Frontal',         phi: 0,              theta: 0.22 * Math.PI },
  { id: 'ZM-02', name: 'Left Temporal',   phi:  0.52 * Math.PI, theta: 0.38 * Math.PI },
  { id: 'ZM-03', name: 'Right Temporal',  phi: -0.52 * Math.PI, theta: 0.38 * Math.PI },
  { id: 'ZM-04', name: 'Left Parietal',   phi:  0.78 * Math.PI, theta: 0.44 * Math.PI },
  { id: 'ZM-05', name: 'Occipital',       phi:  Math.PI,         theta: 0.38 * Math.PI },
];

// ── EEG electrode positions ───────────────────────────────────────────────────
const EEG_POSITIONS = [
  { name: 'Fp1', phi:  0.28 * Math.PI, theta: 0.14 * Math.PI },
  { name: 'Fp2', phi: -0.28 * Math.PI, theta: 0.14 * Math.PI },
  { name: 'F3',  phi:  0.38 * Math.PI, theta: 0.28 * Math.PI },
  { name: 'F4',  phi: -0.38 * Math.PI, theta: 0.28 * Math.PI },
  { name: 'C3',  phi:  0.52 * Math.PI, theta: 0.38 * Math.PI },
  { name: 'C4',  phi: -0.52 * Math.PI, theta: 0.38 * Math.PI },
  { name: 'P3',  phi:  0.68 * Math.PI, theta: 0.46 * Math.PI },
  { name: 'P4',  phi: -0.68 * Math.PI, theta: 0.46 * Math.PI },
];

// ─────────────────────────────────────────────────────────────────────────────

export class HelmetModel {
  constructor(scene) {
    this.scene  = scene;
    this.group  = new THREE.Group();
    this.zones  = {};        // ZM-01 … ZM-05 → ZoneObject
    this.eeg    = {};        // 'Fp1' … 'P4' → EEGObject
    this.parts  = {};        // named mesh references for accessories

    // Canvas-based LED textures, one per zone
    this._ledCanvases = {};

    this._build();
    scene.add(this.group);
  }

  // ─── public ────────────────────────────────────────────────────────────────

  /** Reconfigure a zone (install/remove, wavelengths, frequency). */
  configureZone(id, { installed, wavelengths, frequency }) {
    const z = this.zones[id];
    if (!z) return;
    z.installed   = !!installed;
    z.wavelengths = wavelengths ?? [];
    z.frequency   = frequency ?? 10;

    const panelMat = z.panel.material;
    if (!z.installed) {
      panelMat.opacity = 0.12;
      panelMat.emissiveIntensity = 0;
      z.panel.visible = true;
    } else {
      panelMat.opacity = 0.92;
      const primaryWL = WL[z.wavelengths[0]];
      if (primaryWL) panelMat.emissive.setHex(primaryWL.threeHex);
    }
    this._drawLEDs(id, 0, 0);
  }

  /** Configure which accessories are present. */
  setAccessory(name, visible) {
    if (this.parts[name]) this.parts[name].visible = visible;
  }

  /** Called every animation frame with the current session snapshot. */
  update(wallTimeSec, snap) {
    const t = wallTimeSec;

    // ── PBM zone glow ──────────────────────────────────────────────────────
    ZONE_CONFIGS.forEach(cfg => {
      const z = this.zones[cfg.id];
      if (!z || !z.installed) return;

      const pbmActive  = snap.pbm.active && snap.pbm.zones.includes(cfg.id);
      const freq       = snap.pbm.frequency;
      const duty       = snap.pbm.dutyCycle;
      const rampI      = snap.pbm.intensity;

      let pulse = 0;
      if (pbmActive) {
        if (freq === 0) {
          // CW
          pulse = rampI;
        } else {
          // Pulsed — square wave based on wallTime
          const phase = (t * freq) % 1;
          pulse = phase < duty ? rampI : 0;
        }
      }

      z.panel.material.emissiveIntensity = pbmActive ? 0.15 + pulse * 1.1 : 0.04;
      this._drawLEDs(cfg.id, pulse, t);
    });

    // ── EEG electrodes ─────────────────────────────────────────────────────
    EEG_POSITIONS.forEach(pos => {
      const e = this.eeg[pos.name];
      if (!e) return;
      const impKey = snap.eeg.impedance[pos.name] ?? 'off';
      const col = IMPEDANCE_COLORS[snap.eeg.active ? impKey : 'off'];
      e.material.emissive.setHex(col.three);
      e.material.emissiveIntensity = snap.eeg.active ? 0.8 : 0;
    });

    // ── Visual goggles ─────────────────────────────────────────────────────
    if (this.parts.goggle_left_lens && this.parts.goggle_right_lens) {
      const vis = snap.visual;
      let vPulse = 0;
      if (vis.active) {
        const vPhase = (t * vis.frequency) % 1;
        vPulse = vPhase < 0.5 ? 1 : 0;
      }
      [this.parts.goggle_left_lens, this.parts.goggle_right_lens].forEach(mat => {
        mat.emissive.setHex(vPulse ? 0xFF2200 : 0x000000);
        mat.emissiveIntensity = vPulse * 0.9;
      });
    }

    // ── Hub status LEDs ────────────────────────────────────────────────────
    if (this.parts.hubLedPower) {
      const breathe = 0.4 + 0.4 * Math.sin(t * 1.5);
      this.parts.hubLedPower.material.emissiveIntensity = snap.running ? 0.9 : breathe;
    }
    if (this.parts.hubLedSession) {
      // Pulse at session frequency / 4 for visible indication across room
      const sessionFreq = snap.pbm.frequency || 1;
      const sPhase = (t * (sessionFreq / 4)) % 1;
      this.parts.hubLedSession.material.emissiveIntensity = snap.running
        ? (sPhase < 0.5 ? 0.9 : 0.1)
        : 0;
    }

    // ── VNS clip (auricular) ───────────────────────────────────────────────
    if (this.parts.vnsClip) {
      this.parts.vnsClip.visible = snap.vns.active;
      if (snap.vns.active && this.parts.vnsClipMat) {
        const vnsPhase = (t * snap.vns.frequency) % 1;
        const vnsPulse = vnsPhase < 0.5 ? 0.8 : 0.1;
        this.parts.vnsClipMat.emissiveIntensity = vnsPulse;
      }
    }

    // ── Mastoid haptic pad ─────────────────────────────────────────────────
    if (this.parts.hapticPad) {
      if (snap.pbm?.active && this.parts.hapticPadMat) {
        // Vibrate at 40 Hz visually
        const hPhase = (t * 40) % 1;
        const hPulse = hPhase < 0.5 ? 1 : 0;
        this.parts.hapticPadMat.emissiveIntensity = 0.05 + hPulse * 0.4;
      }
    }
  }

  /**
   * Returns an array of { id, name, x, y, z } (world coords) for zone label
   * placement by the HTML overlay system.
   */
  getZoneLabelPositions(camera, renderer) {
    const positions = [];
    ZONE_CONFIGS.forEach(cfg => {
      const z = this.zones[cfg.id];
      if (!z) return;
      const worldPos = new THREE.Vector3();
      z.panel.getWorldPosition(worldPos);

      // Project to NDC then to screen
      const ndc = worldPos.clone().project(camera);
      const w = renderer.domElement.clientWidth;
      const h = renderer.domElement.clientHeight;
      positions.push({
        id: cfg.id,
        name: cfg.name,
        installed: z.installed,
        screenX: ((ndc.x + 1) / 2) * w,
        screenY: ((-ndc.y + 1) / 2) * h,
        visible: ndc.z < 1, // behind camera test (rough)
      });
    });
    return positions;
  }

  // ─── build helpers ─────────────────────────────────────────────────────────

  _build() {
    this._buildHead();
    this._buildShell();
    this._buildZones();
    this._buildEEGPods();
    this._buildGoggles();
    this._buildEarCups();
    this._buildHub();
    this._buildOccipitalArch();
    this._buildVNSClip();
    this._buildMastoidPad();
    this._buildIntranasal();
  }

  _buildHead() {
    // Semi-transparent mannequin head for anatomical context
    const geo = new THREE.SphereGeometry(0.95, 32, 32);
    const mat = new THREE.MeshStandardMaterial({
      color: 0xd4b896, roughness: 0.9, metalness: 0,
      transparent: true, opacity: 0.12, depthWrite: false,
    });
    const head = new THREE.Mesh(geo, mat);
    head.scale.set(0.78, 1.0, 0.88);
    head.position.set(0, -0.04, 0);
    this.group.add(head);

    // Neck
    const neckG = new THREE.CylinderGeometry(0.27, 0.30, 0.55, 16);
    const neck  = new THREE.Mesh(neckG, mat.clone());
    neck.position.set(0, -1.03, 0);
    this.group.add(neck);
  }

  _buildShell() {
    // Main CFRP dome — upper ~62% of ellipsoid
    const geo = new THREE.SphereGeometry(
      SHELL_R, 64, 32,
      0, Math.PI * 2,   // full azimuth
      0, Math.PI * 0.62 // polar extent (top to just below equator)
    );
    const mat = new THREE.MeshStandardMaterial({
      color: 0x111122,
      roughness: 0.28,
      metalness: 0.55,
      side: THREE.FrontSide,
      envMapIntensity: 1.0,
    });
    const shell = new THREE.Mesh(geo, mat);
    shell.scale.set(SHELL_SX, SHELL_SY, SHELL_SZ);
    shell.castShadow    = true;
    shell.receiveShadow = true;
    this.group.add(shell);
    this.parts.shell = shell;

    // Bottom rim ring
    const rimG = new THREE.TorusGeometry(SHELL_R * 0.98, 0.018, 8, 64);
    const rimM = new THREE.MeshStandardMaterial({ color: 0x2a3050, roughness: 0.5, metalness: 0.7 });
    const rim  = new THREE.Mesh(rimG, rimM);
    rim.rotation.x = Math.PI / 2;
    rim.scale.set(SHELL_SX * 0.99, 1, SHELL_SZ * 0.99);
    rim.position.y = -0.14;
    this.group.add(rim);
  }

  _buildZones() {
    ZONE_CONFIGS.forEach(cfg => {
      const { x, y, z } = this._sph(SHELL_R * 1.002, cfg.phi, cfg.theta);

      const panelGroup = new THREE.Group();
      panelGroup.position.set(x, y, z);
      // Orient outward from sphere centre
      panelGroup.lookAt(0, 0, 0);
      panelGroup.rotateY(Math.PI);

      // Backing panel (PCB-like dark rectangle)
      const backG = new THREE.PlaneGeometry(0.34, 0.34);
      const backM = new THREE.MeshStandardMaterial({ color: 0x080812, roughness: 0.8, metalness: 0.2 });
      const back  = new THREE.Mesh(backG, backM);
      back.position.z = -0.001;
      panelGroup.add(back);

      // LED texture panel
      const c = this._createLEDCanvas(cfg.id);
      const panelG = new THREE.PlaneGeometry(0.33, 0.33);
      const panelM = new THREE.MeshStandardMaterial({
        map: c.texture,
        emissiveMap: c.texture,
        emissive: new THREE.Color(0xFF2200),
        emissiveIntensity: 0,
        transparent: true,
        opacity: 0.12,
        depthWrite: false,
      });
      const panel = new THREE.Mesh(panelG, panelM);
      panelGroup.add(panel);

      // Thin border (zone ID indicator)
      const edgeG = new THREE.EdgesGeometry(new THREE.PlaneGeometry(0.35, 0.35));
      const edgeM = new THREE.LineBasicMaterial({ color: 0x223344, transparent: true, opacity: 0.6 });
      const edge  = new THREE.LineSegments(edgeG, edgeM);
      edge.position.z = 0.001;
      panelGroup.add(edge);

      this.group.add(panelGroup);

      this.zones[cfg.id] = {
        group: panelGroup,
        panel,
        installed: false,
        wavelengths: [],
        frequency: 10,
      };
    });
  }

  _buildEEGPods() {
    EEG_POSITIONS.forEach(pos => {
      const r = SHELL_R * 1.01;
      const { x, y, z } = this._sph(r, pos.phi, pos.theta);

      // Pod sphere
      const geo = new THREE.SphereGeometry(0.026, 8, 8);
      const mat = new THREE.MeshStandardMaterial({
        color: 0x445566, roughness: 0.4, metalness: 0.5,
        emissive: new THREE.Color(0x00ff44), emissiveIntensity: 0,
      });
      const pod = new THREE.Mesh(geo, mat);
      pod.position.set(x, y, z);
      this.group.add(pod);

      // Spring arm (thin cylinder)
      const springLen = 0.035;
      const springG = new THREE.CylinderGeometry(0.005, 0.005, springLen, 6);
      const springM = new THREE.MeshStandardMaterial({ color: 0x667788, roughness: 0.6, metalness: 0.4 });
      const spring  = new THREE.Mesh(springG, springM);

      // Orient spring toward head centre
      const dir = new THREE.Vector3(x, y, z).normalize();
      spring.position.set(
        x - dir.x * (springLen / 2),
        y - dir.y * (springLen / 2),
        z - dir.z * (springLen / 2)
      );
      spring.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), dir);
      this.group.add(spring);

      this.eeg[pos.name] = { mesh: pod, material: mat };
    });
  }

  _buildGoggles() {
    const gogGroup = new THREE.Group();
    // Position at front-upper of helmet, tilted slightly forward
    gogGroup.position.set(0, 0.05, SHELL_R * SHELL_SZ * 0.97);
    gogGroup.rotation.x = 0.12;

    const frameMat = new THREE.MeshStandardMaterial({ color: 0x1a1a30, roughness: 0.35, metalness: 0.65 });

    // Bridge
    const bridgeG = new THREE.BoxGeometry(0.12, 0.036, 0.032);
    gogGroup.add(new THREE.Mesh(bridgeG, frameMat));

    // Arm rails (connect to shell)
    [-0.23, 0.23].forEach(x => {
      const armG = new THREE.BoxGeometry(0.10, 0.016, 0.016);
      const arm  = new THREE.Mesh(armG, frameMat);
      arm.position.set(x, 0, -0.04);
      gogGroup.add(arm);
    });

    // Lenses — left and right
    [{ x: -0.22, key: 'left' }, { x: 0.22, key: 'right' }].forEach(({ x, key }) => {
      // Torus frame
      const torusG = new THREE.TorusGeometry(0.095, 0.014, 8, 40);
      const torus  = new THREE.Mesh(torusG, frameMat);
      torus.position.x = x;
      gogGroup.add(torus);

      // LED lens (108 micro-LEDs per lens — shown as emissive disc)
      const lensG = new THREE.CircleGeometry(0.082, 40);
      const lensMat = new THREE.MeshStandardMaterial({
        color: 0x001122, roughness: 0.05, metalness: 0,
        transparent: true, opacity: 0.72,
        emissive: new THREE.Color(0x000000), emissiveIntensity: 0,
      });
      const lens = new THREE.Mesh(lensG, lensMat);
      lens.position.set(x, 0, 0.014);
      gogGroup.add(lens);

      // Shade slot (bottom edge)
      const slotG = new THREE.BoxGeometry(0.20, 0.008, 0.004);
      const slotM = new THREE.MeshStandardMaterial({ color: 0x334455 });
      const slot  = new THREE.Mesh(slotG, slotM);
      slot.position.set(x, -0.11, 0);
      gogGroup.add(slot);

      this.parts[`goggle_${key}_lens`] = lensMat;
    });

    // IR proximity sensor dots (2 per lens)
    for (const x of [-0.22, 0.22]) {
      for (const ox of [-0.07, 0.07]) {
        const irG = new THREE.SphereGeometry(0.007, 6, 6);
        const irM = new THREE.MeshStandardMaterial({ color: 0x330011, emissive: 0x220000, emissiveIntensity: 0.3 });
        const ir  = new THREE.Mesh(irG, irM);
        ir.position.set(x + ox, 0.07, 0.014);
        gogGroup.add(ir);
      }
    }

    // Hall sensor pill (top centre, for shade detection)
    const hallG = new THREE.BoxGeometry(0.016, 0.008, 0.006);
    const hallM = new THREE.MeshStandardMaterial({ color: 0x334455 });
    gogGroup.add(new THREE.Mesh(hallG, hallM));

    this.group.add(gogGroup);
    this.parts.goggles = gogGroup;
  }

  _buildEarCups() {
    [{ side: -1, key: 'left' }, { side: 1, key: 'right' }].forEach(({ side, key }) => {
      const cupGroup = new THREE.Group();
      cupGroup.position.set(side * SHELL_R * SHELL_SX * 0.985, 0.15, 0.06);

      // Cup outer disc
      const cupG = new THREE.CylinderGeometry(0.118, 0.115, 0.06, 40);
      const cupM = new THREE.MeshStandardMaterial({ color: 0x111126, roughness: 0.3, metalness: 0.55 });
      const cup  = new THREE.Mesh(cupG, cupM);
      cup.rotation.z = Math.PI / 2;
      cupGroup.add(cup);

      // Planar magnetic driver indicator (concentric rings)
      for (const r of [0.095, 0.07, 0.045]) {
        const grillG = new THREE.TorusGeometry(r, 0.004, 5, 32);
        const grillM = new THREE.MeshStandardMaterial({ color: 0x334455, roughness: 0.8, metalness: 0.3 });
        const grill  = new THREE.Mesh(grillG, grillM);
        grill.rotation.y = Math.PI / 2;
        grill.position.x = side * 0.028;
        cupGroup.add(grill);
      }

      // Mesh frame (user-replaceable silver-coated nylon)
      const meshG = new THREE.CircleGeometry(0.108, 40);
      const meshM = new THREE.MeshStandardMaterial({
        color: 0x667788, roughness: 0.9, metalness: 0.05,
        transparent: true, opacity: 0.55, wireframe: true,
      });
      const mesh = new THREE.Mesh(meshG, meshM);
      mesh.rotation.y = side < 0 ? Math.PI / 2 : -Math.PI / 2;
      mesh.position.x = side * 0.031;
      cupGroup.add(mesh);

      // Bone conduction piezo at mastoid
      const piezoG = new THREE.CylinderGeometry(0.018, 0.018, 0.010, 16);
      const piezoM = new THREE.MeshStandardMaterial({ color: 0x445566, roughness: 0.5, metalness: 0.6 });
      const piezo  = new THREE.Mesh(piezoG, piezoM);
      piezo.rotation.z = Math.PI / 2;
      piezo.position.set(side * 0.032, -0.09, -0.04);
      cupGroup.add(piezo);

      this.group.add(cupGroup);
      this.parts[`ear_${key}`] = cupGroup;
    });
  }

  _buildHub() {
    // Control hub sits at back-lower area of the helmet
    const hubGroup = new THREE.Group();
    hubGroup.position.set(0, -0.10, -SHELL_R * SHELL_SZ * 0.93);
    hubGroup.rotation.x = -0.18;

    // Hub body
    const bodyG = new THREE.BoxGeometry(0.24, 0.17, 0.07);
    const bodyM = new THREE.MeshStandardMaterial({
      color: 0x090916, roughness: 0.38, metalness: 0.72,
    });
    const body = new THREE.Mesh(bodyG, bodyM);
    hubGroup.add(body);

    // Logo recess
    const logoG = new THREE.BoxGeometry(0.10, 0.028, 0.002);
    const logoM = new THREE.MeshStandardMaterial({ color: 0x1a2040 });
    const logo  = new THREE.Mesh(logoG, logoM);
    logo.position.set(0, 0.02, 0.036);
    hubGroup.add(logo);

    // Power LED (left temple = green, breathes at idle)
    const pwrG = new THREE.SphereGeometry(0.007, 8, 8);
    const pwrM = new THREE.MeshStandardMaterial({ color: 0x00ff44, emissive: 0x00ff44, emissiveIntensity: 0.6 });
    const pwr  = new THREE.Mesh(pwrG, pwrM);
    pwr.position.set(-0.085, 0.055, 0.036);
    hubGroup.add(pwr);
    this.parts.hubLedPower = pwr;

    // Session LED (right temple = amber)
    const sesG = new THREE.SphereGeometry(0.007, 8, 8);
    const sesM = new THREE.MeshStandardMaterial({ color: 0xff8800, emissive: 0xff8800, emissiveIntensity: 0 });
    const ses  = new THREE.Mesh(sesG, sesM);
    ses.position.set(0.085, 0.055, 0.036);
    hubGroup.add(ses);
    this.parts.hubLedSession = ses;

    // USB-C port
    const usbG = new THREE.BoxGeometry(0.026, 0.013, 0.010);
    const usbM = new THREE.MeshStandardMaterial({ color: 0x2a3a55 });
    const usb  = new THREE.Mesh(usbG, usbM);
    usb.position.set(0, -0.055, 0.036);
    hubGroup.add(usb);

    // Accessory port (3.5 mm audio-style)
    const portG = new THREE.CylinderGeometry(0.007, 0.007, 0.015, 12);
    const portM = new THREE.MeshStandardMaterial({ color: 0x334455 });
    const port  = new THREE.Mesh(portG, portM);
    port.rotation.z = Math.PI / 2;
    port.position.set(0.09, -0.04, 0.036);
    hubGroup.add(port);

    // Air filter vent (hub)
    for (let i = -1; i <= 1; i++) {
      const ventG = new THREE.BoxGeometry(0.004, 0.05, 0.005);
      const ventM = new THREE.MeshStandardMaterial({ color: 0x1a2030 });
      const vent  = new THREE.Mesh(ventG, ventM);
      vent.position.set(i * 0.008, 0, -0.037);
      hubGroup.add(vent);
    }

    this.group.add(hubGroup);
    this.parts.hub = hubGroup;
  }

  _buildOccipitalArch() {
    // Boa-style occipital arch — curved bar from left to right at back-lower
    const curve = new THREE.QuadraticBezierCurve3(
      new THREE.Vector3(-0.64, -0.22, -0.52),
      new THREE.Vector3( 0,    -0.30, -0.80),
      new THREE.Vector3( 0.64, -0.22, -0.52)
    );
    const pts  = curve.getPoints(32);
    const path = new THREE.CatmullRomCurve3(pts);
    const geo  = new THREE.TubeGeometry(path, 40, 0.012, 6, false);
    const mat  = new THREE.MeshStandardMaterial({ color: 0x2a3a55, roughness: 0.4, metalness: 0.6 });
    const tube = new THREE.Mesh(geo, mat);
    this.group.add(tube);

    // Boa dial at centre of arch
    const dialG = new THREE.CylinderGeometry(0.045, 0.040, 0.022, 24);
    const dialM = new THREE.MeshStandardMaterial({ color: 0x3a5070, roughness: 0.3, metalness: 0.7 });
    const dial  = new THREE.Mesh(dialG, dialM);
    dial.rotation.x = Math.PI / 2;
    dial.position.set(0, -0.30, -0.82);
    this.group.add(dial);

    // Dial ridges
    for (let i = 0; i < 12; i++) {
      const a = (i / 12) * Math.PI * 2;
      const ridgeG = new THREE.BoxGeometry(0.004, 0.024, 0.006);
      const ridgeM = new THREE.MeshStandardMaterial({ color: 0x2a4060 });
      const ridge  = new THREE.Mesh(ridgeG, ridgeM);
      ridge.rotation.z = a;
      ridge.position.set(
        Math.cos(a) * 0.042,
        -0.30,
        -0.82 + Math.sin(a) * 0.001
      );
      // orient along arch axis
      ridge.rotation.x = Math.PI / 2;
      ridge.rotation.z = a;
      this.group.add(ridge);
    }
  }

  _buildVNSClip() {
    // Auricular VNS clip — attaches to right ear
    const clipGroup = new THREE.Group();
    clipGroup.position.set(SHELL_R * SHELL_SX + 0.05, 0.10, 0.08);
    clipGroup.visible = false; // hidden until configured

    // Clip body
    const clipG = new THREE.CapsuleGeometry(0.018, 0.06, 4, 12);
    const clipM = new THREE.MeshStandardMaterial({
      color: 0x1a2a3a, roughness: 0.5, metalness: 0.4,
      emissive: 0x44ffaa, emissiveIntensity: 0,
    });
    const clip = new THREE.Mesh(clipG, clipM);
    clipGroup.add(clip);

    // PPG LED indicators
    const ppgG = new THREE.SphereGeometry(0.007, 8, 8);
    const ppgM = new THREE.MeshStandardMaterial({ color: 0xFF7200, emissive: 0xFF7200, emissiveIntensity: 0.4 });
    const ppg  = new THREE.Mesh(ppgG, ppgM);
    ppg.position.y = 0.03;
    clipGroup.add(ppg);

    // Cable to hub
    const cablePath = new THREE.CatmullRomCurve3([
      new THREE.Vector3(0, 0, 0),
      new THREE.Vector3(-0.15, -0.05, -0.10),
      new THREE.Vector3(-0.30, -0.08, -0.20),
    ]);
    const cableG = new THREE.TubeGeometry(cablePath, 12, 0.005, 5, false);
    const cableM = new THREE.MeshStandardMaterial({ color: 0x2a3a4a });
    clipGroup.add(new THREE.Mesh(cableG, cableM));

    this.group.add(clipGroup);
    this.parts.vnsClip    = clipGroup;
    this.parts.vnsClipMat = clipM;
  }

  _buildMastoidPad() {
    // 40 Hz vibrotactile mastoid pad (provisional)
    const padGroup = new THREE.Group();
    padGroup.position.set(-SHELL_R * SHELL_SX * 0.92, 0.06, -0.30);

    const padG = new THREE.CylinderGeometry(0.030, 0.030, 0.010, 24);
    const padM = new THREE.MeshStandardMaterial({
      color: 0x1a2a1a, roughness: 0.7, metalness: 0.1,
      emissive: 0xFFFF44, emissiveIntensity: 0,
    });
    padGroup.add(new THREE.Mesh(padG, padM));
    padGroup.visible = false;

    this.group.add(padGroup);
    this.parts.hapticPad    = padGroup;
    this.parts.hapticPadMat = padM;
  }

  _buildIntranasal() {
    // Intranasal Y-probe — stored in hub dock, shown in storage position
    const probeGroup = new THREE.Group();
    probeGroup.position.set(0.06, -0.16, -SHELL_R * SHELL_SZ * 0.90);
    probeGroup.rotation.x = -0.3;

    // Y-junction body
    const juncG = new THREE.SphereGeometry(0.016, 8, 8);
    const juncM = new THREE.MeshStandardMaterial({ color: 0x334455, roughness: 0.6, metalness: 0.2 });
    probeGroup.add(new THREE.Mesh(juncG, juncM));

    // Two probe arms
    for (const side of [-1, 1]) {
      const path = new THREE.CatmullRomCurve3([
        new THREE.Vector3(0, 0, 0),
        new THREE.Vector3(side * 0.02, 0.04, 0.01),
        new THREE.Vector3(side * 0.025, 0.10, 0.01),
      ]);
      const tubeG = new THREE.TubeGeometry(path, 12, 0.007, 6, false);
      const tubeM = new THREE.MeshStandardMaterial({ color: 0x556677 });
      probeGroup.add(new THREE.Mesh(tubeG, tubeM));
    }

    this.group.add(probeGroup);
    this.parts.intranasal = probeGroup;
  }

  // ─── LED canvas helpers ───────────────────────────────────────────────────

  _createLEDCanvas(zoneId) {
    const canvas  = document.createElement('canvas');
    canvas.width  = 256;
    canvas.height = 256;
    const ctx     = canvas.getContext('2d');
    const texture = new THREE.CanvasTexture(canvas);

    // Initial dark draw
    ctx.fillStyle = '#060610';
    ctx.fillRect(0, 0, 256, 256);
    texture.needsUpdate = true;

    this._ledCanvases[zoneId] = { canvas, ctx, texture };
    return { canvas, ctx, texture };
  }

  _drawLEDs(zoneId, pulse, wallTime) {
    const c = this._ledCanvases[zoneId];
    if (!c) return;
    const z = this.zones[zoneId];
    if (!z) return;

    const { canvas, ctx, texture } = c;
    ctx.clearRect(0, 0, canvas.width, canvas.height);

    // Dark PCB background
    ctx.fillStyle = pulse > 0.01 ? '#080B14' : '#050810';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    if (!z.installed || z.wavelengths.length === 0) {
      // Uninstalled — draw faint slot outline
      ctx.strokeStyle = '#1a2030';
      ctx.lineWidth = 4;
      ctx.strokeRect(4, 4, canvas.width - 8, canvas.height - 8);
      // X mark
      ctx.strokeStyle = '#1a2035';
      ctx.lineWidth = 2;
      ctx.beginPath(); ctx.moveTo(20, 20); ctx.lineTo(236, 236); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(236, 20); ctx.lineTo(20, 236); ctx.stroke();
      texture.needsUpdate = true;
      return;
    }

    const COLS = 10, ROWS = 10;
    const cellW = canvas.width  / (COLS + 1);
    const cellH = canvas.height / (ROWS + 1);
    const nWL   = z.wavelengths.length;

    z.wavelengths.forEach((wl, wi) => {
      const wlDef = WL[wl];
      if (!wlDef) return;

      ctx.fillStyle   = wlDef.hex;
      ctx.shadowColor = wlDef.glowHex;
      ctx.shadowBlur  = pulse > 0.01 ? 14 * pulse : 3;

      const startRow = wi;
      for (let r = startRow; r < ROWS; r += nWL) {
        for (let c = 0; c < COLS; c++) {
          const x = cellW + c * cellW;
          const y = cellH + r * cellH;
          // Slight flicker per LED using noise
          const flicker = pulse > 0.01
            ? 0.8 + 0.2 * Math.sin(wallTime * 37 + r * 7 + c * 13)
            : 0.3;
          const radius = pulse > 0.01
            ? (3.5 + 2.5 * pulse * flicker)
            : 1.8;

          ctx.globalAlpha = pulse > 0.01 ? (0.7 + 0.3 * flicker) : 0.35;
          ctx.beginPath();
          ctx.arc(x, y, radius, 0, Math.PI * 2);
          ctx.fill();
        }
      }
    });

    ctx.globalAlpha = 1;
    ctx.shadowBlur  = 0;

    // LED count label (bottom-right corner)
    ctx.fillStyle = 'rgba(100,130,180,0.4)';
    ctx.font = '11px monospace';
    ctx.fillText(`${COLS * ROWS * nWL} LEDs`, 6, canvas.height - 6);

    texture.needsUpdate = true;
  }

  // ─── geometry utility ─────────────────────────────────────────────────────

  /**
   * Convert spherical (r, phi, theta) → Cartesian (x, y, z) accounting for
   * the asymmetric helmet shell scale.
   * phi   = azimuth from +Z (0 = front)
   * theta = polar angle from +Y (0 = crown)
   */
  _sph(r, phi, theta) {
    const sinT = Math.sin(theta);
    const cosT = Math.cos(theta);
    const sinP = Math.sin(phi);
    const cosP = Math.cos(phi);
    return {
      x: r * sinT * sinP * SHELL_SX,
      y: r * cosT          * SHELL_SY,
      z: r * sinT * cosP * SHELL_SZ,
    };
  }
}
