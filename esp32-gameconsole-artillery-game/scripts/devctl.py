#!/usr/bin/env python3
"""Serial ports / logs / commands for the artillery handheld (same contract as photo-frame)."""
from __future__ import annotations

import argparse
import errno
import json
import re
import struct
import sys
import time
import zlib
from pathlib import Path
from typing import IO, Any

try:
    import serial  # type: ignore
    import serial.tools.list_ports  # type: ignore
except Exception:
    print(
        "devctl: missing dependency. Install with:\n"
        "  python -m pip install --user -U -r scripts/requirements-tools.txt\n",
        file=sys.stderr,
    )
    raise

RESULT_PREFIX = "<<< "

_READY_PATTERNS = [
    re.compile(r"REPL started"),
    re.compile(r"artillery duel boot"),
    re.compile(r"artillery.*boot", re.IGNORECASE),
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
        ser.dtr = False
        ser.rts = False
        ser.open()
        ser.dtr = False
        ser.rts = False
        return ser
    except OSError as e:
        if getattr(e, "errno", None) in (errno.EACCES, errno.EBUSY):
            raise RuntimeError(
                f"Port {port} is busy or permission denied. Close serial monitors first."
            ) from e
        raise
    except serial.SerialException as e:
        raise RuntimeError(
            f"Failed to open {port}. Is the device connected (COM* / /dev/ttyACM*) and unused?"
        ) from e


def _read_chunk(ser: serial.Serial, bufsize: int = 4096) -> bytes:
    waiting = ser.in_waiting
    if waiting:
        return ser.read(min(waiting, bufsize))
    return ser.read(1)


def _iter_lines(
    ser: serial.Serial,
    deadline_ms: int | None,
    *,
    echo: bool,
    out: IO[str] | None,
    until: re.Pattern[str] | None,
    stop_on_result: bool = True,
) -> dict[str, Any] | None:
    buf = bytearray()
    while deadline_ms is None or _now_ms() < deadline_ms:
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
            if echo:
                print(line, flush=True)
            if out is not None:
                out.write(line + "\n")
                out.flush()
            if until is not None and until.search(line):
                return {"event": "until", "line": line}
            if stop_on_result and line.startswith(RESULT_PREFIX):
                payload = line[len(RESULT_PREFIX) :].strip()
                try:
                    return json.loads(payload)
                except json.JSONDecodeError:
                    return {"ok": False, "error": "invalid_json", "raw": payload}
    return None


def _wait_device_ready(ser: serial.Serial, timeout_s: float) -> bool:
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
    data = []
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
            }
        )
    print(json.dumps({"ok": True, "ports": data}, indent=2, sort_keys=True))
    return 0


def _classify_from_logs(text: str) -> str | None:
    if "artillery duel boot" in text or "artillery" in text.lower() and "REPL started" in text:
        return "artillery"
    if "ESP-IDF" in text and "artillery" in text:
        return "artillery"
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
            "serial_number": getattr(p, "serial_number", None),
            "probe": None,
        }
        try:
            with _open_serial(p.device, args.baud, timeout_s=0.2) as ser:
                ser.reset_input_buffer()
                sniff_deadline = _now_ms() + int(args.sniff * 1000)
                sniff_lines: list[str] = []
                while _now_ms() < sniff_deadline:
                    chunk = _read_chunk(ser)
                    if not chunk:
                        continue
                    sniff_lines.extend(chunk.decode("utf-8", errors="replace").splitlines())
                sniff_text = "\n".join(sniff_lines)
                entry["sniff"] = sniff_lines[-8:] if sniff_lines else []
                kind = _classify_from_logs(sniff_text) or "unknown"
                _wait_device_ready(ser, max(0.0, args.sniff))

                def _try(cmd: str) -> dict[str, Any] | None:
                    ser.write((cmd + "\n").encode("utf-8"))
                    ser.flush()
                    return _iter_lines(ser, _now_ms() + int(args.timeout * 1000), echo=False, out=None, until=None)

                res = _try("version")
                if not isinstance(res, dict) or not res.get("ok"):
                    res = _try("status")
                entry["probe"] = res
                if isinstance(res, dict) and res.get("ok"):
                    kind = "artillery"
                entry["kind"] = kind
        except Exception as e:
            entry["kind"] = "unavailable"
            entry["error"] = str(e)
        results.append(entry)
    print(json.dumps({"ok": True, "devices": results}, indent=2, sort_keys=True))
    return 0


