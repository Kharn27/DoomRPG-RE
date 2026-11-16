# AUDIO-SFX-P0 — Inventaire natif des SFX du PAK (CYD en attente)

**16 novembre 2026 — hardware PASS SFX-P0.** Branche `agent/esp32-audio-sfx-p0-pak-wav-inventory` créée depuis le **main exact** `c3be0e7b9bb1a5b521a3962bc0210065add8214f` (SDL-F36 et AUDIO-R2e déjà fusionnés). Milestone borné et temporaire, sans playback.

## Source de vérité : desktop = oracle, pas moteur ESP32

Le vrai `src/Sound.c` contient une table de **95** IDs. Le chargement desktop utilise `Mix_LoadWAV_RW` pour les WAV et FluidSynth pour les index 0, 1, 3 de la table (`5039.mid`, `5040.mid`, `5043.mid`). `Sound_playSound` reçoit ID, flags et priorité ; le desktop prévoit jusqu'à dix canaux, STOP_ALL, LOOP, NOFORCESTOP. Il chargeait les assets depuis un ZIP, pratique interdite dans notre runtime.

Effets concrets du legacy : `5042.wav` (validation/back de menu), `5046.wav` (menu), `5060.wav` (pickup et touches de code), `5061.wav` (explosion), `5062.wav` (certains objets ramassés), `5065.wav` (refus de clé), `5134.wav` (événements spéciaux). Leurs vrais codecs, fréquences et durées ne sont pas déduits du lecteur SDL_mixer : **ils doivent être observés dans le PAK réel**.

## Implémentation P0

- `esp_native_sfx_wav.h` : petit parseur C11 autonome, sans allocation, limité à **512 octets de header et 24 chunks RIFF**, contrôles de dépassement des limites d'entrée, et classes `pcm-candidate` (PCM 8/16 bits, mono/stéréo, 8–48kHz), `ima-candidate` (IMA ADPCM mono 4 bits borné), `unsupported`, `invalid`, `header-too-far`. **Une classification favorable n'autorise pas encore la lecture audio.**
- `esp_native_sfx_probe.cpp` + `.h` : une seule inspection par session active, uniquement avec `EspAssetPack_isMapFlashActive()`, `EspAssetPack_isResident()` et aucun bail PAK déjà ouvert ; PAK indexé et stocké **en flash brut**, jamais accès ZIP/SD pour les SFX. Ressources inspectées : `5042, 5044, 5045, 5046, 5060, 5061, 5062, 5065, 5134`. Ne lit jamais le payload audio complet : 512 octets au maximum par effet. Une seule zone de lecture sur la pile, aucun buffer de lecture SFX permanent.
- `main.cpp` lance le probe après service gameplay et avant `EspEntranceMusic_service()`. `EspAssetPack_close()` ferme proprement le bail. Logs `[SFXP0] BEGIN/ENTRY/END`, format numérique RIFF, sample rate, canaux, bits, alignement de blocs, dataOffset, présence et heap8 avant/après.
- **Aucun changement** à la musique R2e, au ring 16Kio, au DAC2 GPIO26, à son ISR, au tactile GPIO25, à la configuration V3 ou à la mémoire gameplay. Aucun son SFX encore entendu sur P0 ; ne jamais l'annoncer comme un backend terminé.
- Test `ESP32/test/audio_sfx_p0_pak_inventory_host.py` sur fixtures RIFF (PCM8, PCM16 stéréo, IMA mono, chunk trop long, format inconnu, bounds invalides), et test de la frontière PAK-only. CI incluse.

## Test réel nécessaire

