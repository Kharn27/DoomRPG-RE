#!/usr/bin/env python3
"""Execute real native planner bodies and original legacy goal/path on discrete fixtures."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
legacy=(ROOT/"src/Entity.c").read_text(encoding="latin-1")
def extract(signature, corpus=legacy):
    assert corpus.count(signature)==1, signature
    start=corpus.index(signature)
    depth=0
    for i in range(corpus.index("{",start),len(corpus)):
        if corpus[i]=="{": depth+=1
        elif corpus[i]=="}":
            depth-=1
            if not depth: return corpus[start:i+1]
    raise AssertionError(signature)

source=extract("int Entity_calcPath(Entity_t* entity,")+ "\n"+extract("boolean Entity_aiGoal_MOVE(Entity_t* entity,")
trace=extract("void Game_trace(Game_t* game,", (ROOT/"src/Game.c").read_text(encoding="latin-1"))
sight=extract("boolean Entity_checkLineOfSight(Entity_t* entity,")
trace=trace.replace("void Game_trace(", "void RefGame_trace(", 1)
sight=sight.replace("Entity_checkLineOfSight(", "RefActualSight(", 1).replace("Game_trace(", "RefGame_trace(")

for old,new in (("Entity_calcPath","RefPath"),("Entity_aiGoal_MOVE","RefGoal"),
                ("Entity_checkLineOfSight","RefSight"),("DoomRPG_randNextByte","RefByte"),
                ("Game_unlinkEntity","RefUnlink"),("Game_linkEntity","RefLink")):
    source=source.replace(old,new)
# Legacy n5 is intentionally unused; suppress this specific warning in reference only.
source="#pragma GCC diagnostic push\n#pragma GCC diagnostic ignored \"-Wunused-parameter\"\n"+source+"\n#pragma GCC diagnostic pop\n"
with tempfile.TemporaryDirectory(prefix="gameai14-path-") as tmp:
    p=Path(tmp)
    (p/"legacy_goal.inc").write_text(source)
    (p/"legacy_trace.inc").write_text(trace+"\n"+sight)
    for continuation in (0,1):
        exe=p/f"oracle-{continuation}"
        subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                        "-ffunction-sections","-fdata-sections","-Wl,--gc-sections",
                        "-DDOOMRPG_ESP32=1","-DDOOMRPG_PERF_GAME_P1=0",
                        f"-DCONTINUATION={continuation}","-I",str(p),
                        "-IESP32/test/oracles","-IESP32/include","-Isrc",
                        "ESP32/test/game_ai14_path_host.c","-o",str(exe)],
                       cwd=ROOT,check=True)
        subprocess.run([str(exe)],check=True)
