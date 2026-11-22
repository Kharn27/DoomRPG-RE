# RES60-P12 — éviter le BSP inventory complet à chaque rotation (bounded CYD PASS)

**Branch:** `agent/esp32-res60-p1`, [draft PR #364](https://github.com/Kharn27/DoomRPG-RE/pull/364). Builds only `esp32-cyd-res60` and `esp32-cyd-res60-inverted`. **Code candidate after P11 hardware proof, 22 Nov 2026.** Code base remains `4ece448f523d32db6ddcf85d1ca4492513339dbb`; upstream `main` has advanced independently and is not claimed tested.

## Hardware-motivated bottleneck (P11, real CYD)

The P11 [bounded hardware PASS](MILESTONE_RES60_P11_CPU_PROFILE_PENDING_CYD.md) measured **`invUs` 134905–135450 us on each cache-HIT turn**, plus `loadUs` ~49707–49748 us. Those ~185 ms are repeated at all cardinal angles, whereas the 4bpp P8 texture cache already avoids all warm catalog/PAK texel reads. Cold P8 `catalogUs=1314650us` remains untouched. P12 targets **only inventory**, not the compact map load, raycasting, 3D fidelity or LCD. Any measured gain must come from new CYD Serial, not estimates.

## Bounded design and ownership

- Compile-time `DOOMRPG_RES60_P12_INVENTORY_CACHE=1` requires P11/P10 isolated RES60 mode. Production 160×120, inverted production, bringup, trace have it OFF and export no P12 symbols.
- `EspRes60InventoryCache` owns **one scalar `EspBspInventory` snapshot plus one 32-bit arena FNV and a validity flag** for the exact `/intro.bsp` preview. The snapshot has section offsets, resource ID bitsets and source CRC; no pointers, no framebuffer, no large geometry arrays, no additional PAK reads, and no legacy map/ZIP dependency.
- First START after cache reset calls the existing bounded `EspBspReader_inventoryPackEntry` and loads `EspMapRuntime` exactly as P11. After full successful render, P8/P10 overlay, compact arena reset, and PAK close, P12 publishes its snapshot. Every later turn reuses only those inventory scalars. **`EspMapRuntime_loadPackEntry` still reopens the native PAK every turn**, compares real entry size and declared CRC to cached source, reconstructs the compact arena and verifies its FNV against the previous successful turn *before drawing any walls*. CRC/source/geometry/arena mismatch is fail-closed and invalidates the snapshot.
- `main.cpp` resets P12 before each START and after a successful MENU navigation (same owner boundaries as P8's texture reset). An unsuccessful render invalidates the cache; a failed rotation never commits the requested orientation. PAK must be closed and transient runtime released before LCD present. P11 `invUs` on a HIT now measures only snapshot lookup/copy, NOT the original 21.8KiB BSP inventory scan.
- New `[RES60P12] INVENTORY cache=MISS|HIT ... invUs=...` witness follows framebuffer preparation, alongside the existing P3/P8/P9/P10/P11 witnesses.

## Required host and hardware gates

`ESP32/test/res60_p12_inventory_cache_host.py` compiles the actual new C owner, verifies twelve warm hits without a second inventory reader call, valid cold/commit/MENU reset, arena FNV mutation rejection, source CRC/size/line limits and rejection of other map names and invalid configuration; also checks the source guard in the actual `EspMapRuntime_loadPackEntry` and that production emits no P12 cache symbols. Retain all P3–P11 oracle tests, the four baseline firmware build variants, and both RES60 build variants. A CI success is not evidence of hardware performance.

**Hardware procedure:** flash **only** `esp32-cyd-res60-inverted` (the alternate-panel profile used in P11) on the same classic CYD:
```sh
cd ESP32
pio run -e esp32-cyd-res60-inverted -t upload -t monitor
```
START (two taps) → RIGHT through 4 angles → LEFT through 4 angles → MENU → START again → rotate once → MENU. Expected: initial `inventory cache=MISS` with `invUs` near previous ~135ms, subsequent rotation `cache=HIT` with inventory time much smaller, second START `MISS` again. All four original P3 raw FNVs `ed9b68c4/48ae8442/2697640d/f43a2791`; original texture `logical=65` on angle64 yields 26 columns/650 px; `sourceCRC=623f34e4`, arena `c3882516`; heap8 stable, runtime RELEASED, PAK closed; no panic/WDT. Observe actual `preparedUs` by angle; cold catalog cost may dominate first START again and is explicitly out of this optimization's scope.

## Real classic-CYD hardware result — 22 November 2026: bounded P12 PASS

The user-tested code candidate is **`bcb20b0beba776e8e6ea17b35963e99bbb4b52f7`**, on branch `agent/esp32-res60-p1` based on ARCH-RC23 `4ece448f523d32db6ddcf85d1ca4492513339dbb`; full C11 + six firmware-profile CI [SUCCESS #44987144199](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44987144199). User Serial **`platformio-device-monitor-261122-072032.log`**, **302 lines / 41,001 bytes / SHA256 `56630330615242bcb449c8e44737ea3ac3b6372e7f03e8df63990b0fcd1c7502`**. Device: real ESP32-D0WD-V3, dual core 240MHz, 4MiB flash, **0 PSRAM**, microSD mounted, alternate-panel profile `inversion=off`. No binary SHA is emitted by the Serial; attribution to candidate follows the immediately preceding user flash context and the P12 tags, **not** a cryptographic attestation of uploaded firmware.

**Coverage:** 20 complete 192x144 framebuffer views; exactly **2 `[BSPREAD] ENTRY/INVENTORY/STREAM`** cold scans, **2 `[RES60P12] INVENTORY cache=MISS`** (`invUs=138115/138046` us) and **18 cache=HIT** (`invUs=6–15` us). **18 successfully committed quarter-turns** (10 RIGHT / 8 LEFT) through all four original orientations. A successful **MENU return**, FNV `c093a13b`, then a second START forces a new BSP inventory and P8 texture `MISS`, followed by warm HIT rotations. The capture **ends inside the second BSP preview**: no second MENU return is claimed. Repeated PAK map header size **21823B**, source CRC **`623f34e4`**, compact arena **14095B / FNV `c3882516`**, 20 MAPRT READY/released and `pak=closed` on every completed frame.

**Exact geometry/material parity across all views:** four raw P3 cardinal FNVs remain **N64 `ed9b68c4` / E0 `48ae8442` / S192 `2697640d` / W128 `f43a2791`**. P9 wedge candidate counts 346 / 120 / 8 / 41 respectively are unchanged; P8 source texture `logical=65`, sampleFNV `e627fa0d` stable; P10 projects the same **26 columns and 650 real 4bpp wall pixels at N64**, `sampleFNV=e1f75a15`. `catalogUs=0` and `texPakUs=0` on all 18 texture cache HITs. Only one texture was cached across all frames; the zero projected columns at other angles do not imply absence of visible geometry.

### P11 → P12 warm timing: true on-device improvement

The same P11 probe stays enabled in P12. Comparison is between **`preparedUs`**, taken before diagnostic Serial output and before LCD present, for matching angles. The P11 values are the previously hardware-recorded cardinal samples; P12 values below are means of 4–5 **warm** angle-matched profiles in this capture. Because profiles still add observation overhead, only the bounded measured comparison is claimed.

| Cardinal view | P11 warm `preparedUs` | P12 warm `preparedUs` | Reduction |
| --- | ---: | ---: | ---: |
| N64 | ≈348.3 ms | **214.84 ms** (4 HITs) | ≈133.5 ms / **38%** |
| E0 | ≈257.9 ms | **123.99 ms** (5 HITs) | ≈133.9 ms / **52%** |
| S192 | ≈213.1 ms | **79.61 ms** (5 HITs) | ≈133.5 ms / **63%** |
| W128 | ≈226.8 ms | **92.76 ms** (4 HITs) | ≈134.0 ms / **59%** |

`invUs` fell from **≈134.9–135.5 ms P11** to **6–15 microseconds P12** on HIT, confirming that the full inventory was not repeated. `loadUs` **≈51.17–51.24 ms** persists on every P12 warm turn: original PAK entry validation, 14,095B compact arena reconstitution and arena FNV still take place. The cold first START is **not faster** (`preparedUs=1717181/1717192` us) because the unchanged `catalogUs≈1.330 s` dominates. Physical `PRESENT≈36.87 ms` remains unchanged and is not a recommended optimization.

**Memory:** `heap8=133812B` after every frame, `largest8=86004B`, `minHeap8=106764B`, with no drift across repeated rotations, MENU and second START. Compared with P11 `heap8=134204B`, `minHeap8=107156B`, the ~392B lower free/min heap is stable, **not a memory saving**. All observed frames have `mapRuntime=RELEASED pak=closed`; there are zero `FAIL`, panic or WDT witnesses in the capture.

**Boundaries:** P12 is **PASS on real CYD for the inventory reuse + measurable warm-rotation improvement + tested lifecycle/integrity**. There is no injected corrupted PAK on real hardware, no tested second MENU after second START, no gameplay/audio/sprites/multi-material rendering at 60%, no proof of original Doom RPG visual fidelity. The P12 code is frozen at `bcb20b0...`; subsequent PASS documentation commits must be **docs-only**. The pilot remains on its original ARCH-RC23 base; newer upstream `main` is **not** covered by this run. [PR #364](https://github.com/Kharn27/DoomRPG-RE/pull/364) stays **DRAFT / NOT MERGE-READY** because 192x144 is an isolated BSP preview, not production gameplay.
