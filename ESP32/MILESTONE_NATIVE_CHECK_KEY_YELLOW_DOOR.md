# Milestone — Native EV_CHECK_KEY Yellow Door real-CYD pass

Date: 2026-09-24

Branch: `agent/esp32-touch-feedback-facing-race`

Hardware-tested code head: `ffcd0cfa8107af5b4297b5e5a8f9e4a76a72126c`

CI: ESP32 CYD Build #667 SUCCESS; static RAM 45016 B; flash 760585 B.

## Recovered contract

Legacy opcode 41 is `EV_CHECK_KEY`. `arg1` selects Green/Yellow/Blue/Red as
0/1/2/3. Missing key shows `Need <Color> Key`, requests sound 5065, saves/pauses
the event continuation and does not execute later commands. With the key, the
command returns non-blocking and the event walk continues.

## Native boundary

The SELECT filter now receives `EspNativeGameplayPlayerState.keys`. The only
new executable shape is `EV_CHECK_KEY -> 1..8 door commands`. Missing-key
handling is a non-mutating native status/feedback path. Owned-key handling reuses
the existing fully previewed atomic door batch. Any other suffix remains
fail-closed. No new mutable owner was introduced.

## Real-CYD witness

```text
[ACTION] SELECT seq=62 status=KEY_REQUIRED tile=200 event=11 eligible=1 unsupported=0
[CHECKKEY] BLOCK seq=62 event=11 cmd=0 keyId=1 mask=02 message="Need Yellow Key" sound=5065-deferred continuation=paused worldMutation=no removedMutation=no turnAdvance=deferred
[ACTIONFEEDBACK] PAINT kind=12 text="Need Yellow Key" chars=15 ... durationMs=1200
```

The Yellow Door remained closed and no world/script/remove mutation was emitted.
This is the real-hardware PASS for the missing-key route. Sound playback and
turn advancement remain explicitly deferred by their existing native owners.

## Next blocker

Entrance tile 377 / event 43 remains fail-closed at move preflight with two
eligible commands classified COMPLEX. It is the next bounded migration target.
