# MAPFLASH-V2 P1 — complete immutable PAK cache (pending CYD)

Issue #476, draft PR #477, branch `agent/esp32-mapflash-v2`. Base main verified `d1f25ba03688364efba753604f59aa75eb2eaadc`. P0 hardware PASS in `MILESTONE_MAPFLASH_V2_P0_PENDING_CYD.md`. Runtime code `ae018566e25c8ab75c5bfa29ae11a6672d8eebef`.

## Contract

- DMF2 version 2 deliberately rejects V1. One immutable cache for all maps; source PAK header fields represented by the cache header, unchanged complete index and contiguous entire data payload (all entries including all 13 BSPs). Direct translation `payloadFlashOffset + sourceOffset - sourceDataOffset`. No runtime ZIP, no partition changes, no new heap buffer; existing 4096 B transient scratch and 15 KiB L1 retained. No exclusions.
- Current target BSP existence/identity is checked against SD index each prepare. PAK structural identity/index FNV and full flash index/payload FNV required for every HIT; map ID is no longer a cache identity. Runtime target map changes in RAM only. A cache header's persisted map ID is the map that built it, not the most recent visited map; HELP hint may reflect this after deactivation/reboot. No save format change.
- Full capacity checked before erase. Erase header sector first, then only occupied aligned sectors. Index and payload copied with existing callback-safe source seek; verify flash bytes against copy FNV and index FNV against plan. Write header with erased marker, commit marker separately last, read back full header before publishing lease. Failure stays closed. No gameplay fallback to SD.
- P1 preserves V1 integrity semantics (source structural index identity, FNV of copied payload and full flash readback); independent per-entry source CRC validation of every non-BSP asset is not yet added. BSP loader continues existing CRC validation, source probes and preflight unchanged. A modified PAK must be generated with updated CRC/index; arbitrary in-place SD payload corruption with unchanged index is not detected by comparing the index alone.
- No speculative performance claim; full verification on every prepare retained, even where transition and resident arm both call prepare. P2 can investigate duplicated verification/preflight only after P1 hardware PASS.

## Geometry for P0 real pack

Partition 2 752 512 B, source PAK 2 457 398 B, 241 entries, index 4 820 B at flash 4096, payload 2 452 554 B at flash 12288. End 2 464 842 B, aligned erase 2 465 792 B (602 sectors), free aligned 286 720 B (70 sectors). Header 60 B. Index + payload verify 2 457 374 B. First build migrates V1; next transitions should write/erase zero bytes.

## CYD acceptance

Use existing SD and same partition CSV, no erase_flash, normal esp32-cyd or esp32-cyd-inverted if matching panel. Do not alter PAK or saves. First load/start expects V1 MISS and one `[MAPFLASHV2] BUILD` (eraseBytes=2465792, verifiedBytes=2457374, headroom=286720). All subsequent maps and reboot with unchanged PAK expect `[MAPFLASHV2] HIT map=N eraseBytes=0 writeBytes=0 verifiedBytes=2457374 scope=all-maps`, and ARM active=1 verified=1 reused=1 buildUs=0. If two prepare calls occur in transition, both HIT. No additional ERASE/COPY/BUILD. Capture loading RELEASE timings, READY, flash frames sdRangeUs=0, music/SFX, memory and null legacy pointers.

Suggested route: load Sector1 save → exit to menu → New Game Entrance → Junction → Sector1 (or LOAD between distinct map saves if quicker), play/move/shoot/open doors, save to unused slot and reload; power off after a completed build, reboot and load different map to verify persistent HIT. Do not deliberately interrupt a build or corrupt data until dedicated recovery validation. Host model covers direct offsets, all catalog maps, capacity, uncommitted/legacy header and corrupted index/payload rejection; it does not execute actual flash primitives. Hardware PASS and full CI status pending.
