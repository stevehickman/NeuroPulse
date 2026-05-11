# NP-FW-ZA-001 Rev A — Zone Module Bone Conduction Announcement Firmware
**Project:** NeuroPulse  
**Document number:** NP-FW-ZA-001  
**Revision:** A  
**Date:** 2026-05-11  
**Status:** Baselined  
**Author:** Firmware Engineering  
**Related issues:** GitHub Issue #22 (G2-10)

---

## 1. Scope

This document specifies the firmware module that detects zone module insertion via the ZONE_ID resistor (FPC pin 18) and announces the connected zone through the bone conduction piezoelectric element at the mastoid.  This implements **RISK-15 Layer 5** of the five-layer zone module keying system specified in CLAUDE.md §7.1.

**In scope:**
- ZONE_ID ADC classification (5 zones, 1% resistor ladder)
- 3×100 ms debounce per RISK-18
- Bone conduction DDS audio synthesis and SAI3 I2S output
- Zone-specific tone sequences (beep-count encoding)
- SHDR accessory authentication logging
- Five-slot parallel state machines

**Out of scope:**
- PBM zone power control (handled by application layer)
- EEG or stimulation modality logic
- App connectivity or BLE notification

---

## 2. Hardware Context

### 2.1 ZONE_ID resistor ladder

Source: NP-HW-FPC-001 Rev D, §11.4 (five-layer keying, Layer 2).

| Zone module | Anatomical position | Resistor (pin 18 to GND) | Nominal ADC counts |
|-------------|--------------------|--------------------------|--------------------|
| ZM-01 | Frontal Left | 10 kΩ (1%) | 2048 |
| ZM-02 | Frontal Right | 22 kΩ (1%) | 2818 |
| ZM-03 | Vertex | 47 kΩ (1%) | 3378 |
| ZM-04 | Parietal Left | 100 kΩ (1%) | 3723 |
| ZM-05 | Parietal Right | 220 kΩ (1%) | 3918 |
| (none) | — | open circuit | ≈ 4095 |

Pull-up: 10 kΩ to 3.3 V on hub PCB.  
ADC: i.MX RT1062 LPADC1 channel 12, 12-bit, Vref = 3.3 V.  
Voltage divider: Vout = 3.3 V × R_Z / (R_PU + R_Z).

### 2.2 Bone conduction output

| Parameter | Value |
|-----------|-------|
| Peripheral | i.MX RT1062 SAI3 |
| Format | I2S, 16-bit, stereo frame (mono signal replicated L/R) |
| Sample rate | 8000 Hz |
| DMA channel | eDMA 5, ping-pong circular buffer |
| Buffer depth | 128 samples (64 per half) |
| Transducer | Piezoelectric bone conduction element, mastoid placement |
| Isolator | Shore 20–30A silicone (CLAUDE.md §7.2 locked change) |

---

## 3. Configuration Constants

All constants are in `include/np_zone_announce_config.h`.  Key values:

| Constant | Value | Source |
|----------|-------|--------|
| `NP_ZA_DEBOUNCE_READS` | 3 | CLAUDE.md §7.1 RISK-18 |
| `NP_ZA_DEBOUNCE_MAJORITY` | 2 | CLAUDE.md §7.1 RISK-18 |
| `NP_ZA_DEBOUNCE_INTERVAL_MS` | 100 ms | CLAUDE.md §7.1 RISK-18 |
| `NP_ZA_INSERTION_SETTLE_MS` | 20 ms | contact bounce margin |
| `NP_ZA_REMOVAL_DEBOUNCE_MS` | 50 ms | |
| `NP_ZA_AUDIO_SAMPLE_RATE_HZ` | 8000 Hz | |
| `NP_ZA_AUDIO_AMPLITUDE_Q15` | 0x4000 (−6 dBFS) | bone conduction comfort |
| `NP_ZA_AUDIO_RAMP_MS` | 20 ms | piezo click suppression |
| `NP_ZA_TONE_BEEP_HZ` | 440 Hz (A4) | zone position pulse |
| `NP_ZA_TONE_CONFIRM_HZ` | 880 Hz (A5) | "connected" confirmation |
| `NP_ZA_BEEP_DURATION_MS` | 120 ms | |
| `NP_ZA_BEEP_GAP_MS` | 80 ms | |
| `NP_ZA_PRE_CONFIRM_GAP_MS` | 180 ms | |
| `NP_ZA_CONFIRM_DURATION_MS` | 250 ms | |

