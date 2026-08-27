# ESP32 native dialog font hotpath — real CYD hardware PASS

This milestone archives the first hardware-proven optimization of the native dialog/font rendering path on the classic ESP32-2432S028R CYD.

## Hardware-tested boundary

```text
branch = agent/esp32-native-dialog-font-hotpath
base main = 7ff701245b0fda41de3cda7bd2fb65cad15eb218
base main meaning = merged PR #103 native Action dialog + resume
hardware-tested implementation SHA = 777482b038088b232dcbfe64b2421d12aad3de15
status = REAL-CYD HARDWARE PASS
```

The tester reported the dialog as "hyper fluide" after this change. The existing dialog semantics, page fingerprints and state-continuation behavior remained correct.

No local PlatformIO build claim is made here. The supplied Serial transcript and physical-CYD observation are the hardware authority.

## Problem recovered from the previous milestone

The correct dialog implementation progressively repainted visible glyphs. `EspNativeIndexedBmp_blit()` previously read each source BMP row separately.

For Doom's `a.bmp` font:

```text
font source = 144x72 indexed BMP
font glyph = 9x12
filePitch = 72 B
glyph source band = 12 * 72 = 864 B
```

Therefore one glyph blit generated 12 exact PAK range reads even though the complete 12-row band fit inside the already hardware-proven <=1024-B resident small-range cache tier.

Previous real-CYD dialog scale for the 102-B / 7-line Entrance string 88:

```text
paints = 22
fontReads = 3482 .. 3650
font/resource bytes = 250626 .. 262722
PlatformVideo_present ~= 34.3 ms
```

## Permanent optimization

`EspNativeIndexedBmp_blit()` now uses one bounded 1024-B static scratch owner and reads as many consecutive BMP source rows as fit in that scratch.

For `a.bmp`, one complete 12-row glyph band fits in one 864-B range:

```text
before: 12 x readRange(72 B) per glyph
now:     1 x readRange(864 B) per glyph
```

The optimization is generic to indexed-BMP blits. It is not dialog-, Entrance-, glyph- or map-specific.

Permanent memory cost:

```text
BSS scratch = 1024 B
heap allocation = none
PSRAM = none
```

The existing resident PAK cache remains the backing cache. No decoded map-wide font or texel pool is introduced.

## BMP correctness

The grouped read preserves BMP orientation exactly:

- top-down BMP rows remain in source order;
- bottom-up BMP rows are read contiguously in file order and indexed in reverse within the local chunk;
- palette lookup, transparent-magenta handling, framebuffer clipping and pixel writes are unchanged.

The real-CYD run preserved the known dialog page fingerprints:

```text
page 1 complete / FASTFORWARD = 1cf6fa50
page transition blank/start    = 35de63a8
page 2 complete                = 0741a2e6
world after resume             = ed061192
```

## Real-CYD performance proof

A complete naturally typed 102-B / 7-line run produced:

```text
paints = 34
fontReads = 583
bytes = 502050
```

A second run with earlier Action fast-forward produced:

```text
paints = 17
fontReads = 164
bytes = 140034
```

The raw logical `bytes` counter can increase because each grouped range request is larger than an individual 72-B row request. This is not the relevant storage cost signal by itself: the resident exact-range cache can satisfy repeated 864-B requests, while the expensive call/range-lookup count collapses.

Compared with the previous 3482..3650 read count, the hardware runs reduced the font read-call count to hundreds while making the UI subjectively much smoother.

`PlatformVideo_present()` remains approximately 34.3 ms and was intentionally not modified.

## Semantics preserved — hardware PASS

The optimization did not alter gameplay or script behavior. The same run proved:

```text
EV_DIALOG open = YES
25-ms logical typewriter = YES
Action fast-forward = YES
4-line paging = YES
close packClosed=yes = YES
EV_NEXTSTATE state 0->1 = YES
state-gated second dialog = YES
EV_CHANGESTATE state 1->0 = YES
world redraw after resume = YES
```

The tester repeatedly re-entered the dialog sequence successfully.

## Memory evidence

The added 1024-B permanent BSS owner is visible in the expected reduction of free 8-bit heap compared with the previous dialog milestone:

```text
previous dialog heartbeat heap8 ~= 30916
hotpath run heap8 = 29892
delta = 1024 B
largest8 = 16372 stable
```

This is a fixed owner, not a leak.

## Architectural conclusion

The correct optimization target was the bounded asset hotpath, not the screen presenter:

```text
logical 25-ms dialog timeline
 -> bounded indexed-BMP source read
 -> resident PAK range cache
 -> framebuffer mutation
 -> PlatformVideo_present only when dialog visual state changes
```

Do not replace this with a map-wide decoded font/texture pool and do not optimize `PlatformVideo_present()` prematurely.

A possible future optimization, only if later needed, is incremental glyph painting so previously visible glyphs are not redrawn on every typewriter tick. That is not required for this hardware-accepted milestone.

## Exact validated boundary

```text
hardware SHA = 777482b038088b232dcbfe64b2421d12aad3de15
indexed-BMP grouped row reads = YES
1024-B bounded BSS scratch = YES
heap allocation = none
dialog visual fingerprints preserved = YES
dialog semantics preserved = YES
script state 0->1->0 preserved = YES
subjective dialog fluidity = strong PASS
shapeData = NULL
mediaTexels = NULL
```

Every commit after `777482b038088b232dcbfe64b2421d12aad3de15` on this milestone branch must remain documentation-only before merge.
