#!/usr/bin/env bash
set -euo pipefail

# Corre la simulacion desde la raiz del repo para resolver $readmemh.
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

if ! command -v iverilog >/dev/null 2>&1 || ! command -v vvp >/dev/null 2>&1; then
    echo "ERROR: instalar Icarus Verilog (iverilog y vvp)." >&2
    exit 1
fi

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

iverilog -g2012 -s tb_fir_core -o "$TMP_DIR/fir_test.vvp" \
    hw/ip/audio_dsp/fir_core.sv \
    hw/sim/tb_fir_core.sv
vvp "$TMP_DIR/fir_test.vvp"
