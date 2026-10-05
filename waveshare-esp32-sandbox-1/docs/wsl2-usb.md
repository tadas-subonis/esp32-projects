# WSL2 USB passthrough (usbipd-win)

If you’re developing inside WSL2 but your ESP32 is physically attached to Windows, you can share the USB device into WSL using `usbipd-win`.

Project:
https://github.com/dorssel/usbipd-win

## Typical flow (run on Windows)

From the `usbipd-win` README:

```powershell
winget install usbipd
usbipd list
usbipd bind --busid=<BUSID>      # admin (persistent share)
usbipd attach --wsl --busid=<BUSID>
```

Notes (also from `usbipd-win`):

- Binding is persistent (survives reboot).
- Attaching is non-persistent (redo after reboot/unplug/device reset).
- If you use a third-party firewall, you may need to allow inbound TCP port `3240`.

## After attach

Once attached, your ESP32 should show up inside WSL as a `/dev/tty*` device. You can then use `cargo espflash ... --monitor` from WSL.

