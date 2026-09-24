# Finger-first main menu redesign milestone

Status: **REAL-CYD VISUAL/TOUCH PASS — DOCS COMPLETE; one cold-load sanity remains before merge-ready**.

```text
base main                     = 38dfbbf6310045b1b01604048c61f634e246e0cf
branch                        = agent/esp32-main-menu-finger-first
hardware-tested code boundary = cd557d7b727d600cecbff610d6e3e6a21d609853
esp32-cyd CI                  = #729 SUCCESS
static RAM                    = 44640 B / 327680 B
flash                         = 767381 B / 1310720 B
```

## Goal

Replace the thin J2ME/mobile-era main-menu rows with a permanent finger-first
classic-CYD presentation without changing the four existing menu actions or
introducing new memory-heavy UI ownership.

The retained model is still:

```text
0 Start Game
1 Load Game
2 Options
3 Help/About
```

Only MENU_MAIN presentation and its hit geometry change.

## Final real-CYD layout

The accepted final composition is a 2x2 dashboard:

```text
          DOOM RPG

      START     LOAD

      OPTIONS   HELP
```

Exact logical geometry:

```text
title             = 90x62 at y=2
left cards        = x 8..75
right cards       = x 84..151
top cards         = y 67..89
bottom cards      = y 93..115
each card         = 68x23 logical
physical target   = 136x46 at exact 2x
```

The first candidate used 74x28 cards and shrank the title to 74x51. Hardware
review found the cards visually too dominant. The follow-up restored the proven
90x62 title and reduced the cards to 68x23 while keeping them comfortably
finger-sized.

A second hardware review accepted the industrial card treatment but found the
full-width grey/blue dashboard backing visually too heavy. The final code paints
the bounded dashboard band true black, leaves the four cards independent, and
uses black card-corner chamfers. The selected card remains amber; the first tap
arms the card with an ivory + amber double border.

The user explicitly accepted this final rendering on the real classic CYD.

## Touch/action contract

The action contract is intentionally unchanged:

```text
tap a different card
 -> update selectedIndex
 -> repaint bounded dashboard
 -> present armed visual

tap the already selected card
 -> existing gate treats it as confirmation candidate

released second tap on same card
 -> existing Start/Load/Options/Help action
```

The complete card rectangle is the hit target. There is no requirement to hit a
text baseline or legacy hand-cursor row.

Options remains its existing sub-menu. Returning from Options repaints the same
finger-first MENU_MAIN painter.

## Memory ownership

The old MENU_MAIN selection feedback retained four 13x10 RGB565 cursor-underlay
patches. The dashboard does not need them: every selection change repaints the
small bounded dashboard region and all four cards directly.

```text
old main-menu cursor patch storage = four RGB565 patches + bookkeeping
new permanent cursor patch storage = 0 B
main static RAM                    = 45744 B
final redesign static RAM          = 44640 B
delta                              = -1104 B
```

No allocation is performed for selection repaint.

The permanent invariants remain:

```text
shapeData == NULL
mediaTexels == NULL
no PSRAM
no map ZIP runtime dependency
no extra framebuffer
no map-wide UI texel owner
```

## Build proof

Normal production environment:

```text
environment = esp32-cyd
CI run      = #729
code head   = cd557d7b727d600cecbff610d6e3e6a21d609853
RAM         = 44640 B / 327680 B (13.6%)
flash       = 767381 B / 1310720 B (58.5%)
result      = SUCCESS
```

## Hardware boundary

Hardware-proven on the real classic CYD at the final code boundary:

```text
finger-first 2x2 composition = accepted
90x62 title hierarchy        = accepted
smaller 68x23 cards          = accepted
black inter-card background  = accepted
industrial styling           = accepted
touch-oriented target sizing = accepted
```

The inherited V8 direct main-menu Load Game route was already hardware-proven on
the previous main line, including cold catalog-independent validation and real
restore. This milestone does not modify that loader implementation.

However, the final visual/touch code head `cd557d7...` has not yet received an
explicitly reported fresh cold `MENU_MAIN -> Load Game` replay. Because
"no cold Load regression" is part of this milestone's acceptance contract, keep
that one sanity test as the final merge gate rather than silently promoting
historical evidence to a new hardware PASS.

## Merge discipline

After the cold-load sanity passes on `cd557d7...`:

1. record the real serial witness;
2. update this milestone and the two recovery docs with the exact PASS;
3. ensure every commit after `cd557d7...` is documentation-only;
4. declare the branch merge-ready;
5. do not merge into `main` automatically.

After the user merges, re-read the real GitHub `main` SHA before starting the
next gameplay branch. The preferred continuation is the existing native
CHANGEMAP / Entrance level-exit path.
