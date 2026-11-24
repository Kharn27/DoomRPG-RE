#!/usr/bin/env python3
"""RES60-P22: source-backed 192x144 GAMEPLAY admission blockers, host-only.
This is a structural sentinel, NOT a test that real 192 gameplay is working.
"""
from pathlib import Path
import configparser

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/"ESP32"
def read(p):
    s=(SOURCE/p).read_text()
    assert s, p
    return s

ini=configparser.ConfigParser(interpolation=None,inline_comment_prefixes=(";",))
assert ini.read(SOURCE/"platformio.ini")
release=ini["env:esp32-cyd"]["build_flags"]
res60=ini["env:esp32-cyd-res60"]["build_flags"]
assert "DOOMRPG_RES60_P1_DIAGNOSTIC=1" in res60
assert "DOOMRPG_RES60_P21_INTERSECTION32=1" in res60
assert ini["env:esp32-cyd-res60-inverted"]["extends"]=="env:esp32-cyd-res60"
for env in ("env:esp32-cyd","env:esp-cyd-inverted","env:esp32-cyd-bringup","env:esp32-cyd-trace"):
    assert "-D DOOMRPG_RES60_P" not in ini[env]["build_flags"],env
assert "-D DOOMRPG_RES60_P" not in release

config=read("include/platform_video_config.h")
assert "#define DOOMRPG_VIDEO_BUFFER_WIDTH 192" in config
assert "#define DOOMRPG_VIDEO_BUFFER_HEIGHT 144" in config
assert "#define DOOMRPG_VIDEO_BUFFER_WIDTH DOOMRPG_LOGICAL_WIDTH" in config
assert "#define DOOMRPG_VIDEO_BUFFER_HEIGHT DOOMRPG_LOGICAL_HEIGHT" in config
assert '#error "Classic CYD gameplay viewport must remain 160x80"' in config
assert "#define DOOMRPG_HUD_TOP_HEIGHT 20" in config
assert "#define DOOMRPG_HUD_BOTTOM_HEIGHT 20" in config

main=read("src/main.cpp")
assert main.count("#if DOOMRPG_RES60_P1_DIAGNOSTIC")>=2
assert "start=BSP_PREVIEW_ONLY gameplay=NOT_STARTED" in main
assert "gameplay=NOT_STARTED" in main
start=main.index("    initializePlatformVideo();")
guard=main.index("#if DOOMRPG_RES60_P1_DIAGNOSTIC",start)
returnFromPreview=main.index("    return;\n#endif\n    initializeSdCard();",guard)
engine=main.index("    initializeEngineCore();",returnFromPreview)
assert start<guard<returnFromPreview<engine

world=read("src/esp_native_first_frame.c")
assert "#define MAX_SCREEN_WIDTH 160U" in world
assert "render->screenWidth > (int)MAX_SCREEN_WIDTH" in world
assert "EspMapRuntime_isLoaded()" in world

frame=read("src/esp_native_gameplay_frame.c")
assert "render->screenWidth != 160 || render->screenHeight != 80" in frame
assert "DOOMRPG_VIEWPORT_Y != 20" in frame
assert "EspNativeFirstFrame_renderGameplayViewport" in frame

plane=read("src/esp_native_plane_renderer.c")
assert "render->screenWidth != 160 || render->screenHeight != 80" in plane

sprites=read("src/esp_native_sprite_renderer.c")
assert "#define SCREEN_W 160" in sprites
assert "#define SCREEN_H 80" in sprites
assert "width != 160U || height != 120U" in sprites

hud=read("src/esp_native_gameplay_hud.c")
assert "#if DOOMRPG_LOGICAL_WIDTH != 160 || DOOMRPG_LOGICAL_HEIGHT != 120" in hud
assert '#error "The first native gameplay HUD contract is defined only for 160x120"' in hud

preview=read("src/esp_res60_world_preview.c")
assert "PREVIEW_WALL_HEIGHT = 64" in preview
assert "DOOMRPG_RES60_P21_INTERSECTION32" in preview

assert 160*120*2==38400
assert 192*144*2==55296
assert 55296-38400==16896
assert 160*80==12800
assert 192*96==18432
assert 192*96-160*80==5632
assert (192*144)*100 == 144*(160*120)

print("RES60-P22 HOST AUDIT PASS: seven production 160x120/160x80 admission contracts intact; preview isolated; 16896B framebuffer delta; no runtime change")
