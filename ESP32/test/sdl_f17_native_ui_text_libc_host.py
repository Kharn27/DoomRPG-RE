#!/usr/bin/env python3
"""SDL-F17: prove exact pre-F17 sources and compiled native/SDL-libc parity.

This milestone substitutes only existing SDL.h macro aliases with their exact
C-libc targets. SDL_Rect keeps the compatibility ABI; no world/renderer changes.
"""
from pathlib import Path
import hashlib
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "ESP32/src"
HEADER = ROOT / "ESP32/include/SDL.h"

# Exact pre-F17 Git *blob* SHAs, independently read from main at this milestone.
OLD_BLOBS = {
    "esp_native_text_format.c": "d8320e5026103573d746a1ee875cf343d379dd9d",
    "native_main_menu_model.c": "72a3bb69155e08d335464a5e73017659da3f9666",
    "native_main_menu_actions.c": "c13c42d3b03f24004e129e8f6a181019d1cf4dcd",
    "native_main_menu_touch_layout.c": "e63ac60d024148dfbbdb758773e96a7976707afc",
}

# Each old/new pair is a *single exact source occurrence*; reversed on the
# candidate, these must reconstruct four whole original C files byte-for-byte.
PATCHES = {
    "esp_native_text_format.c": (
        ("#include <SDL.h>\n#include <string.h>\n", "#include <string.h>\n"),
        ("len = SDL_strlen(text);", "len = strlen(text);"),
        ("SDL_memcpy(&out[index + 1], text, (size_t)len);",
         "memcpy(&out[index + 1], text, (size_t)len);"),
    ),
    "native_main_menu_model.c": (
        ("#include <stdio.h>\n", "#include <stdio.h>\n#include <string.h>\n"),
        ("SDL_memset(item, 0, sizeof(*item));", "memset(item, 0, sizeof(*item));"),
        ('SDL_snprintf(item->textField, sizeof(item->textField), "%s", text);',
         'snprintf(item->textField, sizeof(item->textField), "%s", text);'),
    ),
    "native_main_menu_actions.c": (
        ("SDL_memset(&helpSpaceBG, 0, sizeof(helpSpaceBG));",
         "memset(&helpSpaceBG, 0, sizeof(helpSpaceBG));"),
        ("SDL_memset(&helpHudBarBG, 0, sizeof(helpHudBarBG));",
         "memset(&helpHudBarBG, 0, sizeof(helpHudBarBG));"),
        ("(int)SDL_strlen(label)", "(int)strlen(label)"),
        ("n = SDL_strlen(prefixes[zone]);", "n = strlen(prefixes[zone]);"),
        ('SDL_snprintf(page, sizeof(page), "%d/%d",',
         'snprintf(page, sizeof(page), "%d/%d",'),
    ),
    "native_main_menu_touch_layout.c": (
        ("SDL_memset(item, 0, sizeof(*item));", "memset(item, 0, sizeof(*item));"),
        ('SDL_snprintf(item->textField, sizeof(item->textField), "%s", text);',
         'snprintf(item->textField, sizeof(item->textField), "%s", text);'),
    ),
}


def git_blob(content: str) -> str:
    raw = content.encode("utf-8")
    return hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()


