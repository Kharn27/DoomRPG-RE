#!/usr/bin/env python3
"""Retire the unused desktop DoomRPG root errorStr buffer only on CYD.

All production/diagnostic/native source modules are scanned. Fatal messages
remain stack-local in the native root, while the desktop/oracle keep ABI.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/DoomRPG.h").read_text(encoding="latin-1")
DESKTOP = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
CORE = (ROOT / "ESP32/src/engine_stubs.c").read_text()
PRESENT = (ROOT / "ESP32/src/esp_doomrpg_presentation.c").read_text()
INTRO = (ROOT / "ESP32/src/native_intro_dispose.c").read_text()

assert re.search(
    r"#if !defined\(DOOMRPG_ESP32\) \|\| defined\(DOOMRPG_ESP32_GEOMETRY_ORACLE\)"
    r"\s+char\s+errorStr\[64\];\s+#endif", HEADER)
assert "doomrpg->errorStr" in DESKTOP, "desktop error lifetime must be intact"
assert "char errMsg[256];" in CORE, "native fatal logging owns its local buffer"
assert "vsnprintf(errMsg, sizeof(errMsg), fmt, ap);" in CORE
assert "DoomRPG_Error(const char* fmt, ...)" in CORE
assert "SDL_calloc(1, sizeof(DoomRPG_t))" in CORE
assert "_Static_assert(sizeof(DoomRPG_t) == 12U," in CORE
assert not re.search(r"\bdoomRpg\s*->\s*errorID\b", CORE)
assert "EspNativePresentation_resetClipState();" in CORE
assert "EspNativePresentation_clipEnabled(void)" in PRESENT
assert "nativeGraphClipSet = false;" in PRESENT
assert "EspNativePresentation_clipEnabled()" in INTRO, "intro fail-closed guard stays active"

# Ban any native owner references, including source currently unlinked by the
# PlatformIO build script. Ignore comments/diagnostics but not executable code.
count = 0
for root in (ROOT / "ESP32/src", ROOT / "ESP32/include"):
    for path in sorted(root.rglob("*")):
        if path.suffix not in (".c", ".cpp", ".h"):
            continue
        count += 1
        source = path.read_text(encoding="latin-1")
        source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', " ", source,
                        flags=re.S)
        assert not re.search(r"\berrorStr\b", source), path
assert count >= 300, "source/header inventory incomplete"

# Actual shared header; don't substitute synthetic structure offsets.
cc = shutil.which("cc") or "cc"
with tempfile.TemporaryDirectory(prefix="root-errorstr-abi-") as tmp:
    path = Path(tmp) / "probe.c"
    path.write_text(
        '#include <SDL.h>\n#include "DoomRPG.h"\n'
        'int root_error_size(DoomRPG_t* r) { return (int)sizeof(r->errorStr); }\n')
    cmd = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
           "-fsyntax-only", "-IESP32/include", "-Isrc"]
    for name, flags, expected in (
        ("normal", ["-DDOOMRPG_ESP32=1"], False),
        ("bringup", ["-DDOOMRPG_ESP32=1",
                     "-DDOOMRPG_ESP32_BRINGUP_PROBES=1"], False),
        ("trace", ["-DDOOMRPG_ESP32=1",
                   "-DDOOMRPG_LOG_LEVEL=4"], False),
        ("oracle", ["-DDOOMRPG_ESP32=1",
                    "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1"], True),
        ("desktop", [], True),
    ):
        result = subprocess.run(cmd + flags + [str(path)], cwd=ROOT,
                                capture_output=True, text=True)
        assert (result.returncode == 0) == expected, (name, result.stderr)
        if not expected:
            assert "errorStr" in result.stderr, name

print(f"PASS root errorStr=desktop/oracle only; native root=12B; "
      f"{count} native source/header files guarded; fatal buffer=stack-local")
