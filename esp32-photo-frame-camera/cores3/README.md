# CoreS3 firmware

**Role:** Capture photos, optional LLM caption, push to M5Paper Color over Wi-Fi.

## Build / flash (repo root)

```powershell
make build-cores3
make identify
make flash-cores3 PORT=COM7
```

WSL: `make flash-cores3 PORT=/dev/ttyACM1`

Long-press **RST** ~3 s (green LED) if upload fails → `make flash-cores3-wait PORT=COM7`.

## Build (direct)

```bash
pio run -e M5CoreS3
pio run -e M5CoreS3 -t upload --upload-port COM7   # Windows
pio run -e M5CoreS3 -t upload --upload-port /dev/ttyACM1   # WSL
```

## Agent commands

```powershell
make cmd-cores3 CMD=status CORES3_PORT=COM7
make cmd-cores3 CMD=capture CORES3_PORT=COM7
make logs-cores3 CORES3_PORT=COM7 SECONDS=8
```

See [docs/agent-tooling.md](../docs/agent-tooling.md).

## Config

```bash
cp include/secrets.h.example include/secrets.h
# edit Wi-Fi and PAPERCOLOR_HOST
```

## References

- Camera: `M5CoreS3` library `examples/Basic/camera/`
- VLM + LLM: `M5Module-LLM` example `VLM/`
- Factory UI patterns: `CoreS3-UserDemo` in vendor tree
