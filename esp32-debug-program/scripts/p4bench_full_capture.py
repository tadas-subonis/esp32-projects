#!/usr/bin/env python3
"""Full P4Bench milestone capture. Usage: python scripts/p4bench_full_capture.py COM5"""
from __future__ import annotations

import sys
import time
from pathlib import Path

import serial


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
    out_path = Path("logs") / "p4bench-full.txt"
    out_path.parent.mkdir(exist_ok=True)

    ser = serial.Serial(port, 115200, timeout=0.25)
    time.sleep(0.3)
    ser.reset_input_buffer()
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.05)
    ser.setRTS(False)

    chunks: list[str] = []
    deadline = time.time() + 10.0
    while time.time() < deadline:
        data = ser.read(4096)
        if data:
            chunks.append(data.decode("utf-8", "replace"))
            joined = "".join(chunks)
            if "P4Bench ready" in joined or "Type 'help'" in joined:
                time.sleep(0.5)
                break

    def send(cmd: str, wait: float) -> None:
        print(f">>> {cmd.strip()}  (wait {wait}s)", flush=True)
        ser.write(cmd.encode("ascii"))
        ser.flush()
        end = time.time() + wait
        while time.time() < end:
            data = ser.read(8192)
            if data:
                text = data.decode("utf-8", "replace")
                chunks.append(text)
                for line in text.splitlines():
                    if any(
                        k in line
                        for k in (
                            "JSONL",
                            "RESULT",
                            "CROSSOVER",
                            "chunk_rows",
                            "AUTO",
                            "->",
                            "fps",
                            "MEMORY",
                            "END ",
                            "pct=",
                        )
                    ):
                        print(line.encode("ascii", "replace").decode("ascii"), flush=True)

    # M2 leftovers / memory / crossover
    send("meta\n", 1.0)
    send("membench\n", 8.0)
    send("crossover 2\n", 120.0)  # 8 pcts * 2 modes * (0.8+2)s ≈ 45s+ margin

    # M3 dirty sprites
    send("set mode dirty\n", 0.3)
    send("set sprite_w 32\n", 0.2)
    send("set sprite_h 32\n", 0.2)
    send("set overlay off\n", 0.2)
    send("auto sprites 15 2\n", 90.0)

    # M5 particles (LCD-capped; still records sim)
    send("set mode full\n", 0.3)
    send("auto particles 10 2\n", 60.0)

    # tiles + compose short runs
    send("set layers 2\n", 0.2)
    send("run tiles\n", 6.0)
    send("report\n", 1.5)
    send("stop\n", 0.5)
    send("run compose\n", 8.0)
    send("report\n", 1.5)
    send("stop\n", 0.5)

    # M6 headless TD — CPU limited
    send("auto simulation 60 2\n", 90.0)
    send("auto simulation 30 2\n", 90.0)

    # M7 visual TD + chaos
    send("set mode full\n", 0.3)
    send("auto td 10 2\n", 60.0)
    send("set enemies 200\n", 0.2)
    send("set towers 40\n", 0.2)
    send("set projectiles 200\n", 0.2)
    send("set sprites 40\n", 0.2)
    send("set particles 300\n", 0.2)
    send("run chaos\n", 8.0)
    send("report\n", 1.5)
    send("stop\n", 0.5)

    text = "".join(chunks)
    out_path.write_text(text, encoding="utf-8")
    print(f"\nWrote {out_path} ({len(text)} bytes)")
    ser.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
