# ARCH-RC16 — transactions de mouvement : PASS matériel CYD borné

**PR #389**, 19 novembre 2026. Base initiale `71b912ffd98e3a11d9dd31a59153a1648e35be77`, base réintégrée avant test `73520c22afa0e849807cd75de3540d9a5f826d45`. Code validé par double CI `66702c1751fd0922c3a372adf5223610b3b64374` ; le Serial ne contient pas le SHA binaire ou l'environnement PIO, donc couplage au commit par contexte utilisateur.

## Frontière

Deux GNU `--wrap` retirés : `EspNativeGameplayDispatch_commitMove` et `EspNativeGameplayDispatch_rollbackMove`. Façades publiques dans `esp_native_gameplay_move_events.c` composant le préflight EXIT/ENTER, les effets, les feuilles position `commitMoveBase/rollbackMoveBase`, le rollback et les leases de dialogue/message. La mutation de position reste dans le dispatcher ; `EspPlayerView_commitPreparedMove` n'a pas été retiré (overlay ramassages). Aucune nouvelle allocation/map-wide, audio, RNG, ZIP runtime, renderer ou sauvegarde.

**Inventaire** : 16→14 wraps production/inverted/trace et 19→17 bringup. **Quatre ELF SUCCESS**, oracle 4 vraies fonctions C + 15 mutants adversariaux rejetés, anciens oracles SHA inchangés. **CI code push** [#44437515671](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44437515671) **SUCCESS** ; **CI code PR** [#44437523419](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44437523419) **SUCCESS**.

## Hardware physique

`platformio-device-monitor-261119-123629.log` : **3 701 lignes, 479 904 octets, SHA256 `a5d3e545f0bf33565d55dd94e25f6526eddda959a94b101e5ff3be6b1e8beaf1`**. ESP32-D0WD-V3 240 MHz, 4MB flash, PSRAM 0, framebuffer 160×120 RGB565 38400B, PAK natif.

NEW GAME Entrance → déplacements/portes/dialogues/ramassages → HUB System EXIT → LOAD V11 slot04 (5604B) → gameplay/combats → décès joueur réel → MAIN/menu. **83/83 MOVE committés** (FORWARD, BACK, STRAFE), 107 VIEWANIM END, 11 MOVEEVENT COMMIT dont 1 entrée dans un dialogue EV_DIALOGNOBACK opcode26 avec WORLD-READY, 14 DOORANIM ARM/COMPLETE, 7 HOLD/RELEASE/POST-MONSTER-COMPLETE, 26 PLAYERRES COMMIT, 10 DIALOG CLOSE, 2 HEADROOM_OK, 2 RESIDENTRESET empty=1 et 2 SYSEXIT MENU-READY resident=empty. `shapeData=0x0 mediaTexels=0x0`. Aucun panic, WDT, FAILED ou crash dans la capture. La mort combattue n'est pas une anomalie.

**Limites** : aucun rollback forcé/fautif injecté sur hardware (mutants validés host seulement), pas de CHANGEMAP ni d'inverted, ni nouvelle écriture SAVE. PASS de non-régression borné, pas preuve exhaustive.

**Audio hors périmètre :** 99 `[AUDIOINTENT] OVERFLOW` observés dans ce log RC16. L'issue [#377](https://github.com/Kharn27/DoomRPG-RE/issues/377) préexistait ; PR audio #390 a ensuite été fusionnée sur main. Ce log antérieur ne valide ni n'invalide #390 ; ne pas prétendre absence de perte audible. Les autres questions audio/affichage restent dans leurs chantiers séparés.

Après test, cette mise à jour de rapport et d'index est **docs-only**, en attendant la double CI documentaire puis la fusion explicitement autorisée.
