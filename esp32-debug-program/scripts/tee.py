#!/usr/bin/env python3
"""Copy stdin to stdout and to a log file. ANSI color codes are stripped in the file."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ANSI_RE = re.compile(rb"(?:\x1b\[[0-9;?]*[ -/]*[@-~]|\x1b\].*?(?:\x07|\x1b\\))")


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: tee.py LOGFILE", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    path.parent.mkdir(parents=True, exist_ok=True)
    out = sys.stdout.buffer
    with path.open("wb") as log:
        while True:
            chunk = sys.stdin.buffer.read(4096)
            if not chunk:
                break
            out.write(chunk)
            out.flush()
            log.write(ANSI_RE.sub(b"", chunk))
            log.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
