#!/usr/bin/env python3
"""SDL-F28: six real native rendering/intro C modules do not import SDL.h in
firmware. Reconstruct exact pre-F28 Git source bodies and compile the real
C11 DoomRPG/Render headers with native, host and geometry SDL boundaries.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ROOT / "ESP32/src"
BEFORE = {
    "esp_native_dynamic_line_render.c": "fa37687c047f075eb54364154339832a4b8a7b8c",
    "esp_native_intro_state.c": "7d01fc603fe36453d8819a6af88391344420eac2",
    "esp_native_plane_renderer.c": "3cff9235ac06f5f108e76c629fd0203bcce0c770",
    "esp_native_sprite_renderer.c": "702fa36c9c02fc9542df576727ff1c7e17fc283a",
    "esp_render_geometry_primitives.c": "5aa161bdd5e697edfc89b29b52812f40f5f2efa0",
    "native_first_frame_color_probe_wrappers.c": "9d4fdb192f8cb7f29df9cce1a429ee483fed63a5"
}
GUARD = ("#if !defined(DOOMRPG_ESP32) || "
         "defined(DOOMRPG_ESP32_GEOMETRY_ORACLE)\n"
         "#include <SDL.h>\n"
         "#endif\n")


def git_blob(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode() + b"\0" + data
    ).hexdigest()



# RC11: reverse only ambient/compose wrapper retirement before the RC10
# inverse; the SHA of the historical F28 renderer still has to match exactly.
RC11_RETIRED_PROTO = (
    "int __real_EspNativeGameplayFrame_renderAmbient(\n"
    "    struct Render_s* render,\n"
    "    EspNativeGameplayFrameStats* outStats);\n"
    "int __real_EspNativeGameplayFrame_composeTurn(\n"
    "    struct Render_s* render,\n"
    "    uint8_t angle,\n"
    "    EspNativeGameplayFrameStats* outStats);\n"
).encode("utf-8")


def restore_rc11_dynamic(value: bytes) -> bytes:
    edits = (
        (b'int EspNativeDynamicLineRender_renderAmbient(',
         b'int __wrap_EspNativeGameplayFrame_renderAmbient('),
        (b'int EspNativeDynamicLineRender_composeTurn(',
         b'int __wrap_EspNativeGameplayFrame_composeTurn('),
        (b'ok = EspNativeGameplayFrame_renderAmbient(',
         b'ok = __real_EspNativeGameplayFrame_renderAmbient('),
        (b'ok = EspNativeGameplayFrame_composeTurn(',
         b'ok = __real_EspNativeGameplayFrame_composeTurn('),
    )
    for newer, old in edits:
        assert value.count(newer) == 1, ("RC11 inverse anchor", newer)
        value = value.replace(newer, old, 1)
    marker = b'int __real_EspMapRuntime_getLine('
    assert value.count(marker) == 1, "RC11 original prototypes anchor"
    return value.replace(marker, RC11_RETIRED_PROTO + marker, 1)


# RC10: restore the original F28 body precisely; verify the frozen Git blob.
RC10_RETIRED_PROTO = "int __real_EspNativeGameplayFrame_renderVisualPose(\n    struct Render_s* render,\n    const struct EspPlayerViewState_s* visual,\n    uint8_t settledAngle,\n    EspNativeGameplayFrameStats* outStats);\nint __real_EspNativeGameplayFrame_renderVisualMove(\n    struct Render_s* render,\n    const struct EspPlayerViewState_s* before,\n    const struct EspPlayerViewState_s* after,\n    uint8_t step,\n    uint8_t denominator,\n    EspNativeGameplayFrameStats* outStats);\n".encode("utf-8")


def restore_rc10_dynamic(value: bytes) -> bytes:
    edits = (
        (b'#include "esp_native_dynamic_line_render.h"\n', b''),
        (b'int EspNativeDynamicLineRender_renderVisualPose(',
         b'int __wrap_EspNativeGameplayFrame_renderVisualPose('),
        (b'int EspNativeDynamicLineRender_renderVisualMove(',
         b'int __wrap_EspNativeGameplayFrame_renderVisualMove('),
        (b'ok = EspNativeGameplayFrame_renderVisualPose(',
         b'ok = __real_EspNativeGameplayFrame_renderVisualPose('),
        (b'ok = EspNativeGameplayFrame_renderVisualMove(',
         b'ok = __real_EspNativeGameplayFrame_renderVisualMove('),
    )
    for newer, older in edits:
        assert value.count(newer) == 1, ("RC10 inverse anchor", newer)
        value = value.replace(newer, older, 1)
    marker = b'int __real_EspMapRuntime_getLine('
    assert value.count(marker) == 1, "RC10 original prototypes anchor"
    return value.replace(marker, RC10_RETIRED_PROTO + marker, 1)



def restore_pre_fire_vis_p2_sprite(value: bytes) -> bytes:
    """Reverse ONLY the FIRE-VIS-P2 LCD witness insertion; 375ms policy
    deliberately restores the pre-P2 scheduler exactly. Keep the frozen
    pre-SDL-F28 SHA immutable and assert this full insertion once.
    """
    inserted = (
        b'        if (!glow && EspNativeAmbientSprite_fireEligible(\n'
        b'                         parent->info, entityType)) {\n'
        b'            /* Only completed masked pixels, never admission/near-cull,\n'
        b'             * count as real fire: record the first LCD witness. The native\n'
        b'             * 375ms ambient clock now owns ALL fast sprites including fire. */\n'
        b'            if (stats->fireRasterCount == 0U) {\n'
        b'                stats->fireSampleIndex = parent->index;\n'
        b'                stats->fireSampleLogical = logical;\n'
        b'                stats->fireSampleActual = frame->actual;\n'
        b'                stats->fireSamplePhase = (uint8_t)animation;\n'
        b'                stats->fireSampleMs = ambientSampleMs;\n'
        b'            }\n'
        b'            ++stats->fireRasterCount;\n'
        b'        }\n'
    )
    assert value.count(inserted) == 1, "FIRE-VIS-P2 historical inverse anchor drift"
    return value.replace(inserted, b'', 1)


def restore_pre_fire_vis_sprite(value: bytes) -> bytes:
    """Invert only FIRE-VIS-P1 renderer bytes before the frozen SDL-F28 blob.

    Historical SHA expectations remain immutable. The running firmware keeps
    the native additive fire routing and its new include as normal.
    """
    inverse = (
        (b'#include "esp_native_sprite_renderer.h"\n'
         b'#include "esp_native_fire_visual.h"\n',
         b'#include "esp_native_sprite_renderer.h"\n'),
        (b'if (EspNativeFireVisual_mapMode(\n'
         b'                spriteRenderMode(resourceLogical), type, info) ==\n'
         b'            RENDER_MODE_ADD) {',
         b'if (spriteRenderMode(resourceLogical) == RENDER_MODE_ADD) {'),
        (b'    /* Legacy Entity_initspawn: only actual non-CROSS fire map entities\n'
         b'     * use additive mode 7. Preserve independent glow/CROSS/transient modes,\n'
         b'     * resource lookup, depth, masking and draw order unchanged. */\n'
         b'    if (!glow) {\n'
         b'        renderMode = EspNativeFireVisual_mapMode(\n'
         b'            renderMode, entityType, parent->info);\n'
         b'    }\n',
         b''),
    )
    for added, previous in inverse:
        assert value.count(added) == 1, "FIRE-VIS-P1 inverse anchor"
        value = value.replace(added, previous, 1)
    return value


def original_body_parity() -> None:
    marker = GUARD.encode("ascii")
    for filename, blob in BEFORE.items():
        current = (SOURCES / filename).read_bytes()
        from perf_game_p2a_history import restore_pre_perf_game_p2a
        current = restore_pre_perf_game_p2a(filename, current)
        if filename == "esp_native_dynamic_line_render.c":
            # AUDIO-DOOR only adds a post-first-successful-door-frame SFX hook.
            # Undo its exact additive bytes before testing the immutable
            # original SDL-F28 Git blob; keep all frozen hashes unchanged.
            from arch_rc23_history import restore_pre_rc23
            current = restore_pre_rc23(filename, current)
            from audio_door_history import restore_pre_audio_door_dynamic
            from arch_rc19_history import restore_pre_rc19_dynamic
            historical = restore_rc10_dynamic(restore_rc11_dynamic(
                restore_pre_audio_door_dynamic(restore_pre_rc19_dynamic(current))))
        elif filename == "esp_native_plane_renderer.c":
            from res60_p27_plane_history import restore_pre_res60_planes
            from perf_game_p3g_history import restore_pre_p3g_plane
            from perf_game_p3h_history import restore_pre_p3h_plane
            historical = restore_pre_p3g_plane(restore_pre_p3h_plane(restore_pre_res60_planes(current)))
        elif filename == "esp_native_sprite_renderer.c":
            from perf_game_p3k_history import restore_pre_p3k_sprite
            historical = restore_pre_fire_vis_sprite(restore_pre_fire_vis_p2_sprite(restore_pre_p3k_sprite(current)))
        else:
            historical = current
        if filename == "esp_render_geometry_primitives.c":
            # Unlike the five other TUs, SDL.h originally follows stdint.
            assert current.startswith(b"#include <stdint.h>\n\n" + marker), filename
            restored = historical.replace(marker, b"#include <SDL.h>\n", 1)
        else:
            assert current.startswith(marker), (
                f"{filename}: expected bounded host/geometry-only SDL guard")
            restored = b"#include <SDL.h>\n" + historical[len(marker):]
        assert git_blob(restored) == blob, (
            f"{filename}: original pre-F28 body not preserved exactly")
        assert current.count(b"#include <SDL.h>") == 1, filename
        clean = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "",
                       current.decode("utf-8"))
        clean = clean.replace(GUARD, "")
        assert not re.search(
            r"\b(?:SDL_[A-Za-z0-9_]+|SDLK_[A-Za-z0-9_]+|"
            r"SDL_Texture|SDL_Rect|SDL_RWops|SDLCALL|Uint(?:8|16|32))\b",
            clean), f"{filename}: SDL API retained in body"
        assert '#include "DoomRPG.h"' in clean, filename
    print("SDL-F28 PASS sources: 6 native render/intro C bodies are exact "
          "pre-F28 Git blobs, native SDL.h imports absent")


def header_probe() -> None:
    code = r"""
