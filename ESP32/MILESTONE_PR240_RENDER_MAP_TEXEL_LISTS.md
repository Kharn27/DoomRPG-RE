# PR #240 — retrait des listes de texels desktop dans Render_t (CANDIDAT, CYD NON TESTÉ)

## Base et frontière

Base GitHub `main=37bf3546321572fb31c772d428ecf129c56b1eb3` (merge #239), branche `agent/esp32-render-retire-map-texel-lists`. Ce document décrit uniquement le **candidat à valider**, jamais un PASS matériel.

Les quatre champs `mapTextureTexels`, `mapTextureTexelsCount`, `mapSpriteTexels`, `mapSpriteTexelsCount` deviennent desktop-only sous `#ifndef DOOMRPG_ESP32`. Le chemin natif ne les alloue ni ne les peuple ; la libération et les vérifications NULL associées sont retirées des gardes de démarrage, première frame et MENU_MAIN START, sans modifier les véritables invariants `shapeData=mediaTexels=NULL`. Le desktop `src/Render.c` et son ABI restent inchangés.

Ne **pas** confondre avec les tableaux vivants `mediaTexelOffsets`, `mediaBitShapeOffsets`, `mediaTexturesIds` et `mediaSpriteIds`, installés par `esp_legacy_config_mappings_startup.c`, ni avec `MappingPlan_t installedPlan`. Les parcours, colonnes, compteurs raster, palettes, BSP immuable, sprite topology, framebuffer et caches restent inchangés.

## Mémoire théorique et validation

Sur ESP32 32 bits, deux pointeurs et deux entiers de quatre octets : `Render_t 1348 → 1332 B` (**−16 B**), assertion de compilation ajustée. Taille `CORE READY used` théorique : `2316 → 2300 B` ; impact heap8 réellement alloué à déterminer sur CYD (granularité malloc). Aucun changement prévu sur les allocations maps/cache/PAK et aucune revendication de gain de temps.

Gardes hôtes modifiés : `native_render_raster_abi_retirement_host.py`, `native_render_runtime_start_host.py` et `native_render_free_retirement_host.py`. Les tests de teardown conservent leurs cycles et contraintes de possession, avec la nouvelle empreinte de corps `Render_freeRuntime=795212f8`. Les chemins desktop et bringup gardent leur propre compilation de référence.

## Critères de PASS, à remplir après logs matériels

Le candidat doit passer CI host et builds `esp32-cyd`, bringup, trace. L'utilisateur doit vérifier HEAD de branche et assertion `1332U`, effectuer un clean/rebuild/flash du firmware normal, relever au boot `Engine structs: Render=1332`, `RENDERCORE bytes=1332`, `CORE READY used=2300`, PSRAM=0, mappings 152 textures/252 sprites (8376 B), puis LOAD checkpoint slot04 → combat/projectiles/porte/EXIT → START intro/Entrance/dialogues/EXIT. Relever les fingerprints, heap8/largest8, deux `RESIDENTRESET released=18008 empty=1`, `SYSEXIT MENU-READY frame=0f2c9fc3`, `shapeData=mediaTexels=NULL`, absence panic/WDT. `NATIVEFRAME BOUNDARY-INLINE` et positions x projectile +0x40000000 sont préexistants, à ne pas imputer d'office au retrait.

**Statut : CODE CANDIDAT, CI/MATÉRIEL À CONFIRMER.** Ne mettre à jour PORTING_STATUS/DOCUMENTATION comme PASS qu'après confirmation du vrai CYD.
