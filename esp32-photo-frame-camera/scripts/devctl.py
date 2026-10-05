#!/usr/bin/env python3
from __future__ import annotations

import argparse
import dataclasses
import errno
import json
import os
import re
import sys
import time
from pathlib import Path
from typing import IO, Any

try:
    import serial  # type: ignore
    import serial.tools.list_ports  # type: ignore
except Exception as e:  # pragma: no cover
    print(
        "devctl: missing dependency. Install with:\n"
        "  python3 -m pip install --user -U -r scripts/requirements-tools.txt\n",
        file=sys.stderr,
    )
    raise


RESULT_PREFIX = "<<< "

# Device is often still booting (or in ROM download) right after the host opens the port.
_READY_PATTERNS = [
    re.compile(r"REPL started"),
    re.compile(r"listening on /api/v1/photo"),
    re.compile(r"cores3.*boot", re.IGNORECASE),
    re.compile(r"ready paper=", re.IGNORECASE),
    re.compile(r"^pf>\s*$"),
]


def _now_ms() -> int:
    return int(time.time() * 1000)


def _eprint(msg: str) -> None:
    print(msg, file=sys.stderr)


def _open_serial(port: str, baud: int, timeout_s: float, *, write_timeout_s: float | None = None) -> serial.Serial:
    try:
        ser = serial.Serial()
        ser.port = port
        ser.baudrate = baud
        ser.timeout = timeout_s
        ser.write_timeout = timeout_s if write_timeout_s is None else write_timeout_s
        # Set before open so we don't pulse the chip into USB download mode.
        ser.dtr = False
        ser.rts = False
        ser.open()
        ser.dtr = False
        ser.rts = False
        return ser
    except OSError as e:
        if getattr(e, "errno", None) in (errno.EACCES, errno.EBUSY):
            raise RuntimeError(
                f"Port {port} is busy or permission denied. "
                "Close serial monitors (PlatformIO, idf.py monitor, etc.). "
                "On Linux/WSL, ensure you are in the 'dialout' group."
            ) from e
        raise
    except serial.SerialException as e:
        raise RuntimeError(
            f"Failed to open {port}. Is the device connected (COM* on Windows, "
            "/dev/ttyACM* in WSL) and unused by another program?"
        ) from e


def _read_chunk(ser: serial.Serial, bufsize: int = 4096) -> bytes:
    """Read available serial data (works on Windows and POSIX; no select)."""
    waiting = ser.in_waiting
    if waiting:
        return ser.read(min(waiting, bufsize))
    return ser.read(1)


def _iter_lines(
    ser: serial.Serial,
    deadline_ms: int,
    *,
    echo: bool,
    out: IO[str] | None,
    until: re.Pattern[str] | None,
) -> dict[str, Any] | None:
    buf = bytearray()
    while _now_ms() < deadline_ms:
        chunk = _read_chunk(ser)
        if not chunk:
            continue
        buf.extend(chunk)
        while True:
            nl = buf.find(b"\n")
            if nl < 0:
                break
            raw = bytes(buf[: nl + 1])
            del buf[: nl + 1]
            try:
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            except Exception:
                line = str(raw)

            if echo:
                print(line)
            if out is not None:
                out.write(line + "\n")
                out.flush()

            if until is not None and until.search(line):
                return {"event": "until", "line": line}

            if line.startswith(RESULT_PREFIX):
                payload = line[len(RESULT_PREFIX) :].strip()
                try:
                    return json.loads(payload)
                except json.JSONDecodeError:
                    return {"ok": False, "error": "invalid_json", "raw": payload}

    return None


def _wait_device_ready(ser: serial.Serial, timeout_s: float) -> bool:
    """Read until a boot/REPL-ready line appears (or timeout)."""
    deadline_ms = _now_ms() + int(timeout_s * 1000)
    buf = bytearray()
    while _now_ms() < deadline_ms:
        chunk = _read_chunk(ser)
        if not chunk:
            continue
        buf.extend(chunk)
        while True:
            nl = buf.find(b"\n")
            if nl < 0:
                break
            raw = bytes(buf[: nl + 1])
            del buf[: nl + 1]
            line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
            for pat in _READY_PATTERNS:
                if pat.search(line):
                    return True
    return False


