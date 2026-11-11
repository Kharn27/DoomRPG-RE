# SDL-F4 — locaux de dessin/images typés natifs : PASS hardware classic CYD

- **PR** [#299](https://github.com/Kharn27/DoomRPG-RE/pull/299), **issue** [#269](https://github.com/Kharn27/DoomRPG-RE/issues/269).
- **Base réelle** `main=5898ba058253965d2de45234051131a65e47b525` (fusion SDL-F3 + Root-H).
- **Candidat code testé** `54831f480ef5587f4be87823dbb81fc94b7d97be` (attribution par flash de l'utilisateur ; le log ne transporte pas de SHA de firmware). Premier essai CI F4 échoué sur la vraie dépendance `DoomRPG.h` aux types SDL : corrigé avant le test matériel, sans modifier le comportement.
- **CI** [#43015398875](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43015398875) **SUCCESS** : tests image create/adopt/free/OOM/rollback, oracle différentiel de présentation (7 corps legacy/1600 opérations RGB565), garde F4, garde SDL-F3, tests Root-H, 3 builds normal/bringup/trace, audit compiledb/ELF zéro symbole `SDL_*` lié et shim non compilé.
- **Tailles des trois builds** : normal **859361 B flash / 80768 B RAM statique**, bringup **882069/80912 B**, trace **877485/80808 B** ; *identiques* à la base combinée SDL-F3 + Root-H. Aucune revendication de gain performance/mémoire.

## Frontière pérenne du code

- `esp_doomrpg_presentation.c` : deux variables `SDL_Rect` locales sont devenues **`EspNativeRect`**, structure native 4× int et géométrie identique, transmis directement à `EspNativeTexture_setClipRect` et `EspNativeTexture_fillRect`. Aucun changement raster, ordre de présentation, cadence, framebuffer ni source des textures.
- `esp_doomrpg_image_resources.c` : `SDL_Texture* texture` locale devenue **`EspNativeTexture* texture`**, résultat de l'adoption native indexed 1/4/8-bpp. Même transfert de propriété, `imgBitmap` historique inchangé, même libération `EspNativeTexture_destroy` et accounting.
- Les deux adaptateurs n'incluent plus `SDL_Video.h`. Ils conservent **explicitement `<SDL.h>` comme déclaration de types transitoire**, car `DoomRPG.h` utilise encore `SDL_Texture`, `SDL_RWops` et `Uint8`. Aucun appel SDL actif ou shim compilé. La séparation de l'ABI d'en-tête constitue une prochaine frontière indépendante ; ne pas revendiquer « SDL.h entièrement retiré ».
- `DoomRPG.h`, `Image_t`, les tests legacy et les alias du header restent inchangés ; le backing store est le PAK natif, pas le ZIP runtime. `shapeData == NULL` et `mediaTexels == NULL` restent invariants.

## Serial matériel `device-monitor-261111-131641.log`

**1634 lignes / 190105 octets**, capture commence par deux `ALIVE` puis vrai `POWERON_RESET`. Chip **ESP32-D0WD-V3**, 240 MHz, flash 4 MB, **PSRAM 0**, écran 320×240, framebuffer logique unique **160×120 RGB565 / 38400 B**, sortie nearest-neighbor ×2, PAK **241 entrées**. Structure `Render=108` observée. Aucun message `[SDL] CYD display attached`.

- **Menu/paramètres** : OPTIONS → VIDEO → thème **CLASSIC 0 → PHOSPHORE 2**, transaction config v3 réussie en SD (orientation NORMAL), retour au MAIN FNV désormais `3721ecf7` en thème 2. HELP 112 lignes paginées 0→7→14→21→28→35→42→49→56 ; retour au même menu.
- **Intro** : 9 décodages et 9 adoptions `NATIVEIMAGE` indexed 4-bpp du démarrage, menu, aide et intro ; INTRO7 taps, libération **34080 B** ; BSP Entrance `/intro.bsp`, CRC `623f34e4`, arène immutable **14095 B**, 2 sessions : `ENGINESESSION FRESH-VISIBLE` puis `RESUME-VISIBLE`.
- **Jeu** : **9 `[DIALOG] OPEN` dont un `OPEN-STANDALONE` d'aide Fire Ext, et 9 CLOSE** ; vrais opcodes 8 et 26, continuation `EV_NEXTSTATE`; **3 `DOORANIM COMPLETE`**, dont fermeture différée après tour monstre ; 2 caisses cassées/transformées au total durant première session ; **8 `PLAYERRES COMMIT`**, **8 `PICKUPMINI START` et 8 END**, avec `input=unblocked` ; mouvements/rotations/strafe, affichage HUD.
- **Premier retour menu** : HUB → système → EXIT confirmé, **`RESIDENTRESET released=18008 ... empty=1`**, `SYSEXIT MENU-READY frame=3721ecf7`. Rechargement depuis `LOAD`, checkpoint **V11 slot04 5604 B**, restauration exacte ressources/scripts/lignes/automap/monstres/topologie/positions/activation/drops selon le log, puis affichage `RESUME-VISIBLE`.
- **Deuxième retour menu : chemin mort**, non deuxième EXIT utilisateur. Trois `[MONSTERRETAL] COMMIT` Malwrath (sprite 322, 3 coups par salve), puis `PLAYERDEATH ARM`, fall/fade et `DEATHMENU READY`, sélection **MAIN** depuis l'écran de mort. **Second `RESIDENTRESET released=18008 empty=1`** et second `SYSEXIT MENU-READY frame=3721ecf7`.
- **RAM** : `2× [RAMBUDGET] RESERVE ... HEADROOM_OK`, marges `heap8=28232/27420 B` et `DMA=69192/68380 B`; `largest8=61428` en jeu. `shapeData=0x0 mediaTexels=0x0` aux **13 frontières conjointes** observées. Menu final `heap8=130700`, `largest8=86004`, dernier `ALIVE` stable.
- **Anomalies connues** : **4× `[NATIVEFRAME] BOUNDARY-INLINE`**, non fatales sur ce parcours ; **aucune occurrence** panic/Guru/WDT/abort/Backtrace/heap corruption. Pas de mesure comparative FPS ni exercice matériel exhaustif des trois formats indexed 1/4/8-bpp (les oracles hôte les couvrent), orientation inverse non exercée.

## Verdict

**PASS matériel ciblé SDL-F4**, observation du vrai classic CYD conforme aux oracles. **Code gelé après test**, clôture **documentation seulement** ; fusion autorisée explicitement par l'utilisateur. Étape suivante à choisir après relecture de `main` : supprimer progressivement les *dépendances de types SDL résiduelles dans les prototypes natifs* ou une autre petite famille, sans modifier le legacy desktop ni élargir le runtime.
