#!/usr/bin/env python3
"""Convert a binary PPM (P6) to PNG without Pillow."""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path


def read_p6(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if not data.startswith(b"P6"):
        raise ValueError(f"{path}: not a P6 PPM")
    i = 2
    while i < len(data) and data[i] in b" \t\r\n":
        i += 1
    if i < len(data) and data[i] == ord("#"):
        while i < len(data) and data[i] not in b"\r\n":
            i += 1
        while i < len(data) and data[i] in b" \t\r\n":
            i += 1
    dims = []
    while len(dims) < 3:
        while i < len(data) and data[i] in b" \t\r\n":
            i += 1
        if i < len(data) and data[i] == ord("#"):
            while i < len(data) and data[i] not in b"\r\n":
                i += 1
            continue
        j = i
        while j < len(data) and data[j] not in b" \t\r\n":
            j += 1
        dims.append(int(data[i:j]))
        i = j
    while i < len(data) and data[i] in b" \t\r\n":
        i += 1
    width, height, maxval = dims
    if maxval != 255:
        raise ValueError(f"{path}: maxval {maxval} unsupported")
    rgb = data[i:]
    expected = width * height * 3
    if len(rgb) < expected:
        raise ValueError(f"{path}: short pixel data")
    return width, height, rgb[:expected]


def write_png(path: Path, width: int, height: int, rgb: bytes) -> None:
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
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(rows), 6))
        + chunk(b"IEND", b"")
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("ppm")
    ap.add_argument("-o", "--out", help="output PNG (default: same stem .png)")
    args = ap.parse_args()
    src = Path(args.ppm)
    dst = Path(args.out) if args.out else src.with_suffix(".png")
    w, h, rgb = read_p6(src)
    write_png(dst, w, h, rgb)
    print(dst.as_posix())
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:  # noqa: BLE001
        print(f"ppm_to_png: {exc}", file=sys.stderr)
        raise SystemExit(1)
