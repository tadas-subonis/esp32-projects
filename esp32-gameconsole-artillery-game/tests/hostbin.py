"""Locate host binaries produced by CMake (Windows .exe or POSIX names)."""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def host_bin(name: str) -> Path:
    root = ROOT / "build-host"
    candidates = [
        root / f"{name}.exe",
        root / name,
        root / "Release" / f"{name}.exe",
        root / "Debug" / f"{name}.exe",
        root / "RelWithDebInfo" / f"{name}.exe",
    ]
    for path in candidates:
        if path.exists():
            return path
    return candidates[0]