def _stamp_path(port: str, suffix: str) -> Path:
    safe = re.sub(r"[^A-Za-z0-9]", "_", port)
    ts = time.strftime("%Y%m%d_%H%M%S")
    return Path("logs") / f"{ts}_{safe}{suffix}"


def _default_out_path(port: str) -> Path:
    return _stamp_path(port, ".log")


def _write_png_rgb(path: Path, width: int, height: int, rgb: bytes) -> None:
    if width <= 0 or height <= 0:
        raise ValueError("invalid snap size")
    expected = width * height * 3
    if len(rgb) != expected:
        raise ValueError(f"rgb length {len(rgb)} != {expected}")
    rows = bytearray()
    stride = width * 3
    for y in range(height):
        rows.append(0)
        rows.extend(rgb[y * stride : (y + 1) * stride])

    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")
    )


def _hard_reset(ser: serial.Serial) -> None:
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    time.sleep(0.05)


def cmd_logs(args: argparse.Namespace) -> int:
    out_path: Path | None = Path(args.out) if args.out else None
    if out_path is None:
        out_path = _default_out_path(args.port) if args.save else None

    out_f: IO[str] | None = None
    if out_path is not None:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_f = out_path.open("w", encoding="utf-8")
        _eprint(f"saving to {out_path}")

    until_re = re.compile(args.until) if args.until else None
    deadline_ms = None if args.seconds <= 0 else _now_ms() + int(args.seconds * 1000)
    res: dict[str, Any] | None = None

    try:
        with _open_serial(args.port, args.baud, timeout_s=0.2) as ser:
            if args.reset:
                _hard_reset(ser)
            ser.reset_input_buffer()
            if args.raw:
                total = 0
                while deadline_ms is None or _now_ms() < deadline_ms:
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

            res = _iter_lines(
                ser, deadline_ms, echo=True, out=out_f, until=until_re, stop_on_result=False
            )
    except KeyboardInterrupt:
        _eprint("stopped")
        return 0
    finally:
        if out_f is not None:
            out_f.close()

    if res is None:
        return 0
    if isinstance(res, dict) and res.get("event") == "until":
        _eprint(json.dumps({"ok": True, "event": "until", "line": res.get("line")}))
        return 0
    _eprint(json.dumps({"ok": True, "event": "result", "result": res}))
    return 0


def cmd_cmd(args: argparse.Namespace) -> int:
    deadline_ms = _now_ms() + int(args.timeout * 1000)
    out_path = Path(args.out) if args.out else None
    out_f: IO[str] | None = None
    if out_path is not None:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_f = out_path.open("w", encoding="utf-8")
    try:
        with _open_serial(args.port, args.baud, timeout_s=0.2, write_timeout_s=args.timeout) as ser:
            ser.reset_input_buffer()
            ser.reset_output_buffer()
            _wait_device_ready(ser, args.boot_wait)
            ser.write((args.command.rstrip("\r\n") + "\n").encode("utf-8"))
            ser.flush()
            res = _iter_lines(ser, deadline_ms, echo=True, out=out_f, until=None)
    finally:
        if out_f is not None:
            out_f.close()
    if res is None:
        print(json.dumps({"ok": False, "error": "timeout"}))
        return 2
    ok = bool(res.get("ok")) if isinstance(res, dict) else False
    print(json.dumps(res))
    return 0 if ok else 1


def _read_until(ser: serial.Serial, buf: bytearray, marker: bytes, deadline_ms: int) -> int:
    while True:
        idx = buf.find(marker)
        if idx >= 0:
            return idx
        if _now_ms() >= deadline_ms:
            return -1
        chunk = _read_chunk(ser, 8192)
        if chunk:
            buf.extend(chunk)


