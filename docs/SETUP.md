# Setup Guide

Complete environment setup for the STM32L475 Edge AI wake-word engine.

---

## Hardware

| Item | Notes |
|------|--------|
| **B-L475E-IOT01A** | STM32L475VG Discovery kit with onboard MEMS mic |
| **ST-Link** | Built into the Discovery board (USB for flash/debug) |
| **USB–TTL adapter** | For UART RX from the board |
| **Common GND** | Required between TTL adapter and board |

### UART wiring

| USB–TTL pin | Board |
|-------------|--------|
| RX | Arduino **D1** (PA0 = UART4_TX) |
| GND | **GND** |

Do **not** connect TTL TX to the board unless you intend to send data into the MCU.

Serial settings: **921600 baud, 8 data bits, 2 stop bits, no parity (8N2)**.

Find the serial device:

```bash
# macOS
ls /dev/tty.usbserial* /dev/tty.usbmodem* 2>/dev/null

# Linux
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

---

## Software — macOS

```bash
xcode-select --install
brew install arm-none-eabi-gcc openocd
python3 -m pip install pyserial matplotlib
```

Verify:

```bash
arm-none-eabi-gcc --version
openocd --version
python3 -c "import serial, matplotlib; print('ok')"
```

---

## Software — Linux (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install -y build-essential gcc-arm-none-eabi binutils-arm-none-eabi \
  openocd python3 python3-pip
python3 -m pip install pyserial matplotlib
```

Add your user to the `dialout` group if the serial port is permission-denied:

```bash
sudo usermod -aG dialout $USER
# log out and back in
```

---

## Clone and first build

```bash
cd /path/to/stm
make clean && make
```

Expected output includes something like:

```text
   text	   data	    bss	    dec	    hex	filename
   ....	   ....	   ....	   ....	   ....	build/firmware.elf
```

Artifacts land in `build/`:

| File | Role |
|------|------|
| `build/firmware.elf` | Linked image (flash this) |
| `build/firmware.map` | Symbol / section map |
| `build/*.o` | Object files |

---

## Flash

Connect the Discovery kit USB (ST-Link), then:

```bash
make flash
```

Or explicitly:

```bash
openocd \
  -f interface/stlink.cfg \
  -f target/stm32l4x.cfg \
  -c "program build/firmware.elf verify reset exit"
```

Success looks like:

```text
** Programming Finished **
** Verified OK **
** Resetting Target **
```

The VS Code **Flash** task also deletes old `buffer_dump_*.csv` files in the repo root so the plotter only shows new dumps.

---

## Python host tools

From the repo root (Anaconda/`python` or `python3` — match what your VS Code tasks use):

```bash
# Terminal 1 — must start first
python serial_monitor.py
# or: python serial_monitor.py /dev/tty.usbserial-XXXX

# Terminal 2
python serial_plot.py
```

| Script | Output |
|--------|--------|
| `serial_monitor.py` | Hex frames on stdout; `mic_capture.csv`; `buffer_dump_NNN.csv` on events |
| `serial_plot.py` | Live PCM / energy / loud / debounce; bottom pane = latest dump |

---

## VS Code (optional)

Recommended extensions:

- **Task Buttons** (`spencerwmiles.vscode-task-buttons`) — status-bar Build / Flash / Monitor / Plotter
- Cortex-Debug or similar if you use F5 debugging (see [PROCEDURE.md](PROCEDURE.md))

Tasks are defined in [`.vscode/tasks.json`](../.vscode/tasks.json).

---

## Checklist

- [ ] `arm-none-eabi-gcc` and `openocd` installed  
- [ ] Board powered via ST-Link USB  
- [ ] TTL RX ↔ D1, GND ↔ GND  
- [ ] `make` succeeds  
- [ ] Flash reports Verified OK  
- [ ] Monitor prints frames after ~7 s warm-up  
- [ ] Plotter shows live traces; dumps appear after sustained speech  

Next: [PROCEDURE.md](PROCEDURE.md) for the day-to-day workflow and tuning.
