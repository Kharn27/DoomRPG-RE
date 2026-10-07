# Milestone — DIALOGCHAIN journal session ownership

Status: **CODE CANDIDATE — REAL-CYD TEST PENDING**

Base `main`: `91e1d8412c98fc10ff7b6a2cff6c97323496ecaf` (merged PR #199).
Branch: `agent/esp32-dialogchain-owner-lifecycle`.

## Proven starting point

The fifth Render retirement documented an independent `ChainTransaction`
rollback journal in `esp_native_gameplay_event_chain.c`: 1020 B payload,
1036 B 8-bit heap observed on the classic CYD when a dialogue with a
continuation first allocates it. `ensureTransactionOwner()` reuses it on
later events. Prior to this change, it survived Exit To Menu. It is **not**
evidence of an unbounded per-dialog leak.

The journal also owns a separately allocated `topologyBytes` snapshot which
can grow via `SDL_realloc()`; it, too, must be freed when the source session
has ended.

## Bounded change

- Add `EspNativeGameplayEventChain_reset()` as an explicit session-lifetime
  teardown API. It is idempotent; no allocation occurs during reset.
- Free `topologyBytes` first, then the journal, nulling the sole BSS owner.
- Call it in `EspNativeGameplaySession_reset()` **after**
  `EspNativeResidentGameplay_reset()`: at this point input/dialog/gameplay
  consumers have been retired and no continuation rollback may still run.
- This covers explicit Exit To Menu, checkpoint LOAD, and the native
  CHANGEMAP session handoff through their existing session reset contract.
- Instrument only real releases:
  `[DIALOGCHAIN] OWNER-RELEASE journal=... topologyCapacity=...
  activeAtTeardown=... heap8=...->... recovered=... owner=none`.
- No change to opcode semantics, chain execution, journal capture, rollback,
  map resident ownership, menu painter, renderer, checkpoint format or
  presentation timing.

## Real-CYD acceptance plan (`esp32-cyd`, not bringup)

1. Cold boot, Help/About/Options/Back, then Exit without ever acquiring a
   chain journal. No `OWNER-RELEASE` should appear from the empty owner.
2. Fresh START -> full intro -> MAP_INTRO, canonical first world frame
   `71ca7465`, arena `c3882516`. Trigger a real chained dialogue
   (e.g. event 88), close/advance, observe `[DIALOGCHAIN] OWNER` and
   `[DIALOGCHAIN] RESUME`; then SYS -> double-confirm EXIT.
3. Observe one `[DIALOGCHAIN] OWNER-RELEASE` with
   `journal=1020`, `owner=none` and a measured positive recovery.
   If no topology snapshot was taken and no other allocation changes during
   the narrow release interval, the expected 8-bit heap gain is 1036 B;
   validate the number using Serial rather than assuming it.
4. Confirm `[RESIDENTRESET] empty=1`, correct `MENU_MAIN` FNV
   `522dc605`, no save write, `shapeData==NULL` and
   `mediaTexels==NULL`.
5. Re-enter START (or LOAD) and trigger another chained dialogue. The
   journal must allocate anew and still resume/rollback correctly, without
   pointer reuse from the retired map.
6. Regression paths: active-session LOAD v11, CHANGEMAP handoff, normal
   movement/rotation, pickups and monster actions. Check telemetry and
   memory across two consecutive session exits.

No CI or real-CYD PASS is claimed by this document. After an exact code SHA
passes normal CI **and** hardware, freeze code and make closure docs-only.
User retains ownership of the main merge.
