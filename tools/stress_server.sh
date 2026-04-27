#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="${PYTHON_BIN:-python3}"
STRESS_PY="$SCRIPT_DIR/stress_server.py"
if [[ ! -f "$STRESS_PY" && -f "$SCRIPT_DIR/tools/stress_server.py" ]]; then
  STRESS_PY="$SCRIPT_DIR/tools/stress_server.py"
fi

if [[ $# -gt 0 ]]; then
  exec "$PYTHON_BIN" "$STRESS_PY" "$@"
fi

exec "$PYTHON_BIN" "$STRESS_PY"
