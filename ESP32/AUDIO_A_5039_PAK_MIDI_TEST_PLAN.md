# AUDIO-A — MIDI 5039 original directement depuis le PAK (candidat, NON PASS CYD)

**Objectif accepté : aucune conversion préalable de MIDI vers WAV/ADPCM.**
La piste `5039.mid` du PAK natif est l'unique source de données musicales.
Le test réel du classic CYD reste nécessaire avant de fusionner.

## Installer / lancer

- Conserver **seulement** le PAK habituel à la racine microSD : `/DoomRPG-ESP32.pak`.
- **NE PAS** copier `/5039.adpcm.wav` ni un autre WAV de musique : il n'est ni lu ni attendu.
- Compiler et flasher **normal `esp32-cyd`** sur la branche [PR #301](https://github.com/Kharn27/DoomRPG-RE/pull/301), puis START GAME.
- La musique est fournie par `5039.mid`, SMF1, 3 pistes, 17 163 octets, PPQN 60 (~4m22). Le parseur natif lit l'entrée par ranges dans `DoomRPG-ESP32.pak`, sans allouer le morceau complet.
- L'audio démarre sur `[AUDIO] MIDI-START resource=5039 file=5039.mid bytes=17163 tracks=3 ppqn=60 source=PAK synth=FM-lite-no-GM-bank`.
- Puis `[AUDIO] PULSE ... speedPct=... notesQueued=... drumsQueued=... programsQueued=... eventsQueued=...`. `speedPct` doit approcher 100 % à partir de quelques mesures, et `dmaErrors=0`.
- Tester les touches, l'ouverture d'Entrance, le gameplay et le retour au menu ; conserver les logs Serial et les chiffres `[RAMBUDGET]` / `[ALIVE]`.

## Portée exacte

- **Ce firmware teste le vrai transport MIDI depuis le PAK et la cadence I2S**, et n'impose aucune conversion de fichiers audio.
- Le synthétiseur actuel est un **FM-lite provisoire** : 12 voix, sinusoïdes/harmoniques sommaires et percussions bruitées. Il **ne peut pas** restituer les vrais presets General MIDI (orgue rock/guitare saturée), et un son méconnaissable **n'invalide pas** le lecteur de SMF.
- L'OGG TimGM6mb précédemment écouté par l'utilisateur reste la référence de reconnaissance musicale, **sans devenir un asset runtime**.
- **Prochain jalon potentiel :** prototype Woody-OPL3 (DOOMESP), en gardant le même parseur/PAK, et vérifier ses timbres et les marges CPU/heap8/largest8 sur le vrai CYD. Ni WAD, ni MUS, ni SF2, ni PSRAM runtime. OPL3 ne sera pas identique à la SoundFont General MIDI.
- **92 effets WAV du PAK** restent le jalon AUDIO-B séparé : ils ne sont pas activés ni convertis dans ce test.
- Sortie DAC GPIO26 uniquement, I2S0 stéréo technique (2×16bit / 16kHz), GPIO25 réservé au tactile ; volume 25/50/75/100 sauvegardé en `/DoomRPG-ESP32.audio.cfg`.
- PR Draft / matériel NON PASS ; **ne pas merger avant log/test validés**.

## Invariants
`shapeData == NULL`, `mediaTexels == NULL`, pas de ZIP runtime, pas de décompression map-wide, aucune lecture SD depuis la tâche PCM core0 ; prioriser le lease PAK du moteur, fail closed sur ressource MIDI absente ou malformée.
