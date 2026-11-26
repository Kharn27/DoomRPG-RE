#!/usr/bin/env python3
"""Compile the original post-drop special-death tail against bounded fixtures."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
legacy = (ROOT / "src/Entity.c").read_text()
template = (ROOT / "ESP32/test/game_ai14_special_death_host.c").read_text()
start = "if ((entity->def->eSubType == 7) && !(entity->info & 0x10000000))"
end = "\n                break;\n\n            case 2: // Humans"
assert legacy.count(start) == 1
left = legacy.index(start)
right = legacy.index(end, left)
assert legacy.index("Entity_spawnDropItem(entity);", legacy.index("void Entity_died(")) < left
assert template.count("__LEGACY_TAIL__") == 1
with tempfile.TemporaryDirectory(prefix="doom-ai14-death-") as d:
    src = Path(d) / "legacy.c"
    exe = Path(d) / "legacy"
    src.write_text(template.replace("__LEGACY_TAIL__", legacy[left:right]))
    # Original Cyberdemon expression mixes & and | without parentheses.
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-Wno-parentheses", "-pedantic", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

# Type-1 GSprite is NOT a cosmetic explosion: execute the original timer
# and radius dispatcher, including the flags&4 suppression used by fire.
game = (ROOT / "src/Game.c").read_text()
def function(signature):
    assert game.count(signature) == 1
    left = game.index(signature)
    brace = game.index("{", left)
    depth = 0
    for i in range(brace, len(game)):
        if game[i] == "{":
            depth += 1
        elif game[i] == "}":
            depth -= 1
            if depth == 0:
                return game[left:i+1]
    raise AssertionError(signature)

alloc = function("GameSprite_t* Game_gsprite_allocAnim(")
type1 = alloc[alloc.index("case 1:"):alloc.index("case 2:")]
for marker in ("gSpr->sprite->info = 180;", "game->f684l = 1;",
               "Sound_playSound(game->doomRpg->sound, 5061, 0, 4);",
               "DoomCanvas_startShake(game->doomRpg->doomCanvas, 200, 2, 0);"):
    assert marker in type1
blast = (ROOT / "ESP32/test/game_ai14_death_blast_host.c").read_text()
blast = blast.replace("__RADIUS__", function("void Game_radiusHurtEntities("))
blast = blast.replace("__UPDATE__", function("void Game_gsprite_update("))
with tempfile.TemporaryDirectory(prefix="doom-ai14-death-blast-") as d:
    src = Path(d) / "legacy.c"
    exe = Path(d) / "legacy"
    src.write_text(blast)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-Wno-parentheses", "-pedantic", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
