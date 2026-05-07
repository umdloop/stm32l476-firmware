#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if command -v python3 >/dev/null 2>&1; then
  python3 "$SCRIPT_DIR/prebuild_dbc_to_c.py"
elif command -v python >/dev/null 2>&1; then
  python "$SCRIPT_DIR/prebuild_dbc_to_c.py"
else
  echo "Failed to regenerate App/dbc/can_dbc_text.c: Python 3 not found." >&2
  exit 1
fi
