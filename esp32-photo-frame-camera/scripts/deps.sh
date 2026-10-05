#!/usr/bin/env bash
# Install toolchain + fetch vendor reference clones and project libraries.
# Pins: vendor/README.md (section "Pinned commits").
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf}"
IDF_VERSION="${IDF_VERSION:-v5.5.1}"
PIO="${PIO:-pio}"

APT_PACKAGES=(
  build-essential
  cmake
  dfu-util
  flex
  bison
  gperf
  git
  libffi-dev
  libssl-dev
  libusb-1.0-0
  ninja-build
  python3
  python3-pip
  python3-venv
  wget
  ccache
)

# relpath|url|ref|submodules(0|1)
VENDOR_REPOS=(
  "vendor/cores3/CoreS3-UserDemo|https://github.com/m5stack/CoreS3-UserDemo.git|90cbcc6ca40c2404de05d3f9fdb01223ae19d3f0|0"
  "vendor/cores3/M5CoreS3|https://github.com/m5stack/M5CoreS3.git|adce2225e7fd8d012b148f46c990f8cc7b8ecc26|0"
  "vendor/cores3/M5Module-LLM|https://github.com/m5stack/M5Module-LLM.git|5d0a761e0618938039d49091570628934b98be0d|0"
  "vendor/common/M5Unified|https://github.com/m5stack/M5Unified.git|8108bfad04a20ff4e57c0751f62dd6fdf6b137a6|0"
  "vendor/common/M5GFX|https://github.com/m5stack/M5GFX.git|27e1ef0f7bab2db8aa77c2768b8934983a656a7c|0"
  "vendor/papercolor/M5PaperColor-UserDemo|https://github.com/m5stack/M5PaperColor-UserDemo.git|1ff998e0cf3b9ce916f2e0319561d043db21cc4a|1"
  "vendor/papercolor/M5PM1|https://github.com/m5stack/M5PM1.git|be9a5456c007c333e7ac963f33bfde1ffa5d82ee|0"
  "vendor/papercolor/m5stack-papercolor-esphome|https://github.com/PFalko/m5stack-papercolor-esphome.git|bd0dca4daab3feae9f5657d4922185cdf0eba562|0"
)

log() {
  printf '==> %s\n' "$*"
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1
}

ensure_path_has_local_bin() {
  if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
    export PATH="$HOME/.local/bin:$PATH"
  fi
}