def cmd_ports(_args: argparse.Namespace) -> int:
    ports = list(serial.tools.list_ports.comports())
    data: list[dict[str, Any]] = []
    for p in ports:
        data.append(
            {
                "device": p.device,
                "description": p.description,
                "hwid": p.hwid,
                "vid": getattr(p, "vid", None),
                "pid": getattr(p, "pid", None),
                "serial_number": getattr(p, "serial_number", None),
                "manufacturer": getattr(p, "manufacturer", None),
                "product": getattr(p, "product", None),
                "location": getattr(p, "location", None),
            }
        )
    print(json.dumps({"ok": True, "ports": data}, indent=2, sort_keys=True))
    return 0


def _classify_from_logs(text: str) -> str | None:
    if "octal_psram" in text or "Failed to init external RAM" in text:
        return "papercolor_fw_psram_error"
    if "photo-frame receiver boot" in text or "pf>" in text:
        return "papercolor"
    if "cores3" in text and "boot" in text:
        return "cores3"
    if "ESP-IDF" in text and "photo_frame_receiver" in text:
        return "papercolor"
    return None


def cmd_identify(args: argparse.Namespace) -> int:
    ports = list(serial.tools.list_ports.comports())
    results: list[dict[str, Any]] = []

    for p in ports:
        if args.only and p.device not in args.only:
            continue

        entry: dict[str, Any] = {
            "device": p.device,
            "description": p.description,
            "hwid": p.hwid,
            "vid": getattr(p, "vid", None),
            "pid": getattr(p, "pid", None),
            "serial_number": getattr(p, "serial_number", None),
            "manufacturer": getattr(p, "manufacturer", None),
            "product": getattr(p, "product", None),
            "location": getattr(p, "location", None),
            "probe": None,
        }

        try:
            with _open_serial(p.device, args.baud, timeout_s=0.2) as ser:
                ser.reset_input_buffer()
                ser.reset_output_buffer()

                # Sniff spontaneous logs first (boot banners, crash loops).
                sniff_deadline = _now_ms() + int(args.sniff * 1000)
                sniff_lines: list[str] = []
                while _now_ms() < sniff_deadline:
                    chunk = _read_chunk(ser)
                    if not chunk:
                        continue
                    sniff_lines.extend(
                        chunk.decode("utf-8", errors="replace").splitlines()
                    )
                sniff_text = "\n".join(sniff_lines)
                entry["sniff"] = sniff_lines[-8:] if sniff_lines else []
                kind_from_logs = _classify_from_logs(sniff_text)

                if kind_from_logs is None:
                    _wait_device_ready(ser, max(0.0, args.sniff))

                def _try(cmd: str) -> dict[str, Any] | None:
                    ser.write((cmd + "\n").encode("utf-8"))
                    ser.flush()
                    deadline_ms = _now_ms() + int(args.timeout * 1000)
                    res = _iter_lines(ser, deadline_ms, echo=False, out=None, until=None)
                    return res

                res = _try("version")
                if not isinstance(res, dict) or not res.get("ok"):
                    res = _try("status")

                entry["probe"] = res

                kind = kind_from_logs or "unknown"
                if isinstance(res, dict):
                    cmd = str(res.get("cmd", ""))
                    if cmd == "version":
                        kind = "papercolor"
                    elif cmd == "status":
                        if "paper_host" in res or "rssi" in res:
                            kind = "cores3"
                        elif "sd_mounted" in res or "ap_clients" in res:
                            kind = "papercolor"
                entry["kind"] = kind

        except Exception as e:
            entry["kind"] = "unavailable"
            entry["error"] = str(e)

        results.append(entry)

    print(json.dumps({"ok": True, "devices": results}, indent=2, sort_keys=True))
    return 0


def _default_out_path(port: str) -> Path:
    safe = port.replace("/", "_").replace(":", "_")
    ts = time.strftime("%Y%m%d_%H%M%S")
    return Path("logs") / f"{ts}{safe}.log"


