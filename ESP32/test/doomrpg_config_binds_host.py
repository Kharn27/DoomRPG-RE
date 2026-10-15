#!/usr/bin/env python3
"""Check process-lifetime Config key maps against desktop and execute the real C reset.

The harness compiles the extracted native definitions and original reference,
not the full ESP32 owner (which also depends on SD/PAK/Arduino). CI's ELF
attribution separately ensures the compiled firmware uses the native owner.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
legacy = (ROOT / "src/DoomRPG.c").read_text(encoding="latin-1")
native = (ROOT / "ESP32/src/esp_legacy_config_mappings_startup.c").read_text(encoding="latin-1")
builder = (ROOT / "ESP32/scripts/build_engine.py").read_text(encoding="utf-8")
declarations = ("keyMapping_t keyMapping[12];", "keyMapping_t keyMappingTemp[12];")
marker = "keyMapping_t keyMappingDefault[12] = {"
signature = "void DoomRPG_setDefaultBinds(DoomRPG_t* doomrpg)"

def table(text):
    assert text.count(marker) == 1, "default table missing or repeated"
    start = text.index(marker)
    end = text.index("\n};", start) + 3
    return text[start:end]

def function(text):
    anchor = signature + "\n{"
    assert text.count(anchor) == 1, "reset function missing or repeated"
    start = text.index(anchor)
    depth = 0
    for pos in range(text.index("{", start), len(text)):
        depth += (text[pos] == "{") - (text[pos] == "}")
        if depth == 0:
            return text[start:pos + 1]
    raise AssertionError("unterminated function")

for declaration in declarations:
    assert legacy.count(declaration) == native.count(declaration) == 1
assert zlib.crc32(table(legacy).encode("latin-1")) == 0x640ab977
assert zlib.crc32(function(legacy).encode("latin-1")) == 0x5c83b22a
assert table(legacy) == table(native)
assert function(legacy) == function(native)
assert "config_bind_table_anchor" in builder
assert "generated=0" in builder

def extracted(text):
    return "\n".join(declarations) + "\n" + table(text) + "\n" + function(text) + "\n"

reference = re.sub(
    r"\b(keyMappingTemp|keyMappingDefault|keyMapping|DoomRPG_setDefaultBinds)\b",
    lambda match: "Legacy_" + match.group(0), extracted(legacy))

shim = r"""
#pragma once
#include <stdint.h>
#include <string.h>
typedef struct DoomRPG_s { int unused; } DoomRPG_t;
#define KEYBINDS_MAX 10
typedef struct { int avk_action; int keyBinds[KEYBINDS_MAX]; } keyMapping_t;
#define SDL_memcpy memcpy
enum {
    AVK_UP=49, AVK_DOWN, AVK_LEFT, AVK_RIGHT,
    AVK_MOVELEFT, AVK_MOVERIGHT, AVK_NEXTWEAPON,
    AVK_PREVWEAPON, AVK_SELECT, AVK_PASSTURN,
    AVK_AUTOMAP, AVK_MENUOPEN,
    AVK_MENU_UP=0x40, AVK_MENU_DOWN=0x80,
    AVK_MENU_PAGE_UP=0x100, AVK_MENU_PAGE_DOWN=0x200,
    AVK_MENU_SELECT=0x400, AVK_MENU_OPEN=0x800
};
enum {
    SDL_SCANCODE_UP=82, SDL_SCANCODE_DOWN=81,
    SDL_SCANCODE_LEFT=80, SDL_SCANCODE_RIGHT=79,
    SDL_SCANCODE_A=4, SDL_SCANCODE_D=7,
    SDL_SCANCODE_Z=29, SDL_SCANCODE_X=27,
    SDL_SCANCODE_RETURN=40, SDL_SCANCODE_C=6,
    SDL_SCANCODE_TAB=43, SDL_SCANCODE_ESCAPE=41
};
"""
harness = r"""
#include "shim.h"
#include <assert.h>
#include <stdio.h>
extern keyMapping_t keyMapping[12], keyMappingTemp[12], keyMappingDefault[12];
extern keyMapping_t Legacy_keyMapping[12], Legacy_keyMappingTemp[12], Legacy_keyMappingDefault[12];
void DoomRPG_setDefaultBinds(DoomRPG_t*);
void Legacy_DoomRPG_setDefaultBinds(DoomRPG_t*);
static uint32_t state=0x5d1126a3u;
static uint32_t rng(void) { state=state*1664525u+1013904223u; return state; }
static void equal(void) {
    assert(memcmp(keyMapping,Legacy_keyMapping,sizeof(keyMapping))==0);
    assert(memcmp(keyMappingTemp,Legacy_keyMappingTemp,sizeof(keyMappingTemp))==0);
    assert(memcmp(keyMappingDefault,Legacy_keyMappingDefault,sizeof(keyMappingDefault))==0);
}
int main(void) {
    assert(sizeof(keyMapping_t)==44);
    assert(sizeof(keyMapping)==528 && sizeof(keyMappingTemp)==528 &&
           sizeof(keyMappingDefault)==528);
    equal(); /* process BSS zero + DATA defaults */
    DoomRPG_setDefaultBinds(0);
    Legacy_DoomRPG_setDefaultBinds(0);
    equal();
    for(int i=0;i<12;i++) {
        assert(keyMapping[i].avk_action==keyMappingDefault[i].avk_action);
        assert(keyMappingTemp[i].keyBinds[0]==keyMappingDefault[i].keyBinds[0]);
        for(int j=1;j<10;j++) assert(keyMapping[i].keyBinds[j]==-1);
    }
    for(int step=0;step<8192;step++) {
        /* Simulate a full Config load + subsequent temporary-key mutation. */
        for(int i=0;i<12;i++) {
            int avk=(int)rng();
            keyMapping[i].avk_action=Legacy_keyMapping[i].avk_action=avk;
            for(int j=0;j<10;j++) {
                int bind=(int)(rng()%2048u)-1024;
                keyMapping[i].keyBinds[j]=Legacy_keyMapping[i].keyBinds[j]=bind;
            }
        }
        memcpy(keyMappingTemp,keyMapping,sizeof(keyMapping));
        memcpy(Legacy_keyMappingTemp,Legacy_keyMapping,sizeof(Legacy_keyMapping));
        equal();
        if((step & 1)==0) {
            /* Reset must copy originals to BOTH working maps, not defaults. */
            DoomRPG_setDefaultBinds(0);
            Legacy_DoomRPG_setDefaultBinds(0);
            equal();
            assert(memcmp(keyMapping,keyMappingDefault,sizeof(keyMapping))==0);
            assert(memcmp(keyMappingTemp,keyMappingDefault,sizeof(keyMappingTemp))==0);
        }
    }
    puts("PASS native Config bind storage=1584B resetParity=8192 mutatedConfigs");
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-config-binds-") as folder:
    p = Path(folder)
    (p / "shim.h").write_text(shim)
    (p / "native.c").write_text('#include "shim.h"\n' + extracted(native))
    (p / "legacy.c").write_text('#include "shim.h"\n' + reference)
    (p / "test.c").write_text(harness)
    exe = p / "config-binds"
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-parameter", "-I", str(p),
                    str(p / "test.c"), str(p / "native.c"), str(p / "legacy.c"),
                    "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
