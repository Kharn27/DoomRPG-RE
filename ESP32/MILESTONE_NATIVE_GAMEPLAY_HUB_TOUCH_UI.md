# Milestone — native gameplay HUB touch UI on real CYD

This record locks the first permanent visible touch-control layer for the native
in-game HUB on the classic ESP32-2432S028R.

Repository state wins over chat history. Serial logs from the real classic CYD
are the final runtime authority.

## Git / build boundary

```text
main at branch creation = bc9e0435f6ee6ea23a68cb4bdbb18324440cf360
branch = agent/esp32-native-gameplay-hub-touch-ui
hardware-tested code boundary = 515bb4b0122c55348251eee726110ec946a4aeb6
status = REAL-CYD GAMEPLAY HUB TOUCH UI + HAND MENU BUTTON PASS
branch policy = LOCKED; documentation-only tail after this SHA
```

GitHub Actions normal `esp32-cyd` run `34091004475` / run #177 completed with
`success` on the exact tested SHA. CI is compile/link evidence only; the hardware
session below is the behavioral authority.

## Permanent architecture now hardware-owned

The world and HUB intentionally use different touch vocabularies:

```text
WORLD -> existing invisible gameplay touch zones
HUB   -> explicit visible native touch controls
```

The HUB remains a compact projection over the shared player owner:

```text
EspNativeGameplayPlayerState = 52 B
EspNativeGameplayHubView      = 28 B
```

No second inventory/player owner is introduced. The HUB remains read-only:
weapon selection, item use, turn consumption and other menu semantics are still
separate milestones.

Visible HUB controls now include:

```text
INV tab      -> Inventory page
STATUS tab   -> Status page
Inventory cards -> direct row selection
MENU zone    -> visible Doom menu hand from p.bmp while HUB is active
```

Touches inside the HUB viewport no longer fall through to the world 3x3 control
grid. Unsupported HUB actions remain absorbed/fail-closed.

## Bounded MENU overlay ownership

The visible MENU affordance intentionally occupies the existing logical top-left
MENU zone:

```text
logical region = x=0..31, y=0..19
pixels         = 640 RGB565
underlay bytes = 1280
asset          = p.bmp
frame          = 0
```

This is not a full framebuffer snapshot. The HUB saves only the 32x20 region it
will overwrite, fingerprints every other HUD pixel as `hudProtected`, draws the
menu hand, then restores the 32x20 underlay bit-exact before returning to the
world.

The rest of the HUD remains protected throughout HUB paints.

## Real-CYD touch/UI witness

The exact tested code boundary opened the HUB from the live Entrance gameplay
frame:

```text
[HUB] FRAME paint=3 page=inventory row=0 frame=79bbc30d
      viewport=160x80/y20..99
      hudProtected=bd7588ae preserved=yes
      menuButton=hand asset=p.bmp frame=0
      menuZone=109b46aa underlayBytes=1280
      playerFNV=e745fce9 exact=yes
      packClosed=yes presented=1 mutation=no turn=no
```

The open record retained the compact permanent owners:

```text
ownerBytes=28
playerStateBytes=52
playerFNV=e745fce9
```

Switching to Status produced:

```text
[HUB] FRAME paint=4 page=status row=0 frame=eea0759d
      hudProtected=bd7588ae preserved=yes
      menuZone=109b46aa
      playerFNV=e745fce9 exact=yes
      packClosed=yes mutation=no turn=no
```

Returning to Inventory reproduced the exact frame fingerprint:

```text
Inventory = 79bbc30d
Status    = eea0759d
Inventory = 79bbc30d
```

The MENU hand region also stayed deterministic at `menuZone=109b46aa` across
page changes.

## Input isolation witness

A PASS_TURN touch while the HUB was active was deliberately absorbed:

```text
[HUB] IGNORE action=14 page=inventory row=0
      worldDispatch=blocked mutation=no turn=no
[RESIDENTGAMEPLAY] HUB-INPUT ... action=PASS_TURN status=IGNORED
      worldDispatch=blocked turnAdvance=no
```

This confirms that visible HUB ownership does not leak gameplay actions into the
world or MonsterTurn scheduling.

The existing INV/STATUS touch routing also remained live:

```text
Inventory -> Status via TURN_RIGHT / REDRAWN
Status -> Inventory via TURN_LEFT / REDRAWN
```

## Exact close / HUD restoration witness

Tapping the visible hand generated the existing semantic MENU action and closed
the HUB. The bounded underlay and complete HUD were restored exactly:

```text
[HUB] CLOSE n=3 page=inventory
      playerFNV=e745fce9->e745fce9 exact=yes
      mutation=no turn=no
      menuUnderlayRestore=exact
      hudBands=6c2aa46f expected=6c2aa46f exact=yes
      packClosed=yes
```

The normal gameplay compositor then rebuilt the exact pre-HUB world frame:

```text
pre-HUB world frame = 22397b55
post-close world frame = 22397b55
worldRedraw=yes
playerMutation=no
turnAdvance=no
```

The touch-feedback overlay itself also restored exactly before each semantic HUB
action.

## RAM / no-PSRAM witness

The real CYD stayed flat before, during and after the tested HUB session:

```text
heap     = 87988
heap8    = 22256
largest8 = 14324
hub owner = 28 B
player owner = 52 B
MENU underlay = 1280 B
full framebuffer snapshot = 0 B
```

Compared with the immediately preceding HUB-v2 hardware witness, the free heap
is lower by 1292 B, consistent with introducing the bounded 1280 B MENU underlay
plus small static bookkeeping. The largest 8-bit block is also lower at 14324 B;
this exact value is now the canonical hardware witness for this milestone and
should be re-read before future RAM-heavy work.

No PSRAM is used. `PlatformVideo_present()` remains around 34.4 ms and is not the
current optimization target.

## Deliberately deferred HUB families

```text
real weapon / ammo / item names and richer content layout
weapon selection
consumable confirmation / use
item-use turn consumption + MonsterTurn integration
Notebook
Automap
Save / Load
Options
store / vending UI
sector/overall stats not already in PlayerState
main-menu transition
sound effects
```

The visible touch layer is now permanent infrastructure; future pages should add
bounded semantic families onto it rather than routing through legacy `MenuSystem`.

## Locked result

`515bb4b0122c55348251eee726110ec946a4aeb6` is the exact code boundary exercised
on the real classic CYD. Commits after it on this branch must be documentation-only.
Before merge, compare the tested SHA to branch head and verify that only Markdown
recovery/milestone files changed.

After the user announces the merge, read the true GitHub `main` SHA again before
creating the next `agent/*` branch.
