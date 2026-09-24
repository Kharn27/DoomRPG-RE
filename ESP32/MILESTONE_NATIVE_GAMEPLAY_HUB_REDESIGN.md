# Native gameplay HUB industrial redesign and feedback coexistence

This milestone records the compact in-game HUB redesign for the classic
ESP32-2432S028R and the two touch-feedback failures exposed by its real-CYD
smoke test. It supersedes the current-layout description in the older HUB
milestones without rewriting their historical hardware boundaries.

## Git and validation boundary

```text
base main = 9e009c0acbb5185afe6d9cc7eeb9dfa02eeea948
branch = fix/inventaire
UI redesign commit = 5139bff
feedback coexistence commit = 5c2e2c6805a5da4b0ee1da40c09dd3155cfe41df
local esp32-cyd build = SUCCESS
static RAM = 47544 B
flash = 749913 B
hardware status = focused real-CYD smoke pass
broader gameplay progression = not yet exercised
```

The user confirmed that the final firmware appears to work through the first
door after reproducing both failures below. This is deliberately recorded as a
focused smoke pass, not as an exhaustive gameplay or checkpoint regression
pass. No CI result is claimed for this branch here.

## Current HUB presentation

```text
INV | WPN | STAT | SYS
```

All four tabs are directly touch-addressable, including destinations two pages
away in the cyclic physical-control order. World dispatch and monster-turn
advancement remain blocked while the HUB is active.

The presentation uses one shared dark industrial RGB565 palette:

```text
background = near-black blue steel
panels     = dark steel
text       = warm ivory
focus      = amber
errors     = muted red
success    = muted green
```

The complete top 20-row title band is temporarily repainted by the HUB. On
close, the permanent gameplay HUD is reconstructed through
`EspNativeGameplayHud_repaint()`; the world is then rendered normally. The
bottom gameplay HUD band remains protected during every HUB page paint.

## Page contracts

### INV

Inventory remains a projection of canonical PlayerState/non-weapon content.
The visible previous/current/next window is presented as three cards with an
amber current-row marker. No item-use behavior was added.

### WPN

The nine conventional weapon IDs 0..8 now fill a complete 3x3 grid. Familiar
IDs 9..11 remain excluded from the normal arsenal rather than occupying three
blank cells.

The original weapon assets store palette words in source BGR565 order. The old
grid copied those words directly into the RGB565 framebuffer, making the Fire
Extinguisher and Axe visibly wrong. The grid now uses the same red/blue swap as
the hardware-proven native weapon renderer:

```text
source BGR565 -> framebuffer RGB565
```

Owned icons retain original colors, unavailable weapons are grayscale, the
equipped border is amber and the focused border is ivory.

### STAT

STAT remains read-only. The old SAVE/LOAD rectangle no longer covers player
statistics.

### SYS

SYS owns one full checkpoint panel with large SAVE and LOAD cards:

```text
first SELECT  -> SAVE? / LOAD?
second SELECT -> execute the operation
missing save  -> NO SAVE; LOAD remains fail-closed
save result   -> SAVED / FAILED
```

The existing V7 checkpoint service and on-disk compatibility remain unchanged;
this milestone changes only presentation and confirmation routing.

## Failure 1: large checkpoint target exhausted touch feedback

The SAVE/LOAD cards are 128x21 logical pixels. Their reversible double outline
plus action glyph needs about 597 pixel edits, exceeding the former 512-entry
touch-feedback owner. The semantic input had already been queued, but the
cosmetic failure disabled resident gameplay:

```text
[NATIVESAVE] TOUCH-HIT row=LOAD ...
[RESIDENTGAMEPLAY] FAILED reason=touch-feedback-draw
```

The historical smoke-fix first raised the bounded owner to 768 entries. More
importantly, inability to create a cosmetic touch overlay no longer kills
gameplay: the queued semantic input is preserved and a nonfatal
`TOUCHFEEDBACK SKIP` witness is emitted.

