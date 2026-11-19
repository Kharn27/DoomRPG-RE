# ARCH-RC18 — handoffs PAK/flash et callback tactile sans GNU --wrap : PASS CYD borné

- PR : [#395](https://github.com/Kharn27/DoomRPG-RE/pull/395)
- Base exacte d'origine : `main=95ba27cfd486639f8a03e324aedd380956b21388` (après #393). Code HEAD testé par contexte : `69158c99ce63c24f95de940641bc88b4f81e52ca`.
- Double CI **sur ce code** : [push 44533381922](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44533381922) et [PR 44533391533](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44533391533), **SUCCESS**, 218/218 étapes chacune, quatre firmwares, oracles/ELF.
- Matériel : **classic CYD ESP32-2432S028R**, ESP32-D0WD-V3, 4 MB flash, **sans PSRAM**.
- Capture : `platformio-device-monitor-261119-145206.log`, **2957 lignes, 389231 octets**, SHA-256 `a0a54972b34acf643123a21f37a861995c5fd0d83ff004cbc684d72b72d84ab2`.
- Verdict : **PASS matériel fonctionnel strictement borné**. Le Serial n'imprime ni SHA du firmware ni env PIO exact : attribution au code de #395 par contexte et CI, pas identification binaire indépendante.

## Frontière RC18

Deux GNU `--wrap` supprimés, **12 → 10 en production/normal/inverted/trace ; 15 → 13 en bringup**.

1. `EspAssetPack_residentBegin` : façade publique sous propriétaire de préflight flash `esp_asset_pack_perf_probe.cpp`, et corps de transaction PAK renommé `EspAssetPack_residentBeginBase` dans `esp_asset_pack.cpp` ; maintien de l'ARM map-flash, du teardown cache sur erreur, de la progression et du fail-closed.
2. `PlatformInput_setTapCallback` : façade publique de l'adaptateur menu natif, `PlatformInput_setTapCallbackBase` au pilote XPT2046 ; priorité WAIT_STATS/NULL, release et double-tap MENU_MAIN conservés.

Tests host RC18 compilant les vrais corps C++ de transactions PAK, callback et menu ; branches refus/rollback/gate/passthrough/NULL WAIT_STATS et inventaire ELF 10+3. Aucune modification du PAK, assets, renderer, RNG, entités, framebuffer, sampling tactile, audio ou allocations.

## Témoins matériels observés

1. Boot normal : `PSRAM: 0`, écran logique 160×120, PAK indexé, menu peint. Choix Options/VIDEO/THEME, retour Options puis MENU_MAIN via deux taps ; `MAINBACK READY`, puis NEW GAME/INTRO.
2. INTRO, chargement natif Entrance `/intro.bsp` : `MAPFLASH ARM map=1 active=1 verified=1 reused=1` ; session fraîche visible, chargement terminé, `HEADROOM_OK`.
3. Gameplay puis System HUB EXIT → `RESIDENTRESET ... released=18008 ... empty=1` → `SYSEXIT MENU-READY`. Retour menu opaque puis **LOAD V11** `/sd/DoomRPG-ESP32-slot04.sav`, **5604 octets** ; états ressources/bytecode/lignes/topologie/positions/monstres restaurés, `ENGINESESSION RESUME-VISIBLE`, deuxième `MAPFLASH ARM ... verified=1 reused=1` et deuxième `HEADROOM_OK`. La capture finit en session restaurée active ; aucun deuxième EXIT montré.
4. Sur les deux sessions : **83 `VIEWANIM END`**, **12 `DOORANIM ARM/COMPLETE`**, 6 `HOLD` + 6 `RELEASE` + 6 `POST-MONSTER-COMPLETE`, 4 `SNAP` non réguliers, **7 `DIALOG CLOSE`**, **20 `PLAYERRES COMMIT`**, **60 `MONSTERACTIVESEQ COMPLETE`**, 10 `MOVEEVENT COMMIT`. Les séquences de fermeture différée sont visibles sur le vrai jeu.
5. Audio : **50 `SFXGEN START` et 50 `SFXGEN END`**, 25 `SFXP1 MIX`, **4 `AUDIOINTENT AUDIT`** : `queueOverflow=0 consumerCursorGap=0 audibleSfxDropped=0 pending=0`. Aucun `SFXGEN DROP`, aucun underrun/underflow/recovery non nul détecté.
6. RAM : **2 `RAMBUDGET RESERVE ... advisory=HEADROOM_OK`**, heap8 de jeu ~55–60 KiB observé, plus grand bloc ~40–45 KiB selon phase ; plusieurs `shapeData=0x0 mediaTexels=0x0`. Pas de panic, WDT, assertion fatale, corruption heap ni `DOORANIM FAILED` dans la capture.

## Limites et discipline

- **Aucun `WAIT_STATS` ou `CHANGEMAP` démontré** par cette capture ; la branche NULL WAIT_STATS, le refus/rollback artificiel PAK/flash et la fidélité exacte des callbacks en cas d'erreur relèvent des tests hôtes/CI.
- La capture ne démontre pas une écriture SAVE V11 neuve, une autre carte, un firmware inverted, une panne SD injectée ou une mesure acoustique. Pas de certification universelle du runtime.
- Pendant l'analyse, `main` a avancé avec des commits parallèles audio à partir de la base originale ; vérifier la comparaison/merge GitHub actuel avant fusion, ne pas écraser le travail concurrent.
- **Après PASS matériel, ne committer sur #395 que documentation**. Re-vérifier les CI finales et le contenu de la PR. Après fusion, relire `main` et repartir de son SHA réel pour RC19.

