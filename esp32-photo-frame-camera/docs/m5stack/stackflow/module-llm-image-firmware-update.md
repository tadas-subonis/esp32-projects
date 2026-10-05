# Module LLM — Image / Firmware Base Package Update

**Source:** https://docs.m5stack.com/en/stackflow/module_llm/image  
**Retrieved:** 2026-06-22

---

Full system upgrade or recovery when the Ubuntu/StackFlow image is damaged. **Flashing tool is Windows-only** as of official docs.

## When to use

- First-time setup with latest base image
- System corruption / unbootable module
- Major StackFlow version jump

For app/model updates only, prefer [apt software update](./module-llm-software-update.md).

## Firmware versions (official table)

| Version | Notes |
|---------|-------|
| M5_LLM_ubuntu_v1.3_20241203-mini | Older mini image |
| M5_LLM_ubuntu_v1.6_20250612 | |
| M5_LLM_ubuntu22.04_20251121 | Ubuntu 22.04 based |

Download `.axp` packages from M5Stack docs download links.

## Tools (Windows)

| Tool | Purpose |
|------|---------|
| AXDL_V1.24.13.1 | Flashing utility |
| AXDL_Driver_V1.20.46 | USB driver |

## Flash procedure

1. Download `.axp` firmware + AXDL tool + driver
2. Open AXDL → **Load** firmware package
3. Click **Start** (wait for device detection)
4. **Hold download button** on module → connect Type-C to PC
5. Device enters download mode; flash proceeds automatically

## Critical warnings

### Do NOT partition `/dev/mmcblk0`

`/dev/mmcblk0` is onboard eMMC (system disk). Repartitioning makes the AX630C treat it as SD-card boot mode and can **brick** the module beyond online recovery.

### Non-standard U-Boot

AXERA firmware does not follow standard U-Boot boot flow. `before_boot_cmd` runs before `ax_boot` (e.g. indicator LED).

## Related

- [Terminal access](./module-llm-adb-uart-ssh.md)
- [apt updates](./module-llm-software-update.md)
