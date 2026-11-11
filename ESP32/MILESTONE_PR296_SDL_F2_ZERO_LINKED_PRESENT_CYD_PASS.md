# SDL-F2 — dernier symbole SDL lié retiré : PASS matériel classic CYD (11 novembre 2026)

- **PR** : https://github.com/Kharn27/DoomRPG-RE/pull/296 ; **issue SDL** : https://github.com/Kharn27/DoomRPG-RE/issues/269
- **Base exacte** : `main=d11600e4caedbdb1f97c2196d3ae80f136cf8a3b`, après merge de #294 (SDL-F1).
- **Code candidat testé** : `f5e05d02fd937c9f100f069e1c34b9e7abc9803c` ; **attribution par parcours de flash de l'utilisateur**, car aucun SHA firmware n'est imprimé par le Serial.
- **CI finale avant flash** : https://github.com/Kharn27/DoomRPG-RE/actions/runs/43003863136 — **SUCCESS**, tests hôte et differential pixel/clip/présentation de 1600 opérations, 3 builds PlatformIO, compiledb et audits ELF. **Exactement 0 symbole `SDL_*` lié** dans les profils normal/bringup/trace ; dans SDL-F1, il en restait 1 (`SDL_RenderPresent`, 40 B). Pas de gain runtime déclaré.
- **Mesures compiler** : SDL-F2 normal **859921 B flash / 80768 B RAM statique**, comparé au firmware SDL-F1 combiné Root-G **860301 B / 80792 B**, soit **−380 B flash / −24 B RAM statique**. Bringup F2 **882657 / 80920 B** et trace **878057 / 80816 B**. Aucun benchmark A/B latence/heap dynamique sur matériel.
- **Log matériel** : `device-monitor-261111-120752.log` (**1680 lignes / 193538 B**) ; 2 lignes ALIVE préalables à un **POWERON_RESET** réel, ESP32-D0WD-V3 240 MHz, flash 4 MB, **PSRAM=0**, PAK natif 241 entrées.

## Frontière SDL-F2

Les **13 sites de présentation de production** dans six fichiers C (ABI `DoomRPG_flushGraphics`, menu principal, options/vidéo, touches, retour BACK, HELP) appellent désormais directement la passerelle **`Esp32PlatformVideo_present()`**, C-vers-C++ vers le même `PlatformVideo_present()`. Le contenu, la cadence, le garde de non-présentation et la sortie physique sont inchangés : **un framebuffer 160×120 RGB565 (38400 B), nearest-neighbor ×2 sur écran 320×240**. Le corps `PlatformVideo_present()` n'a pas été optimisé ni modifié ; pas de second framebuffer, de ZIP runtime, de mutation world/entity, ni d'allocation supplémentaire.

L'ancien symbole `SDL_RenderPresent` et d'autres fonctions SDL dormantes restent dans `esp32_sdl.cpp` comme oracle hôte ou code non lié. Les **noms/types SDL des sources et les headers** ne sont donc **pas encore éliminés** : le jalon démontre **zéro fonction SDL liée**, pas un code source entièrement SDL-free. Le boot produit encore `[SDL] CYD display attached` (fonction de raccord existante du shim), ce qui confirme qu'un nettoyage de source/propriétaire reste nécessaire.

## Tests matériels observés

- Boot et menu natifs complets, `PSRAM: 0 bytes`, log `[VIDEO] Logical framebuffer 160x120 RGB565, 38400 bytes`. `[NATIVEIMAGE] DECODE/ADOPT` **9 fois chacun** pour des images indexed **4-bpp**. Pas de retours BMP map-wide/ZIP.
- **OPTIONS/VIDEO** : passage de `NORMAL→INVERSE` puis `INVERSE→NORMAL`, deux persistances SD V3 `[NATIVECONFIG] SAVE ... committed` ; cycle `INVERSE→INVERSE` sans changement ; thème `1→0` persisté. Retour MAIN et frame FNV **`1945fafa`** après changement de thème, touch réarmé.
- **HELP** : guide 112 lignes, pagination de **0 à 70** par pas de 7, preview mur depuis PAK, retour MAIN `1945fafa`, pas de parcours exhaustif jusqu'à 112.
- **INTRO/Entrance** : `INTROFIT PREPARE elapsedMs=41`, intro tap/reveal/page/continue, `INTRODISP READY`, chargement BSP `/intro.bsp` CRC `623f34e4`, arène `14095 B`, intro disposée, `ENGINESESSION FRESH-VISIBLE`, puis chargement de checkpoint **V11 slot04 5604 B** et `RESUME-VISIBLE` avec restaurations exactes annoncées.
- **Gameplay** : **8 `DIALOG OPEN` / 8 `CLOSE`** incluant `EV_DIALOG` (8), `EV_DIALOGNOBACK` (26) et help d'arme ; **4 `DOORANIM COMPLETE`** et présentation de fermeture différée ; **12 `PLAYERRES COMMIT`**, **12 `PICKUPMINI START/END`**, tirs sur caisses, mouvements et rotations `VIEWANIM` ; menus HUB inventaire/système et grille d'armes, sélection d'arme et reprise monde ; une riposte `MONSTERRETAL COMMIT` du Malwrath sprite322 avec 3 coups et un `MALWRATHPROJECTILE CLEAR ... rngExact=yes`.
- **Deux vrais retours menu complets** : 2× `[RESIDENTRESET] released=18008 empty=1` et 2× `[SYSEXIT] MENU-READY frame=1945fafa session=off resident=empty` ; menu final `heap8=130696 largest8=86004`, touch inactif, pas de session résidente.
- **Marges RAM** : 2× `HEADROOM_OK`, marges **heap8=28228/27416 B**, **DMA=69188/68376 B**. `largest8=61428` en jeu ; `shapeData=0x0` et `mediaTexels=0x0` à 13 frontières observées. Aucun `panic`/`Guru Meditation`/`WDT`/`abort` observé.
- **Diagnostics existants** : 10× `[NATIVEFRAME] BOUNDARY-INLINE` non fatals, sans causalité démontrée avec F2. Pas de test exhaustif erreurs provoquées/OOM, formats 1/8-bpp ni benchmark FPS/latence A/B.

## Verdict et suite

**PASS matériel ciblé SDL-F2** sur le vrai classic CYD. La CI et les logs apportent une validation cohérente de la nouvelle frontière de présentation, de l'orientation, du rendu menu/intro/gameplay, des sauvegardes et du nettoyage mémoire. Aucun code ne doit être modifié après ce test sur la branche #296 : **docs-only**, CI finale, puis fusion autorisée par l'utilisateur.

Sur le prochain `main` fusionné, auditer puis retirer **les derniers éléments de compatibilité SDL dans les sources** par familles cohérentes, sans toucher prématurément au code de transfert TFT.
