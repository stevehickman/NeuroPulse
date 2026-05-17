# NeuroPulse Helmet Simulator — User Manual

**Document:** NP-SIM-001-HOWTO  
**Version:** 1.0  
**Date:** 2026-05-17  
**Applies to:** NP-SIM-001 v0.1.0

---

## Contents

1. [Overview](#1-overview)
2. [What You Need to Install](#2-what-you-need-to-install)
3. [Starting the Simulator](#3-starting-the-simulator)
4. [Interface Layout](#4-interface-layout)
5. [Left Panel — Hardware Configuration](#5-left-panel--hardware-configuration)
6. [Centre Viewport — 3D Helmet](#6-centre-viewport--3d-helmet)
7. [Right Panel — Session Control](#7-right-panel--session-control)
8. [Session Timeline](#8-session-timeline)
9. [Running a Session — Step by Step](#9-running-a-session--step-by-step)
10. [Building a Custom Configuration](#10-building-a-custom-configuration)
11. [Protocol Reference](#11-protocol-reference)
12. [Wavelength False-Colour Key](#12-wavelength-false-colour-key)
13. [Live Metrics Reference](#13-live-metrics-reference)
14. [Stopping and Resetting](#14-stopping-and-resetting)
15. [Browser Compatibility and Performance](#15-browser-compatibility-and-performance)
16. [Known Limitations (v0.1.0)](#16-known-limitations-v010)

---

## 1. Overview

The NeuroPulse Helmet Simulator is a browser-based interactive 3D visualisation of the NeuroPulse device. It runs entirely in the browser with no installation, no build step, and no back-end server required.

**Three intended uses:**

| Use | Who | What you do |
|-----|-----|-------------|
| App test target | iOS / macOS / Windows app developers | Connect the app to the simulator's WebSocket API (Issue #77 — coming) instead of real hardware |
| Marketing and training | Sales, prospective customers, clinical educators | Load a protocol and run the session to show what the helmet does |
| Protocol development | Clinicians, researchers | Build a configuration manually, run it, and inspect the session timeline and live metrics before committing to hardware |

The simulator models the complete hardware as specified in CLAUDE.md Rev 11: five snap-in LED zone modules, integrated goggles, over-ear audio cups with bone conduction, occipital Boa arch and dial, control hub, EEG pods, VNS auricular clip, intranasal Y-probe, and mastoid haptic pad.

---

## 2. What You Need to Install

**Short answer: nothing.** The simulator has no npm packages, no build step, and no back-end dependencies.

### Dependencies and how they are loaded

All external libraries are fetched automatically from CDN the first time you open the simulator. After the first load, the browser caches them and the simulator works offline.

| Library | Version | Source | Used for |
|---------|---------|--------|---------|
| **Three.js** | 0.164.1 | `cdn.jsdelivr.net` | 3D rendering engine |
| **Three.js OrbitControls** | 0.164.1 | `cdn.jsdelivr.net` | Mouse/touch camera orbit |
| **Inter** (font) | Variable | `fonts.googleapis.com` | UI text |
| **JetBrains Mono** (font) | Variable | `fonts.googleapis.com` | Metrics and timer readouts |

All other code (helmet geometry, session engine, protocol definitions, UI) is plain JavaScript ES modules shipped with the repository. No framework, no transpiler, no bundler.

### What you need on your machine

| You want to… | What you need |
|-------------|---------------|
| Just open and use the simulator | A supported web browser (see §15). That's it. |
| Serve it over HTTP (e.g. for iframe embedding, or when the WebSocket API launches in Issue #77) | Python 3 **or** Node.js **or** any static file server. Both Python and Node are pre-installed on most developer machines. |
| Edit the simulator source | Any text editor. No compiler or transpiler needed. |
| Run it in automated tests | Any headless Chromium setup (e.g. Puppeteer, Playwright). No special simulator configuration. |

### Starting a local HTTP server — quick reference

**Python 3** (no extra packages needed — Python 3 ships with this):
```bash
cd /path/to/NeuroPulse
python3 -m http.server 8080
# Then open: http://localhost:8080/simulator/
```

**Node.js** (uses the `serve` package via npx — downloads it once automatically):
```bash
npx serve simulator --listen 8080
# Then open: http://localhost:8080
```

**VS Code Live Server extension:** Right-click `simulator/index.html` → Open with Live Server. No terminal needed.

### First-load internet requirement

The simulator needs an internet connection **only on the very first load** to fetch Three.js and the fonts from CDN. Once the browser caches them, the simulator runs fully offline. If you need guaranteed offline operation from the first load (e.g. an air-gapped environment), download the Three.js files from the CDN and update the importmap in `simulator/index.html` to use local paths.

---

## 3. Starting the Simulator

### Option A — Open directly in a browser (simplest)

1. Navigate to the `simulator/` directory in the repository.
2. Double-click `index.html`, or drag it onto an open browser window.
3. The simulator loads immediately.

> **Recommended browsers:** Chrome 112+, Edge 112+, Safari 17+, Firefox 115+.
> Chrome and Edge give the best WebGL performance.

### Option B — Serve over HTTP

Any static file server works (see §2 for quick-reference commands). Required when the WebSocket device API (Issue #77) is active, and recommended for iframe embedding.

Then open the URL shown by the server in your browser.

### Option C — Embed in a web page

Copy the entire `simulator/` directory to your web server. Point an `<iframe>` at `index.html`:

```html
<iframe src="simulator/index.html"
        width="1280" height="720"
        style="border:none; border-radius:8px;">
</iframe>
```

For full-page use, the simulator's layout is responsive to the viewport size. Minimum recommended viewport: 1024 × 600 px.

---

## 4. Interface Layout

The simulator has four regions:

```
┌─────────────────────────────────────────────────────────────┐
│  Header — NeuroPulse branding + version badge + API status  │
├─────────────────┬───────────────────────┬───────────────────┤
│                 │                       │                   │
│  Left panel     │   Centre viewport     │   Right panel     │
│  Zone modules   │   3D helmet (WebGL)   │   Protocol        │
│  Accessories    │                       │   Session         │
│  Wavelength key │                       │   Live metrics    │
│                 │                       │   Modalities      │
├─────────────────┴───────────────────────┴───────────────────┤
│  Session Timeline                                           │
└─────────────────────────────────────────────────────────────┘
```

The centre viewport takes up the majority of the screen. The left and right panels scroll independently if the content overflows.

---

## 5. Left Panel — Hardware Configuration

### 5.1 Zone Modules

The helmet has five snap-in LED zone module slots:

| ID | Zone name | Head location |
|----|-----------|---------------|
| ZM-01 | Frontal | Above the forehead |
| ZM-02 | Left Temporal | Left side of the head |
| ZM-03 | Right Temporal | Right side of the head |
| ZM-04 | Left Parietal | Upper left |
| ZM-05 | Occipital | Back of the head |

**To install a zone module:**

1. Find the zone's row in the Zone Modules section.
2. Click the **toggle switch** to the ON position.
3. The zone card expands to show **Wavelengths** and **PBM Frequency** controls.
4. The corresponding zone panel on the 3D helmet immediately lights up.

**To remove a zone module:**

Toggle the switch to OFF. The zone panel goes dark and its label on the 3D helmet dims.

#### Wavelength Selection

Each installed zone can emit one, two, or three wavelengths simultaneously. Click a wavelength button to activate it; click again to deactivate. Active buttons are highlighted in their wavelength colour.

| Button | Actual wavelength | Tissue target |
|--------|------------------|---------------|
| 660 nm | Red (visible) | Surface / skin |
| 808 nm | Near-infrared | Cortical ~5–8 mm depth |
| 1064 nm | Deep NIR (NIR-II) | Cortical ~10–15 mm depth — requires 1064 nm smart zone module |

All three can be active simultaneously for the three-tier penetration stack.

#### PBM Frequency

The dropdown sets the LED pulse frequency for that zone:

| Option | Frequency | Brainwave band |
|--------|-----------|---------------|
| CW (continuous) | 0 Hz | Vascular baseline — no pulsing |
| 2 Hz — Delta | 2 Hz | Deep sleep |
| 6 Hz — Theta memory | 6 Hz | Memory consolidation |
| 10 Hz — Alpha | 10 Hz | Relaxation, calm focus |
| 20 Hz — Beta / Focus | 20 Hz | Executive function |
| 40 Hz — Gamma | 40 Hz | Gamma entrainment (GENUS paradigm) |

Different zones can be set to different frequencies for split-zone protocols.

### 5.2 Accessories

Four accessories can be toggled on or off independently of the zone modules:

| Accessory | 3D model element shown |
|-----------|----------------------|
| **Visual Goggles** | Integrated goggle arms and lens assembly with micro-LEDs |
| **VNS + HRV Clip** | Auricular clip at the ear with PPG indicator |
| **Mastoid Haptic Pad** | Small LRA pad behind the ear (provisional hardware) |
| **Intranasal Probe** | Y-probe sitting in the hub dock at the back |

Toggle each accessory with its switch. The 3D model updates in real time.

### 5.3 Wavelength Key

The legend at the bottom of the left panel shows the false-colour mapping used for all wavelengths on the 3D model. See [§12](#12-wavelength-false-colour-key) for the full table.

---

## 6. Centre Viewport — 3D Helmet

### 6.1 Camera Navigation

| Action | Effect |
|--------|--------|
| **Left-drag** | Orbit (rotate) the helmet freely |
| **Scroll wheel** | Zoom in / zoom out |
| **Right-drag** | Pan the camera |
| **Pinch** (touch) | Zoom on touch screens |

The camera has distance limits: minimum ~2 m, maximum ~9 m from the helmet centre.

### 6.2 Camera Presets

Four preset buttons appear at the top of the viewport:

| Button | Camera position |
|--------|----------------|
| **Front** | Facing the forehead directly |
| **Side** | Profile view from the right |
| **Top** | Bird's eye view from above |
| **Free** | Unlocks orbit controls (default) |

Clicking a preset animates the camera smoothly to that position. The Front, Side, and Top presets lock orbit until **Free** is clicked.

### 6.3 What You See on the Helmet

| Visual element | What it represents |
|---------------|-------------------|
| **Zone panels — glowing LEDs** | Active PBM zones, colour-coded by wavelength |
| **Zone panels — dark** | Zone module not installed |
| **Zone labels** (floating text) | Zone ID + anatomical name; green = installed, grey = not installed |
| **EEG pods** (8 small cylinders) | Electrode contact points; colour = impedance: green (good) → amber (marginal) → red (poor) |
| **Power LED** (left temple, green) | Breathes slowly at idle; steady during session |
| **Session LED** (right temple, amber) | Pulses at session frequency ÷ 4 while running |
| **Goggles** | Visual stimulation module; micro-LEDs visible in lens |
| **Audio cups** | Over-ear drivers + bone conduction at mastoid |
| **Hub** | Control unit at the back; USB-C port visible |
| **Boa dial** | Occipital arch fit system |
| **VNS clip** | Auricular clip (when accessory enabled) |
| **Mastoid pad** | Haptic LRA pad (when accessory enabled) |
| **Intranasal probe** | Y-probe in hub dock (when accessory enabled) |

During an active session, LED zones animate: brightness and glow pulse at the configured frequency. For CW (continuous-wave) protocols, zones glow steadily.

---

## 7. Right Panel — Session Control

### 7.1 Protocol Selection

1. Click the **Protocol** dropdown.
2. Select a protocol — labelled `[T1]` (Home tier) or `[T2]` (Pro tier).
3. When you select a protocol:
   - The description, duration, and active modality pills appear below the dropdown.
   - Zone modules are **automatically configured** — correct zones, wavelengths, and frequencies are pre-set.
   - The session timeline populates with phase segments.

You can manually adjust any zone setting after protocol selection.

### 7.2 Session Controls

| Button | Visible when | Action |
|--------|-------------|--------|
| **▶ Start** | Idle | Starts the session from the beginning |
| **⏸ Pause** | Running | Pauses the session clock and all animations |
| **▶ Resume** | Paused | Resumes from the paused point |
| **■ Stop** | Running or Paused | Stops and resets to 0:00 |

The session timer (large digital readout) shows elapsed time in `M:SS` format. It turns bright cyan while running.

The **progress bar** below the controls fills left to right as the session advances.

### 7.3 Live Metrics

Six metrics update in real time during a session:

| Metric | Unit | Active when |
|--------|------|-------------|
| **Alpha power** | µV²/Hz | EEG modality running |
| **NF score** | /10 | EEG modality running |
| **Heart rate** | bpm | VNS + HRV clip running |
| **RMSSD** | ms | VNS + HRV clip running |
| **HRV coherence** | /10 | VNS + HRV clip running |
| **ZM-01 dose** | J/cm² | PBM running with ZM-01 installed |

Metrics show `—` when their source modality is not active.

**Breathing pacer ring:** When a protocol includes HRV coherence training (e.g. Anxiety Relief + HRV, Combined Standard), the breathing pacer ring animates below the metrics. The ring expands on inhale and contracts on exhale at 6 breaths per minute. Follow it to pace your breathing during training demonstrations.

### 7.4 Active Modalities

Modality pills at the bottom of the right panel show which subsystems are active. Pills glow in their modality colour when active; grey when inactive.

| Pill | Modality |
|------|----------|
| PBM | Photobiomodulation (zone LEDs) |
| EEG | Neurofeedback / brain monitoring |
| BES | Brainwave Entrainment Stimulation |
| tDCS | Cortical Priming Stimulation |
| VNS | Vagus Nerve Stimulation + HRV |
| Audio | Binaural beats / bone conduction |
| Visual | Goggle LED stimulation |
| Haptic | Mastoid vibrotactile (reserved) |

---

## 8. Session Timeline

The timeline bar runs along the bottom of the screen.

- **Phase segments** are colour-coded blocks spanning the full session duration. Hover any segment to see its name and exact time range as a tooltip.
- **Cursor line** moves left to right in sync with session progress.
- **Current time** is shown in the top-right corner of the timeline header.

The timeline populates when a protocol is selected and clears when Stop is pressed.

---

## 9. Running a Session — Step by Step

**Step 1 — Open the simulator**

Open `simulator/index.html` in a supported browser. The 3D helmet appears with no zones installed and the session at 0:00.

**Step 2 — Select a protocol**

In the right panel, open the **Protocol** dropdown and choose a protocol. The zone configuration updates automatically and the timeline appears at the bottom.

> **Tip:** For a first run, choose **[T1] Alpha Calm** or **[T1] Gamma 40 Hz** — both use all five zones and are visually striking.

**Step 3 — Review the configuration**

Check the left panel: five zones should be installed (green toggle), with wavelength buttons highlighted. Confirm the frequency matches the protocol description.

**Step 4 — Add accessories (optional)**

Toggle on **Visual Goggles** and **VNS + HRV Clip** to see the full helmet configuration on screen.

**Step 5 — Start the session**

Click **▶ Start**. Simultaneously:
- The session timer starts counting up.
- The progress bar begins filling.
- Zone LEDs begin pulsing at their configured frequency.
- The session LED on the right temple starts pulsing.
- Live metrics begin updating.
- The timeline cursor starts moving.
- Active modality pills light up.

**Step 6 — Navigate the 3D view**

While the session runs, orbit, zoom, and pan the helmet freely. Click **Front**, **Side**, or **Top** to snap to a preset. The LEDs continue animating from all angles.

**Step 7 — Observe the metrics**

Watch the right panel: EEG metrics (alpha power, NF score) simulate realistic session dynamics. If VNS is active, heart rate and HRV coherence update every second. For HRV protocols, follow the breathing pacer ring.

**Step 8 — Stop the session**

Click **■ Stop** at any time to end the session. The timer resets to 0:00 and all LEDs return to idle.

---

## 10. Building a Custom Configuration

For clinicians or researchers who want to explore a specific combination rather than a preset:

**Step 1 — Leave the Protocol dropdown at "— Select a protocol —"** (or select one and then modify it manually).

**Step 2 — Install zones one at a time.** For each zone, toggle it on and then set the wavelengths and frequency you want.

**Example — frontal gamma with deep NIR, occipital alpha (split-zone):**

| Zone | Wavelengths | Frequency |
|------|-------------|-----------|
| ZM-01 Frontal | 660 nm, 808 nm, 1064 nm | 40 Hz — Gamma |
| ZM-05 Occipital | 660 nm, 808 nm | 10 Hz — Alpha |

**Step 3 — Add accessories** as needed.

**Step 4 — Click ▶ Start.** Without a protocol loaded, the session runs indefinitely. Stop it manually with **■ Stop**.

> **Note:** Without a full protocol, the timeline is empty and only PBM and EEG modality pills are active. BES, tDCS, VNS, and Audio require a protocol definition. For a complete multi-modal simulation, use a preset and then modify zones after selection.

---

## 11. Protocol Reference

All nine built-in protocols:

| Protocol | Tier | Duration | Zones | Wavelengths | Frequency | Modalities |
|----------|------|----------|-------|-------------|-----------|-----------|
| **Alpha Calm** | T1 | 20 min | ZM-01–05 | 660, 808 nm | 10 Hz | PBM · EEG · Audio |
| **Focus Prime** | T1 | 15 min | ZM-01–03 | 660, 808 nm | 20 Hz | PBM · EEG · BES · Audio |
| **Gamma 40 Hz** | T1 | 20 min | ZM-01–05 | 660, 808 nm | 40 Hz | PBM · EEG · Visual · Audio |
| **Sleep Deep** | T1 | 30 min | ZM-01, 04, 05 | 660, 808 nm | 2 Hz | PBM · EEG · BES · Audio |
| **Anxiety Relief + HRV** | T1 | 25 min | ZM-01–03 | 660, 808 nm | 10 Hz | PBM · EEG · VNS · Audio (breathing pacer) |
| **Combined Standard** | T1 | 30 min | ZM-01–05 | 660, 808 nm | 40 Hz | PBM · EEG · BES · VNS · Audio (breathing pacer) |
| **PBM Vascular Baseline** | T1 | 20 min | ZM-01–05 | 660, 808 nm | CW | PBM only |
| **Deep NIR — 1064 nm** | T1 | 20 min | ZM-01–05 | 660, 808, 1064 nm | 10 Hz | PBM · EEG · Audio |
| **TMS — Depression (rTMS)** | T2 | 40 min | ZM-01–03 | 660, 808, 1064 nm | 10 Hz | PBM · EEG · TMS · HD-tDCS |

**T2 protocols** require NeuroPulse Pro hardware. In the simulator they run identically to T1 for visualisation purposes — the additional modalities (TMS, HD-tDCS) are reflected in the modality pills and timeline phases.

### Phase breakdown

| Protocol | Phases |
|----------|--------|
| Alpha Calm | Ramp (1 min) → Main (18 min) → Ramp-down (1 min) |
| Focus Prime | Ramp (30 s) → Main (14 min) → Ramp-down (30 s) |
| Gamma 40 Hz | Ramp (1 min) → Main (18 min) → Ramp-down (1 min) |
| Sleep Deep | Onset (5 min) → Deep (20 min) → Fade (5 min) |
| Anxiety Relief + HRV | Baseline (2 min) → RF Sweep (3 min) → Coherence (18 min) → Cool-down (2 min) |
| Combined Standard | Ramp (1 min) → Main (28 min) → Ramp-down (1 min) |
| PBM Vascular Baseline | CW (20 min, single phase) |
| Deep NIR — 1064 nm | Ramp (1 min) → Main (18 min) → Ramp-down (1 min) |
| TMS — Depression | EEG Map (5 min) → HD-tDCS (10 min) → rTMS (20 min) → Post-EEG (5 min) |

---

## 12. Wavelength False-Colour Key

Infrared wavelengths are invisible to the human eye. The simulator uses a false-colour scheme where **longer actual wavelength → longer apparent visible wavelength**, conveying increasing infrared depth as a warm colour progression.

| Wavelength | False colour | Hex | Actual visibility | Tissue target |
|------------|-------------|-----|------------------|---------------|
| **660 nm** | Red | `#FF2200` | Visible (actual colour) | Surface / skin |
| **808 nm** | Orange | `#FF7200` | Near-infrared (invisible) | Cortical ~5–8 mm |
| **1064 nm** | Amber | `#FFBB00` | NIR-II (invisible) | Deep cortical ~10–15 mm |
| **1170 nm** | Gold | `#FFE500` | NIR-II (invisible, T2 only) | Subcortical ~35–40 mm |

The legend is always visible at the bottom of the left panel. Each swatch glows in the colour it represents on the 3D model. The `NIR` badge appears next to wavelengths that are invisible in reality.

> 660 nm is shown in its actual red colour. All other wavelengths are represented by false colour.

---

## 13. Live Metrics Reference

All metrics shown during a session are **simulated** — they model realistic session dynamics but are not connected to real hardware in v0.1.0.

| Metric | How it is simulated |
|--------|-------------------|
| **Alpha power (µV²/Hz)** | Sinusoidal variation centred on a realistic resting value; rises during alpha-frequency protocols |
| **NF score (/10)** | Derived from simulated alpha/theta band ratio; increases as session progresses |
| **Heart rate (bpm)** | ~65 bpm with ±3 bpm natural variability |
| **RMSSD (ms)** | Higher during HRV protocols with breathing pacer active |
| **HRV coherence (/10)** | Oscillates at 0.1 Hz matching the breathing pacer ring; reaches ~7–8 /10 when the pacer is active |
| **ZM-01 dose (J/cm²)** | Accumulates linearly from 0 based on simulated irradiance × elapsed time |

During ramp phases, all metrics scale with the ramp envelope — they rise gradually at the start and taper at the end.

---

## 14. Stopping and Resetting

| Action | How |
|--------|-----|
| **Pause** | Click ⏸ Pause — session clock stops; LEDs freeze at current brightness |
| **Resume** | Click ▶ Resume — session continues from the paused point |
| **Stop** | Click ■ Stop — resets to 0:00; LEDs return to idle; timeline cursor returns to start |
| **Change protocol** | Click ■ Stop first, then select a new protocol |
| **Reset hardware config** | Toggle all zone switches off manually, or select a new protocol (zones reconfigure automatically) |

There is no confirmation dialog — Stop immediately resets. This is intentional for rapid protocol iteration.

---

## 15. Browser Compatibility and Performance

### Requirements

| Requirement | Minimum |
|-------------|---------|
| WebGL 2.0 | Required (all major browsers since 2017) |
| ES module importmap | Required — Chrome 89+, Edge 89+, Safari 16.4+, Firefox 108+ |
| GPU | Integrated graphics sufficient; discrete GPU improves frame rate |
| RAM | 256 MB browser tab allocation |

### Recommended versions

| Browser | Minimum version | Notes |
|---------|----------------|-------|
| **Chrome** | 112+ | Best performance and WebGL support |
| **Edge** | 112+ | Same engine as Chrome |
| **Safari** | 17+ | importmap support arrived in 16.4; 17+ recommended |
| **Firefox** | 115+ | Canvas texture updates slightly slower than Chrome |

### Performance tips

- Close other GPU-intensive browser tabs if LED animations are choppy.
- Reduce browser zoom (Ctrl/Cmd + −) to lower the canvas resolution on slower machines.
- The simulator targets 60 fps on modern hardware. On integrated graphics it may run at 30 fps.

### Mobile browsers

The 3D scene renders correctly on modern mobile browsers. The three-column layout requires a landscape viewport of at least 1024 px wide; on smaller screens the panels may overlap the viewport. Pinch-to-zoom works on touch screens.

### Known browser quirks

- **Safari 16.x:** importmap support was added in 16.4. Earlier versions will show a blank page. Update to 17+.
- **Firefox:** LED canvas texture updates are ~10–15 fps slower than Chrome at equal hardware.

---

## 16. Known Limitations (v0.1.0)

These items are tracked as open sub-issues of GitHub issue #81:

| Issue | Description | Status |
|-------|-------------|--------|
| **#77** | WebSocket device API — real hardware or native app connection | Not yet implemented |
| **#78** | Intranasal probe animation — Y-probe extending from dock during session | Not yet implemented |
| **#79** | T2 TMS coil — figure-8 coil geometry above ZM-01 | Not yet implemented |
| **#80** | Helmet geometry update — pending finalised shell CAD | Blocked on shell design |

The current helmet geometry is adapted from the Neuronic Light dome layout. It will be updated once the final shell CAD is complete.

All simulated metrics are placeholder values. Real closed-loop EEG data, live dose accumulation, and hardware telemetry will flow through the WebSocket API (Issue #77) once real or prototype hardware is available.

---

*NP-SIM-001-HOWTO v1.0 — NeuroPulse Helmet Simulator User Manual — 2026-05-17*
