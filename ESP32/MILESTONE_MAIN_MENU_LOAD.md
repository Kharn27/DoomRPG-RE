# Main-menu native Load Game milestone

Status: **REAL-CYD PASS — MERGE-READY**.

```text
base main                         = 5841b0cb55607428bf74c341112a161213c47c90
direct-load implementation       = 175090c
hardware-tested code boundary    = 18c1cfbdb11236588d7a8160ce643ebe7e00b61d
branch                           = fix/mainMenu
```

## User-visible behavior

The standalone CYD no longer exposes the J2ME `Exit` action. Its main menu is:

```text
Start Game
Load Game
Options
Help/About
```

The existing double-tap confirmation gate remains authoritative. Confirming
`Load Game` uses the same versioned one-slot native checkpoint reader as
`HUB -> STAT -> LOAD`.

Successful route:

```text
MENU_MAIN / item 1
 -> validate readable V1..V6 checkpoint
 -> release menu-only runtime
 -> rebuild immutable BSP state
 -> restore versioned mutable checkpoint owners
 -> configure resume session
 -> ST_PLAYING
```

This route deliberately skips the new-game intro and fresh-map first-frame
semantics. The resident gameplay service is enabled from the configured,
settled session state rather than from intro completion alone.

If the save is absent, truncated, corrupt or otherwise invalid, the load is
fail-closed: the main menu remains active and the selected row displays a red
`No Save` response. The repaint rebases the runtime framebuffer witness so the
menu remains interactive after the feedback is drawn.

The user validated both paths on the real classic CYD at `18c1cfb`:

- an existing checkpoint resumes gameplay correctly;
- no checkpoint leaves the menu usable and visibly displays `No Save`.

## Bounded mappings startup

The first hardware attempt also exposed a startup allocation peak while reading
`mappings.bin`: the largest free heap block was 16,372 bytes, while the legacy
path temporarily needed an approximately 10,992-byte inflater state together
with the 8,392-byte decoded payload and compressed input.

The ESP32 loader now:

```text
mappings.bin ZIP entry
 -> readZipFileEntryInto()
 -> permanent 160x120 framebuffer used as transient scratch
 -> four persistent mapping arrays installed
 -> immutable arrays retained and reused across BSP loads
```

This removes the separate inflated heap allocation and the old release/rebuild
cycle without changing the mapping format or ownership visible to retained
Render code.

## Production build

The normal production environment builds successfully at the tested code
boundary:

```text
environment = esp32-cyd
static RAM   = 44,944 B / 327,680 B (13.7%)
flash        = 718,721 B / 1,310,720 B (54.8%)
result       = SUCCESS
```

Historical `START_GAME.md` remains an archive of the earlier fresh-start
milestone and is intentionally not rewritten by this change.


## 2026-09-24 cold MENU_MAIN V8 validation regression — REAL-CYD PASS

A later V8 checkpoint exposed a startup-only false negative:

```text
cold MENU_MAIN -> Load Game
[NATIVESAVE] READABLE-V8 ... result=invalid
[MAINLOAD] NO-SAVE missing-or-invalid

start gameplay -> HUB/SYS -> Load Game
[NATIVESAVE] LOAD ... version=8 ... restored
```

The file was not corrupt. The cold reader was performing full crate-transform
validation before the native gameplay EntityDef catalog had been materialized.
Crate checkpoint codes are canonical packed values 1..9, but
`EspNativeGameplayCrateState_snapshotShapeValid()` resolved each code through
the live EntityDef catalog. After gameplay initialization that dependency existed;
at cold MENU_MAIN it did not.

The permanent split is:

```text
cold file/readability validation:
  exact V8 size + CRC
  resource/script/line/removal shapes
  crate bitset/count/code(1..9)/padding/FNV
  automap + monster snapshot shapes
  no live EntityDef dependency

real restore:
  rebuild map/runtime + EntityDef catalog
  full crate code -> live target tile validation
  restore or fail closed
```

The V8 monster workspace is also reserved before reopening the SD file, avoiding
an unnecessary cold-menu allocation peak while preserving the same on-disk
record and validator semantics.

Real-CYD witness at code head
`133f67882336f9f70f6294369e1571cde5a07699`:

```text
[NATIVESAVE] READABLE-V8 path=/DoomRPG-ESP32.sav bytes=3548
             monsterWorkspace=1612 allocation=before-reopen
             crateValidation=file-shape/catalog-deferred result=valid
[MAINMENU] Runtime cleanup ... heap8=9704->65112
[ENTITYDEFTYPE] READY defs=115 ...
[CRATECHECKPOINT] RESTORE ... transformed=8 ... stateFNV=a8766473
[MONSTERSTATE] STAGE-RESTORE ... monsters=30 ...
[NATIVESAVE] LOAD ... version=8 bytes=3548 ... session=reprime-pending
[MAINLOAD] READY checkpoint restored; intro=skipped session=resume-pending state=3
```

The resumed resident session then armed normally with
`shapeData == NULL` and `mediaTexels == NULL`.

Build reference:

```text
esp32-cyd CI #714 = SUCCESS
static RAM = 45744 B
flash = 766865 B
```
