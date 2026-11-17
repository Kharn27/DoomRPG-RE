# ARCH-RC6 — reset input natif, sans --wrap, ordre transition-first : PASS CYD

**17 novembre 2026 — [PR #367](https://github.com/Kharn27/DoomRPG-RE/pull/367), [issue #340](https://github.com/Kharn27/DoomRPG-RE/issues/340).** Base exacte de `main` après ARCH-RC5 [#366](MILESTONE_PR366_ARCH_RC5_HUD_DIRECTION_POSTHOOK_CYD_PASS.md) : `70de375aa7cacb0650ff5bb8001f904b8a053e27`. Branche `agent/esp32-arch-rc6-input-reset-hook`. Code et tests soumis au vrai CYD : **`6a2c7a2a477cd9255ba17f1dd5ee09e44dd14089`**. Le firmware n'affiche pas le SHA Git du binaire : attribution au HEAD soumise via branche/validation utilisateur, sans attestation matérielle cryptographique.

## Frontière et contrat

Une unique interposition GNU ld retirée : `-Wl,--wrap=EspNativeGameplayInput_reset`. L'ancien `__wrap_EspNativeGameplayInput_reset()` du propriétaire de transition appelait `EspNativeGameplayTransition_reset()` **avant** `__real_EspNativeGameplayInput_reset()`. La fonction publique `EspNativeGameplayInput_reset()` possède désormais ce chaînage explicite, en conservant l'ordre (reset de transition incluant census DEBUG, puis `memset(inputState)`). En firmware `ARDUINO_ARCH_ESP32`, dépendance **forte** au module transition, impossible à omettre silencieusement au lien ; seuls les petits oracles hôte qui compilent l'input isolément utilisent une déclaration faible avec test de disponibilité. Aucun nouvel objet ou file d'attente, allocation, comportement de transition, rendu, PAK, flash, écran, ZIP, audio, framebuffer ou RNG.

Les autres interpositions et frontières complexes, dont le présentateur `Esp32PlatformVideo_present`, `EspAssetPack_residentBegin`, `PlatformInput_setTapCallback` et la topologie/animation, sont inchangées.

## Oracle hôte, CI et inventaire vrais ELFs

L'oracle `ESP32/test/native_input_reset_boundary_host.py` extrait le **corps C réel** d'input reset, inspecte le reset réel de transition, vérifie trois scénarios de liaison (firmware fort, hôte avec transition, hôte sans transition) avec trois époques de reset chacun, l'ordre transition-first et **7 mutants adversariaux** refusés. Les avertissements GCC `-Waddress` du double *défini dans la même TU* sont isolés uniquement dans ce harnais ; `-Werror` reste actif. Ce n'est pas un test matériel ESP32.

Double CI **SUCCESS** sur ce code exact :
- [push 43995807158](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43995807158)
- [PR 43995814330](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43995814330)

Analyse des **vrais quatre ELFs Xtensa** (déclaré / lié / appel direct témoin ; ce dernier ne prouve pas une exécution matérielle) : `esp32-cyd 40/40/40`, `esp-cyd-inverted 40/40/40`, `esp32-cyd-bringup 43/43/43`, `esp32-cyd-trace 40/40/40`. Release normal et inverted ont un moteur compilé et un inventaire identiques (hors profil LCD).

## Test utilisateur sur le vrai classic CYD — PASS RC6 borné

Capture intégrale `platformio-device-monitor-261117-025241.log`, **1481 lignes, 204465 octets**, SHA-256 `10c25dab4a494307119ce3fe17171131bbd66aadb68c87612474030a3199852a`, classic ESP32-D0WD-V3 240 MHz / 4 MiB flash / **0 PSRAM**, framebuffer 160×120 RGB565 38400B rendu x2, profil `[VIDEO] ... alternate-panel inversion=off` compatible **`esp-cyd-inverted`** (le log n'imprime pas le nom du profil de compilation).

**Session 1 — nouvelle partie Entrance** : intro complète, `M_IN_CIT` arrêté avant load ; BSP `/intro.bsp`, PAK flash `MAPFLASH REUSE HIT`, chargement `TRANSITIONLOAD RELEASE progress=100 frames=8 elapsedMs=5466`, `LOADAUDIO-L1 PRESTART ready=1 beforeWorldPresent=1`, `ENGINESESSION READY map=1 angle=64 shapeData=mediaTexels=0x0`. Mouvements et rotations 64→0→64→128 `committed=yes`, caisse transformée, pickups de shards, dialogue opcode 8 avec reprise opcode 19, HUB inventory/system, confirmation EXIT. `RESIDENTRESET heap8=88192->106200 released=18008 before=1/1/1/1/1/1/1 after=0/0/0/0/0/0/0 empty=1`, puis `SYSEXIT MENU-READY resident=empty saveWrite=no checkpoint=unchanged`.

**Session 2 — LOAD V11 slot04** : `/DoomRPG-ESP32-slot04.sav`, version 11, 5604 octets, map=1 Entrance, compteurs sprites/monstres/positions/activation/loot restorés, flash `REUSE HIT`, chargement `TRANSITIONLOAD RELEASE progress=100 frames=8 elapsedMs=4153`, `LOADAUDIO-L1 PRESTART ready=1 beforeWorldPresent=1`, `ENGINESESSION READY map=1 angle=0 shapeData=mediaTexels=0x0`. Rotation 0→64 et nombreux déplacements, change weapon, ouverture de porte et de porte secrète en lot, attaque de Malwrath avec projectile, impacts et retal player, pickups et SFX, HUB EXIT. `RESIDENTRESET heap8=88204->106212 released=18008 before=1/1/1/1/1/1/1 after=0/0/0/0/0/0/0 empty=1`, puis second `SYSEXIT MENU-READY resident=empty saveWrite=no checkpoint=unchanged`.

Deux `RAMBUDGET RESERVE ... HEADROOM_OK` ; les échantillons `[ALIVE]` indiquent au moins 59564 B heap8 pendant la première session et 60236 B pendant la seconde (ces minima échantillonnés ne sont pas une mesure absolue du pic instantané). Audio `M_STALKS` après chaque `SESSION_EXIT` : **436283** puis **571365** frames, **0 underrun, 0 recovery, 0 gapsOver90us**, `timer=OFF dacPad=OFF file=CLOSED`, `heap8After=127052 largest8=86004` dans les deux cas. Intro audio : **112445 frames**, zéro underrun/recovery. Pas de `PLAYERHUD REFRESH-FAILED`, Guru Meditation, panic, WDT, stack canary, assertion fatale ni marqueur générique `[... ] FAILED` dans les 1481 lignes.

**Portée :** PASS matériel RC6 dans les deux chemins START et LOAD V11 suivis de EXIT avec réinitialisation effective des séquences d'intention (`tap=1` à chacune). Pas de preuve qu'un événement SAVEGAME→CHANGEMAP/WAIT_STATS se déclenche dans la capture, ni d'essai d'échec du reset de transition, de flash MISS/re-stage, d'une autre carte ou de golden pixel full-frame. Rendu, audio et gameplay évoluent normalement dans le périmètre observé, sans prétendre à une parité complète.

## Cadence post RC6

Après ce test, **documentation uniquement**, vérifier la comparaison Git depuis `6a2c7a2a477cd9255ba17f1dd5ee09e44dd14089` et double CI HEAD doc avant merge. Pour les prochains milestones ARCH-RC7+, passer de la règle **un wrapper par PR** à **plusieurs wrappers d'un même propriétaire / même contrat testable / même scénario CYD**. Exiger pour chaque frontière un oracle réel source avec échecs et une comparaison des quatre ELFs ; regrouper les risques similaires dans une seule campagne matérielle, tout en laissant les autres familles sensibles sur des PR séparées. Ne pas confondre wrappers métier avec les 3 wrappers logger toolchain. Partir du SHA exact du vrai `main` après merge.
