# Makefile for waveshare-esp32-sandbox-1
# ESP32-C3 Rust project with Embassy async runtime

# Configuration
TARGET = riscv32imc-unknown-none-elf
CHIP = esp32c3
BINARY = waveshare-esp32-sandbox-1
RELEASE_BINARY = target/$(TARGET)/release/$(BINARY)
DEBUG_BINARY = target/$(TARGET)/debug/$(BINARY)

# Default to release build
PROFILE ?= release
ifeq ($(PROFILE),release)
	BINARY_PATH = $(RELEASE_BINARY)
	CARGO_FLAGS = --release
else
	BINARY_PATH = $(DEBUG_BINARY)
	CARGO_FLAGS =
endif

# Flash tool (prefer cargo-espflash if available, fallback to espflash)
ESPFLASH = $(shell command -v cargo-espflash 2> /dev/null || echo "espflash")
ifeq ($(ESPFLASH),cargo-espflash)
	FLASH_CMD = cargo espflash flash --chip $(CHIP) --target $(TARGET) $(CARGO_FLAGS)
	MONITOR_CMD = cargo espflash monitor --chip $(CHIP)
	FLASH_MONITOR_CMD = cargo espflash flash --chip $(CHIP) --target $(TARGET) $(CARGO_FLAGS) --monitor
else
	FLASH_CMD = espflash flash --chip $(CHIP) $(BINARY_PATH)
	MONITOR_CMD = espflash monitor --chip $(CHIP)
	FLASH_MONITOR_CMD = espflash flash --chip $(CHIP) --monitor $(BINARY_PATH)
endif

# Log level (can be overridden: make flash RUST_LOG=debug)
RUST_LOG ?= info

