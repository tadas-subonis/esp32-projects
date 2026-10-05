# Module LLM — ADB / UART / SSH Connect

**Source:** https://docs.m5stack.com/en/stackflow/module_llm/config  
**Retrieved:** 2026-06-22

---

Use these methods to access the Module LLM **Linux terminal** for `apt` installs, model packages, and debugging.

## ADB (USB Type-C on Module LLM or Mate)

Download [Android Platform-Tools](https://developer.android.com/tools/releases/platform-tools) for your OS.

```bash
# If permission denied:
adb kill-server

# Push file to module
adb push data.json /opt

# Shell access
adb shell
```

## Module13.2 LLM Mate

- Provides stable FPC connection, **Ethernet**, and **USB serial log** port
- Move speaker aside to expose FPC socket before connecting cable

## UART (Mate debug port)

- Tool: PuTTY or `screen` / `minicom`
- **115200 8N1**
- Default login: `root` / `123456`

## SSH (via Ethernet on Mate)

1. Get IP via UART or ADB:

```bash
ip addr
```

2. SSH from LAN:

```bash
ssh root@192.168.x.x
# password: 123456
```

## When to use which

| Method | Best for |
|--------|----------|
| ADB | File transfer, quick shell when USB connected |
| UART | Headless debug, serial console |
| SSH | `apt install`, model management over network |

## Security note

Change default `root` password before deploying on a network.

## Related

- [Software update](./module-llm-software-update.md)
- [Firmware reflash](./module-llm-image-firmware-update.md)