#include <stdint.h>
#if EXPECT_SDL
#include <SDL.h>
#endif
#include "DoomRPG.h"
#include "Render.h"
#if EXPECT_SDL
#ifndef DOOMRPG_ESP32_SDL_H
#error "host SDL oracle header no longer included"
#endif
#else
#ifdef DOOMRPG_ESP32_SDL_H
#error "native DoomRPG/Render headers imported SDL"
#endif
_Static_assert(sizeof(EspNativeRect) == 4*sizeof(int),
               "native render rectangle ABI drift");
#endif
_Static_assert(sizeof(Random_t) == 132, "RNG ABI changed");
int main(void) {
    Random_t state = {0};
    return state.nextRand != 0;
}
"""
    with tempfile.TemporaryDirectory(prefix="sdl-f28-render-intro-") as temp:
        root = Path(temp)
        src, exe = root / "probe.c", root / "probe"
        src.write_text(code, encoding="utf-8")
        cmd = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
               "-I", str(ROOT / "ESP32/test/oracles"),
                "-I", str(ROOT / "ESP32/include"),
               "-I", str(ROOT / "src")]
        for label, flags in (
            ("native", ["-DDOOMRPG_ESP32=1", "-DEXPECT_SDL=0"]),
            ("desktop", ["-DEXPECT_SDL=1"]),
            ("geometry", ["-DDOOMRPG_ESP32=1",
                          "-DDOOMRPG_ESP32_GEOMETRY_ORACLE=1",
                          "-DEXPECT_SDL=1"]),
        ):
            subprocess.run(cmd + flags + [str(src), "-o", str(exe)],
                           check=True)
            subprocess.run([str(exe)], check=True)
            print(f"SDL-F28 PASS C11 {label}: native DoomRPG/Render ABI")


def compiled_profile(profile: str) -> None:
    db = ROOT / "ESP32/compile_commands.json"
    elf = ROOT / "ESP32/.pio/build" / profile / "firmware.elf"
    assert db.is_file(), f"{profile}: missing compiledb"
    assert elf.is_file(), f"{profile}: missing ELF"
    compiled = {Path(x["file"]).name for x in
                json.loads(db.read_text(encoding="utf-8"))}
    assert all(name in compiled for name in BEFORE), (
        f"{profile}: F28 native render/intro source not compiled")
    assert "esp32_sdl.cpp" not in compiled, profile
    assert "esp32_bmp.cpp" not in compiled, profile
    print(f"SDL-F28 PASS {profile}: 6 render/intro TUs compiled; "
          "old SDL shim and BMP excluded; existing ELF SDL-F3 audit retained")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=(
        "esp32-cyd", "esp32-cyd-bringup", "esp32-cyd-trace"))
    args = parser.parse_args()
    original_body_parity()
    if args.profile:
        compiled_profile(args.profile)
    else:
        header_probe()


if __name__ == "__main__":
    main()
