# RES60-P13 — retained compact MAP_INTRO lease, 192×96 BSP preview (CI candidate / real CYD PENDING)

**Branch:** `agent/esp32-res60-p1`, [experimental draft PR #364](https://github.com/Kharn27/DoomRPG-RE/pull/364). After the **P12 real-CYD bounded PASS**, the isolated pilot synchronized actual `main=1588b6983cc5724f7028ac0d2c18f96688aba990` via the history-preserving merge `5337b4ee1a2b9bd1d5fca947a059ddd7c0b0782d`. This is a merge, **not** a rewritten rebase; the P12 physically tested candidate `bcb20b0beba776e8e6ea17b35963e99bbb4b52f7` remains the only hardware-attributed P12 firmware. A post-integration CI success does not replace physical CYD proof.

## Motivation and deliberately limited scope

The P12 CYD Serial `platformio-device-monitor-261122-072032.log` demonstrated `invUs=6–15µs` on 18 cached rotations and the same four raw P3 FNVs. The remaining `loadUs≈51.2ms` on every warm turn comes from **reopening the original PAK and reconstructing the same 14,095-byte immutable `EspMapRuntime` arena** only to free it again before LCD present. Real production 160×120 already retains its native map across frames; the prototype's discard/reload behavior is a deliberate transitional bottleneck, not an optimization for the main engine.

**P13 changes only the RES60 BSP preview.** Keep exactly **one** original compact `EspMapRuntime` arena for the duration of an explicit preview session; no second arena, no world/entity/legacy render mutation, no map-wide texels or shape data. Preserve the existing P12 scalar inventory and P8 single 2,048-byte texel/palette cache. On the first START: P12 inventory MISS and the original `EspMapRuntime_loadPackEntry` cold map validation/load. On a subsequent rotation: P12 inventory HIT, **no compact reload**, but `EspRes60MapLease_acquire` reopens the PAK only for exact `/intro.bsp` entry size/declared CRC/directory-flag comparison, and checks an FNV1a **recomputed over every byte of the retained arena**, its source metadata, and the P12 cached canonical arena FNV, before any projection. The PAK lease closes before P8 drawing and LCD. Any failure is fail-closed; MENU success, next START and failed render/present drop the retained map owner. A failed angle never commits input orientation.

The lease is capped to the **existing immutable 14,095-byte arena**, not a new cache allocation. It changes steady-state RAM: expect a *lower* free/largest heap than P12's `heap8=133812B`, exactly because the arena remains resident; the actual magnitude and peak must be obtained from new Serial, not guessed. The 192×144 framebuffer (55,296B) and nearest physical presentation are unchanged. No ZIP at runtime; no modification to `esp32-cyd`, `esp-cyd-inverted`, bringup or trace firmware.

## Real-C owner, gates and expected witnesses

- Build flag `DOOMRPG_RES60_P13_MAP_LEASE=1` requires P12, P7 and P1; present **only** in `esp32-cyd-res60` and `esp32-cyd-res60-inverted`. In production the C owner compiles to no exported symbols.
- New `esp_res60_map_lease.c/.h` owns one session-validity bit plus the existing `EspMapRuntime` allocation. All runtime pointer and packed-entry comparisons are explicit; recomputes resident arena FNV on every hit.
- `[RES60P13] MAP lease=MISS|HIT ... mapUs=... bytesResident=14095 validated=pak-metadata+resident-arena` is emitted **before** the existing P11 profile and P7 frame witnesses. `loadUs` measures original cold load on MISS, only metadata lookup plus FNV check on HIT.
- P13 `[RES60P7] FRAME ... mapRuntime=RETAINED pak=closed`; pre-P13 `RELEASED` invariant no longer applies to this pilot. The release is proved instead at MENU/START and on error via owner reset, with host tests and explicit hardware session flows.
- Host `ESP32/test/res60_p13_map_lease_host.py` compiles the **real P13 C owner** with mocks, exercises 12 hits with exactly one compact load, rejects edited resident arena bytes, changed PAK CRC/size/directory flag, invalid catalog FNV, source CRC and incorrect session boundaries, asserts PAK closure/reset and excludes P13 from normal production.
- GitHub Actions exercises P1–P13 host tests, original production/diagnostic builds and both experimental RES60 firmware profiles.

## Mandatory next real-CYD protocol / criteria

On the **alternate-panel classic CYD**, flash normal pilot target `cd ESP32 && pio run -e esp32-cyd-res60-inverted -t upload -t monitor`. START double-tap, rotate RIGHT and LEFT across all four cardinal angles (several times), MENU, select START again and rotate, MENU. Capture full serial. Verify:

1. Exactly one P13 `lease=MISS` per START, then `lease=HIT` on warm rotations. The two STARTs force two cold P12 inventory MISS and P8 swatch MISS. A failure rejects rotation; no panic/WDT/FAIL.
2. Four exact raw BSP FNVs `ed9b68c4/48ae8442/2697640d/f43a2791`, source CRC `623f34e4`, arena size 14095 and FNV `c3882516`. Texture ID65 when visible at N64: 26 columns, 650 pixels and P10 `sampleFNV=e1f75a15`. Menu FNV `c093a13b`.
3. On warm HIT compare P13 `mapUs` and `preparedUs` with P12 real-CYD measurements by cardinal angle, retaining P11 profiler. P12 warm `preparedUs`: N64≈214.84ms, E0≈123.99ms, S192≈79.61ms, W128≈92.76ms, `loadUs≈51.2ms`. **No P13 speedup is claimed before test.** Cold `catalogUs≈1.33s` is explicitly not touched.
4. Report stable `heap8`, `largest8`, `minHeap8`, compare occupied arena cost with P12, prove `mapRuntime=RETAINED pak=closed` on every completed frame and clean reset on MENU, including second session. No projection/entity/audio/gameplay changes.

**Status: CODE/CI CANDIDATE ONLY — P13 CYD PENDING. PR #364 stays DRAFT and NOT MERGE-READY.** A hash of the contents of the retained RAM arena plus PAK entry's *declared* CRC/size is not an independent full re-hash of the underlying PAK payload on every turn. The cold load still validates BSP source; active-session source replacement with unchanged PAK index metadata is outside this P13 check. Never overclaim such validation.