# Colors for output (optional, works on most terminals)
NO_COLOR = \033[0m
OK_COLOR = \033[32;01m
ERROR_COLOR = \033[31;01m
WARN_COLOR = \033[33;01m
INFO_COLOR = \033[36;01m

.PHONY: all build build-debug build-release flash flash-debug flash-release monitor flash-monitor clean clean-all doc doc-open fmt fmt-check clippy clippy-fix test test-host help check install-tools

# Default target
.DEFAULT_GOAL := help

# Build targets
all: build-release

build: build-release

build-debug:
	@echo "$(INFO_COLOR)Building debug...$(NO_COLOR)"
	RUST_LOG=$(RUST_LOG) cargo build --target $(TARGET)
	@echo "$(OK_COLOR)✓ Debug build complete$(NO_COLOR)"

build-release:
	@echo "$(INFO_COLOR)Building release...$(NO_COLOR)"
	RUST_LOG=$(RUST_LOG) cargo build --release --target $(TARGET)
	@echo "$(OK_COLOR)✓ Release build complete$(NO_COLOR)"

# Flash targets
flash: build-release
	@echo "$(INFO_COLOR)Flashing release build...$(NO_COLOR)"
	$(FLASH_CMD)

flash-debug: build-debug
	@echo "$(INFO_COLOR)Flashing debug build...$(NO_COLOR)"
	PROFILE=debug $(MAKE) flash

flash-release: flash

# Monitor target
monitor:
	@echo "$(INFO_COLOR)Starting serial monitor...$(NO_COLOR)"
	@echo "$(WARN_COLOR)Press Ctrl+] to exit$(NO_COLOR)"
	$(MONITOR_CMD)

# Flash and monitor in one command
flash-monitor: build-release
	@echo "$(INFO_COLOR)Flashing and starting monitor...$(NO_COLOR)"
	@echo "$(WARN_COLOR)Press Ctrl+] to exit monitor$(NO_COLOR)"
	$(FLASH_MONITOR_CMD)

# Quick development cycle: flash + monitor (alias)
dev: flash-monitor

# Clean targets
clean:
	@echo "$(INFO_COLOR)Cleaning build artifacts...$(NO_COLOR)"
	cargo clean
	@echo "$(OK_COLOR)✓ Clean complete$(NO_COLOR)"

clean-all: clean
	@echo "$(INFO_COLOR)Cleaning all generated files...$(NO_COLOR)"
	rm -rf target/
	@echo "$(OK_COLOR)✓ Deep clean complete$(NO_COLOR)"

# Documentation
doc:
	@echo "$(INFO_COLOR)Generating documentation...$(NO_COLOR)"
	cargo doc --target $(TARGET) --no-deps
	@echo "$(OK_COLOR)✓ Documentation generated at target/$(TARGET)/doc/index.html$(NO_COLOR)"

doc-open: doc
	@echo "$(INFO_COLOR)Opening documentation in browser...$(NO_COLOR)"
	cargo doc --target $(TARGET) --no-deps --open

# Formatting
fmt:
	@echo "$(INFO_COLOR)Formatting code...$(NO_COLOR)"
	cargo fmt
	@echo "$(OK_COLOR)✓ Formatting complete$(NO_COLOR)"

fmt-check:
	@echo "$(INFO_COLOR)Checking code formatting...$(NO_COLOR)"
	cargo fmt -- --check
	@echo "$(OK_COLOR)✓ Formatting check passed$(NO_COLOR)"

# Linting
clippy:
	@echo "$(INFO_COLOR)Running clippy...$(NO_COLOR)"
	cargo clippy --target $(TARGET) -- $(CARGO_CLIPPY_FLAGS)
	@echo "$(OK_COLOR)✓ Clippy check passed$(NO_COLOR)"

clippy-fix:
	@echo "$(INFO_COLOR)Running clippy with auto-fix...$(NO_COLOR)"
	cargo clippy --target $(TARGET) --fix --allow-dirty --allow-staged
	@echo "$(OK_COLOR)✓ Clippy fixes applied$(NO_COLOR)"

# Testing
test:
	@echo "$(INFO_COLOR)Running tests...$(NO_COLOR)"
	cargo test --target $(TARGET) $(CARGO_FLAGS)
	@echo "$(OK_COLOR)✓ Tests complete$(NO_COLOR)"

test-host:
	@echo "$(INFO_COLOR)Running host tests...$(NO_COLOR)"
	cargo test --lib
	@echo "$(OK_COLOR)✓ Host tests complete$(NO_COLOR)"

# Pre-commit checks
check: fmt-check clippy
	@echo "$(OK_COLOR)✓ All checks passed$(NO_COLOR)"

# Verify build without flashing
verify: build-release
	@echo "$(OK_COLOR)✓ Build verified$(NO_COLOR)"

# Size analysis
size: build-release
	@echo "$(INFO_COLOR)Binary size analysis:$(NO_COLOR)"
	@size $(RELEASE_BINARY) || echo "$(WARN_COLOR)size command not available$(NO_COLOR)"
	@echo ""
	@echo "$(INFO_COLOR)Flash usage (if espflash available):$(NO_COLOR)"
	@espflash size $(RELEASE_BINARY) 2>/dev/null || echo "$(WARN_COLOR)espflash size not available$(NO_COLOR)"

# Install required tools
install-tools:
	@echo "$(INFO_COLOR)Installing/updating required tools...$(NO_COLOR)"
	@echo "$(INFO_COLOR)Installing cargo-espflash...$(NO_COLOR)"
	cargo install cargo-espflash --locked
	@echo "$(OK_COLOR)✓ Tools installed$(NO_COLOR)"

# Help target
help:
	@echo "$(INFO_COLOR)Available targets:$(NO_COLOR)"
	@echo ""
	@echo "$(OK_COLOR)Build:$(NO_COLOR)"
	@echo "  $(INFO_COLOR)make build$(NO_COLOR)              - Build release (default)"
	@echo "  $(INFO_COLOR)make build-release$(NO_COLOR)      - Build release"
	@echo "  $(INFO_COLOR)make build-debug$(NO_COLOR)        - Build debug"
	@echo ""
	@echo "$(OK_COLOR)Flash & Monitor:$(NO_COLOR)"
	@echo "  $(INFO_COLOR)make flash$(NO_COLOR)               - Build release and flash"
	@echo "  $(INFO_COLOR)make flash-debug$(NO_COLOR)        - Build debug and flash"
	@echo "  $(INFO_COLOR)make monitor$(NO_COLOR)            - Start serial monitor"
	@echo "  $(INFO_COLOR)make flash-monitor$(NO_COLOR)      - Flash and monitor (recommended)"
	@echo "  $(INFO_COLOR)make dev$(NO_COLOR)                - Alias for flash-monitor"
	@echo ""
	@echo "$(OK_COLOR)Code Quality:$(NO_COLOR)"
	@echo "  $(INFO_COLOR)make fmt$(NO_COLOR)                - Format code"
	@echo "  $(INFO_COLOR)make fmt-check$(NO_COLOR)          - Check formatting"
	@echo "  $(INFO_COLOR)make clippy$(NO_COLOR)             - Run clippy linter"
	@echo "  $(INFO_COLOR)make clippy-fix$(NO_COLOR)         - Run clippy with auto-fix"
	@echo "  $(INFO_COLOR)make check$(NO_COLOR)              - Run fmt-check + clippy"
	@echo ""
	@echo "$(OK_COLOR)Documentation:$(NO_COLOR)"
	@echo "  $(INFO_COLOR)make doc$(NO_COLOR)                - Generate docs"
	@echo "  $(INFO_COLOR)make doc-open$(NO_COLOR)          - Generate and open docs"
	@echo ""
	@echo "$(OK_COLOR)Testing:$(NO_COLOR)"
	@echo "  $(INFO_COLOR)make test$(NO_COLOR)               - Run all tests"
	@echo "  $(INFO_COLOR)make test-host$(NO_COLOR)         - Run host-only tests"
	@echo ""
	@echo "$(OK_COLOR)Utilities:$(NO_COLOR)"
	@echo "  $(INFO_COLOR)make clean$(NO_COLOR)             - Clean build artifacts"
	@echo "  $(INFO_COLOR)make clean-all$(NO_COLOR)         - Deep clean"
	@echo "  $(INFO_COLOR)make verify$(NO_COLOR)            - Verify build succeeds"
	@echo "  $(INFO_COLOR)make size$(NO_COLOR)              - Show binary size info"
	@echo "  $(INFO_COLOR)make install-tools$(NO_COLOR)      - Install required tools"
	@echo ""
	@echo "$(OK_COLOR)Environment variables:$(NO_COLOR)"
	@echo "  $(INFO_COLOR)RUST_LOG=$(RUST_LOG)$(NO_COLOR)              - Set log level (e.g., RUST_LOG=debug make flash)"
	@echo "  $(INFO_COLOR)PROFILE=debug$(NO_COLOR)          - Use debug profile (e.g., PROFILE=debug make build)"
	@echo ""
	@echo "$(OK_COLOR)Examples:$(NO_COLOR)"
	@echo "  $(INFO_COLOR)make dev$(NO_COLOR)                - Quick dev cycle: flash + monitor"
	@echo "  $(INFO_COLOR)make flash RUST_LOG=debug$(NO_COLOR) - Flash with debug logging"
	@echo "  $(INFO_COLOR)make check && make flash$(NO_COLOR) - Check code quality then flash"
