# ESP32 native generic door MOVE close — real-CYD hardware PASS

Date: 2026-08-27
Hardware-tested SHA: `53061cbf6142b03f01de4380dc2fc5f79ef01856`
Branch at validation: `agent/esp32-native-action-select-exec`
Environment: normal `esp32-cyd`

## Boundary

This milestone closes the first real native regular door from resident gameplay
using the recovered generic movement-event path. It does not hard-code Entrance,
line 275, tile 838, or event 87 into the engine. Those values are witnesses read
from the resident `/intro.bsp` data on the real CYD.

The already-validated SELECT path opens a door with `EV_OPENLINE`. MOVE now
reproduces the two legacy tile-event phases around a cardinal move:

- source/EXIT: movement exit flag | `0x400`;
- destination/ENTER: movement enter flag | facing flag | `0x400`.

Both phases are preflighted before mutation. Only eligible `EV_OPENLINE` and
`EV_CLOSELINE` are executable in this boundary. Any other eligible opcode or a
complex event keeps the MOVE fail-closed/deferred.

## Recovered movement flags

- +X: EXIT `0x20`, ENTER `0x08`
- -X: EXIT `0x80`, ENTER `0x02`
- -Y: EXIT `0x10`, ENTER `0x04`
- +Y: EXIT `0x40`, ENTER `0x01`

Both phases include `0x400`. ENTER additionally includes the cardinal facing
flag (`N=0x10000000`, `E=0x20000000`, `S=0x40000000`, `W=0x80000000`).

## Real-CYD witness

The player opened line 275 through SELECT, then backed away without crossing it.
The real resident data produced:

```text
[ACTION] DOOR line=275 opcode=15 status=OK open=0->1 locked=0 removed=0->0 effects=07 sound=5063
[MOVEEVENT] EXIT-PREFLIGHT seq=10 tile=838 flags=00000420 status=DOOR_OK event=87 eligible=1 opcode=16 unsupported=0 line=275 open=1->0 locked=0 removed=0->0 mutation=no
[MOVEEVENT] ENTER-PREFLIGHT seq=10 tile=839 flags=80000408 status=NO_EVENT event=65535 eligible=0 opcode=0 unsupported=0 line=65535 open=0->0 locked=0 removed=0->0 mutation=no
[MOVEEVENT] EXIT seq=10 tile=838 flags=00000420 status=DOOR_OK event=87 eligible=1 opcode=16 unsupported=0 line=275 open=1->0 locked=0 removed=0->0 mutation=yes
[MOVEEVENT] ENTER seq=10 tile=839 flags=80000408 status=NO_EVENT event=65535 eligible=0 opcode=0 unsupported=0 line=65535 open=0->0 locked=0 removed=0->0 mutation=no
[MOVEEVENT] COMMIT seq=10 exitDoor=1 enterDoor=0 render=ok rollbackLease=closed
[RESIDENTGAMEPLAY] FRAME reason=MOVE angle=128 frame=808e96c7 sprites=12/1145 walls=15 pixels=9652 totalUs=351514 presented=1 controls=idle-invisible
[RESIDENTGAMEPLAY] MOVE n=5 seq=10 action=BACK tile=838->839 delta=64,0 pos=480,1696 tileEvents=deferred committed=yes
[ALIVE] uptime=245310 ms heap=97552 heap8=31844 largest8=8692 ...
```

The final `tileEvents=deferred` token is a stale historical log label. At this
SHA the bounded movement door family is live; other movement opcode families
remain deferred.

## Hardware conclusions

- Generic SELECT `EV_OPENLINE` opens line 275: `0 -> 1`.
- Generic MOVE source/EXIT event 87 on tile 838 executes `EV_CLOSELINE` and
  closes the same line: `1 -> 0`.
- The tested +X EXIT flags are exactly `0x00000420`.
- Destination tile 839 has no eligible event for this move; its ENTER flags are
  `0x80000408` because the player faces west (angle 128).
- The rendered frame succeeds after the close and the MOVE transaction closes.
- Heap witness after the operation: heap `97552`, heap8 `31844`, largest8 `8692`.
- No per-map door code was introduced. Line/event/tile identity comes from the
  current immutable resident BSP plus compact mutable overlays.

## Still deferred at this PASS

- legacy regular-door visual interpolation;
- secret/MOVELINE animation family;
- door sound playback;
- legacy entity relink objects (native collision already consumes line state);
- broad movement event opcode execution;
- turn advance / monster AI.

The next bounded visual milestone is a generic regular-door animator using the
recovered legacy default `animFrames=4`, `animPos=16`, with renderer-only
transient geometry and no mutation of `EspMapRuntime`.
