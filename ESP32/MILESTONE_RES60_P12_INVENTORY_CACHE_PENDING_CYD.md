# RES60-P12 — éviter le BSP inventory complet à chaque rotation (CI candidate / CYD PENDING)

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

**Status: candidate only, CYD PENDING.** No RAM, speedup, P3 FNV or full-gameplay performance claims until actual user hardware log is checked. Remains a separate experimental milestone under DRAFT #364, NOT MERGE-READY.
