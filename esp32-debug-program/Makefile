.PHONY: help all deps build flash monitor flash-monitor ports logs clean

IDF_EXPORT ?= $(HOME)/esp/esp-idf/export.sh
PORT ?= /dev/ttyACM0
SECONDS ?= 8
LOGDIR ?= $(CURDIR)/logs
LOG ?= $(LOGDIR)/$(shell date +%Y%m%d_%H%M%S).log

IDF_SH = env IDF_PATH_FORCE=1 bash -lc '. "$(IDF_EXPORT)" && cd firmware && $(1)'

all: build

help:
	@echo "P4 LCD wiring test:"
	@echo "  make deps build flash monitor flash-monitor ports logs clean"
	@echo "Windows PowerShell: .\\make.ps1 flash PORT=COM8"

deps:
	bash scripts/deps.sh

build:
	$(call IDF_SH,if [ ! -f sdkconfig ]; then idf.py set-target esp32p4; fi && idf.py build)

flash: build
	$(call IDF_SH,idf.py -p "$(PORT)" flash)

monitor:
	mkdir -p "$(LOGDIR)"
	@echo "Saving serial output to $(LOG)"
	$(call IDF_SH,python3 "$(CURDIR)/scripts/devctl.py" logs --port "$(PORT)" --baud 115200 --seconds 0 --save --reset --out "$(LOG)")

flash-monitor: build
	mkdir -p "$(LOGDIR)"
	@echo "Saving serial output to $(LOG)"
	$(call IDF_SH,idf.py -p "$(PORT)" flash && python3 "$(CURDIR)/scripts/devctl.py" logs --port "$(PORT)" --baud 115200 --seconds 0 --save --reset --out "$(LOG)")

ports:
	python3 scripts/devctl.py ports

logs:
	python3 scripts/devctl.py logs --port "$(PORT)" --baud 115200 --seconds "$(SECONDS)"

clean:
	$(call IDF_SH,idf.py fullclean)