def cmd_logs(args: argparse.Namespace) -> int:
    out_path: Path | None = Path(args.out) if args.out else None
    if out_path is None:
        out_path = _default_out_path(args.port) if args.save else None

    out_f: IO[str] | None = None
    if out_path is not None:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_f = out_path.open("w", encoding="utf-8")

    until_re = re.compile(args.until) if args.until else None
    deadline_ms = _now_ms() + int(args.seconds * 1000)

    try:
        with _open_serial(args.port, args.baud, timeout_s=0.2) as ser:
            ser.reset_input_buffer()
            if args.raw:
                total = 0
                while _now_ms() < deadline_ms:
                    chunk = _read_chunk(ser)
                    if not chunk:
                        continue
                    total += len(chunk)
                    text = chunk.decode("utf-8", errors="replace")
                    print(text, end="", flush=True)
                    if out_f is not None:
                        out_f.write(text)
                        out_f.flush()
                    if until_re is not None and until_re.search(text):
                        _eprint(json.dumps({"ok": True, "event": "until", "bytes": total}))
                        return 0
                _eprint(json.dumps({"ok": True, "event": "done", "bytes": total}))
                return 0

            res = _iter_lines(ser, deadline_ms, echo=True, out=out_f, until=until_re)
    finally:
        if out_f is not None:
            out_f.close()

    if res is None:
        return 0

    # If `--until` matched, return success (0) but emit JSON summary to stderr for agents.
    if isinstance(res, dict) and res.get("event") == "until":
        _eprint(json.dumps({"ok": True, "event": "until", "line": res.get("line")}))
        return 0

    # If the device emitted a result line during log capture, surface it too.
    _eprint(json.dumps({"ok": True, "event": "result", "result": res}))
    return 0


def cmd_cmd(args: argparse.Namespace) -> int:
    deadline_ms = _now_ms() + int(args.timeout * 1000)
    until_re = re.compile(args.until) if args.until else None

    out_path: Path | None = Path(args.out) if args.out else None
    out_f: IO[str] | None = None
    if out_path is not None:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_f = out_path.open("w", encoding="utf-8")

    try:
        with _open_serial(args.port, args.baud, timeout_s=0.2, write_timeout_s=args.timeout) as ser:
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            _wait_device_ready(ser, args.boot_wait)
            line = args.command.rstrip("\r\n") + "\n"
            ser.write(line.encode("utf-8"))
            ser.flush()

            res = _iter_lines(ser, deadline_ms, echo=True, out=out_f, until=until_re)
    finally:
        if out_f is not None:
            out_f.close()

    if res is None:
        print(json.dumps({"ok": False, "error": "timeout"}))
        return 2

    # If `--until` matched, treat as success and return JSON summary.
    if isinstance(res, dict) and res.get("event") == "until":
        print(json.dumps({"ok": True, "event": "until", "line": res.get("line")}))
        return 0

    ok = bool(res.get("ok")) if isinstance(res, dict) else False
    print(json.dumps(res))
    return 0 if ok else 1


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(prog="devctl", description="PhotoFrame dev helper (serial logs + commands)")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("ports", help="List serial ports (JSON)")
    sp.set_defaults(func=cmd_ports)

    sp = sub.add_parser("identify", help="Probe ports and guess which device is which (JSON)")
    sp.add_argument("--baud", type=int, default=115200)
    sp.add_argument("--timeout", type=float, default=2.0)
    sp.add_argument("--sniff", type=float, default=1.5, help="Seconds to read logs before probing")
    sp.add_argument(
        "--only",
        action="append",
        help="Only probe this port (can be repeated)",
    )
    sp.set_defaults(func=cmd_identify)

    sp = sub.add_parser("logs", help="Stream logs from a serial port")
    sp.add_argument("--port", required=True)
    sp.add_argument("--baud", type=int, default=115200)
    sp.add_argument("--seconds", type=float, default=5.0)
    sp.add_argument("--until", help="Regex: stop when a line matches")
    sp.add_argument("--out", help="Write logs to this path")
    sp.add_argument("--raw", action="store_true", help="Raw byte stream (works with old firmware)")
    sp.add_argument(
        "--save",
        action="store_true",
        help="Save logs to logs/<timestamp>_<port>.log by default",
    )
    sp.set_defaults(func=cmd_logs)

    sp = sub.add_parser("cmd", help="Send a command and wait for <<< {json} result")
    sp.add_argument("--port", required=True)
    sp.add_argument("--baud", type=int, default=115200)
    sp.add_argument("--timeout", type=float, default=10.0)
    sp.add_argument(
        "--boot-wait",
        type=float,
        default=6.0,
        help="Seconds to wait for boot/REPL before sending the command",
    )
    sp.add_argument("--until", help="Regex: succeed when a line matches (no JSON required)")
    sp.add_argument("--out", help="Write logs to this path")
    sp.add_argument("command", help='Command string, e.g. "capture" or "status"')
    sp.set_defaults(func=cmd_cmd)

    args = p.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

