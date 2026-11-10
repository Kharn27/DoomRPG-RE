# SDL-D1 — Texture/blit natifs : PASS matériel classic CYD (10 novembre 2026)

- **PR :** https://github.com/Kharn27/DoomRPG-RE/pull/289
- **Issue :** https://github.com/Kharn27/DoomRPG-RE/issues/269
- **Candidat D1 testé attribué :** `c4823b49831c58c12f7a7893a9719a7b03559d1b` (attribution parcours de flash, absence de SHA Serial)
- **CI avant flash :** https://github.com/Kharn27/DoomRPG-RE/actions/runs/42734310692 — SUCCESS
- **Serial matériel :** `device-monitor-261110-215526.log`, 1653 lignes / 193771 octets
- **Cible :** `esp32-cyd`, ESP32-2432S028R classic CYD, ESP32-D0WD-V3, 4MB flash, 0 PSRAM.

## Frontière et changements vérifiés

`ESP32/src/esp_native_texture.cpp` devient propriétaire du blit RGB565 nearest-neighbor, source/destination, clip, colorkey, modulation et destruction du payload indexed packed. `EspNativeTexture` conserve indices/palettes 1/4/8 bpp et adoption sans copie. Usage production migré (textes, menu, HELP, intro, image free) sans second framebuffer ni retour ZIP/shapeData/mediaTexels. `SDL_Texture` et `SDL_Rect` ne sont que des alias ABI ESP32 provisoires. Primitives rectangle/ligne/present, horloge et fatal SDL non touchées.

La CI D1 candidate validait trois profils et tests hôte 1/4/8-bpp, clip/scaling/colorkey/teinte, ressources et OOM, rollback, raster Canvas et gardes de 358 fichiers. **11→8 fonctions SDL liées** (disparition `SDL_RenderCopy`, `SDL_SetTextureColorMod`, `SDL_DestroyTexture`), corps de symboles SDL liés **1027→442 B**. vs main SDL-C1 : normal flash 859085→859321 (+236 B), statique RAM 80768→80784 (+16 B) ; bringup +296/+16, trace +288/+16. Aucune réduction mémoire/flash nette revendiquée.

## Preuves matériel observées

- Boot complet POWERON_RESET, 241 assets PAK, framebuffer 160×120×2, PSRAM=0.
- 9 décodages + 9 adoptions indexed **4-bpp** (menu, HELP, intro, fonts/hand), sans erreur ni pic OOM enregistré.
- OPTIONS → VIDEO → THEME 0→1 persistant, retour MAIN ; HELP 112 lignes, 9 pages observées, retour MAIN.
- START / prologue natif, disposal +34080 B heap8, Entrance `/intro.bsp` CRC `623f34e4`, arena immutable 14095 B ; gameplay natif prêt.
- 3 DIALOG OPEN et 3 CLOSE dont opcode 26→19 et opcode 8→19, quatre transitions portes complètes ; 12 miniatures pickups START et END.
- EXIT propre vers MAIN, LOAD V11 slot04 5604 B, restauration overlays/monstres/topologie/position/activation.
- Trois attaques Malwrath (sprite 322) trois coups chacune, 3× `MALWRATHPROJECTILE CLEAR rngExact=yes`, 3× `MONSTERRETAL COMMIT rollback=closed`. Trois tirs joueur : HP Malwrath 11→7→3→0, drop dynamique observé ; deuxième EXIT propre.
- 2× `HEADROOM_OK` marges heap8 28188 et 27376B, marges DMA 69148 et 68336B ; `largest8=61428` en jeu ; 2× `RESIDENTRESET released=18008 empty=1`, 2× `SYSEXIT MENU-READY`; menu final `heap8=130656 largest8=86004`.
- `shapeData=NULL` et `mediaTexels=NULL` aux frontières journalisées ; aucune panic/Guru/abort/WDT. **7 `NATIVEFRAME BOUNDARY-INLINE`** déjà connus, non fatals.

## Limites et intégration

**PASS matériel ciblé, pas une preuve exhaustive.** Seul le 4-bpp est exercé directement sur CYD ; 1/8-bpp, erreur fatale/OOM provoquée, joueur mort/RETRY, et métriques performance A/B restent non couverts. Les adresses `MALWRATHPROJECTILE pos=1073743xxx` sont des traces à surveiller indépendamment, sans affirmer une causalité D1.

Pendant le test, `main` a avancé via Root-F PR #288 jusqu'à `317ca310ca4a2f6738cf975e13aced1b64338349`. Le hardware valide D1 **avant cette recombinaison**. Pour merger #289, intégrer Root-F sur la branche D1 (préserver les propriétaires de chaque côté), réussir CI/hôte trois profils, et garder la clôture documentaire séparée du code validé. Ne pas assimiler la CI combinée à une seconde validation hardware.
