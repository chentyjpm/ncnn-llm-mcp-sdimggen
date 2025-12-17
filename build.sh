#!/usr/bin/env bash
set -euo pipefail

NCNN_REF="${NCNN_REF:-20230517}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR_EXPLICIT=0
if [[ -n "${BUILD_DIR+x}" ]]; then BUILD_DIR_EXPLICIT=1; fi
BUILD_DIR="${BUILD_DIR:-build}"
DEPS_DIR="${DEPS_DIR:-_deps}"
GENERATOR="${GENERATOR:-}"                 # default: Ninja if available
BUILD_DEMO="${BUILD_DEMO:-OFF}"            # ON|OFF
BUILD_MCP="${BUILD_MCP:-ON}"               # ON|OFF
PARALLEL="${PARALLEL:-2}"
CLEAN="${CLEAN:-auto}" # auto|on|off (auto: clean build dir on generator mismatch)

usage() {
  cat <<'EOF'
Usage: ./build.sh [options]

Options:
  --build-type <Release|Debug>     CMake build type (default: Release)
  --generator <name>              CMake generator (default: Ninja if available)
  --build-dir <dir>               Build directory (default: build)
  --ncnn-ref <tag>                ncnn git tag/branch (default: 20230517)
  --demo <on|off>                 Build demo executable (default: off)
  --mcp <on|off>                  Build MCP executable (default: on)
  --parallel <N>                  Build parallelism (default: 2)
  --clean <auto|on|off>            Clean build dir on generator mismatch (default: auto)
  --help                          Show help

Environment variables (optional):
  NCNN_REF, BUILD_TYPE, BUILD_DIR, DEPS_DIR, GENERATOR, BUILD_DEMO, BUILD_MCP, PARALLEL, CLEAN

Notes:
  - Windows builds require running from a VS Developer shell so that cl.exe is available.
  - If Ninja is not installed, you must specify a generator via --generator.
EOF
}

onoff_to_cmake() {
  local v="${1,,}"
  if [[ "$v" == "on" || "$v" == "1" || "$v" == "true" ]]; then echo "ON"; else echo "OFF"; fi
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || { echo "Missing required command: $1" >&2; exit 1; }
}

is_windows_bash() {
  case "$(uname -s 2>/dev/null || true)" in
    MINGW*|MSYS*|CYGWIN*) return 0 ;;
    *) return 1 ;;
  esac
}

abs_path() {
  local p="$1"
  if is_windows_bash && command -v cygpath >/dev/null 2>&1; then
    cygpath -m "$p"
    return
  fi
  if command -v python3 >/dev/null 2>&1; then
    python3 - <<PY "$p"
import os,sys
print(os.path.abspath(sys.argv[1]))
PY
    return
  fi
  if command -v python >/dev/null 2>&1; then
    python - <<PY "$p"
import os,sys
print(os.path.abspath(sys.argv[1]))
PY
    return
  fi
  echo "$p"
}

sanitize_name() {
  local s="${1}"
  s="${s,,}"
  s="${s// /-}"
  s="${s//[^a-z0-9_.-]/-}"
  echo "${s}"
}

cmake_cache_generator() {
  local build_dir="$1"
  local cache="${build_dir}/CMakeCache.txt"
  [[ -f "${cache}" ]] || return 1
  awk -F= '/^CMAKE_GENERATOR:INTERNAL=/{print $2; exit 0}' "${cache}" 2>/dev/null || true
}

is_multi_config_generator() {
  case "${1}" in
    "Ninja Multi-Config"|Xcode|Visual\ Studio*) return 0 ;;
    *) return 1 ;;
  esac
}

maybe_clean_build_dir_on_generator_mismatch() {
  local build_dir="$1"
  local expected_generator="$2"
  local label="$3"
  local explicit="${4:-0}"

  local actual_generator
  actual_generator="$(cmake_cache_generator "${build_dir}" || true)"
  [[ -n "${actual_generator}" ]] || return 0
  [[ "${actual_generator}" == "${expected_generator}" ]] && return 0

  if [[ "${CLEAN,,}" == "off" || "${CLEAN,,}" == "0" || "${CLEAN,,}" == "false" ]]; then
    echo "[build] ${label} generator mismatch (${actual_generator} != ${expected_generator}). Remove ${build_dir} or set CLEAN=on." >&2
    exit 1
  fi

  if [[ "${explicit}" == "1" && "${CLEAN,,}" != "on" ]]; then
    echo "[build] ${label} generator mismatch (${actual_generator} != ${expected_generator}). Refusing to delete explicit --build-dir; rerun with --clean on." >&2
    exit 1
  fi

  echo "[build] ${label} generator mismatch (${actual_generator} != ${expected_generator}); cleaning ${build_dir}"
  rm -rf "${build_dir}"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-type) BUILD_TYPE="$2"; shift 2;;
    --generator) GENERATOR="$2"; shift 2;;
    --build-dir) BUILD_DIR="$2"; BUILD_DIR_EXPLICIT=1; shift 2;;
    --ncnn-ref) NCNN_REF="$2"; shift 2;;
    --demo) BUILD_DEMO="$(onoff_to_cmake "$2")"; shift 2;;
    --mcp) BUILD_MCP="$(onoff_to_cmake "$2")"; shift 2;;
    --parallel) PARALLEL="$2"; shift 2;;
    --clean) CLEAN="$2"; shift 2;;
    --help|-h) usage; exit 0;;
    *) echo "Unknown option: $1" >&2; usage; exit 1;;
  esac
done

need_cmd cmake

if [[ -z "${GENERATOR}" ]]; then
  if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
  fi
