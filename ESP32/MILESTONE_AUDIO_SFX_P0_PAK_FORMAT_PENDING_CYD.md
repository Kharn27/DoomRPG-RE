# AUDIO-SFX-P0 — Inventaire natif des SFX du PAK (CYD en attente)

**16 novembre 2026.** Branche `agent/esp32-audio-sfx-p0-pak-wav-inventory` créée depuis le **main exact** `c3be0e7b9bb1a5b521a3962bc0210065add8214f` (SDL-F36 et AUDIO-R2e déjà fusionnés). Milestone borné et temporaire, sans playback.

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

**État P0 : CYD PENDING.** Après vérification des vrais formats, concevoir SFX-P1 : petit lecteur/mixeur natif sur les vrais bytes du PAK, effets progressivement raccordés à leurs événements propriétaires, priorité/volume/STOP_ALL fail-closed. Ne pas compiler Sound.c / SDL_mixer dans le firmware, ne pas présumer dix canaux ni allouer un WAV entier. Ne pas merger P0 avant retour CYD et autorisation expresse.
