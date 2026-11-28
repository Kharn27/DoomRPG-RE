#!/usr/bin/env python3
"""Execute the actual native coordinate radius leaf and its barrel adapter.

Map lookup, damage and removal are fixtures, not a full world transaction.
The leaf resolves impacts in order; the enclosing transaction owns rollback.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "ESP32/src/esp_native_gameplay_action_engine.c").read_text()
host = (ROOT / "ESP32/test/game_ai14_blast_source_host.c").read_text()

def extract(signature):
    assert source.count(signature) == 1, signature
    left = source.index(signature)
    brace = source.index("{", left)
    depth = 0
    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[left:i+1]
    raise AssertionError(signature)

defines = []
for name in ("ACTION_ENTITY_ENEMY", "ACTION_BARREL_CHAIN_MAX",
             "ACTION_BARREL_RADIUS_NEIGHBORS", "ACTION_TRAP_EXPLOSION_LOGICAL",
             "ACTION_TRAP_EXPLOSION_FRAMES", "ACTION_ENTITY_DESTRUCTIBLE",
             "ACTION_DESTRUCTIBLE_BARREL_SUBTYPE", "ACTION_DESTRUCTIBLE_CRATE_SUBTYPE"):
    m = re.findall(r"^#define " + name + r" [^\n]+", source, re.M)
    assert len(m) == 1, name
    defines.extend(m)
target = extract("typedef struct ActionTarget_s") + " ActionTarget;"
leaf = extract("static int explosionTriggerNeighborsAt(")
wrapper = extract("static int barrelTriggerNeighbors(")
assert "EspMapRuntime_getMapSpriteBase(" not in leaf
assert wrapper.count("EspMapRuntime_getMapSpriteBase(") == 1
assert wrapper.count("explosionTriggerNeighborsAt(") == 1
assert source.count("EspNativeGameplayBarrelRadius_terminalDeathOwnedAt(\n"
                    "                                barrelExpectedChainCount, blastCursor),") == 1
assert "barrelImpact.deathRandValid != 0U\n                            ? &barrelImpact.randomBeforeDeathByte" in source
assert "barrelPreviewWords == barrelExpectedChainCount &&" in source
assert "barrelPreviewWords < barrelExpectedChainCount ||" in source
assert "blastCursor == 0U && barrelPreviewWords == 1U" in source
assert "EspNativeGameplayBarrelRadius_previewWordsForChain(" in source
host = (host.replace("__DEFINES__", "\n".join(defines))
            .replace("__TARGET__", target)
            .replace("__JOURNAL__", extract("typedef struct ExplosionImpactJournal_s") + " ExplosionImpactJournal;")
            .replace("__CONTAINS__", extract("static int barrelChainContains("))
            .replace("__PREFLIGHT__", extract("static int barrelChainPreflight("))
            .replace("__LEAF__", leaf).replace("__WRAPPER__", wrapper)
            .replace("__PREPARE_DEATH__", extract("int EspNativeGameplayActionEngine_prepareDeathBlast("))
            .replace("__RUN_DEATH__", extract("int EspNativeGameplayActionEngine_runDeathBlast("))
            .replace("__ROLLBACK_BARRELS__", extract("static int barrelRollbackRemoved("))
            .replace("__ENEMY_READY__", extract("int EspNativeGameplayActionEngine_enemyBarrelReady("))
            .replace("__ENEMY_IMPACT__", extract("int EspNativeGameplayActionEngine_enemyBarrelImpact(")))
with tempfile.TemporaryDirectory(prefix="doom-ai14-blast-source-") as d:
    src = Path(d) / "native.c"
    exe = Path(d) / "native"
    src.write_text(host)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-pedantic", "-I", str(ROOT / "ESP32/include"),
                    str(src), "-o", str(exe)], check=True)
    result = subprocess.run([str(exe)], check=True, text=True, capture_output=True)
    print('\n'.join(line for line in result.stdout.splitlines() if line.startswith('GAMEAI14')))

# A2-P1b: actual combat sequence commit/cancel/restore, using bounded fixture
# snapshots. Verify terminal admission source gates in the enclosing action.
combat = (ROOT / "ESP32/src/esp_native_gameplay_monster_combat.c").read_text()
def take(s, signature):
    assert s.count(signature) == 1, signature
    left = s.index(signature)
    start = s.index("{", left)
    depth = 0
    for i in range(start, len(s)):
        if s[i] == "{":
            depth += 1
        elif s[i] == "}":
            depth -= 1
            if depth == 0:
                return s[left:i+1]
    raise AssertionError(signature)

assert "if (barrelPreflightMonsters != 0U &&\n                    !EspNativeGameplayMonsterCombat_beginSplashSequence(" in source
assert "if (barrelPreflightMonsters != 0U && barrelPlayerLethal == 0U) {" in source
assert 'if ((isBarrel || barrelExplosionArmed != 0U) && barrelPlayerLethal == 0U) {\n            printf("[BARREL] COMMIT' in source
assert "if (barrelExplosionArmed != 0U && barrelPlayerLethal == 0U &&\n            barrelImpact.monsterHits != 0U" in source
assert "CRATETRAP] LETHAL-DEFER" not in source and "explosionPeekWordLowByte" not in source
assert source.count("!barrelChainPreflight(pending.spriteIndex, pending.subtype, view,") == 1
assert source.count("if (barrelExplosionArmed != 0U) {\n            uint8_t waveStart") == 1
assert source.count("blastRootPending = 1U;") == 2
assert "if (isCrate) crateRemoved = 1U;" in source
cell = extract("static int barrelRadiusCellSupportedExcept(")
assert "EspNativeGameplayBarrelRadius_lineNoop(lineType, lineSubtype)" in cell
assert "EspNativeGameplayBarrelRadius_entityNoop(outTarget->type," in cell
assert cell.index("findRadiusSpriteOnTile(") < cell.index("EspNativeGameplayBarrelRadius_entityNoop(") < cell.index("if (outTarget->type == ACTION_ENTITY_ENEMY)")
terminal = source[source.index("if (barrelExplosionArmed != 0U && barrelPlayerLethal != 0U) {"):]
arm = terminal.index("!EspNativeGameplayPlayerDeath_armWithReservedRand(")
cancel = terminal.index("barrelRollbackRemoved(")
commit = terminal.index("EspNativeGameplayMonsterCombat_commitTerminalSplashSequence(")
audio = terminal.index("EspNativeGameplayMonsterCombat_publishSplashAudioAfterCommit(")
assert arm < cancel < commit < audio
assert "return rollbackOk ? 1 : 0;" in terminal[cancel:commit]
assert "EspNativeGameplayMonsterCombat_cancelSplashSequence(runtime)" in take(source, "static int barrelRollbackRemoved(")
host = (ROOT / "ESP32/test/game_ai14_terminal_splash_host.c").read_text()
for marker, signature in (
    ("__RESTORE__", "static int restoreSplashSnapshot("),
    ("__BEGIN__", "int EspNativeGameplayMonsterCombat_beginSplashSequence("),
    ("__CANCEL__", "int EspNativeGameplayMonsterCombat_cancelSplashSequence("),
    ("__COMMIT__", "int EspNativeGameplayMonsterCombat_commitSplashSequence("),
    ("__QUERY__", "int EspNativeGameplayMonsterCombat_splashSequenceArmed("),
    ("__TERMINAL__", "int EspNativeGameplayMonsterCombat_commitTerminalSplashSequence("),
):
    host = host.replace(marker, take(combat, signature))
with tempfile.TemporaryDirectory(prefix="doom-ai14-terminal-splash-") as d:
    src = Path(d) / "native.c"
    exe = Path(d) / "native"
    src.write_text(host)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-pedantic", str(src), "-o", str(exe)], check=True)
    result = subprocess.run([str(exe)], check=True, text=True, capture_output=True)
    print(result.stdout.splitlines()[-1])


# A2-P1f: actual transient lease clock, actual viewport mover, wrap and expiry.
sprite = (ROOT / "ESP32/src/esp_native_sprite_renderer.c").read_text()
pain = (ROOT / "ESP32/src/esp_native_gameplay_player_pain_visual.c").read_text()
frame = (ROOT / "ESP32/src/esp_native_gameplay_frame.c").read_text()
assert frame.index("blastShake = EspNativeSpriteRenderer_blastShakeOffsets(") < frame.index("EspNativeFirstFrame_renderGameplayViewport(")
assert frame.index("if (blastShake) {") < frame.index("stats->viewportAfterSpritesFNV = viewportFNV();")
assert frame.index("if (blastShake) {") < frame.index("EspNativeGameplayHudDirection_render(angle, hud)")
start = sprite.index("typedef struct TransientWorldBatch_s")
end = sprite.index("static uint8_t scratchOwnerLogged;", start)
host = r"""
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef struct { int16_t worldX, worldY; } EspNativeSpriteTransient;
static int64_t clockUs;
static int resourceOk = 1;
static int64_t esp_timer_get_time(void) { return clockUs; }
static const void* EspNativeGraphicsCatalog_findSprite(uint16_t logical) {
    return resourceOk && logical < 256U ? &clockUs : NULL;
}
"""
host += sprite[start:end]
for signature in ("int EspNativeSpriteRenderer_armTransientBatch(",
                  "void EspNativeSpriteRenderer_clearTransient(",
                  "int EspNativeSpriteRenderer_blastShakeOffsets("):
    host += take(sprite, signature) + "\n"
host += take(pain, "int EspNativePlayerPainVisual_shiftViewport(")
host += r"""
static uint16_t fb[160*120], original[160*120];
int main(void) {
    EspNativeSpriteTransient item = {672,672};
    int8_t dx,dy,repeatX,repeatY;
    unsigned tests=0;
    assert(!EspNativeSpriteRenderer_blastShakeOffsets(0,&dx,&dy));
    for (unsigned epoch=0;epoch<2;++epoch) {
        uint32_t base=epoch?UINT32_MAX-99U:1000U;
        clockUs=(int64_t)base*1000LL;
        assert(EspNativeSpriteRenderer_armTransientBatch(180,0,&item,1));
        for(unsigned age=0;age<=250;++age) {
            clockUs=(int64_t)(uint32_t)(base+age)*1000LL;
            if(age==150)assert(EspNativeSpriteRenderer_armTransientBatch(180,1,&item,1));
            int expected=age<200;
            assert(EspNativeSpriteRenderer_blastShakeOffsets(base+age,&dx,&dy)==expected);
            if(expected){
                assert(dx>=-2 && dx<=1 && dy>=-2 && dy<=1);
                assert(EspNativeSpriteRenderer_blastShakeOffsets(base+age,&repeatX,&repeatY));
                assert(dx==repeatX && dy==repeatY);
            }
            ++tests;
        }
        assert(!EspNativeSpriteRenderer_blastShakeOffsets(base+250,&dx,&dy));
        EspNativeSpriteRenderer_clearTransient();
        assert(!EspNativeSpriteRenderer_blastShakeOffsets(base,&dx,&dy));
    }
    clockUs=1000000;
    assert(EspNativeSpriteRenderer_armTransientBatch(180,1,&item,1));
    assert(!EspNativeSpriteRenderer_blastShakeOffsets(1000,&dx,&dy));
    assert(EspNativeSpriteRenderer_armTransientBatch(180,0,&item,1));
    resourceOk=0;
    assert(!EspNativeSpriteRenderer_armTransientBatch(180,0,&item,1));
    assert(EspNativeSpriteRenderer_blastShakeOffsets(1199,&dx,&dy));
    assert(!EspNativeSpriteRenderer_blastShakeOffsets(1200,&dx,&dy));
    resourceOk=1;
    assert(EspNativeSpriteRenderer_armTransientBatch(181,0,&item,1));
    assert(!EspNativeSpriteRenderer_blastShakeOffsets(1000,&dx,&dy));
    assert(EspNativeSpriteRenderer_armTransientBatch(180,0,&item,1));
    assert(EspNativeSpriteRenderer_blastShakeOffsets(1000,&dx,&dy));
    assert(!EspNativeSpriteRenderer_blastShakeOffsets(1000,NULL,&dy));
    EspNativeSpriteRenderer_clearTransient();
    for(int y=0;y<120;++y)for(int x=0;x<160;++x)original[y*160+x]=(uint16_t)(y*160+x+1);
    for(int yoff=-2;yoff<=1;++yoff)for(int xoff=-2;xoff<=1;++xoff){
        memcpy(fb,original,sizeof fb);
        assert(EspNativePlayerPainVisual_shiftViewport(fb,160,120,(int8_t)xoff,(int8_t)yoff));
        for(int y=0;y<120;++y)for(int x=0;x<160;++x){
            int sy=y-yoff,sx=x-xoff;
            uint16_t expected=original[y*160+x];
            if(y>=20 && y<100)expected=sy>=20 && sy<100 && sx>=0 && sx<160?original[sy*160+sx]:0;
            assert(fb[y*160+x]==expected);
        }
    }
    printf("GAMEAI14 A2-P1f PASS %u clock samples including wrap, lease clear/switch, 16 exact viewport shifts, HUD invariant\n",tests);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-ai14-blast-shake-") as d:
    src = Path(d) / "shake.c"
    exe = Path(d) / "shake"
    src.write_text(host)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-pedantic", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