Sur le CYD classic sans PSRAM avec le firmware **normal `esp32-cyd`**, conserver `/DoomRPG-ESP32.pak` et tous les WAV musicaux déjà copiés sur microSD. Démarrer Entrance ; relever les neuf `[SFXP0] ENTRY` et le `[SFXP0] END` (absence de `pak-read-failed`, pas d'allocation et `packLease=closed`). Jouer avec déplacements/portes puis vérifier `[ENTRANCEMUSIC] PLAY/STREAM/FINAL` et `underruns=0`, sortie HUB/EXIT, `shapeData/mediaTexels=NULL`. Le Serial réel déterminera les formats supportés **avant** SFX-P1.

**État P0 : CYD PASS (sur le scope ci-dessous), SFX playback toujours deferred.** Après vérification des vrais formats, concevoir SFX-P1 : petit lecteur/mixeur natif sur les vrais bytes du PAK, effets progressivement raccordés à leurs événements propriétaires, priorité/volume/STOP_ALL fail-closed. Ne pas compiler Sound.c / SDL_mixer dans le firmware, ne pas présumer dix canaux ni allouer un WAV entier. Ne pas merger P0 avant retour CYD et autorisation expresse.

## Validation CYD matérielle : PASS — 16 novembre 2026

**Firmware réellement testé :** `c7562863470f44fc040850b949fca2d6f429bfea`, `esp32-cyd` (classic CYD ESP32-D0WD-V3, flash 4 Mo, **PSRAM=0**). **Capture Serial authentique :** `platformio-device-monitor-261116-012759.log`, **758 lignes / 93 489 octets**, SHA256 `69fbc83544a4285c2ad9e6b1ce8b0b5c14cd5c26147361b98db3640410a7ce70`. La capture porte un `POWERON_RESET`, un Start Game/Intro, Entrance, déplacements, prises d'objets, dialogues, porte, HUB/EXIT et retour au menu ; aucun Guru/panic/WDT relevé. Le log ne contient aucun SHA firmware intégré : attribution par la branche testée dans la conversation.

**Résultat exact de la sonde :** `[SFXP0] BEGIN` avec `heap8=81696`, `source=verified-map-flash-pak`, `pakLease=closed`. Les **neuf** entrées existent, `RIFF/WAVE` format `0x0001` = **PCM signé 16 bits**, **1 canal**, **8000 Hz**, `blockAlign=2`, `dataOffset=44`, `fact=0` ; toutes `status=pcm-candidate`. Aucune entrée ADPCM ni inconnue dans cet *échantillon* de neuf, sans généraliser aux autres effets du PAK.

| Ressource | Taille WAV | Data PCM | Échantillons | Durée calculée |
|---|---:|---:|---:|---:|
| 5042.wav | 5164 B | 5120 B | 2560 | 320 ms |
| 5044.wav | 9004 B | 8960 B | 4480 | 560 ms |
| 5045.wav | 12844 B | 12800 B | 6400 | 800 ms |
| 5046.wav | 8364 B | 8320 B | 4160 | 520 ms |
| 5060.wav | 7084 B | 7040 B | 3520 | 440 ms |
| 5061.wav | 27244 B | 27200 B | 13600 | 1700 ms |
| 5062.wav | 11564 B | 11520 B | 5760 | 720 ms |
| 5065.wav | 5804 B | 5760 B | 2880 | 360 ms |
| 5134.wav | 8364 B | 8320 B | 4160 | 520 ms |

`[SFXP0] END scanned=9 present=9 pcm=9 ima=0 unsupported=0 invalid=0 packLease=closed music=untouched playback=no heap8=81696->81696 delta=0`. **Aucune heap supplémentaire observée** pour la sonde, aucun fichier WAV entier chargé en RAM, et pas d'effet sonore audible attendu à P0.

**Non-régression audio/gameplay :** `[MAPFLASH] ARM active=1 verified=1 reused=1`, RAMBUDGET `HEADROOM_OK` ; musique R2e `gainDiv=14`, `witness=none`, WAV SD `M_STALKS`, `ring=16384`. `[ENTRANCEMUSIC] FINAL frames=579956` = **26,30 secondes** de samples joués, **underruns=0 recoveries=0 gapsOver90us=0**, `pumpCalls=82 pumpReads=134`, `maxSdReadUs=4124` et aucune lecture >20ms, `maxIsrCycles=475`. Flux producer/ISR FNV `48a2c808` égal ; DAC `23..236`. Heap8 `81696` juste avant la sonde, `59676` en jeu, `59232` juste avant sortie ; `[RESIDENTRESET] empty=1`, `[SYSEXIT] MENU-READY`, `[ENTRANCEMUSIC] FINAL ... heap8After=127152 largest8=86004 timer=OFF dacPad=OFF file=CLOSED`. Invariants `shapeData=0x0 mediaTexels=0x0` à l'entrée/sortie.

**Verdict : AUDIO-SFX-P0 PASS matériel pour le catalogue de neuf SFX et absence de perturbation observée du flux R2e pendant 26 secondes.** Pas de playback SFX validé, pas d'inventaire exhaustif des ~92 WAV du PAK, ni test du WAV musical de 292,8s. La source firmware testée reste exactement `c7562863470f44fc040850b949fca2d6f429bfea`; après le test, commits **strictement docs-only**, puis intégration d'éventuels changements déjà fusionnés dans `main` seulement après vérification d'absence de changement des sources firmware P0.

**Direction P1 :** lire un PCM16/8000 mono à partir des offsets de `EspAssetPackEntry`, conversion/resampling **dans le producteur uniquement**, et mixer de façon saturée/bornée avec la trame musicale à 22050Hz vers le **même DAC2** ; commencer par un effet/canal, petite réserve bornée, pas de blocage SD ni de mutation SDL, aucune tâche audio concurrente. Activer graduellement les premiers `EspNativeAudioIntent` après preuve sur CYD. Compatibilité future des formats reste fail-closed.