install_apt_packages() {
  if [[ "${DEPS_SKIP_APT:-0}" == "1" ]]; then
    log "skip apt (DEPS_SKIP_APT=1)"
    return 0
  fi
  if ! need_cmd apt-get; then
    log "apt-get not found — install system packages manually"
    return 0
  fi

  local missing=()
  local pkg
  for pkg in "${APT_PACKAGES[@]}"; do
    if ! dpkg -s "$pkg" >/dev/null 2>&1; then
      missing+=("$pkg")
    fi
  done

  if ((${#missing[@]} == 0)); then
    log "apt packages already installed"
    return 0
  fi

  log "installing apt packages: ${missing[*]}"
  if need_cmd sudo; then
    sudo apt-get update -qq
    sudo apt-get install -y --no-install-recommends "${missing[@]}"
  else
    apt-get update -qq
    apt-get install -y --no-install-recommends "${missing[@]}"
  fi
}

install_platformio() {
  if [[ "${DEPS_SKIP_PIO:-0}" == "1" ]]; then
    log "skip PlatformIO (DEPS_SKIP_PIO=1)"
    return 0
  fi

  ensure_path_has_local_bin
  if need_cmd "$PIO"; then
    log "PlatformIO already installed ($("$PIO" --version))"
    return 0
  fi

  log "installing PlatformIO (user pip)"
  python3 -m pip install --user -U platformio
  ensure_path_has_local_bin
  "$PIO" --version
}

install_esp_idf() {
  if [[ "${DEPS_SKIP_IDF:-0}" == "1" ]]; then
    log "skip ESP-IDF (DEPS_SKIP_IDF=1)"
    return 0
  fi

  if [[ -f "$IDF_PATH/export.sh" ]]; then
    # shellcheck disable=SC1090
    . "$IDF_PATH/export.sh"
    local installed
    installed="$(git -C "$IDF_PATH" describe --tags --exact-match 2>/dev/null || git -C "$IDF_PATH" describe --tags 2>/dev/null || true)"
    log "ESP-IDF already at $IDF_PATH (${installed:-unknown version})"
  else
    log "cloning ESP-IDF $IDF_VERSION to $IDF_PATH"
    mkdir -p "$(dirname "$IDF_PATH")"
    git clone -b "$IDF_VERSION" --depth 1 --recursive https://github.com/espressif/esp-idf.git "$IDF_PATH"
    # shellcheck disable=SC1090
    . "$IDF_PATH/export.sh"
    log "running ESP-IDF install.sh esp32s3 (first time; may take several minutes)"
    "$IDF_PATH/install.sh" esp32s3
    return 0
  fi

  log "ensuring ESP-IDF Python/toolchain deps for esp32s3"
  # shellcheck disable=SC1090
  . "$IDF_PATH/export.sh"
  "$IDF_PATH/install.sh" esp32s3
}

ensure_vendor_repo() {
  local relpath="$1" url="$2" ref="$3" submodules="$4"
  local path="$ROOT/$relpath"

  if [[ -d "$path/.git" ]]; then
    log "vendor: checkout $relpath @ ${ref:0:12}"
    git -C "$path" fetch origin --tags
    git -C "$path" checkout -q "$ref"
  else
    log "vendor: clone $relpath"
    mkdir -p "$(dirname "$path")"
    git clone --filter=blob:none "$url" "$path"
    git -C "$path" checkout -q "$ref"
  fi

  if [[ "$submodules" == "1" ]]; then
    git -C "$path" submodule update --init --recursive
  fi
}

install_vendor_repos() {
  if [[ "${DEPS_SKIP_VENDOR:-0}" == "1" ]]; then
    log "skip vendor clones (DEPS_SKIP_VENDOR=1)"
    return 0
  fi

  local entry relpath url ref submodules
  for entry in "${VENDOR_REPOS[@]}"; do
    IFS='|' read -r relpath url ref submodules <<<"$entry"
    ensure_vendor_repo "$relpath" "$url" "$ref" "$submodules"
  done
}

install_project_libraries() {
  ensure_path_has_local_bin

  if [[ "${DEPS_SKIP_TOOLS_PY:-0}" == "1" ]]; then
    log "skip python tools (DEPS_SKIP_TOOLS_PY=1)"
  else
    log "python tools: install scripts/requirements-tools.txt (user pip)"
    python3 -m pip install --user -U -r "$ROOT/scripts/requirements-tools.txt"
  fi

  if [[ "${DEPS_SKIP_PIO:-0}" != "1" ]] && need_cmd "$PIO"; then
    log "PlatformIO: fetch cores3 lib_deps"
    "$PIO" pkg install -e M5CoreS3 -d cores3
  fi

  if [[ "${DEPS_SKIP_IDF:-0}" != "1" ]] && [[ -f "$IDF_PATH/export.sh" ]]; then
    log "ESP-IDF: set papercolor target esp32s3 (downloads toolchain on first run)"
    # shellcheck disable=SC1090
    . "$IDF_PATH/export.sh"
    cd "$ROOT/papercolor"
    if [[ ! -f sdkconfig ]]; then
      idf.py set-target esp32s3
    else
      idf.py reconfigure
    fi
    cd "$ROOT"
  fi
}

main() {
  log "repo root: $ROOT"
  install_apt_packages
  install_platformio
  install_esp_idf
  install_vendor_repos
  install_project_libraries
  log "done — try: make build-cores3 && make build-papercolor"
}

main "$@"
