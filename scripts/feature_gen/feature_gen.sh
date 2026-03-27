#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
# feature_gen.sh — Test runner for features_gen.py
# Edit the configuration section below and run:  ./feature_gen.sh
# ─────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ═════════════════════════════════════════════════════════════════════════════
# CONFIGURATION — tweak these to test different scenarios
# ═════════════════════════════════════════════════════════════════════════════

# ── Input / output ────────────────────────────────────────────────────────────
INPUT="${SCRIPT_DIR}/test/base.h"
OUTPUT="${SCRIPT_DIR}/tmp/features_generated.h"
INCLUDE_PATH="${SCRIPT_DIR}/test"

# ── Target board ──────────────────────────────────────────────────────────────
TARGET="rp2040"          # rp2040 | rp2350

# ── Pre-include: injected before base.h, mimics compiler -include ─────────────
# Point to the generated autoconf.h from your build directory, or leave "" to skip.
# Example: AUTOCONF="${SCRIPT_DIR}/../../samples/blinky/build/include/generated/autoconf.h"
AUTOCONF=""

# ── Feature flags (INCLUDE_FEATURE=1 / NOT_INCLUDE_FEATURE=0) ────────────────
TECH_BT="INCLUDE_FEATURE"
TECH_BLE="INCLUDE_FEATURE"
TECH_AUDIO="NOT_INCLUDE_FEATURE"
TECH_PARAM="INCLUDE_FEATURE"

# ── Raw overrides (leave empty "" to use header defaults) ────────────────────
BT_VER=""               # e.g. 4, 5, 6 — overrides the #ifndef default in bt_feat.h

# ── Output options ────────────────────────────────────────────────────────────
JSON=true              # true → also emit features_generated.json
VERBOSE=true           # true → show per-macro parse/eval trace

# ═════════════════════════════════════════════════════════════════════════════
# END OF CONFIGURATION — nothing below should need editing
# ═════════════════════════════════════════════════════════════════════════════

PYTHON="$(command -v python3)" || {
    echo "[ERROR] python3 not found" >&2; exit 1
}

ARGS=(
    --input   "${INPUT}"
    --output  "${OUTPUT}"
    --path    "${INCLUDE_PATH}"
    --target  "${TARGET}"
    "TECH_BT=${TECH_BT}"
    "TECH_BLE=${TECH_BLE}"
    "TECH_AUDIO=${TECH_AUDIO}"
    "TECH_PARAM=${TECH_PARAM}"
)

[[ -n "${BT_VER}" ]]     && ARGS+=("BT_VER=${BT_VER}")
[[ -n "${AUTOCONF}" ]]   && ARGS+=(--pre-include "${AUTOCONF}")
[[ "${JSON}"    == true ]] && ARGS+=(--json)
[[ "${VERBOSE}" == true ]] && ARGS+=(--verbose)

exec "${PYTHON}" "${SCRIPT_DIR}/features_gen.py" "${ARGS[@]}"
