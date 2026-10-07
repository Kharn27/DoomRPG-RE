# Milestone — Native camera interpolation for quarter-turns

Status: **REAL-CYD ROTATION VISUAL PASS — limited regression coverage**

Hardware-tested production code SHA: `db0eb476d8070d4a560893718ae79e7570f72fcc`.
CI on docs candidate `f534498636d829ee501757c7bd2b8751e26524ce`:
GitHub Actions run 37703393521, normal `esp32-cyd` **SUCCESS**,
static RAM **45056 B**, flash **773329 B**.
Branch `agent/esp32-camera-rotation-visual` is stacked on the merged PR #200 DIALOGCHAIN
closure (main merge SHA `f2a178e1fa0d04e0e163215bef6d745b7ea64da5`).
The rotation-specific code and its hardware-tested SHA remain frozen.
At the user's explicit request the same branch subsequently gains a separate
MOVE interpolation candidate. That later code is not part of the earlier
rotation hardware PASS; see MILESTONE_NATIVE_CAMERA_MOVE_VISUAL.md.

## Real classic CYD witness (2026-10-08)

User observed genuinely smooth and readable turning direction on the
physical screen, explicitly reporting no perceived lag. Four separate
quarter-turns completed with **two intermediate frames each**, all
`rendered=yes presented=1 gameplayStable=yes`, no preview FALLBACK:

| Action | Logical turn | Preview camera angles | Preview totalUs |
|---|---|---|---|
| TURN_LEFT | 64 -> 128 | 85, 106 | 105303, 93047 |
| TURN_RIGHT | 128 -> 64 | 107, 86 | 96047, 98072 |
| TURN_RIGHT | 64 -> 0 | 43, 22 | 92070, 91126 |
| TURN_LEFT | 0 -> 64 | 21, 42 | 89443, 94766 |

Every intermediate paint reported `presented=1`; each pair was
followed by `[VIEWANIM] END ... intermediates=2 logicalCommit=once
monsterTurn=no finalCardinal=next` and a normal
`[RESIDENTGAMEPLAY] TURN ... committed=yes`.
There was no `[MONSTERTURN]` schedule for these four rotations.
The preceding forward MOVE did produce a normal
`[MONSTERTURN] ORDERED-DISPATCH reason=MOVE turnToken=1`.
Automap discovery was observed only on canonical final renders
(`lines+=12` and `lines+=2`), never on the preview frames.
`[ALIVE]` at 31.7, 36.9 and 41.9 seconds remained exactly
`heap8=118288 largest8=86004 heap=184212`.
The user's direct visual feedback, not just CI, validates the UX goal.

**Scope limitation:** This excerpt did not include angle wrap
0 <-> 192, visible-monster rotation tests, automap-mode rotation,
dialog/HUB/SYS EXIT, LOAD or post-exit menu FNV. Therefore
no claim is made for these unexercised regressions, although they
retain the canonical native paths in the code. The tested world
rotation behavior and memory stability constitute the bounded
visual-rotation hardware PASS. Review/merge is reasonable with
the above regression scope explicitly recorded; deeper scenario
coverage can be done separately.

## Design and behavioral invariant

- A TURN still commits a single settled cardinal `EspPlayerView` before
  rendering. No intermediate pose is published to gameplay, events,
  collision, checkpoints or monster turn producers.
- Two non-cardinal camera poses are short-lived stack copies; no new
  persistent allocation and no second framebuffer.
- Intermediate wall/plane/sprite rendering bypasses gameplay monster
  activation and automap reveal mutation, skips FreshFrame notification
  and paints the compass from the committed cardinal angle.
- Final frame uses the unchanged canonical production route with its
  ordinary rollback/error handling. If a preview fails, the intended
  behavior is fail-open for the optional animation but fail-closed for
  the required canonical final frame.
- MOVE interpolation is a separately documented, later candidate on this
  same user-requested branch; the previously tested rotation code is frozen.

## Merge boundary

Hardware-proven code is `db0eb476d8070d4a560893718ae79e7570f72fcc`.
Commit `f534498636d829ee501757c7bd2b8751e26524ce` added only
this milestone's candidate documentation. This closure updates
documentation only. The user controls merging to main.