fi
if [[ -z "${GENERATOR}" ]]; then
  echo "No generator selected and Ninja not found. Install ninja or pass --generator." >&2
  exit 1
fi

ROOT="$(pwd)"
ROOT_ABS="$(abs_path "$ROOT")"

NCNN_INSTALL_PREFIX="${ROOT_ABS}/${DEPS_DIR}/ncnn/install"
NCNN_INSTALL_CMAKE_DIR="${NCNN_INSTALL_PREFIX}/lib/cmake/ncnn"
NCNN_BUILD_DIR="${DEPS_DIR}/ncnn/build-$(sanitize_name "${GENERATOR}")"

build_ncnn_from_source() {
  need_cmd git

  if is_windows_bash; then
    command -v cl >/dev/null 2>&1 || { echo "cl.exe not found. Run this script from a VS Developer shell." >&2; exit 1; }
    if [[ "${GENERATOR}" == "Ninja" || "${GENERATOR}" == "Ninja Multi-Config" ]]; then
      command -v ninja >/dev/null 2>&1 || { echo "ninja not found. Install Ninja or pass --generator." >&2; exit 1; }
    fi
  fi

  mkdir -p "${DEPS_DIR}"
  if [[ ! -d "${DEPS_DIR}/ncnn/.git" ]]; then
    git clone --depth 1 --branch "${NCNN_REF}" https://github.com/Tencent/ncnn.git "${DEPS_DIR}/ncnn"
  fi

  # Optional workaround: ensure fixed-width integers are available for some toolchains.
  local usability_h="${DEPS_DIR}/ncnn/src/layer/x86/x86_usability.h"
  if [[ -f "${usability_h}" ]] && ! grep -qE '^[[:space:]]*#include[[:space:]]+<cstdint>' "${usability_h}"; then
    if command -v python3 >/dev/null 2>&1 || command -v python >/dev/null 2>&1; then
      (command -v python3 >/dev/null 2>&1 && PY=python3 || PY=python)
      "$PY" - <<PY "${usability_h}"
from pathlib import Path
import sys
p = Path(sys.argv[1])
s = p.read_text(encoding="utf-8", errors="ignore")
needle = "#include <math.h>\\n"
if "#include <cstdint>" not in s:
    if needle in s:
        s = s.replace(needle, needle + "#include <cstdint>\\n", 1)
    else:
        s = "#include <cstdint>\\n" + s
    p.write_text(s, encoding="utf-8")
    print("patched", p)
PY
    fi
  fi

  maybe_clean_build_dir_on_generator_mismatch "${NCNN_BUILD_DIR}" "${GENERATOR}" "ncnn" 0

  local ncnn_args=(
    -S "${DEPS_DIR}/ncnn"
    -B "${NCNN_BUILD_DIR}"
    -G "${GENERATOR}"
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DNCNN_VULKAN=OFF
    -DNCNN_OPENMP=OFF
    -DNCNN_BUILD_TOOLS=OFF
    -DNCNN_BUILD_EXAMPLES=OFF
    -DNCNN_BUILD_BENCHMARK=OFF
    -DNCNN_BUILD_TESTS=OFF
    -DNCNN_SHARED_LIB=OFF
    "-DCMAKE_INSTALL_PREFIX=${NCNN_INSTALL_PREFIX}"
  )
  if ! is_multi_config_generator "${GENERATOR}"; then
    ncnn_args+=(-DCMAKE_BUILD_TYPE="${BUILD_TYPE}")
  fi
  if is_windows_bash; then
    ncnn_args+=(-DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl)
  fi

  cmake "${ncnn_args[@]}"
  if is_multi_config_generator "${GENERATOR}"; then
    cmake --build "${NCNN_BUILD_DIR}" --config "${BUILD_TYPE}" --parallel "${PARALLEL}"
    cmake --install "${NCNN_BUILD_DIR}" --config "${BUILD_TYPE}"
  else
    cmake --build "${NCNN_BUILD_DIR}" --parallel "${PARALLEL}"
    cmake --install "${NCNN_BUILD_DIR}"
  fi
}

echo "[build] os=$(uname -s 2>/dev/null || true) generator=${GENERATOR} build_type=${BUILD_TYPE} ncnn=source"
echo "[build] Building ncnn from source into: ${NCNN_INSTALL_PREFIX}"
build_ncnn_from_source

maybe_clean_build_dir_on_generator_mismatch "${BUILD_DIR}" "${GENERATOR}" "project" "${BUILD_DIR_EXPLICIT}"

mkdir -p "${BUILD_DIR}"

project_args=(
  -S .
  -B "${BUILD_DIR}"
  -G "${GENERATOR}"
  -DSD_BUILD_DEMO="${BUILD_DEMO}"
  -DSD_BUILD_MCP="${BUILD_MCP}"
)
if ! is_multi_config_generator "${GENERATOR}"; then
  project_args+=(-DCMAKE_BUILD_TYPE="${BUILD_TYPE}")
fi

project_args+=("-Dncnn_DIR=${NCNN_INSTALL_CMAKE_DIR}")

cmake "${project_args[@]}"
if is_multi_config_generator "${GENERATOR}"; then
  cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --parallel "${PARALLEL}"
else
  cmake --build "${BUILD_DIR}" --parallel "${PARALLEL}"
fi

echo "[build] Done."
if is_windows_bash; then
  echo "[build] Output: ${BUILD_DIR}/stable-diffusion-ncnn-mcp.exe"
else
  echo "[build] Output: ${BUILD_DIR}/stable-diffusion-ncnn-mcp"
fi
