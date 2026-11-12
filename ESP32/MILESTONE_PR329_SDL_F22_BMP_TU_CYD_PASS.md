# SDL-F22 — sortie du vieux décodeur BMP SDL_RWops du graphe de build : PASS classic CYD

## Frontière et empreinte de provenance

- PR : https://github.com/Kharn27/DoomRPG-RE/pull/329 ; suivi : https://github.com/Kharn27/DoomRPG-RE/issues/269
- Base exacte après fusion F21 : `c848bfcfe6d75c20342450f8103984d50933eb62`.
- **Code/build effectivement soumis au CYD :** `5f238ae15b43278e9b215e3347fd5e1192b3f0af`. Premier commit F22 `e64ac6159d5c5cdde53e96cf42b7f3c8172d9433` puis correction strictement Python du test F3 pour accepter l'ajout de la deuxième exclusion.
- Preuve d'inutilité : CI SDL-F21 compilait réellement `esp32_bmp.cpp.o` trois fois, mais les ELFs n'incluaient pas `Esp32Bmp_LoadRW` ni `SDL_*`. Le lecteur de production est `EspNativeIndexedBmp_open` via `EspAssetPack_readRange`, puis `EspNativeImage_loadPack` et `EspNativeTexture_adoptIndexed`.
- **Seule frontière de compilation modifiée :** `ESP32/platformio.ini`, `-<esp32_bmp.cpp>` après `-<esp32_sdl.cpp>`. Les trois profils héritent du filtre. L'ancien `ESP32/src/esp32_bmp.cpp` et `ESP32/include/esp32_bmp.h` restent exacts pour les tests SDL historiques ; vrais Git blobs verrouillés `88a62dfdd3d4fbb2c991dba4d480d4596834563c` et `f896aa522c6c1cbf3d38852cadc086331f98405a`.
- Nouveau `sdl_f22_bmp_tu_retirement_host.py` : blobs Git exacts + zéro appel restant, implémentation native présente, puis `compile_commands.json`, fichier `.o` absent, `nm` ELF sur normal/bringup/trace. Test F3 source-filter ajusté strictement, pipeline inchangé, aucun code runtime C/C++ modifié.

## CI pré-matériel : les deux runs concluants

- https://github.com/Kharn27/DoomRPG-RE/actions/runs/43268208153 — **SUCCESS 157/157**.
- https://github.com/Kharn27/DoomRPG-RE/actions/runs/43268222532 — **SUCCESS 157/157**.
- Trois firmwares PlatformIO + 3 compiledb/ELF ; 0 objet `esp32_bmp.cpp.o`, 0 objet `esp32_sdl.cpp.o`, 0 `Esp32Bmp_LoadRW` lié, 0 fonction `SDL_*` liée, `EspNativeIndexedBmp_open` bien lié. Tests historiques C/host et F22 PASS.
- Flash CI : normal **859693 B**, bringup **882477 B**, trace **877845 B**, chaque profil **−64 B** vs F21. RAM statique respectivement **80736 B**, **80880 B**, **80776 B**, identique à F21 ; ne pas interpréter cela comme un gain de heap dynamique.

## Vérité matérielle — log Serial normal

- `platformio-device-monitor-261112-202701.log`, 1628 lignes, 191655 octets, SHA256 `835bfb8292bf9231335b904e9f912fa3e2fbd979287311451186108ce9daa31e`.
- Capture ROM `rst:0x1 POWERON_RESET` unique, ESP32-D0WD-V3 dual core 240 MHz, flash 4194304 B, PSRAM=0, SD et PAK prêts/241 entrées ; framebuffer 160×120×2 RGB565, physical 320×240. Aucun reboot secondaire.
- HELP 112 lignes, PAGE-DOWN×4 et BACK ; LOAD menu sélecteur page 2 et BACK ; START puis intro complète, `INTRODISP recovered=34116`, carte `/intro.bsp` Entrance (CRC `623f34e4`), mouvement et turn, 4 animations porte complètes, caisse transformée/pickups/arme extincteur, dialogues EV_DIALOG 8 et EV_DIALOGNOBACK 26 avec reprise NEXTSTATE 19, HUB SYSTEM EXIT sans SAVE, LOAD checkpoint V11 slot04 **5604 B** avec restauration des états map/players/scripts/lines/monsters/topology/position/activation/drops et second HUB EXIT.
- **4 DIALOG OPEN et 4 CLOSE**, dont un dialogue autonome ; **12 PICKUPMINI START/END** ; **4 DOORANIM COMPLETE** ; **3 MONSTERCOMBAT COMMIT** sur Malwrath sprite322 avec mort `hp=3->0`, **2 MONSTERRETAL COMMIT** avec animations projectiles, effets de douleur et RNG guards. **MONSTERDROP COMMIT** une fois `def=92 tile=284 visible=1` (état natif), suivi `RENDER-CULL reason=player-tile` ; **pas de preuve que ce drop ait été visible sur l'écran**.
- Mémoire : deux `HEADROOM_OK`, marge heap8 **28092 B** en nouvelle partie, **27280 B** après LOAD ; `largest8=61428 B` pendant gameplay. **2× `RESIDENTRESET heap8=112552->130560 released=18008 empty=1`** et **2× `SYSEXIT MENU-READY frame=4c01eb2b`** ; **2× `ENGINESESSION READY`** ; `shapeData=0x0 mediaTexels=0x0` à chacune des neuf frontières qui l'impriment. En fin de capture, `[ALIVE] uptime=329580 ms heap8=130560 largest8=86004`, menu stable.
- Aucune trace `panic`, Guru Meditation, watchdog, `OOM` en tant qu'erreur, `fail=SPRITES`, `SETTLE-FAILED` ou `DEATH-REDRAW-FAILED`.
- **Diagnostics préexistants** hors périmètre SDL-F22 : 6× `[NATIVEFRAME] BOUNDARY-INLINE`, 18× `[MALWRATHPROJECTILE] FRAME` affichant des coordonnées x de type `1073743xxx` (bit haut), sans crash déduit.
- **Limites** : aucun passage OPTIONS, aucune NOTE exécutée, ni rare drop `defTile=100`/`CATALOG-PRIME`, ni SAVE neuf, ni audio validé dans ce log. Le comportement démontré couvre ces scènes et pas toutes les cartes.

## Décision

Le log sur le vrai matériel **valide la non-régression du retrait de TU BMP SDL RWops** pour les parcours observés. Le firmware code/build testé est gelé, seuls des fichiers Markdown sont ajoutés après PASS. Vérifier le nouveau CI post-doc ; fusion **autorisée explicitement** par l'utilisateur, puis prendre le SHA réel du `main` fusionné avant tout SDL-F23. SDL.h et typedefs historiques encore présents dans des entêtes ne sont pas automatiquement des dépendances SDL liées.
