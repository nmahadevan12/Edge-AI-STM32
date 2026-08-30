# Edge AI Wake Word Engine — STM32L475 (Bare Metal)

Register-level firmware and host tooling for an on-device wake-word engine on the **B-L475E-IOT01A** (`STM32L475VG`). No HAL — peripherals are programmed via register maps in [`src/main.c`](src/main.c).

**Goal:** capture clean, fixed-length audio windows from the onboard MEMS microphone, then train and deploy on-device inference for a “hey STM32” wake word.

---

## Documentation

| Guide | Contents |
|-------|----------|
| **[docs/SETUP.md](docs/SETUP.md)** | Toolchain, wiring, first build/flash, Python deps |
| **[docs/PROCEDURE.md](docs/PROCEDURE.md)** | Day-to-day run loop, tuning, debugging, failure modes |
| **[docs/SIGNAL_PROCESSING.md](docs/SIGNAL_PROCESSING.md)** | DC blocker / EMA equations, capture state machine |
| **[docs/PROTOCOL.md](docs/PROTOCOL.md)** | UART live frames + buffer-dump framing |

---

## Highlights

- **Bare-metal STM32L4** — RCC, GPIO, UART4, SysTick, DFSDM at the register level
- **PDM → PCM** — DFSDM channel 2 / filter 0 (Sinc3, FOSR 64) on PE7/PE9
- **Real-time DSP** — DC-blocking IIR, EMA energy, energy-gated 256-sample capture with debounce + quiet abort
- **Event dumps** — 1024-byte PCM windows over UART with start/end markers
- **Host tools** — serial monitor + live dual-pane plotter for calibration and dataset collection

---

## Architecture

```text
MEMS mic (PDM) → DFSDM (Sinc3) → DC blocker → EMA energy → Sound_Detect
                                                              │
                              UART4 @ 921600 8N2 (PA0 / D1) ←─┘
                                 ├─ 13-byte live frames
                                 └─ 1026-byte event dumps
```

Capture summary: start on first `energy > 500`, store every PCM sample for 256 frames, keep/send only if ≥ `DEBOUNCE_VAL` loud frames; abort early if quiet too long before debounce. Details and equations: [SIGNAL_PROCESSING.md](docs/SIGNAL_PROCESSING.md).

---

## Quick start

Full install and wiring: **[SETUP.md](docs/SETUP.md)**. Everyday workflow: **[PROCEDURE.md](docs/PROCEDURE.md)**.

```bash
# 1. Dependencies (macOS example)
brew install arm-none-eabi-gcc openocd
pip install pyserial matplotlib

# 2. Wire USB-TTL RX → Arduino D1 (PA0), GND → GND  (921600 8N2)

# 3. Build & flash
make clean && make
make flash

# 4. Host tools (monitor first)
python serial_monitor.py          # → mic_capture.csv, buffer_dump_NNN.csv
python serial_plot.py             # live PCM / energy / flag + latest dump
```

Wait ~7 s after reset (warm-up). Speak into the onboard mic; watch for `[DUMP]` in the monitor and a waveform in the plotter bottom pane.

---

## Skills demonstrated

| Area | Detail |
|------|--------|
| Embedded C | Bare-metal drivers, ISRs, linker script, startup assembly |
| Digital audio | PDM/DFSDM, DC removal, EMA energy, event segmentation |
| Protocols | Custom binary UART framing, host reassembly |
| Tooling | Make + OpenOCD; Python (`pyserial`, `matplotlib`) debug UI |
| Systems | Sensor → MCU → PC path for ML data collection |

---

## Repository layout

| Path | Role |
|------|------|
| [`src/main.c`](src/main.c) | Firmware |
| [`startup_stm32l475xx.s`](startup_stm32l475xx.s) | Startup / vectors |
| [`stm32l475vg.ld`](stm32l475vg.ld) | Linker script |
| [`Makefile`](Makefile) | Build / flash |
| [`serial_monitor.py`](serial_monitor.py) | UART parser → CSV |
| [`serial_plot.py`](serial_plot.py) | Live visualizer |
| [`docs/`](docs/) | Setup, procedure, math, protocol |

---

## Status & next steps

**Done:** bare-metal audio capture pipeline, gated fixed-window buffering, UART export, host visualization.

**Next:** labeled dataset → features (e.g. MFCC) → train classifier → quantize and run inference on the MCU.

---

## References

| Document | Use |
|----------|-----|
| [RM0351](https://www.st.com/resource/en/reference_manual/rm0351-stm32l47xxx-stm32l48xxx-stm32l49xxx-and-stm32l4axxx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) | RCC, GPIO, UART4, DFSDM |
| [STM32L475VG](https://www.st.com/en/microcontrollers-microprocessors/stm32l475vg.html) | Pin AF table |
| [B-L475E-IOT01A / UM2153](https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html) | Board pinout, mic |
| PM0214 | SysTick, vector table |