After this milestone was rebased together with later gameplay work, the 768-entry
6-byte journal proved too expensive for the classic CYD boot heap. The final
integration keeps the same large-target semantics with **640 compact 4-byte
entries**. The largest measured SAVE/LOAD target needs about 597 edits. Each
compact record stores the saved RGB565 value and packs the framebuffer offset
plus halo/core selector into 16 bits; the touch-painted value is reconstructed
exactly during reverse restore. This reduces static RAM by 2048 B compared with
the rebased 768x6 implementation while retaining `newerOverlayWins` semantics.

## Failure 2: pickup overlays invalidated full-frame restoration

The first-door test collected two Armor Shards immediately before SELECT. The
pickup message and white viewport flash legitimately changed the framebuffer
during the 120 ms touch animation. The old restoration demanded that the
entire framebuffer return to its earlier FNV and disabled gameplay before the
door action could execute:

```text
[RESIDENTGAMEPLAY] QUEUE ... action=SELECT ... context=WORLD
[TOUCHFEEDBACK] FLASH ...
[RESIDENTGAMEPLAY] FAILED reason=touch-feedback-restore
```

Each reversible edit now stores both its saved and painted RGB565 value. During
reverse restoration:

```text
pixel still equals touch-painted value -> restore saved value
pixel changed by a newer overlay       -> preserve newer value
changes elsewhere in framebuffer      -> accepted
```

The diagnostic reports edit conflicts and whether the full-frame baseline
still matches, but neither legitimate drift nor a newer overlay disables the
session. Framebuffer absence and physical presentation failure remain fatal.

## Focused hardware evidence

The redesign run exercised:

```text
world -> open HUB
INV -> WPN -> STAT -> SYS
SYS -> close HUB -> exact gameplay HUD/world redraw
reopen HUB -> direct INV -> SYS touch
large LOAD touch failure reproduction
world movement and Armor Shard pickup
first-door SELECT restoration failure reproduction
post-fix first-door smoke retest accepted by the user
```

Representative successful ownership witness before the discovered feedback
faults:

```text
[HUB] CLOSE ... menuUnderlayRestore=exact hudRepaint=yes
      hudBands=... expectedHud=... exactHud=yes packClosed=yes
[RESIDENTGAMEPLAY] FRAME reason=HUB-CLOSE ... presented=1
```

## Remaining verification

Before promoting this branch beyond its focused smoke boundary, exercise on the
real CYD:

```text
SAVE? -> SAVED
LOAD? -> checkpoint resume
NO SAVE response
rapid tab changes and repeated HUB open/close
pickup feedback followed immediately by MOVE/TURN/SELECT
continued progression beyond the first door
```

Notebook activation, consumable use, Options/store integration and familiar
weapon presentation remain intentionally outside this milestone.

## Post-rebase boot integration

When this HUB redesign was combined with the later native gameplay branch, the
first rebased image rebooted in `Render_beginLoadMapData()` before the menu
could start. ELF symbolization showed `SDL_calloc(mapSprites)` had returned
NULL; the next write at `src/Render.c:566` caused `StoreProhibited` at address
zero.

The feedback owner was the exact BSS regression:

```text
pre-rebase event43 image feedback = 2068 B
rebased 768x6 feedback           = 4628 B
delta                            = +2560 B
```

Final integration code head:

```text
6cd637c370415450dda5e89abcc13e7982368adf
esp32-cyd CI #683 = SUCCESS
static RAM = 45736 B
flash = 764349 B
```

The 640x4 compact journal recovers 2048 B versus the failing rebased image. An
ESP32-only allocation guard also makes future `mapSprites` OOM fail closed with
an exact diagnostic instead of dereferencing NULL. The user reports that the
reboot loop is gone and the firmware appears to work on the real CYD.

Checkpoint note: a save created before this rebase is currently rejected both
from main-menu Load and from SYS Load after starting gameplay. That compatibility
issue is explicitly left open; this milestone does not claim it resolved.
