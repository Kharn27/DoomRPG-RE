#!/usr/bin/env sh
# Open the log BEFORE rebooting the classic CYD, so the ROM boot is captured.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -x "${HOME}/.platformio/penv/bin/python" ]; then
    PYTHON="${HOME}/.platformio/penv/bin/python"
else
    PYTHON="python3"
fi
exec "$PYTHON" "$SCRIPT_DIR/tools/monitor_cyd.py" "$@"
