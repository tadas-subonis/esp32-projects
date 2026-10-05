#!/usr/bin/env python3
"""Capture a P4Bench baseline from serial. Usage: python scripts/p4bench_capture.py COM5"""
from __future__ import annotations

import sys
import time
from pathlib import Path

import serial


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
    out_path = Path("logs") / "p4bench-baseline.txt"
    out_path.parent.mkdir(exist_ok=True)

    ser = serial.Serial(port, 115200, timeout=0.25)
    time.sleep(0.3)
    ser.reset_input_buffer()
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.05)
    ser.setRTS(False)

    chunks: list[str] = []
    deadline = time.time() + 8.0
    while time.time() < deadline:
        data = ser.read(4096)
        if data:
            chunks.append(data.decode("utf-8", "replace"))
            if "P4Bench ready" in "".join(chunks) or "Type 'help'" in "".join(chunks):
                break

    def send(cmd: str, wait: float) -> None:
        ser.write(cmd.encode("ascii"))
        ser.flush()
        end = time.time() + wait
        while time.time() < end:
            data = ser.read(8192)
            if data:
                chunks.append(data.decode("utf-8", "replace"))

    send("meta\n", 1.5)
    send("chunks\n", 30.0)
    send("stop\n", 0.5)
    send("set sprites 100\n", 0.3)
    send("set mode full\n", 0.3)
    send("run sprites\n", 5.0)
    send("report\n", 1.5)
    send("stop\n", 0.5)
    send("auto sprites 20 2\n", 50.0)

    text = "".join(chunks)
    out_path.write_text(text, encoding="utf-8")
    # Also print ASCII-safe summary to console
    for line in text.splitlines():
        if any(
            k in line
            for k in (
                "P4BENCH",
                "JSONL",
                "chunk_rows",
                "FPS",
                "RESULT",
                "spi_hz",
                "full_frame",
                "AUTO",
                "->",
            )
        ):
            print(line.encode("ascii", "replace").decode("ascii"))
    print(f"\nWrote {out_path} ({len(text)} bytes)")
    ser.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
