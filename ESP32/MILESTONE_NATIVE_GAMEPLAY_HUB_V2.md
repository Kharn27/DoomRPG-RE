# Milestone — native gameplay HUB v2 on real CYD

This record locks the first permanent in-game HUB shell on the classic
ESP32-2432S028R and its read-only Inventory + Status projections.

Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git / build boundary

```text
main at branch creation = dbcbf7b52f9aef851503a919a5a2871743a7de20
branch = agent/esp32-native-gameplay-hub-inventory-view
hardware-tested code boundary = bc136c735c9f5ba173a57fcbbb1f5bea6732b3bd
status = REAL-CYD GAMEPLAY HUB V2 INVENTORY + STATUS READ-ONLY PASS
branch policy = LOCKED; documentation-only tail after this SHA
```

GitHub Actions normal `esp32-cyd` run `34060186783` / run #161 completed with
`success` on the exact tested SHA. CI is compile/link evidence only; the hardware
session below is the behavioral authority.

## Permanent architecture now hardware-owned

The native HUB is not a port of the pointer-heavy legacy `MenuSystem`. It is a
small modal projection over permanent native owners:

```text
EspNativeGameplayPlayerState (52 B, sole gameplay owner)
 -> EspNativeGameplayHubView (28 B modal UI owner)
 -> shared 160x120 RGB565 framebuffer
```

The HUB does not duplicate inventory/player state, does not retain a framebuffer
snapshot and does not mutate legacy `Player_t`, `MenuSystem_t`, world entities or
render ownership.

The first two pages are read-only:

```text
page 0 = Inventory
page 1 = Status
```

Input contract:

```text
MENU                 -> close HUB and rerender world
TURN_LEFT/RIGHT      -> switch Inventory <-> Status
FORWARD/BACK         -> move Inventory cursor only
FORWARD/BACK Status  -> absorbed / world dispatch blocked
SELECT               -> fail closed, no item use / no weapon selection yet
all HUB input         -> no MonsterTurn / no world dispatch
```

Item use, weapon selection, store/save/load and other menu families remain
separate milestones.

## Viewport-only composition fix

The first HUB prototype painted all 120 logical rows. The gameplay compositor
intentionally preserves the native HUD bands (`y=0..19` and `y=100..119`), so a
full-screen HUB left menu pixels in those bands after returning to the world.

The permanent fix does not add a framebuffer or HUD snapshot. The HUB is instead
strictly clipped to the world viewport:

```text
logical framebuffer = 160x120 RGB565
HUB writable area   = 160x80, y=20..99
HUD top band         = y=0..19  untouched
HUD bottom band      = y=100..119 untouched
snapshot bytes       = 0
```

Each HUB paint fingerprints both HUD bands before and after. Any changed HUD pixel
fails the paint closed.

## Real-CYD Inventory + Status witness

The supplied hardware session on `bc136c73...` opened the HUB from the live
Entrance gameplay frame and preserved the same player fingerprint through all
page/cursor operations:

```text
playerStateBytes = 52
hub ownerBytes = 28
playerFNV = e745fce9
hudBandsFNV = 6c2aa46f
```

Inventory page:

```text
[HUB] FRAME paint=6 page=inventory row=0 frame=06138e61
      viewport=160x80/y20..99 hudBands=6c2aa46f preserved=yes
      reads=72 bytes=60546 playerFNV=e745fce9 exact=yes
      packClosed=yes presented=1 mutation=no turn=no
```

Status page after `TURN_RIGHT`:

```text
[HUB] FRAME paint=7 page=status row=0 frame=b35c12b9
      viewport=160x80/y20..99 hudBands=6c2aa46f preserved=yes
      reads=66 bytes=55362 playerFNV=e745fce9 exact=yes
      packClosed=yes presented=1 mutation=no turn=no
[HUB] PAGE page=inventory->status direction=right
      playerMutation=no turn=no worldDispatch=blocked
```

Repeated right/left navigation deterministically returned the same Inventory and
Status frame fingerprints. `FORWARD` on Status was explicitly absorbed:

```text
[HUB] IGNORE action=1 page=status row=0
      worldDispatch=blocked mutation=no turn=no
```

Returning to Inventory preserved cursor behavior:

```text
row 0 -> 2 frame=71bffc61
row 2 -> 1 frame=1a817e61
```

No player/world mutation or turn advance occurred.

## Exact return-to-world witness

Closing the HUB kept the player fingerprint exact and left both HUD bands
untouched:

```text
[HUB] CLOSE n=2 page=inventory
      playerFNV=e745fce9->e745fce9 exact=yes
      mutation=no turn=no worldRedraw=pending
      viewportOnly=yes hudBands=untouched packClosed=yes
```

The normal native gameplay compositor then rebuilt the world viewport and
returned to the pre-HUB frame fingerprint:

```text
pre-HUB touch baseline frame = 22397b55
[RESIDENTGAMEPLAY] FRAME reason=HUB-CLOSE ... frame=22397b55 ... presented=1
[RESIDENTGAMEPLAY] HUB-CLOSE ... worldRedraw=yes playerMutation=no turnAdvance=no
```

This closes the earlier top/bottom-band residue bug without permanent framebuffer
backup RAM.

## RAM / no-PSRAM witness

The supplied hardware session remained flat before, during and after repeated HUB
navigation:

```text
heap = 89280
heap8 = 23548
largest8 = 20468
hub owner = 28 B
player owner = 52 B
HUB framebuffer snapshot = 0 B
shapeData = NULL
mediaTexels = NULL
```

No PSRAM is used. `PlatformVideo_present()` remained around 34.4 ms and is not the
current optimization target.

## Read-only data projection

Inventory reads the canonical native player fields directly: current weapon,
weapon bitmask, six ammo counters, five inventory counters, key bitmask, credits,
level and XP.

Status mirrors the useful player portion of legacy `Menu_fillStatus()` without
creating legacy menu items: health/max health, armor/max armor, level/current XP
and next-level XP, defense, strength, agility, accuracy, credits and keys. These
values come from the existing compact `param1` / `param2` and scalar fields of
`EspNativeGameplayPlayerState`.

## Deliberately deferred HUB families

```text
entity/item names and richer Doom RPG visual chrome
weapon selection from Inventory
consumable item selection / confirmation / use
item-use turn consumption and MonsterTurn integration
Notebook content
Automap page integration
Save / Load
Options
store / vending UI
sector/overall stats not already present in PlayerState
main-menu transition
sound effects
```

Different semantic families remain fail closed rather than being enabled through
legacy `MenuSystem` as a shortcut.

## Locked result

`bc136c735c9f5ba173a57fcbbb1f5bea6732b3bd` is the exact code boundary exercised
on the real classic CYD. Commits after it on this branch must be documentation-only.
Before merge, compare the tested SHA to branch head and verify that only Markdown
recovery/milestone files changed.

After the user announces the merge, read the true GitHub `main` SHA again and
create the next `agent/*` from that exact commit. A strong next bounded milestone
is a visual/readability pass: recover the legacy in-game menu grid/chrome and
entity names while preserving the 28 B HUB owner, viewport-only clipping and
read-only semantics. Do not add item-use mutations in that visual milestone.
