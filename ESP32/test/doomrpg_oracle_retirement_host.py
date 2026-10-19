#!/usr/bin/env python3
"""Prove the CYD builder generates only the live packed SDL shim.

Run the *real* SCons pre-script with a mocked environment and a deliberately
unusable desktop DoomRPG.c. The real source-parity/CRC and behavior regression
tests exercise src/DoomRPG.c -> permanent native owners independently.
This test is not a firmware/ELF or hardware-parity claim.
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
    assert 'retired_oracle = join(patched_dir, "DoomRPG.c")' in real_builder
    assert 'src_filter=["+<esp32_sdl.cpp>"]' in real_builder

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
        for name in ("esp32_sdl.cpp", "esp_legacy_doomcanvas_bridge.c",
                     "native_main_menu_model.c"):
            shutil.copyfile(ROOT / "ESP32/src" / name, native / name)

        obsolete = staged / "DoomRPG.c"
        obsolete.write_text("stale generated C root\n")
        env = FakeEnv(project, build)
        log = execute_build_script(env)
        assert "C oracle generation retired" in log
        assert "compiledOriginal=0 compiledPatchedC=0" in log
        assert not obsolete.exists(), "stale generated root was not retired"

        generated = staged / "esp32_sdl.cpp"
        assert generated.exists()
        shim = generated.read_text(encoding="utf-8")
        assert "Uint8* indexedPixels;" in shim
        assert "SDL_CreateTextureFromSurface" in shim
        assert "texture->indexed = true;" in shim
        assert "surface->pixels = nullptr;" in shim
        assert "surface->format->palette->colors = nullptr;" in shim
        assert "if (texture->indexedBitsPerPixel == 8)" in shim
        assert "else if (texture->indexedBitsPerPixel == 4)" in shim
        assert "SDL_DestroyTexture" in shim
        assert shim != (native / "esp32_sdl.cpp").read_text(encoding="utf-8")
        assert env.sources == [
            (str(build / "doomrpg_esp32_sdl_patched"),
             str(staged), ["+<esp32_sdl.cpp>"])]
        assert len(env.includes) == 3

        # Deterministic regeneration: an independent second run creates the
        # same packed shim, with no generated DoomRPG.c or extra build input.
        expected = generated.read_bytes()
        env2 = FakeEnv(project, build)
        execute_build_script(env2)
        assert generated.read_bytes() == expected
        assert not obsolete.exists()
        assert env2.sources == env.sources

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
        # disappears, even though the three Canvas compatibility exports remain.
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

        # SDL patching also retains its exact single-anchor failure guard.
        (desktop / "Unexpected.c").unlink()
        (native / "esp32_sdl.cpp").write_text("broken SDL source\n")
        try:
            execute_build_script(FakeEnv(project, build))
        except RuntimeError as exc:
            assert "Unable to locate SDL_Texture layout" in str(exc)
        else:
            raise AssertionError("SDL anchor drift was accepted")

    print("PASS generated DoomRPG C=0, packed SDL only, stale copy removed, "
          "desktop TU inventory and SDL anchors fail closed")


if __name__ == "__main__":
    main()
