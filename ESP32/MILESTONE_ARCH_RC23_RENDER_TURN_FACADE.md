# ARCH-RC23 — façade native renderTurn, dernier wrapper cardinal éliminé — PASS CYD borné

Date : 21 novembre 2026. PR [#410](https://github.com/Kharn27/DoomRPG-RE/pull/410). Base initiale : `main 6dc19d1c25f660cbacbd17046b196c2c9a2ff2b2`. HEAD code validé en double CI : `6c81d2b04033be3be35950f25373f20a74f5bcf6`.

## Frontière architecturale

L'ancienne interception GNU `--wrap=EspNativeGameplayFrame_renderTurn` a été retirée. L'ancien corps de rendu direct est la feuille explicite `EspNativeGameplayFrame_renderTurnBase`, appelée par le rendu scoped dynamique ; `EspNativeGameplayFrame_renderTurn` devient la façade publique de transaction (HOLD/RELEASE, portes, MOVE/ENTER, SFX et annulations). Aucun ajout de grosse allocation, de lecture ZIP, de chemin `shapeData` ni `mediaTexels`. **2→1 wrapper de production** : `Esp32PlatformVideo_present` reste à traiter en RC24 ; bringup = 4 (1 production + 3 diagnostics).

Oracle `native_frame_turn_boundary_host.py` : véritables corps C compilés, **20 mutants adversariaux rejetés**. Inventaire `native_wrap_linkage_inventory_host.py` : 4 vrais ELF, cardinal direct et symboles GNU résiduels attendus. Rétrocompatibilité SDL-F27/SDL-F28 assurée par transformation strictement inverse des seuls symboles d'entrée, empreintes historiques conservées.

Double CI code sur HEAD : [push SUCCESS #44899400332](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44899400332) et [PR SUCCESS #44899406314](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44899406314), incluant les profils PlatformIO normal/inverted/bringup/trace. Ces CI précèdent la documentation matérielle ci-dessous.

## Preuve hardware réelle bornée

`platformio-device-monitor-261121-122913.log` : **3296 lignes**, **428638 octets**, SHA256 `cd81e3d52d59e1bfdecd9824d1aabd6c14b263a707cbb78fe431975a23eea672`. Classic CYD `ESP32-D0WD-V3`, **4 Mo flash, 0 PSRAM**, logique RGB565 160x120 et écran 320x240 x2. Attribution de la branche firmware **par contexte de test uniquement** (pas de SHA binaire ni env sérialisés dans Serial).

Scénario : Options/Sound/Video + Back → NEW GAME / INTRO / Entrance → gameplay réel avec **118 `VIEWANIM END`**, **19 `DOORANIM COMPLETE`**, **10 `DOORANIM HOLD`** (9 `RELEASE` explicites), **14 `PLAYERRES COMMIT`**, **7 `DIALOG CLOSE`**, **4 `MONSTERCOMBAT COMMIT`**, **6 `MONSTERRETAL COMMIT`**, **5 `MONSTER3GOAL COMMIT`** → HUB/System EXIT → LOAD V11 slot04, 5604 octets, reprise → second EXIT. Pas de panic, Guru Meditation ni WDT dans ce log.

Deux `[RAMBUDGET] RESERVE ... HEADROOM_OK` avec `margin8=17736` et `16476`. Deux `[RESIDENTRESET] ... empty=1`, deux `[SYSEXIT] MENU-READY`. `shapeData=0x0 mediaTexels=0x0`. Les cinq `[AUDIOINTENT] AUDIT` indiquent zéro overflow, gap de curseur et SFX audible perdu ; cinq `[ENTRANCEMUSIC] FINAL` montrent `underruns=0 recoveries=0`. Une `gapsOver90us=1` apparaît lors de la dernière musique : préservée comme observation, **sans underrun**.

Limites : ce n'est pas une certification exhaustive de la campagne ni une preuve de chaque fault path ; pas de nouveau SAVE complet, d'épreuve hardware inverted, d'identité binaire cryptographique ni d'injection de panne. L'issue [#371](https://github.com/Kharn27/DoomRPG-RE/issues/371) reste distincte.

## Discipline de fusion

Après ce PASS, **commits documents uniquement** ; revérifier la double CI du HEAD final puis fusionner sur le `main` courant uniquement si GitHub juge la PR fusionnable. RC24 traitera séparément le dernier wrapper `Esp32PlatformVideo_present` et les transactions écran/menu/transition.
