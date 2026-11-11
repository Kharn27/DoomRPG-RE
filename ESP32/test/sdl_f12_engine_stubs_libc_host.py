#!/usr/bin/env python3
"""SDL-F12: restore the exact pre-F12 engine_stubs.c Git blob from libc calls.

This is an exhaustive source oracle, not a mocked runtime equivalence claim.
Previous host C tests still execute the real root/render/canvas/layout bodies.
No SDL video ABI, SDL.h typedef, allocation expression, order, error path,
rollback route, PAK owner or shared framebuffer lifetime may change in F12.
"""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "ESP32/src/engine_stubs.c"
SDL = ROOT / "ESP32/include/SDL.h"
PRE_F12_GIT_BLOB = "47783298dfcc4824585ef6fbd733ade6a1d5402b"

# All and only the 16 transformed production calls. Inverting exactly these
# statements (not global malloc/free: Z_Malloc already used libc in F11)
# reconstructs the full pre-F12 source byte for byte.
UNDO = (
    ("free(doomCanvas);", "SDL_free(doomCanvas);", 1),
    ("free(render->columnScale);", "SDL_free(render->columnScale);", 2),
    ("free(render->mediaPalettes);", "SDL_free(render->mediaPalettes);", 1),
    ("free(render->framebuffer);", "SDL_free(render->framebuffer);", 1),
    ("free(render);", "SDL_free(render);", 1),
    ("malloc(sizeof(DoomCanvas_t));", "SDL_malloc(sizeof(DoomCanvas_t));", 1),
    ("malloc(sizeof(Render_t));", "SDL_malloc(sizeof(Render_t));", 1),
    ("malloc(render->screenWidth * sizeof(int));",
     "SDL_malloc(render->screenWidth * sizeof(int));", 1),
    ("memset(&nativeRootIdentity, 0, sizeof(nativeRootIdentity));",
     "SDL_memset(&nativeRootIdentity, 0, sizeof(nativeRootIdentity));", 2),
    ("memset(doomCanvas, 0, sizeof(DoomCanvas_t));",
     "SDL_memset(doomCanvas, 0, sizeof(DoomCanvas_t));", 1),
    ("memset(render, 0, sizeof(Render_t));",
     "SDL_memset(render, 0, sizeof(Render_t));", 1),
    ("memset(&coreInitReport, 0, sizeof(coreInitReport));",
     "SDL_memset(&coreInitReport, 0, sizeof(coreInitReport));", 1),
    ("memset(&nativeRootOwners, 0, sizeof(nativeRootOwners));",
     "SDL_memset(&nativeRootOwners, 0, sizeof(nativeRootOwners));", 1),
    ("memset(&layoutReport, 0, sizeof(layoutReport));",
     "SDL_memset(&layoutReport, 0, sizeof(layoutReport));", 1),
)

def restore_pre_f12(current: str) -> str:
    """For historical fingerprint tests only; firmware compiles `current`."""
    old_includes = "#include <stdlib.h>\n"
    new_includes = old_includes + "#include <string.h>\n"
    assert current.count(new_includes) == 1
    restored = current.replace(new_includes, old_includes, 1)
    for libc, alias, count in UNDO:
        assert restored.count(libc) == count, (libc, restored.count(libc), count)
        restored = restored.replace(libc, alias)
    return restored

def git_blob_sha(text: str) -> str:
    binary = text.encode("utf-8")
    return hashlib.sha1(b"blob " + str(len(binary)).encode("ascii") +
                        b"\0" + binary).hexdigest()

if __name__ == "__main__":
    shim = SDL.read_text(encoding="utf-8")
    for alias, native in (("SDL_malloc", "malloc"),
                          ("SDL_free", "free"),
                          ("SDL_memset", "memset")):
        assert re.search(r"^#define\s+" + alias + r"\s+" + native + r"\s*$",
                         shim, re.M), (alias, native)
    owner = OWNER.read_text(encoding="utf-8")
    assert owner.startswith("#include <SDL.h>\n")
    assert '#include "SDL_Video.h"' in owner
    for abi in ("SDLVideo_t sdlVideo = {", "SDLVideo_t* SDL_GetVideo(void)",
                "SDL_Rect* windowRect", "SDL_Rect screenRect;",
                "SDL_CloseAudio();", "SDL_Close();",
                "EspNativeRenderRuntime_releaseCompatibility(render);",
                "EspNativeEngineRoot_releaseCanvas(", "DoomRPG_Error("):
        assert abi in owner, abi
    active = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", owner)
    assert not re.search(r"\bSDL_(?:malloc|free|memset)\s*\(", active)
    assert sum(item[2] for item in UNDO) == 16
    restored = restore_pre_f12(owner)
    digest = git_blob_sha(restored)
    assert digest == PRE_F12_GIT_BLOB, (digest, PRE_F12_GIT_BLOB)
    print("PASS SDL-F12 16 libc aliases; full pre-F12 Git blob parity "
          + digest + "; SDL video ABI and native owners intact")
