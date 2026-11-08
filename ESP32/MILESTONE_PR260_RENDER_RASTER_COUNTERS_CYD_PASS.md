# PR #260-B — retrait des compteurs de raster : PASS matériel ciblé classic CYD

**8 novembre 2026** ; branche unique `agent/esp32-render-retire-desktop-screen-origin`, [PR #260](https://github.com/Kharn27/DoomRPG-RE/pull/260). **Commit code éprouvé :** `b323fee6213f7195370a66bd68ebbd591df0e593`, parent `09e4f2f9b3394173259ae2df219d818c203e612e`. CI hôte et firmwares normal/bringup/trace : [#42430844225](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42430844225), **SUCCESS** (97/97 étapes du job, aucun échec).

## Frontière

Les compteurs desktop `Render_t::lineRasterCount` et `nodeRasterCount` sont retirés des trois profils ESP32 et des snapshots privés de `esp_native_first_frame.c`, `esp_native_bsp_visibility.c`, `esp_native_sprite_renderer.c`. Ils restent déclarés sous garde desktop/oracle `DOOMRPG_ESP32_GEOMETRY_ORACLE`. `lineCount` et `nodeCount`, les opérateurs de projection et le clipping demeurent inchangés. `sizeof(Render_t)` **176 → 168 octets** ; gain de **8 octets de structure**, pas preuve d'un gain équivalent de heap. Pas de modification du PAK, des buffers média, du gameplay, des sprites, du RNG ni des animations.

## Capture et preuve

Fichier fourni `device-monitor-261108-014743.log` : **1 765 lignes / 215 163 octets**, démarrage normal `POWERON_RESET` (le log commence par un `[ALIVE]` antérieur), ESP32-D0WD-V3 / 240 MHz, 4 MB flash, aucune PSRAM. `Engine structs: Render=168 Game=0 Canvas=44 Total=972 bytes`; `[RENDERCORE] INIT ... bytes=168` ; `[RENDERCORE] SETUP ... view=160x80@0,20 arrays=1280B`; framebuffer unique `160x120 RGB565 / 38400 B`, PAK 241 entrées.

**START neuf** → intro/Entrance : première frame `71ca7465` identique au précédent témoin #260-A ; mouvements, rotations/strafe, une porte animée 275 et fermeture différée, caisses/ressources, dialogues 26 et 8, reprise de script avec mutation d'état, HUB puis EXIT. **LOAD V11 slot04** → Entrance, restitution checkpoint de ressources/script/topologie et monstres ; première interaction, HUB/SYSTEM/LOAD puis retour HUB avec `[HUB] CLOSE n=1 exactHud=yes exactBottom=yes menuUnderlayRestore=exact` ; porte113, Malwrath sprite322, trois ripostes complètes avec animation/projectile et commits non létaux (HP 34→30→23→17), trois `PLAYERPAINFX FIRST-PRESENT` puis trois `EXPIRE`, trois `MONSTERCOMBAT COMMIT` (dernier létal hp11→7→3→0), DROP et exploration, second `[HUB] CLOSE n=2 exactHud=yes exactBottom=yes`. Deux `RESIDENTRESET empty=1` et deux `SYSEXIT MENU-READY frame=6221bb63`. 

**Mémoire :** les deux `[RAMBUDGET] RESERVE` = `HEADROOM_OK`, avec `margin8=53248` et `52432` ; gameplay `[ALIVE] heap8=109780` puis `109776` et `109736`, `largest8=86004`; retour menu `heap8=155720 largest8=110580`. `shapeData=0x0 mediaTexels=0x0` aux deux entrées gameplay et teardown ; `PSRAM=0`. Aucun `Guru Meditation`, panic, WDT, `FAILED` ou `ERROR` dans la capture. L'intention audio demeure `backend=silent`.

**Décision : PASS matériel fonctionnel ciblé pour `b323fee6213f7195370a66bd68ebbd591df0e593`** et clôture de la sous-frontière #260-B. Pas de photographie TFT ni comparaison pixel-à-pixel complète, pas d'A/B de performance, pas de preuve d'un SHA embarqué dans le fichier Serial, pas de validation de tous les BSP/monstres/audio. Premier reset `POWERON_RESET` non attribué à un watchdog.

**Suite demandée par l'utilisateur :** poursuivre un nouveau retrait borné **sur la même branche #260**, avec son propre commit de code, sa propre CI et son propre essai CYD. Le présent PASS ne vaut pas validation automatique du code suivant. Aucune fusion dans `main` par l'agent.
