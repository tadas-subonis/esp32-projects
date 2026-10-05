#!/usr/bin/env python3
from __future__ import annotations

import argparse
import errno
import json
import re
import sys
import threading
import time
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
    re.compile(r"p4-debug boot"),
    re.compile(r"^dbg>\s*$"),
    re.compile(r"REPL started"),
]


def _now_ms() -> int:
    return int(time.time() * 1000)


def _eprint(msg: str) -> None:
    try:
        print(msg, file=sys.stderr)
    except UnicodeEncodeError:
        print(msg.encode(sys.stderr.encoding or "utf-8", errors="replace").decode(
            sys.stderr.encoding or "utf-8", errors="replace"), file=sys.stderr)


def _safe_print(text: str, *, end: str = "\n", flush: bool = False) -> None:
    """Print UART text without crashing on Windows cp1252 consoles."""
    try:
        print(text, end=end, flush=flush)
    except UnicodeEncodeError:
        enc = getattr(sys.stdout, "encoding", None) or "utf-8"
        sys.stdout.buffer.write(text.encode(enc, errors="replace"))
        if end:
            sys.stdout.buffer.write(end.encode(enc, errors="replace"))
        if flush:
            sys.stdout.buffer.flush()


def _configure_stdio() -> None:
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[attr-defined]
        except Exception:
            pass


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
                f"Port {port} is busy or permission denied. "
                "Close serial monitors (idf.py monitor, etc.)."
            ) from e
        raise
    except serial.SerialException as e:
        raise RuntimeError(
            f"Failed to open {port}. Is the device connected (COM* on Windows) "
            "and unused by another program?"
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
                _safe_print(line, flush=True)
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
            }
        )
    print(json.dumps({"ok": True, "ports": data}, indent=2, sort_keys=True))
    return 0


def _classify_from_logs(text: str) -> str | None:
    if "p4-debug boot" in text or "dbg>" in text:
        return "p4_debug"
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
            "probe": None,
        }

        try:
            with _open_serial(p.device, args.baud, timeout_s=0.2) as ser:
                ser.reset_input_buffer()
                ser.reset_output_buffer()

                sniff_deadline = _now_ms() + int(args.sniff * 1000)
                sniff_lines: list[str] = []
                while _now_ms() < sniff_deadline:
                    chunk = _read_chunk(ser)
                    if not chunk:
                        continue
                    sniff_lines.extend(chunk.decode("utf-8", errors="replace").splitlines())
                sniff_text = "\n".join(sniff_lines)
                entry["sniff"] = sniff_lines[-8:] if sniff_lines else []
                kind_from_logs = _classify_from_logs(sniff_text)

                if kind_from_logs is None:
                    _wait_device_ready(ser, max(0.0, args.sniff))

                def _try(cmd: str) -> dict[str, Any] | None:
                    ser.write((cmd + "\n").encode("utf-8"))
                    ser.flush()
                    deadline_ms = _now_ms() + int(args.timeout * 1000)
                    return _iter_lines(ser, deadline_ms, echo=False, out=None, until=None)

                res = _try("version")
                if not isinstance(res, dict) or not res.get("ok"):
                    res = _try("status")

                entry["probe"] = res

                kind = kind_from_logs or "unknown"
                if isinstance(res, dict):
                    project = str(res.get("project", ""))
                    if project == "p4_debug" or res.get("cmd") in ("version", "status"):
                        if "lcd_ok" in res or project == "p4_debug":
                            kind = "p4_debug"
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


def _hard_reset(ser: serial.Serial) -> None:
    ser.dtr = False
    ser.rts = True
    time.sleep(0.1)
    ser.rts = False
    time.sleep(0.05)


def _stdin_to_serial(ser: serial.Serial, stop: threading.Event) -> None:
    """Forward keyboard input to the UART (needed for P4Bench s/p/u and commands)."""
    try:
        if sys.platform == "win32":
            import msvcrt

            while not stop.is_set():
                if msvcrt.kbhit():
                    ch = msvcrt.getch()
                    if ch == b"\x03":  # Ctrl+C
                        stop.set()
                        break
                    if ch in (b"\r", b"\n"):
                        ser.write(b"\n")
                    elif ch == b"\x00" or ch == b"\xe0":
                        # arrow/function prefix — consume second byte
                        if msvcrt.kbhit():
                            msvcrt.getch()
                    else:
                        ser.write(ch)
                    ser.flush()
                else:
                    time.sleep(0.02)
        else:
            import select

            while not stop.is_set():
                r, _, _ = select.select([sys.stdin], [], [], 0.05)
                if not r:
                    continue
                data = sys.stdin.buffer.read1(64) if hasattr(sys.stdin.buffer, "read1") else sys.stdin.buffer.read(1)
                if not data:
                    break
                data = data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
                ser.write(data)
                ser.flush()
    except Exception as e:
        _eprint(f"stdin pump stopped: {e}")