---

## 4. Type Definitions

Source: `include/np_zone_announce_types.h`.

### 4.1 `np_zone_id_t`

```c
typedef enum {
    NP_ZONE_NONE            = 0,
    NP_ZONE_FRONTAL_LEFT    = 1,   /* ZM-01 */
    NP_ZONE_FRONTAL_RIGHT   = 2,   /* ZM-02 */
    NP_ZONE_VERTEX          = 3,   /* ZM-03 */
    NP_ZONE_PARIETAL_LEFT   = 4,   /* ZM-04 */
    NP_ZONE_PARIETAL_RIGHT  = 5,   /* ZM-05 */
    NP_ZONE_UNKNOWN         = 6,   /* unrecognised resistor value */
} np_zone_id_t;
```

### 4.2 Slot state machine

```c
typedef enum {
    NP_ZA_STATE_IDLE,
    NP_ZA_STATE_SETTLING,
    NP_ZA_STATE_DEBOUNCING,
    NP_ZA_STATE_ANNOUNCING,
    NP_ZA_STATE_ACTIVE,
    NP_ZA_STATE_REMOVING,
} np_za_state_t;
```

### 4.3 Tone segment

```c
typedef struct {
    uint16_t freq_hz;       /* 0 = silence */
    uint16_t duration_ms;
} np_za_tone_segment_t;
```

---

## 5. Public API

Source: `include/np_zone_announce.h`.

| Function | Description |
|----------|-------------|
| `np_za_init()` | Initialise module: register callbacks, set session counter |
| `np_za_deinit()` | Tear down SAI3 audio, zero context |
| `np_za_tick(now_ms)` | Drive all 5 slot state machines; call from 50 ms task |
| `np_za_audio_tick(half)` | Fill DMA ping-pong half-buffer; call from eDMA ISR |
| `np_za_get_zone(slot)` | Query confirmed zone for a slot (returns NONE if empty) |
| `np_za_zone_name(zone)` | Human-readable zone name string |

---

## 6. Zone Detection (np_zone_detect.c)

### 6.1 ADC classification

`classify_adc(counts)` maps 12-bit ADC value to `np_zone_id_t` using midpoint thresholds:

| Range (counts) | Zone |
|----------------|------|
| < 1024 | UNKNOWN (short-to-GND or bad contact) |
| 1024 – 2433 | FRONTAL_LEFT (ZM-01) |
| 2434 – 3098 | FRONTAL_RIGHT (ZM-02) |
| 3099 – 3550 | VERTEX (ZM-03) |
| 3551 – 3820 | PARIETAL_LEFT (ZM-04) |
| 3821 – 3999 | PARIETAL_RIGHT (ZM-05) |
| ≥ 4000 | NONE (no module) |

Thresholds are set at geometric midpoints between adjacent nominal values.  1% resistor tolerance produces ≤ ±20 ADC count variation at nominal; thresholds have > 100 count margin on each side.

### 6.2 Debounce algorithm (RISK-18)

```
np_za_detect_poll() called at 100 ms intervals:

  read_count < 3:  take one ADC reading, classify, store in deb->zone[i]
  read_count == 3: evaluate majority_zone()
                     - tally occurrences of each zone across 3 reads
                     - return zone with count ≥ 2 (NP_ZA_DEBOUNCE_MAJORITY)
                     - return UNKNOWN if no zone meets threshold
```

Majority vote algorithm: `majority_zone(zone_reads[], count)`.  NONE votes are ignored in the majority tally (module disappearing between reads triggers re-entry to IDLE rather than a false negative).

---

## 7. Audio Synthesis (np_zone_audio.c)

### 7.1 DDS synthesis

A 256-entry Q15 sine LUT (`np_sine_lut[]`) stored in flash (512 bytes).  Direct digital synthesis with a 32-bit phase accumulator:

```
phase_inc = (freq_hz × 2^32) / Fs        [computed once per segment]
phase_acc += phase_inc                    [per sample]
sample    = LUT[phase_acc >> 24]          [top 8 bits as LUT index]
output    = (int32_t)sample × amplitude >> 15   [Q15 → Q0 int16_t]
```

No libm dependency.  All integer arithmetic.  Cortex-M7 FPU not required for audio path.

### 7.2 Amplitude ramp

