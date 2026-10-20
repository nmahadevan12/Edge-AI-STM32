# Edge AI Wake-Word Engine — STM32L475 (Bare Metal)

Bare-metal wake-word **capture and energy-detect pipeline** on the **B-L475E-IOT01A** (`STM32L475VG`, Cortex-M4F @ 80 MHz), plus a host Python toolchain that trains an MFCC logistic classifier and exports weights to C.

All peripherals (RCC, GPIO, DFSDM, UART4, SysTick) are driven by **direct register access** — no ST HAL. Firmware streams PDM audio, runs on-chip DSP and an event FSM, and ships framed PCM over UART at **921,600 baud (8N2)**.

## What runs where

| On the STM32 today | On the host today |
| --- | --- |
| DFSDM PDM→PCM, DC block, EMA energy, event FSM, UART framing | librosa MFCC / Δ / ΔΔ features, training, evaluation |
| Exported 82-term dot-product + bias (`wake_word_model.h`) against a feature vector | Building that 82-D vector from labeled dumps |

MFCC extraction is **not** ported to the M4 yet. Do not read this as a fully on-device TinyML detector until features and a measured latency / RAM / flash budget land on chip.

## Key pieces

- **Bare-metal drivers:** custom linker script + startup assembly; RCC / GPIO / UART4 / DFSDM / SysTick by register.
- **Hardware PDM→PCM:** MP34DT01 via DFSDM Filter 0, Sinc3, **FOSR = 64**.
- **On-device DSP:** DC block `R = 0.995`; EMA energy `α = 0.2`.
- **Event FSM (matches `src/main.c`):** start capture when `energy > ENERGY_LOUD` (**1500**); keep the 256-sample window only if `loud_count >= DEBOUNCE_VAL` (**20**). Quiet-abort discards mechanical bumps.
- **UART telemetry:** 13-byte live frames + 1032-byte event dumps (`0xFFFFFFFF` … samples … `0x00000000`). Markers are in-band; the host parses **fixed-length** frames after a dump is signaled, so a colliding sample cannot shift framing.
- **Host ML:** 82-D features → logistic regression → `ml/artifacts/wake_word_model.h`.

## Results (hold-out, 50-clip stratified split)

Dataset: **200** board-captured clips (100 `hey_stm` / 100 `other`). Speaker count and ambient false-accepts/hour were not measured.

| Metric | Result |
| --- | --- |
| Hold-out accuracy | **92.0%** (46 / 50) |
| 5-fold CV | **93.5% ± 4.1%** |
| Wake-word recall | **100%** (25 / 25) |
| False accepts on negatives | **4 / 25 (16% FAR)** |
| Wake-word precision | **≈ 86%** (25 / 29 predicted wake) |

All four hold-out errors were **false accepts**. “Non-target precision 100%” in older notes was wrong — that phrase mixed up recall with precision. Accuracy alone overstates how safe an always-listening wake word is.

Scoring cost on-device (once features exist) is an **82-MAC + bias** linear score — not “single-cycle,” and no wall-clock inference time is claimed here.

## Architecture

```text
 MEMS mic → DFSDM (Sinc3, FOSR 64) → DC block → EMA energy → event FSM
                                                              ↓
                                                    UART4 @ 921600
                                                              ↓
                              serial_monitor / serial_plot → data/*.csv
                                                              ↓
                                         ml/train_wake_word.py → wake_word_model.h
```

## Quick start

```bash
brew install arm-none-eabi-gcc openocd
python3 -m venv .venv && source .venv/bin/activate
pip install -r ml/requirements.txt pyserial matplotlib

make clean && make && make flash
python serial_monitor.py    # Terminal 1
python serial_plot.py       # Terminal 2
python ml/train_wake_word.py
```

See `docs/PROTOCOL.md`, `docs/SIGNAL_PROCESSING.md`, `docs/SETUP.md`, and `docs/PROCEDURE.md` for wire format, DSP notes, and bring-up.

## Hardware

| Pin / part | Role |
| --- | --- |
| STM32L475VG | Cortex-M4F @ 80 MHz |
| MP34DT01 | PDM MEMS mic |
| PE9 / PE7 | DFSDM1_CKOUT / DATIN2 (AF6) |
| PA0 (D1) | UART4 TX, 921600 8N2 |
| SysTick | 1 ms timebase |
