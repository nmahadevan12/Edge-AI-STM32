#!/usr/bin/env python3
"""Hex serial monitor for STM32 binary UART frames (matches main.c).

Normal frame (13 bytes, big-endian):
  [0:4]   signed int32    mic sample (DC-blocked)
  [4:8]   unsigned uint32 energy
  [8]     unsigned uint8  debounce_passed (0 or 1)
  [9:13]  unsigned uint32 timer_ms

Buffer dump (Send_Sample_Buffer):
  [0:4]       0xFFFFFFFF start marker
  [4:1028]    256 × signed int32 sample_buffer
  [1028:1032] 0x00000000 end marker
  Total: 1032 bytes
"""

import argparse
import sys
from pathlib import Path
from typing import List, Optional, Tuple

try:
    import serial
except ImportError:
    print("Install pyserial: pip3 install pyserial", file=sys.stderr)
    sys.exit(1)

DEFAULT_LOG = Path(__file__).resolve().parent / "mic_capture.csv"
DEFAULT_DUMP = Path(__file__).resolve().parent / "buffer_dump.csv"
FRAME_LEN = 13
MAX_TIMER_STEP_MS = 500

DUMP_START = b"\xff\xff\xff\xff"
DUMP_END = b"\x00\x00\x00\x00"
DUMP_SAMPLES = 256
DUMP_PAYLOAD_BYTES = DUMP_SAMPLES * 4  # 1024
DUMP_TOTAL_BYTES = 4 + DUMP_PAYLOAD_BYTES + 4  # 1032


def parse_frame(b: bytes):
    sample = int.from_bytes(b[0:4], "big", signed=True)
    energy = int.from_bytes(b[4:8], "big", signed=False)
    flag = b[8]
    timer = int.from_bytes(b[9:13], "big", signed=False)
    return sample, energy, flag, timer


def frame_ok(b: bytes, prev_timer: Optional[int]) -> bool:
    if len(b) != FRAME_LEN:
        return False
    flag = b[8]
    if flag not in (0, 1):
        return False
    if prev_timer is None:
        return True
    timer = int.from_bytes(b[9:13], "big", signed=False)
    dt = (timer - prev_timer) & 0xFFFFFFFF
    return dt <= MAX_TIMER_STEP_MS


def _parse_dump_payload(payload: bytes) -> Optional[list]:
    if len(payload) != DUMP_PAYLOAD_BYTES:
        return None
    samples = []
    for i in range(DUMP_SAMPLES):
        val = int.from_bytes(payload[i * 4 : (i + 1) * 4], "big", signed=True)
        samples.append(val)
    return samples


def read_aligned_frame(
    ser, prev_timer: Optional[int], buf: bytearray
) -> Tuple[Optional[bytes], Optional[List[int]], Optional[int]]:
    """Decode live frames or a buffer dump. `buf` must persist across calls."""
    while True:
        chunk = ser.read(1)
        if not chunk:
            continue
        buf.extend(chunk)

        # Dump marker is unique enough; still prefer live if sample==-1 (also FFFFFFFF).
        if len(buf) >= 4 and bytes(buf[:4]) == DUMP_START:
            if len(buf) >= FRAME_LEN and frame_ok(bytes(buf[:FRAME_LEN]), prev_timer):
                candidate = bytes(buf[:FRAME_LEN])
                del buf[:FRAME_LEN]
                timer = int.from_bytes(candidate[9:13], "big", signed=False)
                return candidate, None, timer

            if len(buf) >= DUMP_TOTAL_BYTES:
                if bytes(buf[DUMP_TOTAL_BYTES - 4 : DUMP_TOTAL_BYTES]) == DUMP_END:
                    samples = _parse_dump_payload(
                        bytes(buf[4 : 4 + DUMP_PAYLOAD_BYTES])
                    )
                    if samples is not None:
                        del buf[:DUMP_TOTAL_BYTES]
                        return None, samples, prev_timer
                del buf[0]
            continue

        while len(buf) >= FRAME_LEN:
            candidate = bytes(buf[:FRAME_LEN])
            if frame_ok(candidate, prev_timer):
                del buf[:FRAME_LEN]
                timer = int.from_bytes(candidate[9:13], "big", signed=False)
                return candidate, None, timer
            del buf[0]


