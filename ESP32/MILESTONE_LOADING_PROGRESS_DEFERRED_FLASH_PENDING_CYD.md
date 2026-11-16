# Loading bar: deferred flash staging during fresh Start and checkpoint LOAD

Status: PENDING CYD. Base main: c3be0e7b9bb1a5b521a3962bc0210065add8214f. Branch: agent/esp32-loading-deferred-flash-progress.

## Diagnosis
CHANGEMAP already registers a synchronous mapFlash progress callback and
shows real ERASE/COPY/VERIFY work. Fresh Start and checkpoint LOAD show
fixed UI checkpoints up to 85%, then __wrap_EspAssetPack_residentBegin
performs the expensive flash prepare (including a cache MISS rebuild).

## Bounded fix
Do not move the verified staging/ownership boundary. Only fresh Start
and checkpoint LOAD mark the loading UI as flash-deferred. The initial
checkpoints use 0..20%, real erase/copy/verify in residentBegin use 20..85%,
and priming/READY use 90..100%. The callback is cleared on either result.
REUSE HIT has no chunk callbacks and advances straight to FLASH-READY.
Normal CHANGEMAP and death recovery keep their previous percent mapping.
No changes to PAK, SD-to-flash data, saves, or firmware panel profiles.

## Test plan (normal esp32-cyd, physical board)
- Restart New Game after Junction (flash cache holds another map):
  inspect MAPFLASH REUSE MISS and ERASE/COPY/VERIFY with TRANSITIONLOAD FRAME.
- LOAD a save from a different active map: same continuity, exact restore.
- LOAD a save from the same cached map: verify REUSE HIT and quick FLASH-READY.
- CHANGEMAP and death recovery: verify no regression.
- Collect full Serial, observe WDT/heap8, verify both firmware variants build.

No hardware PASS or CI result is claimed before evidence.
