#!/usr/bin/env python3
"""
60 MHz SPI endurance host orchestrator.

Phases:
  continuous   — board runs `endurance N` (default 45 min); watch LCD
  resets       — RTS reset ×N, each followed by a short endurance probe
  coldboots    — same as resets but longer settle (simulates cold boot)
  powercycles  — prompts you to unplug/replug USB ×N, then probe

Usage:
  python scripts/p4bench_endurance.py COM5 --all
  python scripts/p4bench_endurance.py COM5 --continuous 45
  python scripts/p4bench_endurance.py COM5 --resets 20 --coldboots 10 --powercycles 10
  python scripts/p4bench_endurance.py COM5 --probe   # 1-minute smoke

Close any other monitor on COM5 first.
"""
from __future__ import annotations

import argparse
import re
import sys
import time
from datetime import datetime
from pathlib import Path

import serial

READY_RE = re.compile(r"P4Bench ready", re.M)
DONE_RE = re.compile(
    r'JSONL \{"test":"endurance_done".*?"ok":(true|false)',
    re.M,
)
RESULT_RE = re.compile(r"<<< \{.*\"cmd\":\"endurance\".*\}")
HB_RE = re.compile(r"^HB |JSONL \{\"test\":\"endurance_hb\"")


def _ts() -> str:
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def _hard_reset(ser: serial.Serial) -> None:
    ser.dtr = False
    ser.rts = True
    time.sleep(0.12)
    ser.rts = False
    time.sleep(0.05)


def _read_until(
    ser: serial.Serial,
    deadline: float,
    *,
    patterns: list[re.Pattern[str]],
    log: list[str],
    echo: bool = True,
) -> re.Match[str] | None:
    buf = ""
    while time.time() < deadline:
        chunk = ser.read(4096)
        if not chunk:
            continue
        text = chunk.decode("utf-8", errors="replace")
        buf += text
        log.append(text)
        if echo:
            try:
                print(text, end="", flush=True)
            except UnicodeEncodeError:
                sys.stdout.buffer.write(text.encode("utf-8", errors="replace"))
                sys.stdout.flush()
        for pat in patterns:
            m = pat.search(buf)
            if m:
                return m
    return None


def _wait_ready(ser: serial.Serial, log: list[str], timeout_s: float = 20.0) -> bool:
    ser.reset_input_buffer()
    m = _read_until(ser, time.time() + timeout_s, patterns=[READY_RE], log=log)
    return m is not None


def _send(ser: serial.Serial, cmd: str) -> None:
    ser.write((cmd.rstrip("\r\n") + "\n").encode("utf-8"))
    ser.flush()


def _run_endurance(
    ser: serial.Serial,
    minutes: int,
    log: list[str],
    *,
    label: str,
) -> bool:
    print(f"\n--- {label}: endurance {minutes} min ---\n", flush=True)
    _send(ser, f"endurance {minutes}")
    # Allow minutes + 3 min slack for init/summary
    deadline = time.time() + (minutes * 60) + 180
    m = _read_until(ser, deadline, patterns=[DONE_RE], log=log)
    if not m:
        print(f"\n[{label}] TIMEOUT waiting for endurance_done\n", flush=True)
        return False
    ok = m.group(1) == "true"
    print(f"\n[{label}] endurance_done ok={ok}\n", flush=True)
    return ok


def phase_continuous(ser: serial.Serial, minutes: int, log: list[str]) -> bool:
    if not _wait_ready(ser, log, 25.0):
        print("FAIL: board not ready", flush=True)
        return False
    return _run_endurance(ser, minutes, log, label="continuous")


def phase_resets(
    ser: serial.Serial,
    count: int,
    log: list[str],
    *,
    settle_s: float,
    probe_min: int,
    label: str,
) -> tuple[int, int]:
    passed = 0
    for i in range(1, count + 1):
        print(f"\n=== {label} {i}/{count} ===\n", flush=True)
        _hard_reset(ser)
        time.sleep(settle_s)
        if not _wait_ready(ser, log, 25.0):
            print(f"[{label} {i}] FAIL: no ready after reset", flush=True)
            continue
        if _run_endurance(ser, probe_min, log, label=f"{label}-{i}"):
            passed += 1
        else:
            print(f"[{label} {i}] FAIL", flush=True)
    return passed, count