# The bottom HELP demo added a full-button blue breath after SDL-F17.
# Restore just that visually-owned function block before verifying the old
# whole-file Git blob; the historical reference hash is not relaxed.
PRE_HELP_BLUE_BOTTOM = "/* Original k/m/l/o/a BMPs, exact gameplay HUD painter, no substitutes.\n * Stage y100..119 once before HELP footer plates overwrite those pixels,\n * and keep only a 128x16 RGB565 thumbnail for subsequent soft repaints. */\nstatic int helpPaintBottomHudIllustration(uint16_t* fb) {\n    const int step = helpBreathStepAt(DoomRPG_GetUpTimeMS());\n    const uint8_t power = EspNativeHelpScene_breathPower(\n        helpBreathLevel(step));\n    int x, y;\n    if (fb == NULL) return 0;\n    if (!helpBottomHudReady) {\n        if (!EspNativeGameplayHud_paintHelpBottomBand(fb)) {\n            printf(\"[MAINHELP] BOTTOM-HUD FAIL native k/m/l/o/a ownership\\n\");\n            return 0;\n        }\n        for (y = 0; y < MAIN_HELP_HUD_BAR_HEIGHT; ++y) {\n            for (x = 0; x < MAIN_HELP_HUD_BAR_WIDTH; ++x) {\n                const int sy = 100 + y * 20 / MAIN_HELP_HUD_BAR_HEIGHT;\n                const int sx = x * 160 / MAIN_HELP_HUD_BAR_WIDTH;\n                helpBottomHudMini[y * MAIN_HELP_HUD_BAR_WIDTH + x] =\n                    fb[sy * DOOMRPG_LOGICAL_WIDTH + sx];\n            }\n        }\n        helpBottomHudReady = 1U;\n    }\n    for (y = 0; y < MAIN_HELP_HUD_BAR_HEIGHT; ++y)\n        for (x = 0; x < MAIN_HELP_HUD_BAR_WIDTH; ++x)\n            fb[(MAIN_HELP_HUD_BAR_Y + y) * DOOMRPG_LOGICAL_WIDTH +\n               MAIN_HELP_HUD_BAR_X + x] =\n                helpBottomHudMini[y * MAIN_HELP_HUD_BAR_WIDTH + x];\n\n    /* Scale the ACTUAL weapon hitbox x100..159 to x96..143 of this\n     * 128px miniature. Highlight the same four 3px touch corners as the\n     * canonical gameplay HUD (not arbitrary full-height blue dividers).\n     * x97/x142 and y44..46/y55..57; no framebuffer region is tinted. */\n    {\n        const int buttonLeft = MAIN_HELP_HUD_BAR_X +\n            ESP_NATIVE_HUD_WEAPON_LEFT * MAIN_HELP_HUD_BAR_WIDTH /\n            DOOMRPG_LOGICAL_WIDTH;\n        const int buttonRight = MAIN_HELP_HUD_BAR_X +\n            (ESP_NATIVE_HUD_WEAPON_RIGHT + 1) *\n            MAIN_HELP_HUD_BAR_WIDTH / DOOMRPG_LOGICAL_WIDTH - 1;\n        const uint16_t amber = (uint16_t)(\n            (((31U * (uint32_t)power) / 255U) << 11) |\n            (((40U * (uint32_t)power) / 255U) << 5));\n        int edge, corner;\n        if (power > 0U) {\n            for (edge = 0; edge < 2; ++edge) {\n                const int buttonX = edge == 0\n                    ? buttonLeft + 1 : buttonRight - 1;\n                for (corner = 0; corner < 2; ++corner) {\n                    const int firstY = MAIN_HELP_HUD_BAR_Y +\n                        (corner == 0 ? 1 : MAIN_HELP_HUD_BAR_HEIGHT - 4);\n                    for (y = firstY; y < firstY + 3; ++y)\n                        fb[y * DOOMRPG_LOGICAL_WIDTH + buttonX] = amber;\n                }\n            }\n        }\n    }\n    helpHudBreathStep = step;\n    return 1;\n}\n\n"

def restore(filename: str, current: str) -> str:
    original = current
    if filename == "native_main_menu_actions.c":
        blue_start = "/* Preserve the light/dark texture"
        next_function = "/* Build the exact native 4x3 WEAPONS panel"
        assert original.count(blue_start) == 1
        assert original.count(next_function) == 1
        first = original.index(blue_start)
        after = original.index(next_function, first)
        assert "static uint16_t helpTintBottomHudWeaponBlue(" in original[first:after]
        assert "helpTintBottomHudWeaponBlue(*pixel, power)" in original[first:after]
        original = original[:first] + PRE_HELP_BLUE_BOTTOM + original[after:]
    # SDL-F19 only renames local layout typedef uses; reconstruct the exact
    # historical pre-F17 source before the F17 alias reversions.
    if filename in ("native_main_menu_actions.c",
                    "native_main_menu_touch_layout.c"):
        expected = 3 if filename == "native_main_menu_actions.c" else 1
        assert original.count("EspNativeRect") == expected, filename
        original = original.replace("EspNativeRect", "SDL_Rect")
    for old, new in reversed(PATCHES[filename]):
        assert original.count(new) == 1, (filename, new, original.count(new))
        original = original.replace(new, old, 1)
    actual = git_blob(original)
    assert actual == OLD_BLOBS[filename], (filename, actual, OLD_BLOBS[filename])
    return original


def real_function(source: str, signature: str) -> str:
    assert source.count(signature) == 1, signature
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i+1]
    raise AssertionError(f"Unclosed function: {signature}")


def renamed(function: str, name: str, version: str) -> str:
    signature = name + "("
    assert function.count(signature) == 1, signature
    return function.replace(signature, name + "_" + version + "(", 1)


