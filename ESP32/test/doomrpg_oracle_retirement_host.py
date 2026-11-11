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
        for name in ("esp32_sdl.cpp", "esp_native_canvas_text.c",
                     "native_main_menu_model.c", "engine_stubs.c",
                     "render_startup_bridge.c"):
            shutil.copyfile(ROOT / "ESP32/src" / name, native / name)

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

        # SDL-A keeps its packed implementation for host oracle tests only; F3 no longer compiles it in firmware.
        permanent = native / "esp32_sdl.cpp"
        snapshot = permanent.read_bytes()
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
        assert not obsolete.exists() and not stale_shim.exists()
        assert env2.sources == []

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
            assert "Missing permanent SDL shim source" in str(exc)
        else:
            raise AssertionError("missing permanent SDL source accepted")

    print("PASS generated C/SDL=0, permanent packed shim, stale copies/objects removed, "
          "desktop TU inventory and missing source fail closed")


if __name__ == "__main__":
    main()
