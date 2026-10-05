.PHONY: help all deps build build-cores3 build-papercolor \
        flash-cores3 flash-cores3-wait flash-cores3-windows flash-papercolor monitor-cores3 monitor-papercolor \
        usb-status usb-help usb-attach verify-photo-post \
        ports identify cmd-cores3 cmd-papercolor logs-cores3 logs-papercolor rawlogs capture dev-cores3 dev-papercolor \
        wake \
        clean

IDF_EXPORT ?= $(HOME)/esp/esp-idf/export.sh
PIO ?= $(firstword $(wildcard $(HOME)/.espressif/python_env/idf5.5_py3.12_env/bin/pio) $(shell which pio 2>/dev/null) pio)
PORT ?= /dev/ttyACM0
CORES3_PORT ?= $(PORT)
PAPERCOLOR_PORT ?= $(PORT)
SECONDS ?= 5
UNTIL ?=
OUT ?=
CMD ?=
BOOT_WAIT ?= 8
CORES3_COM ?= COM9
CORES3_BUSID ?= 4-4
TRIES ?= 30
RAW ?= 0

# Run ESP-IDF commands in a login shell so export.sh works under make
IDF_BUILD = env IDF_PATH_FORCE=1 bash -lc '. "$(IDF_EXPORT)" && cd papercolor && idf.py build'
IDF_FLASH = env IDF_PATH_FORCE=1 bash -lc '. "$(IDF_EXPORT)" && cd papercolor && idf.py -p "$(PORT)" -D ESPTOOLPY_AFTER=no_reset flash'
IDF_MONITOR = env IDF_PATH_FORCE=1 bash -lc '. "$(IDF_EXPORT)" && cd papercolor && idf.py -p "$(PORT)" monitor'
IDF_CLEAN = env IDF_PATH_FORCE=1 bash -lc '. "$(IDF_EXPORT)" && cd papercolor && idf.py fullclean'

all: build

build: build-cores3 build-papercolor

help:
	@echo "Photo frame monorepo — common targets:"
	@echo "  make deps                 Toolchain + vendor (WSL)"
	@echo "  make build                cores3 + papercolor"
	@echo "  make build-cores3         PlatformIO M5CoreS3"
	@echo "  make build-papercolor     ESP-IDF papercolor"
	@echo "  make flash-cores3         Build + upload CoreS3     (PORT=$(PORT))"
	@echo "  make flash-cores3-windows Flash CoreS3 via Windows COM (CORES3_COM=$(CORES3_COM), CORES3_BUSID=$(CORES3_BUSID))"
	@echo "  make flash-papercolor     Build + flash PaperColor      (PORT=$(PORT))"
	@echo "  make monitor-cores3       Serial monitor CoreS3     (PORT=$(PORT))"
	@echo "  make monitor-papercolor   Serial monitor PaperColor (PORT=$(PORT))"
	@echo "  make usb-status           lsusb + /dev/ttyACM* in WSL"
	@echo "  make usb-help             usbipd-win steps (Windows)"
	@echo "  make usb-attach           re-attach detached ESP32 via usbipd (from WSL)"
	@echo "  make verify-photo-post    curl golden JPEG to frame"
	@echo "  make ports                list serial ports (JSON)"
	@echo "  make identify             probe ports and guess device (JSON)"
	@echo "  make logs-cores3          capture CoreS3 serial logs   (CORES3_PORT=$(CORES3_PORT), SECONDS=$(SECONDS))"
	@echo "  make logs-papercolor      capture PaperColor logs      (PAPERCOLOR_PORT=$(PAPERCOLOR_PORT), SECONDS=$(SECONDS))"
	@echo "  make cmd-cores3           send CoreS3 command          (CMD=..., CORES3_PORT=$(CORES3_PORT))"
	@echo "  make cmd-papercolor       send PaperColor command      (CMD=..., PAPERCOLOR_PORT=$(PAPERCOLOR_PORT))"
	@echo "  make capture              CoreS3: capture+send photo   (CORES3_PORT=$(CORES3_PORT))"
	@echo "  make rawlogs              raw log dump (old firmware)  (PORT=$(PORT), SECONDS=$(SECONDS), RAW=1)"
	@echo "  make dev-cores3           build+flash+capture logs     (CORES3_PORT=$(CORES3_PORT))"
	@echo "  make dev-papercolor       build+flash+capture logs     (PAPERCOLOR_PORT=$(PAPERCOLOR_PORT))"
	@echo "  make wake                 esptool watchdog_reset (exit download mode)  (PORT=$(PORT))"
	@echo "  make clean                Clean both builds"
	@echo ""
	@echo "Windows native: make.cmd + docs/windows-native.md (same targets, COM* ports)"
	@echo "WSL + USB: docs/wsl-usb-flash.md"

