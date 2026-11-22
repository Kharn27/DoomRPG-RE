# AUDIO-LOAD-V11 — musique rétablie après LOAD sauvegarde sur classic CYD (PASS matériel borné)

Date : 21 novembre 2026. PR [#415](https://github.com/Kharn27/DoomRPG-RE/pull/415), branche `agent/esp32-audio-load-v11-heap-guard`.

## Symptôme et diagnostic

Sur `platformio-device-monitor-261121-131040.log` (ancien firmware), START→Entrance lançait correctement `M_STALKS.wav`, mais deux reprises LOAD V11 slot04 avaient `[ENTRANCEMUSIC] FINAL reason=FAIL-HEAP`, puis `[LOADAUDIO-L1] PRESTART ... ready=0 failClosed=1`. L'ouverture du WAV réussissait ; `FAIL-HEAP` pouvait désigner l'allocation du ring ou le garde *post-ring*, sans les distinguer. Le modèle à partir du chemin START prédisait **48 568 B** disponibles après le ring de 16 384 B au LOAD, contre une ancienne limite de **49 152 B**.

La correction native conserve le ring `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT` de 16 384 B et les gardes de pré-allocation inchangés ; elle ramène uniquement le garde de RAM libre *après ring* de 49 152 à **47 104 B (46 KiB)**, maintient le minimum de bloc contigu de 16 384 B, et expose `HEAP-POST-REJECT` en cas de rejet. Aucune PSRAM, taille de ring réduite, décompression ZIP runtime ou nouveau lecteur SD. Fonctions pures dans `esp_audio_entrance_heap_guard.h` testées avec le vrai C, cas bornes et 13 mutants adversariaux par `audio_load_v11_heap_guard_host.py`.

## Validation matérielle réelle

Capture utilisateur : `platformio-device-monitor-261121-143238.log`, **2 565 lignes, 308 501 octets, SHA256 `2b91d882d0eafb1093d1b5c7c4769ffe6e13dc5b6a624d7fd6ceda1a8eeef309`**. Environnement de référence : `esp32-cyd` sur ESP32-D0WD-V3 240 MHz, flash 4 MiB, **PSRAM 0 B**. Les logs ne contiennent pas de SHA du firmware flashé : attribution à la branche de correctif basée sur le contexte du test utilisateur, pas sur une empreinte binaire indépendante.

Séquence observée :
1. START → intro → Entrance : `[ENTRANCEMUSIC] PRIMING ... heap8=49388 largest8=31732` (ligne 492), `PLAY` (496), `[LOADAUDIO-L1] PRESTART ... ready=1 failClosed=0` (497). Gameplay réel, porte, dialogues, ramassages et SFX.
2. HUB EXIT : `[RESIDENTRESET] ... released=18008 ... empty=1` (1193), arrêt propre de `M_STALKS.wav` avec `underruns=0 recoveries=0` (1197), musique menu `PLAY` (1206), `[SYSEXIT] MENU-READY` (1221).
3. MENU LOAD, slot **04** confirmé (1239). `[NATIVESAVE] LOAD ... version=11 bytes=5604 map=1 ... world=...restored-exact` (1286). `[RAMBUDGET] RESERVE ... margin8=12000 advisory=HEADROOM_OK` (1337) ; heap8 avant lancement audio **69 344 B** (1335, 1442).
4. Reprise : `WAV_OK M_STALKS.wav` (1445), **`[ENTRANCEMUSIC] PRIMING ringBytes=16384 ... heap8=48568 largest8=31732`** (1446), `PLAY track=ENTRANCE` (1450), `[LOADAUDIO-L1] PRESTART map=1 ... ready=1 ... ringBytes=16384 failClosed=0` (1451), `RESUME-VISIBLE` (1453). **48 568 B est cette fois mesuré par le matériel et correspond exactement à la valeur initialement modélisée.**
5. Gameplay poursuivi pendant ~44,5 s de consommation de frames audio : STREAM à ~10/20/30/40 s avec `underflows=0 recoveries=0` (1662, 1857, 2198, 2435). La clôture (2540) indique **`underruns=0 recoveries=0 maxSilentFrames=0`**, `gapsOver90us=2`, `maxPublishGapMs=513 gapsOver500Ms=1` : latence publication présente mais *aucune rupture audio comptabilisée*. Audit à la sortie : `queueOverflow=0 audibleSfxDropped=0 sfxPlayedTrack=34 sfxRejectedTrack=0` (2541).
6. Deuxième HUB EXIT : `[RESIDENTRESET] released=18008 ... empty=1` (2534), `M_STALKS.wav` fermé, musique menu lancée (2549-2550), `[SYSEXIT] MENU-READY` (2564). État final `[ALIVE] ... heap8=94428 largest8=65524` (2565).

**Contrôles négatifs** : aucun `FAIL-HEAP` / `HEAP-POST-REJECT`, aucun Guru/panic/WDT/abort, aucune perte de musique d'après les compteurs ; `shapeData=0x0 mediaTexels=0x0` répétés. La capture atteste **un** vrai LOAD depuis le menu après START/EXIT, suivi de jeu et EXIT ; **elle ne valide pas deux LOAD successifs** ni toutes les cartes, toutes les tailles de sauvegarde, ni un cycle d'endurance indéfini. Le Serial prouve l'activation et la continuité de la sortie DAC (et ses compteurs), pas à lui seul une écoute subjective.

## CI et clôture

Code candidat matériel (avant commits documentaires) : **`d8e81d3315f023beccf49dcafbf70dde5857af8f`**, main de base **`b6a910d699a69688d94f75d02f55a0ec8f0d7499`**. Double CI complète **SUCCESS** : [push #44904289067](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44904289067) et [PR #44904292214](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44904292214). Le test hôte audio dédié est SUCCESS sur les deux, incluant le vrai C, bornes, cas aléatoires et 13 mutants. Aucun build local matériel revendiqué.

**Verdict : PASS CYD borné du correctif AUDIO-LOAD-V11**, avec maintien des invariants mémoires et du comportement fail-closed. Les commits post-PASS sont **documentation seulement**, sans changement de logique, et nécessitent une ultime double CI du HEAD documentaire avant merge. **Ne pas fusionner sans permission explicite.** La PR architecture RC24 #414 (retrait du dernier GNU wrapper production) reste indépendante et n'est pas validée par ce log audio.
