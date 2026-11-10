# SDL-B7 — retrait du décodeur BMP SDL redondant : PASS matériel classic CYD

**Date : 10 novembre 2026** · [PR #284](https://github.com/Kharn27/DoomRPG-RE/pull/284) · [issue SDL #269](https://github.com/Kharn27/DoomRPG-RE/issues/269)

## Frontière du code testé

- Base exacte de la branche : `main=0828a245b36762ccb1797da615cf152ffabd5691`, après [B6 #283](https://github.com/Kharn27/DoomRPG-RE/pull/283).
- **Code candidat B7 testé sur le vrai CYD selon le flash utilisateur : `07578e7d2bd9fcd5b6638164c5775f1f50493f0d`.** Le log ne contient pas de SHA firmware embarqué ; cette attribution résulte du parcours de test indiqué par l'utilisateur.
- Retrait **ESP32-only** de l'ancien `SDL_LoadBMP_RW` limité aux BMP 8 bits et de ses trois helpers privés `rwSeek`, `readLe16`, `readLe32` : **4612 octets de texte source** supprimés de `ESP32/src/esp32_sdl.cpp`, plus déclaration morte retirée de `ESP32/include/SDL.h`. Aucun changement desktop.
- **Chemin actif conservé** : `Esp32Bmp_LoadRW` dans `ESP32/src/esp32_bmp.cpp`, décodeur indexé packed 1/4/8 bpp appelé par `ESP32/src/esp_doomrpg_image_resources.c`, puis `SDL_CreateTextureFromSurface`; ces deux fichiers propriétaires ne changent pas. Conserver SDL_Surface, SDL_RWops read/seek/close, SDL_FreeSurface, texture indexed/RGB565/update, palette/colorkey, blit/present, le framebuffer unique 160×120 RGB565 (38400 octets), `PlatformVideo_present`, et le chemin fatal `DoomRPG_Error → SDL_ShowMessageBox`.
- Garde `ESP32/test/sdl_dead_duplicate_bmp_host.py` : **352** fichiers sources/headers ESP32 inspectés, aucun appel effectif à `SDL_LoadBMP_RW`, helpers privés absents ; vérification positive du vrai lecteur indexé et des chemins SDL vivants. Gardes B5/B6 ajustées sans changer leurs protections actives.
- [CI push #42723462590](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42723462590) et [CI PR #42723537286](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42723537286) : **SUCCESS**, trois profils `esp32-cyd`, `esp32-cyd-bringup`, `esp32-cyd-trace`, tests hôte et contrôle ELF. Ancien décodeur absent des ELF candidat et baseline historique SDL-A ; **19 fonctions SDL liées** sur chacun des trois profils candidats, **1608 octets symboliques** contre **1547 octets** de baseline SDL-A historique. Cet écart agrège d'autres PR : **aucune économie ROM/RAM/FPS B7 isolée prouvée**. La suppression de 4612 octets de source n'implique pas 4612 octets économisés dans le firmware.

## Rapport physique : `device-monitor-261110-005625.log`

**1942 lignes / 226829 octets**, une séquence de boot `POWERON_RESET` puis **deux sessions de jeu**. ESP32-D0WD-V3 dual core 240 MHz, flash 4194304 octets, **PSRAM=0**, PAK 241 entrées, écran 320×240 nearest-neighbor ×2 à partir du framebuffer partagé **160×120 RGB565 / 38400 B**. `Render=112`, `Game=0`, `Canvas=40`. Les checkpoints `shapeData=0x0 mediaTexels=0x0` sont répétés.

**Preuve directe du propriétaire BMP** : **7× `[BMP] decode`** et **7× `[SDL] Adopt packed indexed texture`**, portant sur 144×72, 108×74, 192×128 (2 fois), 128×128, 9×9 et 13×10, **tous 4-bpp packed**. Aucune erreur BMP. Cela valide physiquement la voie 4 bits réellement parcourue ; les formats 1 et 8 bits ne sont **pas** exercés par ce rapport, mais restent couverts par le code/les tests hôte.

**Session 1** : START, intro et progression complète jusqu'à Entrance `/intro.bsp` CRC `623f34e4`, arena immutable **14095 B**, texture/HUD natifs, mouvements et rotations, ouverture/fermeture de portes (dont fermeture différée après tour monstre), dialogues `opcode 8/26` et reprise `19`, objets/arme, feu, secret/ennemis non nécessaires au critère, miniatures de ramassage, menu HUB puis EXIT. **Premier `RESIDENTRESET empty=1` et `SYSEXIT MENU-READY`**.

**Session 2** : menu LOAD → sauvegarde **slot 04 V11 / 5604 B** valide ; restauration des ressources, scripts, lignes, topologie, positions et activations des monstres, automap et drops ; retour au gameplay Entrance sans rejouer le prologue. **Malwrath sprite322** : deux `MONSTERRETAL COMMIT` trois tirs, PV joueur **34→26→19**, puis coups joueur et mise à mort `hp=7→0`, **un `MONSTERDROP COMMIT`**. Déplacements, ramassages, menu HUB puis second EXIT propre vers menu.

**Comptages log** : `RESIDENTGAMEPLAY QUEUE` **144**, `MOVE` **54**, `DIALOG OPEN` **8** dont 1 standalone, `CLOSE` **8**, `PICKUPMINI START`/**END** **14/14**, `MONSTERRETAL COMMIT` **2**, `MONSTERCOMBAT COMMIT` **2**, `MONSTERDROP COMMIT` **1**, `DOORANIM COMPLETE` **7**. **2× `HEADROOM_OK`** : `margin8=28412/27600` et `marginDMA=69372/68560`; `largest8=61428` durant gameplay ; **2× `RESIDENTRESET released=18008 empty=1`** et **2× `SYSEXIT MENU-READY frame=3721ecf7`**, sans sauvegarde écrite. Aucune panic, Guru Meditation, WDT, abort, backtrace ni erreur BMP observée.

**Réserves** : **11× `[NATIVEFRAME] BOUNDARY-INLINE`** (logical15: 5, logical49: 3, logical48: 3), diagnostics de frontière visuelle à suivre sans attribuer automatiquement leur origine à B7. Le rapport n'exerce ni OPTIONS/HELP, ni le chemin d'erreur fatale volontaire `DoomRPG_Error`, ni la mort du joueur/RETRY. Le parcours matériel est un **PASS ciblé**, pas une preuve de couverture exhaustive de toutes les voies SDL/BMP, et ne permet aucune revendication de gain de performance.

## Clôture

**PASS matériel sur `esp32-cyd` normal** au code candidat `07578e7d`. Toute écriture GitHub post-test pour clôturer B7 est **docs-only**. Avant fusion : vérifier l'état actuel de `main`, les CI, la PR et la compatibilité. Après fusion, relever **le SHA exact de `main`** avant de créer la branche suivante. La prochaine direction doit cibler des **propriétaires SDL réellement liés**, avec petite API ESP32-native permanente, par frontière bornée, gardes et nouveau test matériel.
