# Edge AI Wake Word Engine — STM32L475 (Bare Metal)

[![Target: STM32L475VG](https://img.shields.io/badge/Target-STM32L475VG%20(ARM%20Cortex--M4F)-blue.svg)](#hardware--peripherals)
[![Framework: Bare Metal](https://img.shields.io/badge/Framework-Bare--Metal%20C%20(No%20HAL)-orange.svg)](#firmware--digital-signal-processing)
[![Baud: 921600](https://img.shields.io/badge/UART-921600%208N2-green.svg)](#custom-binary-uart-telemetry)
[![Model: TinyML](https://img.shields.io/badge/TinyML-MFCC%20%2B%20LogReg%20(92%25%20Test%20Acc)-purple.svg)](#tinyml-pipeline--model-performance)

An end-to-end, bare-metal wake-word engine and data collection pipeline engineered on the **B-L475E-IOT01A** (`STM32L475VG`, ARM Cortex-M4F @ 80 MHz). All peripherals (RCC, GPIO, DFSDM, UART4, SysTick) are controlled via **direct register manipulation** without ST HAL or standard peripheral libraries.

The system streams real-time PDM audio, applies fixed-point DSP filters on-chip, segments candidate speech events via a state machine, transfers framed binary audio payloads to host tools at **921,600 baud**, and evaluates a lightweight wake-word classifier achieving **92% test accuracy** with a sub-millisecond on-device C inference footprint.

---

## Key Engineering Highlights

- **Bare-Metal Register-Level Driver Architecture:** Direct configuration of memory-mapped registers (`RCC`, `GPIO`, `UART4`, `DFSDM`, `SysTick`) without HAL or runtime overhead. Custom linker script and startup assembly.
- **Hardware PDM to PCM Demodulation:** Interfaced onboard MP34DT01 MEMS microphone using DFSDM Channel 2 / Filter 0 with hardware **Sinc3 decimation (FOSR = 64)** yielding 24-bit PCM samples.
- **Real-Time On-Device DSP:** Single-pole high-pass IIR filter ($R=0.995$) for DC offset cancellation combined with an Exponential Moving Average (EMA, $\alpha=0.2$) envelope detector.
- **Acoustic Event Detection FSM:** Energy-gated finite-state machine with dynamic debounce ($E[n] > 500$, count $\ge 30$) and quiet-abort heuristics to reject mechanical noise and capture fixed 256-sample audio frames.
- **High-Throughput Binary Telemetry (921.6 kbps):** Dual-channel packet protocol multiplexing 13-byte real-time sample frames with 1032-byte validated PCM dump buffers over UART4 (8N2) without data loss.
- **TinyML Feature & Model Pipeline:** 82-dimensional acoustic representation (13 MFCCs + $\Delta$ + $\Delta\Delta$ + temporal RMS & Zero-Crossing Rate stats), offline train-time augmentation, and automated C header (`wake_word_model.h`) generation for single-cycle dot-product inference.

---

## System Architecture

```text
 ┌──────────────────────────────────────────────────────────────────────────┐
 │                        STM32L475 BARE-METAL FIRMWARE                     │
 │                                                                          │
 │  ┌─────────────┐   PDM    ┌────────────────┐  24-bit PCM  ┌───────────┐  │
 │  │ MEMS Mic    │ ───────> │ DFSDM1 Fltr 0  │ ───────────> │ DC-Block  │  │
 │  │ (MP34DT01)  │ PE7/PE9  │ (Sinc3, FOSR64)│              │ (R=0.995) │  │
 │  └─────────────┘          └────────────────┘              └─────┬─────┘  │
 │                                                                 │        │
 │  ┌─────────────┐         ┌─────────────────┐   Energy E[n]      │ y[n]   │
 │  │ Event FSM   │ <────── │ Energy Detector │ <──────────────────┤        │
 │  │ (256-Buffer)│         │ (EMA, α = 0.2)  │                    │        │
 │  └──────┬──────┘         └─────────────────┘                    │        │
 │         │                                                       │        │
 │         ▼ Buffer Dump (1032 B)                Live Stream (13 B)│        │
 │  ┌──────────────────────────────────────────────────────────────┴─────┐  │
 │  │                     UART4 TX @ 921600 Baud (PA0)                   │  │
 │  └──────────────────────────────────┬─────────────────────────────────┘  │
 └─────────────────────────────────────┼────────────────────────────────────┘
                                       │
                                       ▼ USB-TTL Serial
 ┌──────────────────────────────────────────────────────────────────────────┐
 │                          HOST PYTHON / TINYML SUITE                      │
 │                                                                          │
 │   ┌───────────────────────┐              ┌───────────────────────────┐   │
 │   │ serial_monitor.py     │ ───────────> │ data/hey_stm/ & other/    │   │
 │   │ (Binary Framer & CSV) │              │ (200 Labeled PCM Buffers) │   │
 │   └──────────┬────────────┘              └─────────────┬─────────────┘   │
 │              │                                         │                 │
 │              ▼                                         ▼                 │
 │   ┌───────────────────────┐              ┌───────────────────────────┐   │
 │   │ serial_plot.py        │              │ ml/train_wake_word.py     │   │
 │   │ (Real-Time Dual View) │              │ (82-Feat MFCC + Classifier)│  │
 │   └───────────────────────┘              └─────────────┬─────────────┘   │
 │                                                        │                 │
 │                                                        ▼                 │
 │                                          ┌───────────────────────────┐   │
 │                                          │ ml/artifacts/             │   │
 │                                          │  ├─ wake_word_model.joblib│   │
 │                                          │  └─ wake_word_model.h     │   │
 │                                          └───────────────────────────┘   │
 └──────────────────────────────────────────────────────────────────────────┘
```

---

## TinyML Pipeline & Model Performance

The machine learning workflow trains an ultra-compact binary classifier distinguishing the target keyword (**`hey_stm`**) from background conversations, noise, and non-target speech (**`other`**).

### Feature Engineering (82-D Vector)
Each 256-sample audio segment is normalized and processed across overlapping frames to capture temporal-spectral dynamics:
- **13 MFCCs** (Mean and Variance across time) $\rightarrow$ 26 features
- **Delta MFCCs ($\Delta$)** (Mean and Variance) $\rightarrow$ 26 features
- **Delta-Delta MFCCs ($\Delta\Delta$)** (Mean and Variance) $\rightarrow$ 26 features
- **Temporal Dynamics:** Energy RMS (Mean, Std) & Zero Crossing Rate (Mean, Std) $\rightarrow$ 4 features

### Evaluation Metrics & Validation

```text
Dataset Size:        200 audio captures (100 "hey_stm" / 100 "other")
Feature Dim:         82 floating-point values per window
Augmentation:        3x train-time jitter (random time shift, gain scaling, Gaussian noise)
```

| Metric | Result | Notes |
|---|---|---|
| **Hold-Out Test Accuracy** | **92.0%** | Evaluated on unseen 50-sample stratified test split |
| **5-Fold Cross-Validation** | **93.5% ± 4.1%** | Evaluated across unaugmented raw audio dataset |
| **Wake Word Recall (`hey_stm`)** | **100.0%** | Zero false negatives (25/25 detected) |
| **Non-Target Precision (`other`)** | **100.0%** | Zero false rejections on background negative class |
| **Inference Complexity** | $\mathcal{O}(D)$ | 82 MAC operations + 1 bias add on MCU |

### Automated C Header Export
`ml/train_wake_word.py` exports pre-scaled linear weights directly into `ml/artifacts/wake_word_model.h`, eliminating the need for a runtime matrix-multiplication library:
```c
#define WAKE_N_FEATURES 82
#define WAKE_N_SAMPLES  256

static const float WAKE_BIAS = -0.41289104f;
static const float WAKE_WEIGHTS[WAKE_N_FEATURES] = {
    0.04812310f, -0.19842109f,  0.31284910f, /* ... */
};

/* On-device evaluation: score = dot_product(features, WAKE_WEIGHTS) + WAKE_BIAS */
```

---

## Firmware & Digital Signal Processing

### 1. DFSDM PDM-to-PCM Decimation
The MP34DT01 digital MEMS microphone produces a PDM pulse stream clocked by DFSDM. Filter 0 is configured with a 3rd-order Sinc filter (`Sinc3`) and an oversampling ratio ($FOSR$) of 64:
$$\text{Full Scale Magnitude} \approx FOSR^3 = 64^3 = 262,144$$

### 2. High-Pass DC Blocker
Removes drift and ADC DC bias using a single-pole difference equation:
$$y[n] = x[n] - x[n-1] + R \cdot y[n-1] \quad (R = 0.995)$$

### 3. Exponential Moving Average (EMA) Energy Detector
Computes immediate signal power envelope:
$$E[n] = \alpha \cdot |y[n]| + (1 - \alpha) \cdot E[n-1] \quad (\alpha = 0.2)$$

### 4. Acoustic Segmentation Finite-State Machine
```text
  [ IDLE ] ──( E[n] > 500 )──> [ CAPTURING (256 Samples) ]
                                    │
    ┌───────────────────────────────┴──────────────────────────────┐
    │                                                              │
    ▼ (low_count > 250 & loud_count < 30)                          ▼ (index == 256)
[ ABORT / DISCARD ]                                     [ EVALUATE DEBOUNCE ]
    │                                                              │
    │                                        loud_count >= 30 ─────┴───── loud_count < 30
    ▼                                                │                           │
  [ IDLE ] <────────────────────────────────── [ SEND BUFFER ]              [ DISCARD ]
```

---

## Custom Binary UART Telemetry

Communication is established via **UART4** at **921,600 baud (8N2)** on pin `PA0` (Arduino header D1) to accommodate high data bandwidth without blocking the audio loop.

### Stream A: Continuous Telemetry (13 Bytes)
Emitted periodically with signal diagnostics:
```text
┌─────────────────┬──────────────────┬──────────────┬─────────────────┐
│ Sample y (int32)│ Energy E (uint32)│ Flag (uint8) │ Timer ms(uint32)│
│     4 Bytes     │     4 Bytes      │    1 Byte    │     4 Bytes     │
└─────────────────┴──────────────────┴──────────────┴─────────────────┘
```

### Stream B: Event Buffer Dump (1032 Bytes)
Dispatched immediately when speech debounce validation passes:
```text
┌─────────────────┬──────────────────────────────────┬─────────────────┐
│ Start Marker    │ 256 Signed int32 Samples (y[n]) │ End Marker      │
│  0xFFFFFFFF     │          1024 Bytes              │  0x00000000     │
└─────────────────┴──────────────────────────────────┴─────────────────┘
```

---

## Repository Structure

```text
├── src/
│   └── main.c                  # Bare-metal register firmware (DFSDM, UART, DSP, FSM)
├── ml/
│   ├── train_wake_word.py      # Feature extraction, cross-validation, model export
│   ├── requirements.txt        # ML dependencies (librosa, scikit-learn, joblib)
│   └── artifacts/              # Generated model binaries (.joblib) and C header (.h)
├── data/
│   ├── hey_stm/                # 100 labeled wake-word buffer dumps (.csv)
│   └── other/                  # 100 labeled negative/speech buffer dumps (.csv)
├── docs/
│   ├── PROTOCOL.md             # Wire frame specifications and packet alignment
│   ├── SIGNAL_PROCESSING.md    # Math derivations for IIR, EMA, and FSM
│   ├── SETUP.md                # Toolchain and hardware setup guide
│   └── PROCEDURE.md            # Calibration and tuning runbook
├── serial_monitor.py           # Host binary packet decoder and automated CSV logger
├── serial_plot.py              # Real-time multi-channel matplotlib visualization tool
├── Makefile                    # Bare-metal build and OpenOCD flash rules
├── stm32l475vg.ld              # Custom linker script
└── startup_stm32l475xx.s       # Vector table and startup assembly
```

---

## Quick Start & Reproduction

### Prerequisites
- **Embedded Toolchain:** `arm-none-eabi-gcc`, `openocd`
- **Host Environment:** Python 3.10+ with `pyserial`, `librosa`, `scikit-learn`, `matplotlib`

```bash
# 1. Install toolchain & dependencies
brew install arm-none-eabi-gcc openocd
python3 -m venv .venv && source .venv/bin/activate
pip install -r ml/requirements.txt pyserial matplotlib

# 2. Compile and flash firmware to B-L475E-IOT01A
make clean && make
make flash

# 3. Launch live monitor and visualizer
python serial_monitor.py        # Terminal 1: Handles incoming frames & saves dumps
python serial_plot.py           # Terminal 2: Real-time waveform & energy plot

# 4. Train model & generate C header
python ml/train_wake_word.py
```

---

## Hardware & Peripherals

| Component / Pin | Function | Configuration |
|---|---|---|
| **STM32L475VG** | Core MCU | ARM Cortex-M4F @ 80 MHz, 1 MB Flash, 128 KB SRAM |
| **MP34DT01** | MEMS Microphone | PDM Digital Output |
| **PE9 / PE7** | DFSDM1_CKOUT / DATIN2 | Alternate Function AF6 |
| **PA0 (D1)** | UART4 TX | 921,600 baud, 8 data bits, no parity, 2 stop bits (8N2) |
| **SysTick** | System Timer | 1 ms timebase interrupt |

---

## Technical Skills Demonstrated

- **Embedded Systems & Firmware:** Bare-metal C, Register Maps, Linker Scripts, Startup Assembly, Interrupt Handling (`SysTick`, `DFSDM`), OpenOCD, GDB, ARM Cortex-M4F Architecture.
- **Digital Signal Processing (DSP):** PDM Demodulation, Sinc Filter Decimation, Fixed/Floating IIR Filters, EMA Smoothing, MFCC Spectral Analysis, Discrete Cosine Transforms (DCT).
- **Edge AI & TinyML:** Time-series feature extraction, acoustic modeling, data augmentation, cross-validation, model compression, C-header weight export.
- **Systems & Tooling:** High-speed binary protocol design, Python hardware-in-the-loop tooling (`pyserial`, `matplotlib`), GNU Make.
