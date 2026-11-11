# Root-I — retrait du back-pointer Canvas → DoomRPG : PASS classic CYD

- **PR** : https://github.com/Kharn27/DoomRPG-RE/pull/298
- **Candidat Root-I testé** : `cd2d8c24d2c25dce2bd0ae7a623472fb9671fadc`, basé sur `main=5898ba058253965d2de45234051131a65e47b525` (SDL-F3). Attribution au flash utilisateur ; **aucun SHA firmware n'est imprimé dans le Serial**.
- **CI du candidat matériel** : [push #43017003705](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43017003705), [PR #43017011943](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43017011943) — **SUCCESS**.
- **Log CYD** : `device-monitor-261111-150840.log`, **1704 lignes / 196811 octets** ; vrai `POWERON_RESET`, ESP32-D0WD-V3, 4 Mo de flash, **PSRAM=0**, PAK natif indexé **241 entrées**.
- **Intégration après SDL-F4 #299** : `main=2638820fdd1f4b8ea087690d5ffb450d90be63d4` ; branche Root-I reconstruite sur ce SHA en `0f2cec89b3345998375d39644aa727853e4ed481` avec les **20 blobs code/test Root-I identiques** au candidat testé et les **deux gardes CI** Root-I + SDL-F4 dans le workflow. **Le firmware combiné n'est pas retesté matériellement par ce log** ; exiger la CI d'intégration avant merge.

## Frontière technique

Sur les builds `esp32-cyd`, `esp32-cyd-bringup` et `esp32-cyd-trace`, `DoomCanvas_t::doomRpg` est retiré du layout. Le champ reste disponible uniquement desktop/oracle. Le constructeur natif de Canvas est sans root, tandis que le startup, la libération (story hand → intro → font → canvas), les transitions MENU/INTRO et `Esp32StoryFit_prepare/draw/release` reçoivent le `DoomRPG_t*` explicitement à la frontière de leur propriétaire. Le canvas ne mémorise plus un retour vers le processus.

Le vrai Serial confirme `Engine structs: Render=108 Game=0 Canvas=36 Total=641 bytes` contre **Canvas=40 B** avant Root-I, soit **−4 B par instance** ; aucune économie équivalente de RAM statique n'est déduite. La racine globale identitaire `DoomRPG_t` et `doomRpg` restent présentes. Aucun changement de `shapeData`, `mediaTexels`, framebuffer 160×120 RGB565/nearest ×2, `PlatformVideo_present()`, backing store PAK ou gameplay.

## Verrous CI du code testé

- `root_i_canvas_backref_host.py` : absence du champ sur **3 profils ESP32**, conservation desktop/oracle sur **2 profils**, scan de **360 sources**, vérification des chemins root explicites.
- Fixtures de constructeur/libération/teardown : **4096 cycles** chacun, allocation/OOM/fail-closed, ordre de destruction avec racine explicite, double root et idempotence. Fingerprints de constructeur, layout, Menu et StoryFit conservés sous normalisation documentée dans la PR.
- Builds PlatformIO normal/bringup/trace, tests hôte, audit compiledb/ELF, `esp32_sdl.cpp` non compilé et **0 symbole SDL lié**.
- Candidat Root-I+SDL-F3 : normal **859457 B flash / 80768 B RAM statique**, bringup **882161/80912 B**, trace **877577/80808 B**. Mesures de compilation, **pas** benchmark RAM dynamique ni A/B. Intégration SDL-F4 recontrôlée séparément en CI.

## Parcours matériel constaté

1. **Boot et layout** : `PSRAM: 0 bytes`, framebuffer 38400 B partagé, `Render=108 Game=0 Canvas=36 Total=641`, `DoomRPG used=0`, `CORE READY allocatedObjects=3 used=788`. Aucun legacy `Game_t` recréé.
2. **Menus et HELP** : carte OPTIONS, retour natif MENU, HELP 112 lignes et quatre PAGE-DOWN jusqu'au scroll 28, fond `c.bmp`, preview PAK, retour MENU_MAIN avec FNV **3721ecf7**, touch réarmé.
3. **INTRO native** : START, hand `p.bmp`, chargement images 4-bpp, animation clock 50 ms (56 frames rendues), texte `REVEAL/MORE`, progression des pages, `FINAL-CONTINUE`, puis `INTRODISP` récupère **34116 B** et laisse le monde legacy absent. Pas de régression observable dans les transitions du root explicite.
4. **Entrance** : `/intro.bsp` CRC32 **623f34e4**, arène immutable **14095 B**, `ENGINESESSION FRESH-VISIBLE`, framebuffer et HUD ; `9 NATIVEIMAGE DECODE` et `9 ADOPT` au total.
5. **Gameplay** : **6 `[DIALOG] OPEN` / 6 `CLOSE`**, script resume après dialogues, **4 `DOORANIM COMPLETE`**, 2 caisses transformées et pickups, **12 `PICKUPMINI START` / 12 `END`** sur les deux sessions (miniatures non bloquantes). Déplacements, armes, HELP standalone, HUB.
6. **Combat** : reprise sauvegarde V11 `slot04` **5604 B**, 3 ripostes du Malwrath sprite 322 (`MONSTERRETAL COMMIT`), chacune après animation et `MALWRATHPROJECTILE CLEAR`, HP/armor modifiés ; le gameplay continue.
7. **Cycle mémoire complet** : premier HUB→EXIT puis LOAD V11→deuxième HUB→EXIT. **2× `RESIDENTRESET released=18008 empty=1`**, **2× `SYSEXIT MENU-READY frame=3721ecf7`** ; aucune écriture de sauvegarde parasite, checkpoint inchangé.
8. **Budget** : **2× `HEADROOM_OK`** ; marges heap8 **28436 / 27624 B**, DMA **69396 / 68584 B** ; plus grand bloc heap8 gameplay **61428 B**. `shapeData=0x0 mediaTexels=0x0` à **13 frontières** relevées. **7 `NATIVEFRAME BOUNDARY-INLINE`** diagnostics connus non fatals. Aucun panic, Guru Meditation, watchdog, abort ni erreur d'allocation critique dans la capture.

## Verdict, limites et suite

**Root-I : PASS matériel ciblé** avec preuve directe de **Canvas=36 B** et exercice de ses principaux chemins de durée de vie (menu, intro/StoryFit, destruction et rechargement). Limites : pas de SHA firmware dans le Serial, pas de campagne OOM/fatal ni de test exhaustif de toutes les cartes, ni de benchmark A/B. La combinaison SDL-F4 #299 repose sur les deux PASS matériels séparés et nécessite **une CI d'intégration verte** avant fusion ; ne pas lui inventer de nouveau PASS matériel.

**Code Root-I gelé après test** : seul ajout documentaire autorisé. Après merge, repartir du SHA exact de `main` ; auditer séparément le singleton identitaire `DoomRPG_t`, le global `doomRpg` et les consommateurs ABI sans supprimer cette racine prématurément. Les branches SDL-F5 et AUDIO-A sont parallèles et ne doivent pas être écrasées.