Linear ramp over `NP_ZA_AUDIO_RAMP_MS` (20 ms = 160 samples at 8 kHz) prevents piezoelectric click artefacts at clip start and end.  Ramp step = `NP_ZA_AUDIO_AMPLITUDE_Q15 / RAMP_SAMPLES`.

### 7.3 Zone tone sequences

| Zone | Sequence | Total duration |
|------|----------|----------------|
| Frontal Left (1) | 1 beep + confirm | ≈ 550 ms |
| Frontal Right (2) | 2 beeps + confirm | ≈ 730 ms |
| Vertex (3) | 3 beeps + confirm | ≈ 910 ms |
| Parietal Left (4) | 4 beeps + confirm | ≈ 1090 ms |
| Parietal Right (5) | 5 beeps + confirm | ≈ 1270 ms |
| Unknown (fault) | 200 Hz → 150 Hz descending pair | ≈ 700 ms |

Beep: 440 Hz A4 for 120 ms.  Gap between beeps: 80 ms silence.  Pre-confirm gap: 180 ms.  Confirmation: 880 Hz A5 for 250 ms.

### 7.4 DMA buffer management

Ping-pong buffer: `int16_t dma_buf[128]` in `np_za_ctx_t` (256 bytes, static allocation).  eDMA configured for circular transfer with half-complete and complete interrupts.  `np_za_audio_tick(half)` is called from both ISRs to refill the inactive half.  Stereo frame packing: identical sample written to L and R channels (bone conduction is mono).

---

## 8. Slot State Machine (np_zone_announce.c)

### 8.1 State transition diagram

```
         first non-NONE ADC read
IDLE ──────────────────────────────► SETTLING
  ▲                                      │ settle delay (20 ms)
  │                                      ▼
  │  absent after          DEBOUNCING (3 × 100 ms reads)
  │  debounce fail    ◄───────┤ majority ≥ 2/3 → ANNOUNCING
  │                           │ fail → IDLE
  │                           │
  │                    ANNOUNCING
  │                    (audio playing)
  │                           │ audio complete
  │                           ▼
  │                       ACTIVE
  │                    (module present)
  │                           │ absence detected for 50 ms
  │                           ▼
  └──────────────────── REMOVING
```

### 8.2 Announcement queue

Up to 5 slots may request audio simultaneously (all 5 zone modules inserted at once).  A FIFO queue (length 5) serialises announcements in insertion-detection order.  The `np_za_tick()` function starts the next queued clip as soon as `np_za_audio_busy()` returns false.

### 8.3 Application callbacks

| Callback | When called | Arguments |
|----------|-------------|-----------|
| `insert_cb(zone, false)` | Immediately on debounce pass | Zone confirmed |
| `insert_cb(zone, true)` | After audio clip completes | Announcement done |
| `remove_cb(zone)` | After removal debounce | Zone removed |
| `shdr_cb(entry)` | On every debounce exit | Auth log entry |

The double insert callback allows the application to enable PBM zone power immediately (before audio) while also receiving a signal when the user experience is complete.

---

## 9. SHDR Logging

### 9.1 Authentication log entry

```c
typedef struct {
    uint8_t  zone_id;                /* np_zone_id_t: 1–5 (valid), 6 (unknown) */
    uint8_t  slot_index;             /* 0-based physical slot                   */
    uint8_t  auth_result;            /* 0 = pass, 1 = fail                     */
    uint8_t  debounce_reads_passed;  /* 0–3                                    */
    uint32_t session_count;          /* unsigned session counter from SHDR      */
} np_za_shdr_auth_entry_t;
```

### 9.2 Classification rationale

Per NP-FW-EMMC-001 Rev A §12 session data classification table:
- **SHDR** (not UHDR): "accessory authentication pass/fail" — records device accessory detection result only.  Contains no user biology.  Linked to device ID, not user identity.
- Analogous to "accessory authentication pass/fail" already listed in the SHDR column.
- SHDR key: `"zm_auth_v1"`.

---

## 10. Platform HAL (Open Items)

The following functions are declared in `np_zone_announce.h` and must be implemented by the firmware platform team for the i.MX RT1062 target.