def cmd_logs(args: argparse.Namespace) -> int:
    out_path: Path | None = Path(args.out) if args.out else None
    if out_path is None:
        out_path = _default_out_path(args.port) if args.save else None

    out_f: IO[str] | None = None
    if out_path is not None:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_f = out_path.open("w", encoding="utf-8")
        _eprint(f"saving to {out_path}")

    interactive = bool(args.interactive) or (args.seconds <= 0 and not args.no_input)
    until_re = re.compile(args.until) if args.until else None
    deadline_ms = None if args.seconds <= 0 else _now_ms() + int(args.seconds * 1000)
    res: dict[str, Any] | None = None
    stop = threading.Event()
    pump: threading.Thread | None = None

    try:
        with _open_serial(args.port, args.baud, timeout_s=0.05) as ser:
            if args.reset:
                _hard_reset(ser)
            ser.reset_input_buffer()

            if interactive:
                _eprint("interactive: keyboard → UART (Ctrl+C to quit)")
                pump = threading.Thread(target=_stdin_to_serial, args=(ser, stop), daemon=True)
                pump.start()

            if args.raw or interactive:
                # Byte stream (and interactive) — don't wait for newlines only
                total = 0
                while not stop.is_set() and (deadline_ms is None or _now_ms() < deadline_ms):
                    chunk = _read_chunk(ser)
                    if not chunk:
                        continue
                    total += len(chunk)
                    text = chunk.decode("utf-8", errors="replace")
                    _safe_print(text, end="", flush=True)
                    if out_f is not None:
                        out_f.write(text)
                        out_f.flush()
                    if until_re is not None and until_re.search(text):
                        _eprint(json.dumps({"ok": True, "event": "until", "bytes": total}))
                        return 0
                _eprint(json.dumps({"ok": True, "event": "done", "bytes": total}))
                return 0

            res = _iter_lines(ser, deadline_ms, echo=True, out=out_f, until=until_re)
    except KeyboardInterrupt:
        _eprint("stopped")
        return 0
    finally:
        stop.set()
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

    if isinstance(res, dict) and res.get("event") == "until":
        print(json.dumps({"ok": True, "event": "until", "line": res.get("line")}))
        return 0

    ok = bool(res.get("ok")) if isinstance(res, dict) else False
    print(json.dumps(res))
    return 0 if ok else 1


def main(argv: list[str]) -> int:
    _configure_stdio()
    p = argparse.ArgumentParser(prog="devctl", description="P4 debug serial helper")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("ports", help="List serial ports (JSON)")
    sp.set_defaults(func=cmd_ports)

    sp = sub.add_parser("identify", help="Probe ports and guess device (JSON)")
    sp.add_argument("--baud", type=int, default=115200)
    sp.add_argument("--timeout", type=float, default=2.0)
    sp.add_argument("--sniff", type=float, default=1.5)
    sp.add_argument("--only", action="append")
    sp.set_defaults(func=cmd_identify)

    sp = sub.add_parser("logs", help="Stream logs from a serial port")
    sp.add_argument("--port", required=True)
    sp.add_argument("--baud", type=int, default=115200)
    sp.add_argument("--seconds", type=float, default=5.0, help="Capture window; 0 means until Ctrl+C")
    sp.add_argument("--until")
    sp.add_argument("--out")
    sp.add_argument("--raw", action="store_true")
    sp.add_argument("--save", action="store_true")
    sp.add_argument("--reset", action="store_true", help="Pulse RTS to reset the chip after opening the port")
    sp.add_argument(
        "--interactive",
        action="store_true",
        help="Forward keyboard to UART (default when --seconds 0)",
    )
    sp.add_argument(
        "--no-input",
        action="store_true",
        help="Read-only even when --seconds 0",
    )
    sp.set_defaults(func=cmd_logs)

    sp = sub.add_parser("cmd", help='Send a command and wait for <<< {json} result')
    sp.add_argument("--port", required=True)
    sp.add_argument("--baud", type=int, default=115200)
    sp.add_argument("--timeout", type=float, default=10.0)
    sp.add_argument("--boot-wait", type=float, default=6.0)
    sp.add_argument("--until")
    sp.add_argument("--out")
    sp.add_argument("command")
    sp.set_defaults(func=cmd_cmd)

    args = p.parse_args(argv)
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