def cmd_snap(args: argparse.Namespace) -> int:
    deadline_ms = _now_ms() + int(args.timeout * 1000)
    out_path = Path(args.out) if args.out else _stamp_path(args.port, "_snap.png")
    with _open_serial(args.port, args.baud, timeout_s=0.2, write_timeout_s=args.timeout) as ser:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        _wait_device_ready(ser, args.boot_wait)
        ser.write(b"snap\n")
        ser.flush()
        buf = bytearray()
        json_idx = _read_until(ser, buf, b"<<< ", deadline_ms)
        if json_idx < 0:
            print(json.dumps({"ok": False, "error": "timeout", "cmd": "snap"}))
            return 2
        nl = buf.find(b"\n", json_idx)
        while nl < 0:
            if _now_ms() >= deadline_ms:
                print(json.dumps({"ok": False, "error": "timeout", "cmd": "snap"}))
                return 2
            chunk = _read_chunk(ser, 8192)
            if chunk:
                buf.extend(chunk)
            nl = buf.find(b"\n", json_idx)
        raw_line = bytes(buf[json_idx:nl]).decode("utf-8", errors="replace").rstrip("\r")
        print(raw_line, flush=True)
        payload = raw_line[len(RESULT_PREFIX) :].strip()
        try:
            res = json.loads(payload)
        except json.JSONDecodeError:
            print(json.dumps({"ok": False, "error": "invalid_json", "raw": payload}))
            return 1
        if not isinstance(res, dict) or not res.get("ok"):
            print(json.dumps(res))
            return 1
        width = int(res.get("w") or 0)
        height = int(res.get("h") or 0)
        needed = int(res.get("bytes") or (width * height * 3))
        marker = b">>>SNAP"
        snap_idx = _read_until(ser, buf, marker, deadline_ms)
        if snap_idx < 0:
            print(json.dumps({"ok": False, "error": "missing_snap_marker", "meta": res}))
            return 2
        i = snap_idx + len(marker)
        while i < len(buf) and buf[i] in (13, 10):
            i += 1
        while len(buf) < i + needed:
            if _now_ms() >= deadline_ms:
                print(
                    json.dumps(
                        {"ok": False, "error": "timeout_payload", "got": max(0, len(buf) - i), "need": needed}
                    )
                )
                return 2
            chunk = _read_chunk(ser, 8192)
            if chunk:
                buf.extend(chunk)
        rgb = bytes(buf[i : i + needed])
        try:
            _write_png_rgb(out_path, width, height, rgb)
        except ValueError as e:
            print(json.dumps({"ok": False, "error": str(e), "meta": res}))
            return 1
        res["png"] = str(out_path.as_posix())
        print(json.dumps(res))
        return 0


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(prog="devctl", description="Artillery handheld serial helper")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("ports")
    sp.set_defaults(func=cmd_ports)

    sp = sub.add_parser("identify")
    sp.add_argument("--baud", type=int, default=115200)
    sp.add_argument("--timeout", type=float, default=2.0)
    sp.add_argument("--sniff", type=float, default=1.5)
    sp.add_argument("--only", action="append")
    sp.set_defaults(func=cmd_identify)

    sp = sub.add_parser("logs")
    sp.add_argument("--port", required=True)
    sp.add_argument("--baud", type=int, default=115200)
    sp.add_argument("--seconds", type=float, default=5.0)
    sp.add_argument("--until")
    sp.add_argument("--out")
    sp.add_argument("--raw", action="store_true")
    sp.add_argument("--save", action="store_true")
    sp.add_argument("--reset", action="store_true", help="Pulse RTS to reset the chip after opening the port")
    sp.set_defaults(func=cmd_logs)

    sp = sub.add_parser("cmd")
    sp.add_argument("--port", required=True)
    sp.add_argument("--baud", type=int, default=115200)
    sp.add_argument("--timeout", type=float, default=20.0)
    sp.add_argument("--boot-wait", type=float, default=6.0)
    sp.add_argument("--out")
    sp.add_argument("command")
    sp.set_defaults(func=cmd_cmd)

    sp = sub.add_parser("snap")
    sp.add_argument("--port", required=True)
    sp.add_argument("--baud", type=int, default=115200)
    sp.add_argument("--timeout", type=float, default=30.0)
    sp.add_argument("--boot-wait", type=float, default=6.0)
    sp.add_argument("--out")
    sp.set_defaults(func=cmd_snap)

    args = p.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
