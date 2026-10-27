# PR #235 — Jalon B : retrait des palettes de span desktop, PASS CYD

Date de validation : 27 octobre 2026.
Branche : `agent/esp32-render-retire-tmpline`.
Base exacte : `main=47466adb879c2e4be0965fd0e62de0bf1b6fb242` (PR #234 mergée).
[PR #235](https://github.com/Kharn27/DoomRPG-RE/pull/235).
**Code B matériellement éprouvé sur classic CYD, firmware normal `esp32-cyd` : `ff28238ad441fa6dafc307c326fdd59680f80332`.**
L'attribution au commit provient du workflow de flash/branche ; les logs Serial ne contiennent pas de SHA embarqué.
CI code B : [push #40560076246](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40560076246) et [PR #40560086753](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40560086753) SUCCESS (gardes hôte, différentiel géométrique et trois variantes de firmware).

## Frontière B

Dans `src/Render.h`, `Render_t::spanPalettes` et `Render_t::newSpanPalette[16]` sont conservés par le moteur desktop, mais absents lorsque `DOOMRPG_ESP32` est défini. Les anciens `Render_SpanMode*` appartiennent au moteur desktop `src/Render.c` exclu de la cible CYD. Le renderer natif ne doit pas réintroduire ces champs ; garde hôte supplémentaire dans `ESP32/test/native_render_raster_abi_retirement_host.py`. `ESP32/src/render_startup_bridge.c` impose `sizeof(Render_t)==1464` pour le firmware normal, contre `1500` à l'issue du jalon A. Aucun changement de la logique de palettes native, des pixels volontaires, du framebuffer, du renderer sprites, du BSP, des assets PAK, de la SD, du gameplay, du RNG ou des SAVEs.

## Campagne matérielle : START → INTRO → Entrance → EXIT

Au boot : `ESP32-D0WD-V3`, flash 4 MB, **PSRAM=0**, `Engine structs: Render=1464 Game=0 Canvas=44 Total=2268`, `[RENDERCORE] INIT bytes=1464`. Frame menu initiale `0f2c9fc3`. Prologue `ade0195d`, `INTRODISP recovered=34056`, BSP `/intro.bsp` 21823 B CRC `623f34e4`, arène native `14095/c3882516`, première frame **`71ca7465`** (8 murs/4430 pixels). Déplacements, rotations et strafe testés ; caisse127 transformée en Armor Shard, armure `0→4→8→12`. Dialogue événement 88 (opcode8), continuation opcode19 `stateMutation=1`, second dialogue annulé sans mutation. Porte275 : ouverture en 4 frames, fermeture différée HOLD/RELEASE après tour monstre. Caisse56 transformée en Rocket récupérée tile867, `ammo3=1`. `DIALOGCHAIN OWNER-RELEASE recovered=1144`. EXIT sans SAVE : **`RESIDENTRESET 141928→159936 released=18008 empty=1`**, menu **`0f2c9fc3`**, `ALIVE heap8=159936 largest8=110580`.

## Campagne matérielle : LOAD V11 slot04 → Malwrath → EXIT

Slot04 `5604 B`, checkpoint complet/restauré : 30 monstres `beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activation 13→14 par porte113. Porte113 ouverte 4 frames puis fermée HOLD/RELEASE après tour monstre. Sprite324 : `copies=4`, `draws=4→2→0`, clipping et near-boundary observés. Malwrath322 HP **`11→6→2→0`** sous deux tirs shotgun et un tir pistolet, `death4→corpse2`. Deux ripostes de trois projectiles204 chacune, avec `MALWRATHPROJECTILE CLEAR` avant chaque `MONSTERRETAL COMMIT` ; joueur HP **`34→27→20`**, armure **`23→16→9`** avant récupération. Drop Health Vial tile284 (`20→24 HP`), Bullet Clips sprite324 tile317 (ammo pistolet `15→25`), Flak Jacket tile253 (armure `9→23`), portes secrètes112/111 `+5XP`. EXIT sans SAVE : **`RESIDENTRESET 141928→159936 released=18008 empty=1`**, menu `0f2c9fc3`, `ALIVE heap8=159936 largest8=110580`.

## RAM comparée à la campagne A, limites

Aux checkpoints comparables : `heap8` gameplay `113972→113988` (**+16 B**), menu post-EXIT `159908→159936` (**+28 B**), plus gros blocs inchangés à 86004 B en gameplay et 110580 B au menu. Au boot, `Heap8=249684`. La réduction structurelle est de 36 B, mais **ne pas attribuer automatiquement les variations d'allocateur 16/28 B aux seuls champs supprimés**. `shapeData=mediaTexels=NULL` à START, READY et cleanup. Aucun panic, `WORLD_POST fail` ni `RNG_REPLAY_DIVERGED` dans les extraits. `NATIVEFRAME BOUNDARY-INLINE` et `MALWRATHPROJECTILE.pos.x - NATIVESPRITE TRANSIENT.pos.x = 0x40000000` persistent, non diagnostiqués et hors scope. Pas de fingerprint SHA embarqué, SAVE neuf, test d'endurance, toutes cartes, équivalence pixels exhaustive ni mesure A/B contrôlée.

**Décision : PASS matériel ciblé du code B `ff28238ad...` ; code gelé. Après le PASS, seuls commits docs-only autorisés avant fusion ; vérifier le diff et la CI post-doc. Merge dans main uniquement par l'utilisateur.**
