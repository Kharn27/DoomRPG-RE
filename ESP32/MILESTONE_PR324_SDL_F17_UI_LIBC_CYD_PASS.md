# SDL-F17 — alias libc natifs menu et texte — PASS classic CYD

**Date :** 12 novembre 2026  
**PR :** [#324](https://github.com/Kharn27/DoomRPG-RE/pull/324)  
**Issue :** [#269](https://github.com/Kharn27/DoomRPG-RE/issues/269)  
**Branche :** `agent/esp32-sdl-f17-native-menu-text-libc`  
**Base exacte main :** `4e2853c859abade9b7f3b1dac4f43522676d685c`  
**Firmware testé :** `9a1ab5ff6cb66bfb30ff7d58cf72bb0f2781672a`  
**Matériel :** `platformio-device-monitor-261112-114500.log`, 2840 lignes, 355154B, SHA256 `ed8656c52b6399074dbcd7267a6bf3a1eb816e7bc56e44c76295d1b963626094`

## Frontière

Quatre unités de production C changées, 11 alias libc SDL retirés :
- `esp_native_text_format.c` : 1 strlen + 1 memcpy ; retire son SDL.h direct, conserve string.h.
- `native_main_menu_model.c` : 1 memset + 1 snprintf ; ajoute string.h explicite.
- `native_main_menu_actions.c` : 2 memset + 2 strlen + 1 snprintf.
- `native_main_menu_touch_layout.c` : 1 memset + 1 snprintf.

Les quatre macros identiques existaient dans `ESP32/include/SDL.h`. SDL_Rect reste la vraie ABI de compatibilité sur les unités qui l'utilisent. Aucun transfert d'ownership, pas de nouvelles allocations, pas de modification du renderer/caches/jeu/saves/PAK/USB/son. `PlatformVideo_present()` et l'animation restent intacts.

## Oracles et CI

Oracle host `ESP32/test/sdl_f17_native_ui_text_libc_host.py` : quatre sources C pré-F17 reconstituées byte-exact et comparées à leurs SHA Git blob historiques : `d8320e5026103573d746a1ee875cf343d379dd9d`, `72a3bb69155e08d335464a5e73017659da3f9666`, `c13c42d3b03f24004e129e8f6a181019d1cf4dcd`, `2234884200da94bd81566f4091e65d2fef640251`. Les vrais corps C old/new du `EspNativeText_buildDivider`, `setFixedItem` et `setNativeMainItem` ont été compilés et exécutés pour vérifier les octets identiques. CI définitive du firmware testé : [push #43126627106](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43126627106) et [PR #43126634827](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43126634827), 151/151 étapes SUCCESS chacun, trois profils + chaîne SDL-F1..F16.

## Rapport Serial réel, hors diagnostics et bringup

Log de 2840 lignes ; commence directement par `ets Jul 29 2019 12:21:46` puis `rst:0x1 (POWERON_RESET)` : RTS reset et armement du fichier avant boot **validés**. ESP32-D0WD-V3 240MHz, flash 4MB, 0 PSRAM ; PAK 241 ressources, microSD prête, 160×120×2B. Options→Video→Luminosité 100→75 (persist SD V3), retour MENU_MAIN exact FNV `e687f0c7`; HELP 112 lignes, cinq PAGE-DOWN + BACK, même retour. Intro entière et `INTRODISP recovered=34080`, Entrée map #1 `/intro.bsp` CRC `623f34e4`, native immutable arena 14095B, FRESH-VISIBLE.

Jeu : 11 dialogues OPEN + 11 CLOSE dont weapon-standalone ; 12 complétions portes ; 29 démarrages et 29 fins de miniatures de pickup ; sept ripostes monster COMMIT et huit combats monster COMMIT, secrets, caisses, pickups, armes. Deux EXIT confirmés sans save write ; un chargement V11 slot04 de 5604B avec ressources, scripts, lignes, toplogie/positions/monstres/activation/drops restaurés, puis RESUME-VISIBLE. Deux RESIDENTRESET exactement `released=18008 empty=1`, deux SYSEXIT MENU-READY FNV `e687f0c7`. Deux réserves RAM HEADROOM_OK : `margin8=27892/27080` et `marginDMA=68852/68040` ; 13 mentions `shapeData=0x0 mediaTexels=0x0`. Aucun Guru, panic, WDT, ERROR, FAILED, abort. Quelques `NATIVEFRAME BOUNDARY-INLINE` déjà signalés et coordonnée projectile Malwrath haute `0x40000000` à investiguer séparément si visuel incorrect : ne pas déclarer le moteur exempt de tous défauts. Firmware SHA non imprimé par Serial.

**Verdict : PASS matériel sur la frontière SDL-F17.** Validé explicitement par utilisateur : « Oui ça marche ».

## Correctif host-only demandé après PASS

`monitorCyd.sh` conservé. Son propriétaire `ESP32/tools/monitor_cyd.py` crée maintenant les captures par défaut dans **`ESP32/logs/`**, indépendamment du dossier courant, via `Path(__file__).resolve().parents[1] / "logs"`. Le log reste ouvert avant le reset RTS et `--log-dir` permet de modifier le répertoire. README, ignore et test CLI host mis à jour. **Aucun octet de production C/CPP modifié après le test.** Comparer la CI finale suite à ce changement avant fusion.

## Prochaine frontière

Relire vrai main et SHA exact post-merge ; auditer SDL.h et les consommateurs réellement compilés, sélectionner une famille sûre, fournir test C/empreintes et attendre le PASS hardware avant toute fusion. Ne pas retirer des types SDL qui restent réellement consommés.
