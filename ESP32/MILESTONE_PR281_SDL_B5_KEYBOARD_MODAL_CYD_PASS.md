# SDL-B5 — retrait des façades clavier / boîte simple : PASS matériel classic CYD

**Date : 9 novembre 2026** · [PR #281](https://github.com/Kharn27/DoomRPG-RE/pull/281) · [issue SDL #269](https://github.com/Kharn27/DoomRPG-RE/issues/269)

## Périmètre code exactement validé

- Base originelle de la branche : `main=797406ad4d121b3bdf60e9513acc67d8ce9e36bd`.
- Code candidat avant clôture docs-only : `9e0c2d66243ff94e47efec97cfee31fdbb19c1f3`.
- Deux seules entrées retirées du shim et du header ESP32 : `SDL_GetScancodeName` (libellé factice `CYD input`) et `SDL_ShowSimpleMessageBox` (facade simple inutilisée). **`SDL_ShowMessageBox` reste présent**, appelé par `DoomRPG_Error` depuis `ESP32/src/engine_stubs.c`. Le premier essai à trois suppressions a été rejeté en CI, puis corrigé avant flash.
- Invariants de B5 : aucune modification des chemins SDL actifs `SDL_GetError`, `SDL_RWops`, BMP/indexed 1/4/8 bpp, palette, colorkey, textures compactes, framebuffer partagé, `PlatformVideo_present`, PAK, gameplay ou desktop/oracle. Le retrait ne prétend pas supprimer le reste de SDL.
- Garde `ESP32/test/sdl_dead_ui_stub_host.py` sur 350 fichiers natifs C/C++/headers et contrôle positif du chemin `DoomRPG_Error → SDL_ShowMessageBox`. Audit ELF des deux symboles retirés sur baseline SDL-A historique **et** trois profils candidats : mêmes 19 noms `SDL_*` liés. **1548 octets symboliques candidat vs 1547 octets baseline historique** ; ces chiffres ne donnent pas un gain B5. Les écarts de sections/RAM par rapport au vieux baseline SDL-A englobent des changements concurrents de main.
- [CI push #42547816894](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42547816894) et [CI PR #42547821100](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42547821100) : **SUCCESS** au SHA candidat `9e0c2d66` (firmware normal et auxiliaires en CI, seuls les tests matériels normal sont revendiqués).

## Preuve Serial : `device-monitor-261109-233353.log`

Fichier joint par l'utilisateur, **1547 lignes / 188056 octets**, commence par deux `[ALIVE]` à uptime 150/155 s avant une **nouvelle séquence ROM complète** `POWERON_RESET`, `ESP32-D0WD-V3`, 240 MHz, flash 4 MB, **PSRAM=0**. Aucune empreinte SHA firmware dans le Serial : l'attribution au code B5 suit la procédure de flash utilisateur, pas une comparaison binaire du log.

**Démarrage et affichage.** Un framebuffer `160×120 RGB565 = 38400 B` partagé avec la couche SDL, sortie `320×240` nearest-neighbor ×2 ; `Render_t=112 B`, `Game=0 B`, `DoomCanvas=40 B`, `shapeData=0x0` et `mediaTexels=0x0` aux points de contrôle. Initialisation PAK (241 entrées), BMP 4-bpp adoptés en textures indexées compactes et rendu menu FNV `1945fafa`.

**Session 1 : NEW / intro / Entrance.** Prologue exécuté et nettoyé (`INTRODISP recovered=34068`). BSP `/intro.bsp` CRC32 `623f34e4`, runtime immutable `14095 B`. Le joueur se déplace, tourne et strafe ; plusieurs caisses sont transformées ou retirées, medkit/armor/ammo ramassés, porte 275 ouverte puis refermée en animation différée, dialogue `EV_DIALOGNOBACK=26` événement 79 ouvert/fermé et `EV_NEXTSTATE=19` repris. Dialogue standalone d'aide et sortie au HUB. Premier `[RESIDENTRESET] ... released=18008 ... empty=1`, puis `[SYSEXIT] MENU-READY frame=1945fafa`.

**Après premier retour menu.** L'utilisateur ouvre HELP : `112` lignes, `7` visibles, pagination jusqu'au scroll maximal `105` ; retour natif MENU réussi. Le rapport **ne démontre pas de visite du sous-menu OPTIONS/VIDEO** dans ce run. Chargement du slot 04 V11 : `5604 B`, map Entrance, scripts, ressources, états monstres/topologie/position/activation restaurés.

**Session 2 : LOAD / Malwrath.** Sprite `322`, subtype 6. Trois séries complètes de **3 projectiles** chacune, chacune terminée par `[MALWRATHPROJECTILE] CLEAR`, suivies de trois `[MONSTERRETAL] COMMIT` ; HP joueur **34→28→21→16**. Trois attaques du joueur réduisent les HP Malwrath **11→7→3→0**, mort `death4→corpse2`, `xp=6`, pas de GIBFX déclenché dans ce cas. Porte et retour HUB/EXIT : deuxième `[RESIDENTRESET] released=18008 empty=1`, deuxième `[SYSEXIT] MENU-READY frame=1945fafa`. Pas de nouvelle écriture de sauvegarde (deux EXIT `saveWrite=no`).

**Mémoire, absence de panne.** Deux `[RAMBUDGET] ... advisory=HEADROOM_OK` : `margin8=30788` et `29976`, `marginDMA=71748` et `70936`; plus grand bloc 8-bit `61428 B` pendant gameplay post-cache, `86004 B` au menu. **2× RESIDENTRESET empty=1, 2× SYSEXIT MENU-READY**, pas de panic, Guru Meditation, abort, assert ni WDT. Les marqueurs `[ALIVE]` ne suggèrent pas de panne en session.

**Diagnostics hors périmètre.** Six `[NATIVEFRAME] BOUNDARY-INLINE` (logical 15 deux fois près de la fin, une fois initialement ; logical 49 trois fois) demeurent visibles ; diagnostic connu d'autres jalons, aucune preuve d'une régression causée par B5 ni de parité pixel-perfect totale. Pas de test death/RETRY, pas de démonstration d'OPTIONS/VIDEO et pas de benchmark A/B sur ce code. Le Serial ne peut pas exercer la route d'erreur fatale `DoomRPG_Error` sans provoquer une panne ; cette route est **protégée par la preuve source / CI**, pas matériellement déclenchée.

## Verdict et intégration

**PASS matériel ciblé SDL-B5** sur profil normal `esp32-cyd`. Les commits post-test sur la branche B5 doivent être **strictement docs-only**. Entre le SHA de code B5 et la clôture, `main` a évolué par d'autres PR indépendantes ; une éventuelle CI sur la combinaison ne prouvera pas que cette combinaison est déjà testée matériellement. Ne pas attribuer à B5 un gain RAM/ROM/FPS.

**Suite après fusion de #281** : vérifier le SHA exact de `main` et démarrer un jalon SDL indépendant depuis ce SHA ; ne pas retirer `SDL_ShowMessageBox` sans un chantier spécifique pour le chemin d'erreur fatal.