| OI | Function | Peripheral | Notes |
|----|----------|------------|-------|
| OI-ZA-01 | `np_za_platform_adc_read()` | LPADC1 ch 12 | Blocking single conversion, ≤ 1 ms |
| OI-ZA-02 | `np_za_platform_sai_init()` | SAI3 + eDMA 5 | 8 kHz, 16-bit stereo, ping-pong |
| OI-ZA-03 | `np_za_platform_sai_start()` | SAI3 | Enable FIFO TX, start eDMA |
| OI-ZA-04 | `np_za_platform_sai_stop()` | SAI3 | Disable TX, tri-state output |
| (inline) | `np_za_platform_now_ms()` | FreeRTOS tick | `xTaskGetTickCount() × portTICK_PERIOD_MS` |

**OI-ZA-02 notes:** SAI3 MCLK = 2.048 MHz (256 × Fs); BCLK = 512 kHz (64 × Fs); word length = 16 bits; frame sync = 8 kHz; stereo (2 channels × 32-bit slot width).  DMA buffer address = `&np_za_ctx.dma_buf[0]`; transfer size = `NP_ZA_DMA_BUF_SAMPLES × sizeof(int16_t)`.

---

## 11. Memory Budget

| Item | Size |
|------|------|
| Sine LUT (`np_sine_lut`) | 512 bytes (flash) |
| Clip segment arrays (all 6) | 232 bytes (flash) |
| `np_za_ctx_t` (static) | ≈ 1024 bytes (SRAM) |
| DMA buffer (`dma_buf`) | 256 bytes (SRAM, DMA-accessible) |
| Stack (task) | 2048 bytes (512 words × 4) |
| **Total SRAM** | **≈ 3.3 KB** |
| **Total flash** | **≈ 744 bytes** |

---

## 12. Integration

```c
/* Application startup */
np_za_init(my_insert_cb, my_remove_cb, my_shdr_cb, shdr_session_count);

/* 50 ms FreeRTOS task */
void zone_task(void *arg) {
    for (;;) {
        np_za_tick(np_za_platform_now_ms());
        vTaskDelay(pdMS_TO_TICKS(NP_ZA_POLL_IDLE_MS));
    }
}

/* eDMA half-complete ISR */
void EDMA_CH5_IRQHandler(void) {
    np_za_audio_tick(0);   /* fill first half */
    /* clear DMA interrupt flag */
}

/* eDMA transfer-complete ISR */
void EDMA_CH5_TC_IRQHandler(void) {
    np_za_audio_tick(1);   /* fill second half */
}

/* Insert callback */
void my_insert_cb(np_zone_id_t zone, bool announcement_done) {
    if (!announcement_done) {
        pbm_zone_enable(zone);  /* enable PBM zone power immediately */
    }
    /* After announcement_done=true: UX is complete */
}
```

---

## 13. Verification

| Test ID | Description | Pass criterion |
|---------|-------------|----------------|
| FAI-ZA-01 | ADC classification accuracy | All 5 zones correctly identified within ±20 ADC counts of nominal |
| FAI-ZA-02 | Debounce 3×100 ms timing | Inter-read interval 100 ± 5 ms, verified with logic analyser |
| FAI-ZA-03 | Majority vote — 3/3 agree | Returns correct zone |
| FAI-ZA-04 | Majority vote — 2/3 agree | Returns correct zone (one outlier) |
| FAI-ZA-05 | Majority vote — 1/3 agree | Returns ERR_DEBOUNCE_FAIL → IDLE |
| FAI-ZA-06 | Unknown resistor (e.g. 33 kΩ) | Returns ZONE_UNKNOWN, plays error tone |
| FAI-ZA-07 | Tone frequency accuracy | 440 Hz ± 2 Hz, 880 Hz ± 2 Hz (FFT on bone cond. output) |
| FAI-ZA-08 | Amplitude ramp | No click artefact; ramp ≥ 15 ms on/off (oscilloscope) |
| FAI-ZA-09 | All 5 zones simultaneous insertion | Queue: all 5 announcements play in slot order (ZM-01→05) |
| FAI-ZA-10 | Removal detection | remove_cb fires after 50 ms confirmed absence |
| FAI-ZA-11 | SHDR auth log | Entry written on every debounce exit; auth_result correct |
| FAI-ZA-12 | Insert callback timing | insert_cb(false) before audio; insert_cb(true) after audio |

---

## 14. Revision History

| Rev | Date | Changes |
|-----|------|---------|
| A | 2026-05-11 | Initial release — Issue #22, closes G2-10 |