def save_dump(samples: list, dump_path: Path, dump_count: int) -> None:
    path = dump_path.parent / f"{dump_path.stem}_{dump_count:03d}{dump_path.suffix}"
    with path.open("w", encoding="utf-8") as f:
        f.write("index,y\n")
        for i, v in enumerate(samples):
            f.write(f"{i},{v}\n")
    print(f"[DUMP] Saved {len(samples)} samples → {path.name}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="STM32 serial monitor: decode main.c UART frames + buffer dumps"
    )
    parser.add_argument(
        "port",
        nargs="?",
        default="/dev/tty.usbserial-FTG8MPCG",
        help="serial port path",
    )
    parser.add_argument("-b", "--baud", type=int, default=921600)
    parser.add_argument(
        "--stopbits",
        type=int,
        choices=(1, 2),
        default=2,
        help="stop bits (firmware currently uses 2)",
    )
    parser.add_argument(
        "--log",
        default=str(DEFAULT_LOG),
        help="CSV log for normal frames (timer_ms,sample,energy,flag)",
    )
    parser.add_argument(
        "--dump",
        default=str(DEFAULT_DUMP),
        help="base path for buffer dump CSVs (buffer_dump_001.csv etc.)",
    )
    parser.add_argument(
        "--append",
        action="store_true",
        help="append to the log instead of starting a new capture",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="print every live frame (default: edges/dumps only)",
    )
    args = parser.parse_args()

    stop = serial.STOPBITS_TWO if args.stopbits == 2 else serial.STOPBITS_ONE
    ser = serial.Serial(args.port, args.baud, stopbits=stop, timeout=1.0)
    ser.reset_input_buffer()

    log_path = Path(args.log)
    dump_path = Path(args.dump)
    mode = "a" if args.append else "w"
    log = log_path.open(mode, encoding="utf-8", buffering=1024 * 1024)
    if mode == "w":
        log.write("timer_ms,sample,energy,flag\n")
        log.flush()

    print(f"Opened {args.port} @ {args.baud} stopbits={args.stopbits}")
    print(f"Normal frames → {log_path.name} (13 bytes)")
    print(f"Buffer dumps  → {dump_path.stem}_NNN.csv (1032 bytes: FFFFFFFF + 1024 + 00000000)")
    print("Syncing to frame boundary...")
    if not args.verbose:
        print("(quiet console — use -v for every frame)")
    print("Ctrl+C to stop\n")

    prev_timer = None
    dump_count = 0
    buf = bytearray()
    last_flag = None
    frames_since_flush = 0

    try:
        while True:
            frame, dump_samples, prev_timer = read_aligned_frame(ser, prev_timer, buf)

            if dump_samples is not None:
                print("[DUMP] Send_Sample_Buffer")
                dump_count += 1
                save_dump(dump_samples, dump_path, dump_count)
                print(
                    f"[DUMP] {len(dump_samples)} samples, "
                    f"peak={max(abs(v) for v in dump_samples)}, "
                    f"min={min(dump_samples)}, max={max(dump_samples)}"
                )
                last_flag = 0
                log.flush()
                continue

            b = frame
            sample, energy, flag, timer = parse_frame(b)
            log.write(f"{timer},{sample},{energy},{flag}\n")
            frames_since_flush += 1
            if frames_since_flush >= 32:
                log.flush()
                frames_since_flush = 0

            edge = last_flag is None or flag != last_flag
            last_flag = flag
            if args.verbose or edge:
                print(
                    f"{b[0:4].hex(' ')}  {b[4:8].hex(' ')}  {b[8]:02x}  {b[9:13].hex(' ')}  "
                    f"sample={sample}  energy={energy}  loud={int(energy > 1000)}  "
                    f"debounce={flag}  t={timer}"
                )

    except KeyboardInterrupt:
        print("\nClosed")
    finally:
        log.close()
        ser.close()


if __name__ == "__main__":
    main()
