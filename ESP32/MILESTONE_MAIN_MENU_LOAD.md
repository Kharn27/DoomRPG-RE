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