def phase_powercycles(
    ser: serial.Serial,
    count: int,
    log: list[str],
    probe_min: int,
) -> tuple[int, int]:
    passed = 0
    for i in range(1, count + 1):
        print(f"\n=== powercycle {i}/{count} ===", flush=True)
        print("1) Unplug USB (board power off)", flush=True)
        print("2) Wait ~2 s", flush=True)
        print("3) Plug USB back in", flush=True)
        input(f"Press Enter when board is powered for cycle {i}/{count}... ")
        # Port may disappear/reappear — reopen
        port = ser.port
        baud = ser.baudrate
        try:
            ser.close()
        except Exception:
            pass
        time.sleep(1.5)
        for attempt in range(15):
            try:
                ser.port = port
                ser.baudrate = baud
                ser.timeout = 0.2
                ser.dtr = False
                ser.rts = False
                ser.open()
                ser.dtr = False
                ser.rts = False
                break
            except Exception as e:
                print(f"  reopen attempt {attempt + 1}: {e}", flush=True)
                time.sleep(1.0)
        else:
            print(f"[powercycle {i}] FAIL: could not reopen {port}", flush=True)
            continue
        if not _wait_ready(ser, log, 30.0):
            print(f"[powercycle {i}] FAIL: no ready", flush=True)
            continue
        if _run_endurance(ser, probe_min, log, label=f"powercycle-{i}"):
            passed += 1
    return passed, count


def main() -> int:
    ap = argparse.ArgumentParser(description="P4Bench 60 MHz endurance host suite")
    ap.add_argument("port", nargs="?", default="COM5")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--continuous", type=int, metavar="MIN", help="Long run minutes")
    ap.add_argument("--resets", type=int, default=0, help="RTS reset cycles")
    ap.add_argument("--coldboots", type=int, default=0, help="RTS reset + long settle")
    ap.add_argument("--powercycles", type=int, default=0, help="Manual power cycles")
    ap.add_argument("--probe-min", type=int, default=1, help="Minutes per reset/power probe")
    ap.add_argument("--cold-settle", type=float, default=5.0, help="Seconds after cold reset")
    ap.add_argument("--reset-settle", type=float, default=1.0, help="Seconds after RTS reset")
    ap.add_argument("--probe", action="store_true", help="1-minute smoke only")
    ap.add_argument(
        "--all",
        action="store_true",
        help="continuous 45 + resets 20 + coldboots 10 + powercycles 10",
    )
    ap.add_argument("--out", type=str, default="")
    args = ap.parse_args()

    if args.probe:
        args.continuous = 1
        args.resets = 0
        args.coldboots = 0
        args.powercycles = 0
    elif args.all:
        if args.continuous is None:
            args.continuous = 45
        if args.resets == 0:
            args.resets = 20
        if args.coldboots == 0:
            args.coldboots = 10
        if args.powercycles == 0:
            args.powercycles = 10
    elif args.continuous is None and args.resets == 0 and args.coldboots == 0 and args.powercycles == 0:
        # Default: full suite as requested
        args.continuous = 45
        args.resets = 20
        args.coldboots = 10
        args.powercycles = 10

    out = Path(args.out) if args.out else Path("logs") / f"{_ts()}_endurance_{args.port}.log"
    out.parent.mkdir(parents=True, exist_ok=True)
    log: list[str] = []

    print(f"Logging to {out}", flush=True)
    print(
        f"Plan: continuous={args.continuous} resets={args.resets} "
        f"coldboots={args.coldboots} powercycles={args.powercycles} "
        f"probe_min={args.probe_min}",
        flush=True,
    )
    print("Watch the LCD for corruption / shifted lines / wrong colors / lockups.", flush=True)

    summary: dict[str, object] = {}
    overall_ok = True

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        ser.dtr = False
        ser.rts = False

        if args.continuous and args.continuous > 0:
            _hard_reset(ser)
            time.sleep(args.reset_settle)
            ok = phase_continuous(ser, args.continuous, log)
            summary["continuous"] = ok
            overall_ok = overall_ok and ok

        if args.resets > 0:
            p, n = phase_resets(
                ser,
                args.resets,
                log,
                settle_s=args.reset_settle,
                probe_min=args.probe_min,
                label="reset",
            )
            summary["resets"] = f"{p}/{n}"
            overall_ok = overall_ok and (p == n)

        if args.coldboots > 0:
            p, n = phase_resets(
                ser,
                args.coldboots,
                log,
                settle_s=args.cold_settle,
                probe_min=args.probe_min,
                label="coldboot",
            )
            summary["coldboots"] = f"{p}/{n}"
            overall_ok = overall_ok and (p == n)

        if args.powercycles > 0:
            p, n = phase_powercycles(ser, args.powercycles, log, args.probe_min)
            summary["powercycles"] = f"{p}/{n}"
            overall_ok = overall_ok and (p == n)

    out.write_text("".join(log), encoding="utf-8")
    print("\n======== ENDURANCE SUITE SUMMARY ========", flush=True)
    for k, v in summary.items():
        print(f"  {k}: {v}", flush=True)
    print(f"  overall: {'PASS' if overall_ok else 'FAIL'}", flush=True)
    print(f"  log: {out}", flush=True)
    return 0 if overall_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
