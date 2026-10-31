# PR #240 — retrait des listes de texels desktop dans Render_t (PASS CYD ciblé, code gelé)

## Base et frontière

Base GitHub `main=37bf3546321572fb31c772d428ecf129c56b1eb3` (merge #239), branche `agent/esp32-render-retire-map-texel-lists`. Ce document réunit le candidat et les preuves du **PASS matériel ciblé après test normal sur CYD**.

Les quatre champs `mapTextureTexels`, `mapTextureTexelsCount`, `mapSpriteTexels`, `mapSpriteTexelsCount` deviennent desktop-only sous `#ifndef DOOMRPG_ESP32`. Le chemin natif ne les alloue ni ne les peuple ; la libération et les vérifications NULL associées sont retirées des gardes de démarrage, première frame et MENU_MAIN START, sans modifier les véritables invariants `shapeData=mediaTexels=NULL`. Le desktop `src/Render.c` et son ABI restent inchangés.

Ne **pas** confondre avec les tableaux vivants `mediaTexelOffsets`, `mediaBitShapeOffsets`, `mediaTexturesIds` et `mediaSpriteIds`, installés par `esp_legacy_config_mappings_startup.c`, ni avec `MappingPlan_t installedPlan`. Les parcours, colonnes, compteurs raster, palettes, BSP immuable, sprite topology, framebuffer et caches restent inchangés.

## Mémoire théorique et validation

Sur ESP32 32 bits, deux pointeurs et deux entiers de quatre octets : `Render_t 1348 → 1332 B` (**−16 B**), assertion de compilation ajustée. Taille `CORE READY used` théorique : `2316 → 2300 B` ; impact heap8 réellement alloué à déterminer sur CYD (granularité malloc). Aucun changement prévu sur les allocations maps/cache/PAK et aucune revendication de gain de temps.

Gardes hôtes modifiés : `native_render_raster_abi_retirement_host.py`, `native_render_runtime_start_host.py` et `native_render_free_retirement_host.py`. Les tests de teardown conservent leurs cycles et contraintes de possession, avec la nouvelle empreinte de corps `Render_freeRuntime=795212f8`. Les chemins desktop et bringup gardent leur propre compilation de référence.

## Validation matérielle et limites

Le candidat doit passer CI host et builds `esp32-cyd`, bringup, trace. L'utilisateur doit vérifier HEAD de branche et assertion `1332U`, effectuer un clean/rebuild/flash du firmware normal, relever au boot `Engine structs: Render=1332`, `RENDERCORE bytes=1332`, `CORE READY used=2300`, PSRAM=0, mappings 152 textures/252 sprites (8376 B), puis LOAD checkpoint slot04 → combat/projectiles/porte/EXIT → START intro/Entrance/dialogues/EXIT. Relever les fingerprints, heap8/largest8, deux `RESIDENTRESET released=18008 empty=1`, `SYSEXIT MENU-READY frame=0f2c9fc3`, `shapeData=mediaTexels=NULL`, absence panic/WDT. `NATIVEFRAME BOUNDARY-INLINE` et positions x projectile +0x40000000 sont préexistants, à ne pas imputer d'office au retrait.

**Résultat CYD : PASS ciblé sur le code c23d8925...** — `device-monitor-261031-021905.log`, normal `esp32-cyd`. Observations : `Engine structs Render=1332`, `RENDERCORE bytes=1332`, `CORE READY used=2300`, mappings 152/252, 8376B. Heap8 initial menu 153160, LOAD prime 112068, START prime 112880, après EXIT 158004 ; largest8 gameplay 86004 et menu 110580. LOAD slot04 V11 : Malwrath 322, triple projectile204, mort/drop, pickups, secret, EXIT ; START intro/Entrance première frame 71ca7465, caisse, pickups, portes/dialogues, EXIT. Deux resets `released=18008 empty=1`, deux `SYSEXIT MENU-READY frame=0f2c9fc3`, aucune panic/WDT. `shapeData/mediaTexels=NULL`, PSRAM 0.

**Réserve UX :** l'utilisateur constate moins de nervosité dans la zone Malwrath et le flash blanc fixe des pickups. Le flash de 500ms ne bloque pas les mouvements jusqu'à son expiration ; une lease tactile distincte de 120ms retarde le dispatch. La PR #241 distincte vise l'UX sans toucher au retrait Render. Pas de comparaison causale de performance #239B/#240 et pas de PASS de la PR #241. **Code #240 gelé ; ce commit est docs-only. Aucun merge automatique.**
