#!/usr/bin/env python3
"""Prove the CYD builder no longer generates any shim or desktop oracle.

Run the *real* SCons pre-script with a mocked environment and a deliberately
unusable desktop DoomRPG.c. The real source-parity/CRC and behavior regression
tests exercise src/DoomRPG.c -> permanent native owners independently.
This test verifies pre-script ownership and stale cleanup, not ELF or hardware parity.
"""
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BUILDER = ROOT / "ESP32/scripts/build_engine.py"
ORIGINALS = (
    "Combat.c", "CombatEntity.c", "DoomCanvas.c", "DoomRPG.c",
    "Entity.c", "EntityDef.c", "EntityMonster.c", "Game.c", "Hud.c",
    "Main.c", "Menu.c", "MenuItem.c", "MenuSystem.c", "ParticleSystem.c",
    "Player.c", "Render.c", "SDL_Video.c", "Sound.c", "Weapon.c",
    "Z_Zip.c", "Z_Zone.c",
)


class FakeEnv:
    def __init__(self, project, build):
        self.project = project
        self.build = build
        self.sources = []
        self.includes = []

    def subst(self, var):
        return {"$PROJECT_DIR": str(self.project),
                "$BUILD_DIR": str(self.build)}[var]

    def Append(self, **kwargs):
        self.includes.extend(kwargs["CPPPATH"])

    def BuildSources(self, dest, source, src_filter):
        self.sources.append((dest, source, src_filter))


def execute_build_script(env):
    scope = {"env": env, "Import": lambda name: None}
    output = StringIO()
    with redirect_stdout(output):
        exec(compile(BUILDER.read_text(encoding="utf-8"),
                     str(BUILDER), "exec"), scope)
    return output.getvalue()


