.PHONY: help all deps test play play-local play-remote emu server cli build build-host build-firmware flash flash-monitor monitor \
        ports identify cmd snap logs clean

IDF_EXPORT ?= $(HOME)/esp/esp-idf/export.sh
PORT ?= /dev/ttyACM0
SECONDS ?= 5
UNTIL ?=
OUT ?=
CMD ?=
BOOT_WAIT ?= 8

LOG ?=
IDF_BUILD = env IDF_PATH_FORCE=1 bash -lc '. "$(IDF_EXPORT)" && cd firmware && (test -f sdkconfig || idf.py set-target esp32p4) && idf.py build'
IDF_FLASH = env IDF_PATH_FORCE=1 bash -lc '. "$(IDF_EXPORT)" && cd firmware && idf.py -p "$(PORT)" flash'
IDF_CLEAN = env IDF_PATH_FORCE=1 bash -lc '. "$(IDF_EXPORT)" && cd firmware && idf.py fullclean'

all: build

help:
	@echo "Tank Duel (artillery) — common targets:"
	@echo "  make deps              pyserial + ESP-IDF esp32p4"
	@echo "  make test              host C++ tests + sim CLI + server tests"
	@echo "  make server            authoritative TCP sim (port 7420)"
	@echo "  make play              local vs-bot (in-process server)"
	@echo "  make play-local        same as play"
	@echo "  make play-remote       TCP client (needs make server)"
	@echo "  make emu               SDL window (same RGB565 compose as firmware)"
	@echo "  make cli               send one JSON command (CMD=status)"
	@echo "  make build             host + firmware"
	@echo "  make build-host        C++ sim/host/tests"
	@echo "  make build-firmware    ESP-IDF ESP32-P4"
	@echo "  make flash             build + flash     (PORT=$(PORT))"
	@echo "  make flash-monitor     flash, then serial to screen + logs/"
	@echo "  make monitor           serial to screen + logs/"
	@echo "  make ports / identify  serial discovery"
	@echo "  make cmd               send command      (CMD=..., PORT=$(PORT))"
	@echo "  make snap              120x80 PNG        (PORT=$(PORT))"
	@echo "  make logs              capture logs      (PORT=$(PORT))"
	@echo "  make clean             host + firmware"
	@echo ""
	@echo "Windows native: make.cmd + docs/windows-native.md (COM* ports)"

deps:
	bash scripts/deps.sh

build-host:
	cmake -S . -B build-host
	cmake --build build-host -j

build-firmware:
	$(IDF_BUILD)

build: build-host build-firmware

test: build-host
	./build-host/artillery-tests
	python3 tests/test_sim.py
	python3 tests/test_server.py

server: build-host
	./build-host/artillery-server --port 7420

play: build-host
	./build-host/artillery-host --local --want bot

play-local: play

play-remote: build-host
	./build-host/artillery-host --remote --host 127.0.0.1 --port 7420

emu: build-host
	./build-host/artillery-emu

cli: build-host
	./build-host/artillery-cli $(CMD)

flash: build-firmware
	$(IDF_FLASH)

monitor:
	mkdir -p logs
	python3 scripts/devctl.py logs --port "$(PORT)" --baud 115200 --seconds 0 --save --reset $(if $(LOG),--out "$(LOG)")

flash-monitor: build-firmware
	$(IDF_FLASH)
	mkdir -p logs
	python3 scripts/devctl.py logs --port "$(PORT)" --baud 115200 --seconds 0 --save --reset $(if $(LOG),--out "$(LOG)")

ports:
	python3 scripts/devctl.py ports

identify:
	python3 scripts/devctl.py identify

logs:
	python3 scripts/devctl.py logs --port "$(PORT)" --baud 115200 --seconds "$(SECONDS)" $(if $(UNTIL),--until "$(UNTIL)") $(if $(OUT),--out "$(OUT)")

cmd:
	python3 scripts/devctl.py cmd --port "$(PORT)" --baud 115200 --boot-wait "$(BOOT_WAIT)" --timeout 20 $(if $(OUT),--out "$(OUT)") "$(CMD)"

snap:
	python3 scripts/devctl.py snap --port "$(PORT)" --baud 115200 --boot-wait "$(BOOT_WAIT)" --timeout 30 $(if $(OUT),--out "$(OUT)")

clean:
	rm -rf build-host
	$(IDF_CLEAN)
