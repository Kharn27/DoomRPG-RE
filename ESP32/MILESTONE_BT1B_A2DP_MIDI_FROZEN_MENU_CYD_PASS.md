# BT-1/B — A2DP Source + MIDI PAK avec menu figé — PASS matériel CYD, qualité audio FAIL

**Date :** 11 novembre 2026  
**PR :** [#321](https://github.com/Kharn27/DoomRPG-RE/pull/321) (Draft, non fusionnée)  
**Issue :** [#261](https://github.com/Kharn27/DoomRPG-RE/issues/261)  
**Branche :** `agent/esp32-bt1-menu-midi-a2dp-isolation`  
**Main exact à la création de BT-1 :** `e432ffa7ff09d8332a286896dcb4a138a62aa024`  
**Firmware testé (par les traces de compilation du profil) :** `a911c03ad50816bc1fc28e9cf481fec0a8677b61`  
**Environnement :** `esp32-cyd-bt-test` ; classic CYD ESP32-D0WD-V3, 4 MB flash, zéro PSRAM. Aucun test du firmware `esp32-cyd` en jeu avec Bluetooth.

## Périmètre et limites

Le profil expérimental utilise le peintre exact `MENU_MAIN`, avec PAK natif sur SD. Il n'appelle ni input, ni menu actions, ni intro, ni moteur de carte/jeu. Après `PlatformVideo_present()` final, les pixels sont retenus dans la GRAM ILI9341 ; seuls les propriétaires **BT-test** détachent `Render.framebuffer` et libèrent les mappings de compatibilité et le framebuffer de 38 400 octets. Aucun redraw ensuite. Ce mécanisme est **interdit avec gameplay ou redraw actifs**.

Bluetooth Classic BR/EDR/A2DP Source via ESP-IDF/Arduino : `btInUse=true` fort dès `initVariant()`, libération de BLE seulement, découverte par service Rendering et nom, connexion automatique d'un seul renderer trouvé, `MEDIA_CHECK_READY` / `MEDIA_START`. MIDI `5039.mid` est lu du PAK (**17 163 B, 3 pistes, ppq=60**) avec un parser borné et de petits buffers, **non FluidSynth/SF2** ; les 12 voix triangulaires synthétisent des notes, pas les instruments/percussions fidèles de l'original. La callback A2DP n'effectue ni accès SD/PAK ni allocation heap.

## Compilation et flash réels

PlatformIO utilisateur **compile/link/upload SUCCESS** sur `esp32-cyd-bt-test`, RAM statique 94 028 / 327 680 B (28,7 %), flash **1 362 921 / 1 703 936 B (80 %)**. Le log contient les compilations de `esp_bt_test_freeze_bridge.c`, `esp_bt_test_main.cpp`, `esp_bt_test_midi.cpp`, la création ELF/BIN et la vérification de l'upload. `TOUCH_CS pin not defined` n'est qu'un avertissement TFT_eSPI, le profil n'initialise volontairement pas le tactile. `main` non fusionné. Le log ne contient pas d'attestation de SHA embarquée ; le rattachement au candidat repose sur la version des sources et les sorties de compilation.

## RAM : preuve de l'économie

```text
[MAINBOOT] READY owner=native-opaque ... frame=1945fafa
[BTTEST] MENU_STATIC ready=1 heap8=80500 largest8=65524
[BTTEST] FRAMEBUFFER_RELEASED bytes=38400 screen=ILI9341-GRAM-retained touch=off redraw=forbidden
[BTTEST] FROZEN_RELEASE before=80500 after=127356 gained=46856 largest8=65524 dma=127356 display=latched gameplay=off
[BTTEST MIDI] READY song=5039.mid size=17163 tracks=3 ppq=60 ...
[BTTEST] RAM BEFORE heap8=122992 largest8=65524 dma=122992 largestDMA=65524 minHeap=90348 tasks=6
```

Gain net **46 856 B** observé, comprenant les 38 400 B de framebuffer, ~8 440 B de mappings et des frais d'allocation. Après chargement MIDI/PAK, 122 992 B libres. Comparaison approximative au test BT-1/A sans freeze : 76 512 B avant Bluetooth, 1 256 B après init A2DP ; la récupération est validée mais **non applicable aux frames gameplay**.

## Contrôleur, connexion et flux numérique

| Étape | Verdict Serial | Heap8 libre |
| --- | --- | ---: |
| CTRL_INIT | ESP_OK | 110 204 B |
| CTRL_ENABLE | ESP_OK | 110 204 B |
| BLUEDROID_INIT | ESP_OK | 49 032 B |
| BLUEDROID_ENABLE | ESP_OK | 47 680 B |
| A2DP_SOURCE_INIT | ESP_OK | 44 068 B |
| A2DP_PCM_CALLBACK | ESP_OK | 44 452 B |
| Discovery running | ESP_OK | 40 324 B |
| Connected and media started | ESP_OK | **35 480 B** |
| Later active | still connected | **35 144 B** |
| After user disconnect | disconnected | 40 808 B |

```text
[BTTEST] DISCOVERY renderer=1 addr=00:1D:DF:A7:09:F4 cod=00240404 name=PHILIPS SBT30
[BTTEST] DISCOVERY done candidates=1
[BTTEST] A2DP_CONNECT_REQUEST result=ESP_OK code=0
[BTTEST] GAP auth=SUCCESS
[BTTEST] A2DP connection=2 reason=0
[BTTEST] MEDIA_CHECK_READY result=ESP_OK code=0
[BTTEST] MEDIA_ACK cmd=1 status=0
[BTTEST] MEDIA_START_REQUEST result=ESP_OK code=0
[BTTEST] MEDIA_ACK cmd=2 status=0
[BTTEST] A2DP audioState=2 started=1
[BTTEST MIDI] PCM callbacks=4216 bytes=2158592 elapsedMs=12236 events=126 pending=21 underrunFrames=0 done=0 error=0
[BTTEST] A2DP audioState=1 started=0
[BTTEST] A2DP connection=0 reason=0
```

4 216 callbacks, **2 158 592 octets PCM** (12,236 s à 44,1 kHz × stéréo × 16 bits), événements MIDI traités sans `ioFailed`. Le compteur `underrunFrames` signale uniquement l'absence d'événements disponibles, **pas** la qualité acoustique. La déconnexion en fin de log est **volontaire : utilisateur a éteint/occupé l'enceinte**, pas de crash/WDT rapporté.

## Résultat double et prochaine étape

- **PASS matériel :** le CYD sans PSRAM initialise BR/EDR + Bluedroid, trouve l'enceinte PHILIPS, authentifie, connecte, reçoit les ACK et transmet un flux PCM musical stable avec ~35 Kio de heap8 libre. Pas un simple callback de silence.
- **FAIL acoustique / ressenti utilisateur :** « le son était encore dégueulasse ». Aucun PASS de fidélité/qualité ni original Doom RPG annoncé. Le synth expérimental ignore les vrais programmes General MIDI, envelopes, sustain et percussions ; mais la seule écoute ne distingue pas encore mauvais synth et déformation du flux A2DP.
- **BT-1/C proposé :** introduire d'abord un son de référence sinusoïdal faible amplitude, puis jouer le **MIDI inchangé** afin de comparer le chemin PCM/BT à un signal connu, sans toucher aux invariants de jeu. Ne pas fusionner #321 dans main.

Le profil expérimental utilise une partition BT spécifique ; son slot raw PAK réduit n'est pas validé pour plusieurs cartes. `esp32-cyd` normal conserve son partitionnement et son framebuffer de 38 400 octets.