# These are the actual macro definitions used by the old firmware, not
# inferred SDL2 behavior or external runtime functions.
ALIAS_NAMES = ("strlen", "memcpy", "memset", "snprintf")
shim = HEADER.read_text(encoding="utf-8")
for name in ALIAS_NAMES:
    assert re.search(r"^#define\s+SDL_" + name + r"\s+" + name + r"\s*$",
                     shim, re.M), name

old = {}
new = {}
for filename in OLD_BLOBS:
    new[filename] = (SRC / filename).read_text(encoding="utf-8")
    old[filename] = restore(filename, new[filename])
    assert not re.search(r"\bSDL_(?:strlen|memcpy|memset|snprintf)\s*\(",
                         new[filename]), filename

assert not re.search(r"#\s*include\s*[<\"]SDL\.h[>\"]",
                     new["esp_native_text_format.c"])
for name in ("native_main_menu_actions.c", "native_main_menu_touch_layout.c"):
    # SDL-F19 uses the exact native alias for local rectangles; legacy
    # DoomRPG headers still require the transitional SDL typedef ABI.
    assert "EspNativeRect" in new[name], name
    assert "SDL_Rect" not in new[name], name
    assert "#include <SDL.h>" in new[name], name

# Compile real bodies from both old and new translation units into one tiny
# host harness, exercising a bounded text divider and both fixed-item builders.
# The only mock is the consumed MenuItem struct layout; the bodies are real.
pair_defs = []
for file, name, signature in (
    ("esp_native_text_format.c", "EspNativeText_buildDivider",
     "char* EspNativeText_buildDivider("),
    ("native_main_menu_model.c", "setFixedItem",
     "static void setFixedItem("),
    ("native_main_menu_touch_layout.c", "setNativeMainItem",
     "static void setNativeMainItem("),
):
    pair_defs.append(renamed(real_function(old[file], signature), name, "old"))
    pair_defs.append(renamed(real_function(new[file], signature), name, "new"))

fixture = r"""
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char byte;
typedef struct MenuItem_s {
    char textField[32];
    uint8_t flags;
    int action;
    uint8_t spare[16];
} MenuItem_t;
/* Direct declarations from ESP32/include/SDL.h, as checked above. */
#define SDL_strlen strlen
#define SDL_memcpy memcpy
#define SDL_memset memset
#define SDL_snprintf snprintf
__PRODUCTION_FUNCTIONS__

int main(void) {
    for (int size = 0; size <= 16; size++) {
        char input[20];
        char a[32], b[32];
        for (int i=0; i<size; i++) input[i] = (char)('A' + i%26);
        input[size] = '\0';
        memset(a, 0xa5, sizeof(a));
        memset(b, 0xa5, sizeof(b));
        assert(EspNativeText_buildDivider_old(a, input) == a);
        assert(EspNativeText_buildDivider_new(b, input) == b);
        assert(memcmp(a, b, sizeof(a)) == 0);
    }
    const char* names[] = { NULL, "", "Start Game", "Load Game ",
        "Help/About", "LONG-TEXT-FOR-TRUNCATION-012345678901234567890" };
    for (size_t i=0; i<sizeof(names)/sizeof(names[0]); i++) {
        MenuItem_t a,b;
        memset(&a,0xa5,sizeof(a)); memset(&b,0xa5,sizeof(b));
        setFixedItem_old(&a, names[i], (int)(i*71));
        setFixedItem_new(&b, names[i], (int)(i*71));
        assert(memcmp(&a, &b, sizeof(a)) == 0);
        memset(&a,0xa5,sizeof(a)); memset(&b,0xa5,sizeof(b));
        setNativeMainItem_old(&a, names[i]);
        setNativeMainItem_new(&b, names[i]);
        assert(memcmp(&a, &b, sizeof(a)) == 0);
    }
    return 0;
}
""".replace("__PRODUCTION_FUNCTIONS__", "\n\n".join(pair_defs))
with tempfile.TemporaryDirectory(prefix="sdl-f17-ui-text-") as tmp:
    code = Path(tmp) / "parity.c"
    program = Path(tmp) / "parity"
    code.write_text(fixture, encoding="utf-8")
    subprocess.run(
        ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
         str(code), "-o", str(program)], check=True,
    )
    subprocess.run([str(program)], check=True)

aliases = sum(len(PATCHES[name]) for name in OLD_BLOBS) - 2
assert aliases == 11
print("SDL-F17 PASS: 4 whole-file pre-F17 Git blobs exact; 11 libc aliases "
      "replaced; real C divider + two fixed-item builders old/new identical; "
      "SDL_Rect ABI untouched")
