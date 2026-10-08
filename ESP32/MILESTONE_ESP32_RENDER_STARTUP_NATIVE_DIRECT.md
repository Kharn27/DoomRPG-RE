# Milestone — retire desktop Render_startup / linker wrap

Status: **REAL-CYD HARDWARE PASS — direct startup**.

Tested code SHA: `dd577ad94f66ae642cc1ecaca7dc331b68c5b8e5`.
CI `esp32-cyd` run **37708924926 SUCCESS** (static RAM
45056 B, flash 774785 B).

## Exact physical CYD witness

The user's standard firmware log confirms:
`[RENDERSTART] OWNER api=Render_startup source=esp-native-render-startup direct=yes wrap=no`.
Shared platform framebuffer 160x120 RGB565 38400 B; sin table 1024 B;
3280 palette entries / 6560 B, mappings payload 8376 B.
After Render startup heap8=167780, after mappings heap8=159340.
Initial MENU_MAIN FNV 522dc605, native MAP_INTRO arena FNV
c3882516, first world frame FNV 71ca7465 and native
`shapeData==NULL, mediaTexels==NULL` invariants held.

Two FORWARD movements each had one camera midpoint preview, one
logical MOVE and exactly one MONSTERTURN; TURN_RIGHT had two
preview frames and no monster turn. A subtype-2 crate was
transformed into an Armor Shard by a successful attack and
subsequently picked up (armor 0->4, ammo 8->7).
HUB SYS double-confirm EXIT returned `RESIDENTRESET empty=1`,
releasing 18008 B of resident owners; final MENU_MAIN FNV 522dc605,
heap8=164184 largest8=110580 and saveWrite=no.

The latest excerpt has **no dialog, LOAD, CHANGEMAP, active-monster
combat, blocked movement or invocation of Render_free**.
Do not describe these as witnessed. The normal SYS EXIT path only
cleared native owners, it does not prove Render_free's destructor.
No post-PASS code change to the tested milestone: documentation
closure is docs-only, and further retirement gets its own candidate.



Same branch: `agent/esp32-retire-desktop-source-registration`; previous palette/mappings hardware PASS
code `ea221b4a7248d05ceda077f6c226e7da0851d302`,
docs closure `b5ef5c4efaaf348e38ef5f098320d995c37a7a54`.

## Boundary

The production ESP32 has already executed a full native implementation
of `Render_startup` in `render_startup_bridge.c` for months, via
`-Wl,--wrap=Render_startup`. The desktop original `Render_startup`
function instead creates an SDL streaming texture and allocates
a second full-frame RGB565 buffer. The ESP32 native path reuses the
existing 160x120 framebuffer (38400 B) and does not create `piDIB`.

This milestone removes the obsolete linker wrap flag and renames
the existing, previously hardware-proven `__wrap_Render_startup`
native definition to the real `Render_startup` ABI symbol.
Its entire body and allocation semantics remain unchanged except
for a source-ownership diagnostic. The original desktop definition
is excluded from generated ESP32 `Render.c` by `#if
!defined(DOOMRPG_ESP32)`; the unmodified desktop source stays
the executable specification. Its source body is pinned by
CRC32 `0xf0d935e8`, and strict entry/exit anchor checks.

Expected build-script marker:
`[ESP32] Desktop Render_startup unlinked;
owner=esp-native-render-startup direct=yes linkerWrap=no`.
Expected boot marker:
`[RENDERSTART] OWNER api=Render_startup
source=esp-native-render-startup direct=yes wrap=no`.

The `Render_free` linker wrapper must **remain enabled**, because
it protects the shared platform framebuffer on teardown; the
rest of generated desktop `Render.c` remains linked.
No `Render_t` field, native BSP runtime, render cache,
visual interpolation or palette semantics changes.
No new allocations and no claimed RAM saving from this symbol
ownership migration.

## Hardware acceptance

CI normal `esp32-cyd` SUCCESS before flashing;
confirm boot owner marker, palette entries=3280 bytes=6560,
mappings=8376 B, shared framebuffer=38400 B,
`piDIB==NULL`, first gameplay frame FNV `71ca7465`,
MAP_INTRO arena `c3882516`, and initial/menu final
`522dc605`. Observe turns, forward movement and native dialogue,
then SYS -> EXIT: `RESIDENTRESET empty=1`, heap8 final
164184 B, `shapeData=mediaTexels=NULL`.
Physical results must be obtained on this exact SHA;
do not label PASS based only on CI.