deps:
	bash scripts/deps.sh

build-cores3:
	$(PIO) run -e M5CoreS3 -d cores3

build-papercolor:
	$(IDF_BUILD)

flash-cores3: build-cores3
	$(PIO) run -e M5CoreS3 -d cores3 -t upload --upload-port $(PORT) || true
	bash scripts/cores3-post-flash.sh "$(PORT)"
	@echo "If upload failed to connect, long-press CoreS3 RST ~3s and: make flash-cores3-wait CORES3_PORT=$(PORT)"

flash-cores3-wait:
	PORT="$(CORES3_PORT)" bash scripts/flash-cores3-wait.sh

flash-cores3-windows:
	CORES3_COM="$(CORES3_COM)" CORES3_BUSID="$(CORES3_BUSID)" TRIES="$(TRIES)" bash scripts/flash-cores3-windows.sh

flash-papercolor: build-papercolor
	$(IDF_FLASH)

monitor-cores3:
	$(PIO) device monitor -d cores3 -p $(PORT) -b 115200

monitor-papercolor:
	$(IDF_MONITOR)

usb-status:
	bash scripts/usb-wsl-status.sh

usb-help:
	bash scripts/usb-wsl-help.sh

usb-attach:
	bash scripts/usb-wsl-attach.sh

verify-photo-post:
	bash scripts/verify-photo-post.sh

ports:
	python3 scripts/devctl.py ports

identify:
	python3 scripts/devctl.py identify

logs-cores3:
	python3 scripts/devctl.py logs --port "$(CORES3_PORT)" --baud 115200 --seconds "$(SECONDS)" $(if $(UNTIL),--until "$(UNTIL)",) $(if $(OUT),--out "$(OUT)",)

logs-papercolor:
	python3 scripts/devctl.py logs --port "$(PAPERCOLOR_PORT)" --baud 115200 --seconds "$(SECONDS)" $(if $(UNTIL),--until "$(UNTIL)",) $(if $(OUT),--out "$(OUT)",)

rawlogs:
	python3 scripts/devctl.py logs --port "$(PORT)" --baud 115200 --seconds "$(SECONDS)" --raw $(if $(UNTIL),--until "$(UNTIL)",) $(if $(OUT),--out "$(OUT)",)

cmd-cores3:
	@if [ -z "$(CMD)" ]; then echo "CMD is required (e.g. CMD='status' or CMD='capture')"; exit 2; fi
	python3 scripts/devctl.py cmd --port "$(CORES3_PORT)" --baud 115200 --boot-wait "$(BOOT_WAIT)" --timeout 20 $(if $(OUT),--out "$(OUT)",) "$(CMD)"

cmd-papercolor:
	@if [ -z "$(CMD)" ]; then echo "CMD is required (e.g. CMD='status' or CMD='gallery')"; exit 2; fi
	python3 scripts/devctl.py cmd --port "$(PAPERCOLOR_PORT)" --baud 115200 --boot-wait "$(BOOT_WAIT)" --timeout 20 $(if $(OUT),--out "$(OUT)",) "$(CMD)"

capture:
	$(MAKE) cmd-cores3 CMD="capture"

dev-cores3: flash-cores3
	$(MAKE) logs-cores3 SECONDS=4

dev-papercolor: flash-papercolor
	$(MAKE) logs-papercolor SECONDS=4

# ESP32-S3 USB Serial/JTAG: after opening /dev/ttyACM* the chip may stay in ROM
# download mode until a watchdog reset re-samples strapping pins (see esptool docs).
wake:
	env IDF_PATH_FORCE=1 bash -lc '. "$(IDF_EXPORT)" && esptool.py --chip esp32s3 -p "$(PORT)" --before no_reset --after watchdog_reset run'

clean:
	$(PIO) run -e M5CoreS3 -d cores3 -t clean
	$(IDF_CLEAN)