def main():
    real_builder = BUILDER.read_text(encoding="utf-8")
    assert "doom_rpg_source_text" not in real_builder
    assert "doom_rpg_patched" not in real_builder
    assert "retired_desktop_units" in real_builder
    assert 'for stale_name in ("DoomRPG.c", "esp32_sdl.cpp")' in real_builder
    assert "env.BuildSources(" not in real_builder
    assert "texture_struct_needle" not in real_builder

    with tempfile.TemporaryDirectory(prefix="cyd-no-desktop-oracle-") as temp:
        work = Path(temp)
        project = work / "ESP32"
        native = project / "src"
        native.mkdir(parents=True)
        desktop = work / "src"
        desktop.mkdir()
        build = work / "build"
        staged = build / "doomrpg_patched_sources"
        staged.mkdir(parents=True)

        # A legacy source that CANNOT possibly be patched into a valid engine.
        # The build must not open or transform it.
        for name in ORIGINALS:
            (desktop / name).write_text("NOT A C IMPLEMENTATION\n")
        for name in ("esp_native_canvas_text.c", "native_main_menu_model.c",
                     "engine_stubs.c", "render_startup_bridge.c"):
            shutil.copyfile(ROOT / "ESP32/src" / name, native / name)
        # SDL-F33: archived shim lives under test/oracles, not project/src.
        fixture_oracles = project / "test/oracles"
        fixture_oracles.mkdir(parents=True)
        shutil.copyfile(ROOT / "ESP32/test/oracles/esp32_sdl.cpp",
                        fixture_oracles / "esp32_sdl.cpp")
        shutil.copyfile(ROOT / "ESP32/test/oracles/esp32_sdl_platform.h",
                        fixture_oracles / "esp32_sdl_platform.h")
        shutil.copyfile(ROOT / "ESP32/test/oracles/SDL_mixer.h",
                        fixture_oracles / "SDL_mixer.h")
        shutil.copyfile(ROOT / "ESP32/test/oracles/SDL.h",
                        fixture_oracles / "SDL.h")
        fixture_include = project / "include"
        fixture_include.mkdir()

        obsolete = staged / "DoomRPG.c"
        obsolete.write_text("stale generated C root\n")
        stale_shim = staged / "esp32_sdl.cpp"
        stale_shim.write_text("stale generated shim\n")
        stale_objects = build / "doomrpg_esp32_sdl_patched"
        stale_objects.mkdir()
        (stale_objects / "esp32_sdl.cpp.o").write_text("stale object\n")
        env = FakeEnv(project, build)
        log = execute_build_script(env)
        assert "C oracle generation retired" in log
        assert "compiledOriginal=0 compiledPatchedC=0" in log
        assert not obsolete.exists(), "stale generated root was not retired"

        # The exact SDL-A/F3 packed oracle was relocated outside the firmware
        # sources and must never be rewritten by the real build script.
        assert not (native / "esp32_sdl.cpp").exists()
        permanent = fixture_oracles / "esp32_sdl.cpp"
        snapshot = permanent.read_bytes()
        platform_header = fixture_oracles / "esp32_sdl_platform.h"
        header_snapshot = platform_header.read_bytes()
        mixer_header = fixture_oracles / "SDL_mixer.h"
        mixer_snapshot = mixer_header.read_bytes()
        sdl_header = fixture_oracles / "SDL.h"
        sdl_snapshot = sdl_header.read_bytes()
        assert not (fixture_include / "SDL_mixer.h").exists()
        assert not (fixture_include / "esp32_sdl_platform.h").exists()
        shim = snapshot.decode("utf-8")
        # SDL-D1: generated shim absent; packed texture ownership and
        # sampling are now native; only host fixtures retain this SDL oracle.
        native_header = (ROOT / "ESP32/include/esp_native_texture.h").read_text()
        native_owner = (ROOT / "ESP32/src/esp_native_texture.cpp").read_text()
        assert "uint8_t* indexedPixels;" in native_header
        assert "texture->indexed = true;" in shim
        assert "surface->pixels = nullptr;" in shim
        assert "surface->format->palette->colors = nullptr;" in shim
        assert "texture->indexedBitsPerPixel == 8" in native_owner
        assert "texture->indexedBitsPerPixel == 4" in native_owner
        assert "free(texture->indexedPixels);" in native_owner
        assert "SDL_RenderCopy(" not in shim
        assert env.sources == [], "generated sources unexpectedly registered"
        assert not stale_shim.exists() and not stale_objects.exists()
        assert len(env.includes) == 3

        # A second build must not write or mutate the host-only oracle source.
        env2 = FakeEnv(project, build)
        execute_build_script(env2)
        assert permanent.read_bytes() == snapshot
        assert platform_header.read_bytes() == header_snapshot
        assert mixer_header.read_bytes() == mixer_snapshot
        assert sdl_header.read_bytes() == sdl_snapshot
        assert not obsolete.exists() and not stale_shim.exists()
        assert env2.sources == []

        # A legacy SDL shim reappearing in ESP32/src is an explicit build
        # ownership violation, even though the old src_filter excludes it.
        revived = native / "esp32_sdl.cpp"
        revived.write_text("FORBIDDEN LEGACY SHIM\n")
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "Retired SDL shim returned to production source tree" in str(exc)
        else:
            raise AssertionError("revived production SDL shim accepted")
        revived.unlink()

        # The archived host oracle must exist; no silent fallback to a
        # production decoder is permitted.
        permanent.unlink()
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "Missing host-only SDL shim oracle source" in str(exc)
        else:
            raise AssertionError("missing host-only shim oracle accepted")
        permanent.write_bytes(snapshot)

        # SDL-F34: a display attachment header under production includes
        # reintroduces a dead SDL ABI, even if no source calls it.
        revived_header = fixture_include / "esp32_sdl_platform.h"
        revived_header.write_text("FORBIDDEN SDL DISPLAY HEADER\n")
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "Retired SDL display header returned to production includes" in str(exc)
        else:
            raise AssertionError("revived SDL display header accepted")
        revived_header.unlink()

        # The preserved oracle header is mandatory for the host SDL harness.
        platform_header.unlink()
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "Missing host-only SDL display header oracle" in str(exc)
        else:
            raise AssertionError("missing host SDL display header accepted")
        platform_header.write_bytes(header_snapshot)

        # SDL-F35: reject a production mixer header and a missing host oracle.
        revived_mixer = fixture_include / "SDL_mixer.h"
        revived_mixer.write_text("FORBIDDEN SDL MIXER HEADER\n")
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "Retired SDL mixer header returned to production includes" in str(exc)
        else:
            raise AssertionError("revived production mixer header accepted")
        revived_mixer.unlink()
        mixer_header.unlink()
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "Missing host-only SDL mixer header oracle" in str(exc)
        else:
            raise AssertionError("missing host mixer oracle accepted")
        mixer_header.write_bytes(mixer_snapshot)

        # SDL-F36: forbid the core header in production but require the oracle.
        revived_sdl = fixture_include / "SDL.h"
        revived_sdl.write_text("FORBIDDEN SDL CORE HEADER\n")
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "Retired SDL core header returned to production includes" in str(exc)
        else:
            raise AssertionError("revived production SDL header accepted")
        revived_sdl.unlink()
        sdl_header.unlink()
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "Missing host-only SDL core header oracle" in str(exc)
        else:
            raise AssertionError("missing host SDL core oracle accepted")
        sdl_header.write_bytes(sdl_snapshot)

        # The 21-file denylist is intentional and fails closed if the desktop
        # tree changes, rather than silently compiling a new original C file.
        (desktop / "Unexpected.c").write_text("new original TU\n")
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "Desktop original TU inventory changed" in str(exc)
        else:
            raise AssertionError("desktop source inventory drift was accepted")

        # Build input ownership must fail closed when native MENU state owner
        # disappears, after all desktop Canvas lifecycle exports are gone.
        owner_source = native / "native_main_menu_model.c"
        owner_body = owner_source.read_text(encoding="utf-8")
        owner_source.unlink()
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "native menu state owner missing" in str(exc)
        else:
            raise AssertionError("native menu state owner removal was accepted")
        owner_source.write_text(owner_body, encoding="utf-8")

        # A missing native layout owner must fail closed in isolation too.
        layout_source = native / "engine_stubs.c"
        layout_body = layout_source.read_text(encoding="utf-8")
        layout_source.unlink()
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "native engine layout owner missing" in str(exc)
        else:
            raise AssertionError("native layout owner removal was accepted")
        layout_source.write_text(layout_body, encoding="utf-8")

        # A missing permanent shim must fail closed at the build boundary.
        permanent.unlink()
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "Missing host-only SDL shim oracle source" in str(exc)
        else:
            raise AssertionError("missing permanent SDL source accepted")

    print("PASS generated C/SDL=0, permanent packed shim, stale copies/objects removed, "
          "desktop TU inventory and missing source fail closed")


if __name__ == "__main__":
    main()
