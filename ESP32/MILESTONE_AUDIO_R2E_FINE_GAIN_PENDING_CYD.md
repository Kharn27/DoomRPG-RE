# AUDIO-R2e — réglage fin du volume musical (+1,16 dB), PASS CYD limité

**15 novembre 2026.** Branche `agent/esp32-audio-r2e-gain-trim`, basée exactement sur
`56b5bb1d1f87bc456f14d2d57226b0c6d3fd58a9` : AUDIO-R2d avec son compte rendu
[matériel PASS transport](MILESTONE_AUDIO_R2D_CYD_STREAMING_PASS.md). PR empilée sur
`agent/esp32-audio-r2a-entrance-sd-pcm` / #347, sans toucher à `main`.

## Pourquoi ce micro-jalon

Sur le vrai CYD, R2d a exécuté deux lectures Entrance et le gameplay de manière
fluide (0 underrun, 0 recovery, son continu) mais l'utilisateur trouve que le niveau
`100%` manque « encore un poil », sans saturation audible. Il entend un très léger
bruit de « moirage » sous la musique. **L'origine du bruit n'est pas connue** :
aucun enregistrement analogique, aucune mesure électrique, ni comparaison objective
du PCM fourni n'ont été effectués. Il est interdit de prétendre que R2e répare ce bruit.

## Diff borné

- Dans `esp_audio_entrance_adpcm.h`, convertir le PCM16 via **division par 14**
  au lieu de **16** (≈20 log10(16/14) = **+1,16 dB** seulement dans la portion
  linéaire). Le soft knee à **±96**, l'asymptote à **±112 codes DAC8**
  et le fade de **200ms** sont strictement conservés.
- Les valeurs extrêmes restent dans **[16,240]**. Pour le pic PCM16 `2252`
  précédemment observé, le DAC passe seulement **235 → 236** ; un PCM16
  plus représentatif de `700` donne **171 → 178**. La présence de ces
  calculs ne prouve pas l'absence de saturation de l'amplificateur analogique.
- Le compteur `softKneeFrames` observe désormais **exactement** la
  même division entière `pcm/14` et le même seuil `>|96|` que la
  conversion ; l'instrumentation est hors ISR.
- **Aucun autre changement** de ring 16Kio, ISR 22,05kHz, SD, PAK, TFT,
  framebuffer, tactile GPIO25, DAC2 GPIO26, timer TG1/T1, volume Options,
  gestion des sorties de session, son bip (reste retiré) ou des deux
  environnements normal/inverted. Pas de nouvelle tâche ni de RAM supplémentaire.
- `gainDiv=14` annoncé dans `BLOCK0` et `PLAY` pour différencier
  matériellement R2e de R2d.

## Protocole de validation matérielle

Firmware **normal `esp32-cyd`**, WAV **identique** sur
`/doomrpg/music/opl/M_STALKS.wav`, volume Sound à **100%**.
Écouter les mêmes 30–60 secondes que R2d, en bougeant/tournant/ouvrant une porte,
puis EXIT ; facultativement comparer Sound à 50% ou 75% pour voir si le
léger bruit change proportionnellement au niveau musical. Relever
`[ENTRANCEMUSIC] BLOCK0/PLAY/SIGNAL/STREAM/FINAL`, les `underruns`,
`recoveries`, `pumpCalls`, la RAM, et surtout le retour acoustique
(distortion/saturation, niveau perçu, bruit).

**R2e = PENDING CYD** jusqu'au reflash réel. R2d demeure le fallback PASS
transport prouvé, conservé sur sa propre branche. Le WAV propriétaire ne va pas
dans Git. Pas de fusion de PR sans demande explicite. Toute hypothèse bruit
analogique/dither/filtre/source requiert un milestone spécifique après A/B.

## Validation CYD réelle — AUDIO-R2e PASS (15 novembre 2026)

**Hardware testé :** classic CYD ESP32-2432S028R sans PSRAM, environnement normal `esp32-cyd` ; **firmware testé, SHA exact** `63281d7aff25e549aaa5e6963d406d51b5fc3e2b`. Retour utilisateur : « On va dire que c'est pas mal », demande explicite de fusionner dans l'état actuel. Léger bruit de fond (« moirage ») précédemment signalé : **amélioration ouverte**, pas de preuve de cause ni de traitement dans R2e. Aucun clipping/saturation audible rapporté sur la variante R2d, et l'utilisateur accepte subjectivement R2e pour le merge.

**Log CYD de R2e, conservé hors Git :** `platformio-device-monitor-261115-234914.log`, SHA-256 `8c736921f987077c84efc250ef65d1b6993e703257b99df1ce51ca4faeda36d0` (871 lignes).

- `BLOCK0 gainDiv=14 softKneeFrames=18`; `PLAY ... gainDiv=14 masterVolume=100 witness=none ring=16384`. Son direct depuis `/doomrpg/music/opl/M_STALKS.wav`, identique au fichier R2d, pas de bip synthétique ; lecture PCM à fréquence cible 22050Hz, préflight `CLOCK measuredHz=21885`.
- `FINAL frames=1080177` ≈ **49,0 s** audio ; **underruns=0**, **recoveries=0**, **maxSilentFrames=0**, **gapsOver90us=0**, **maxIsrCycles=475**, `maxGapCycles=21351`. Producteur/ISR préfixe FNV identique `a69b262c` ; DAC codes observés **23..236** ; `softKneeFrames=1320`, pas de rail numérique 0/255.
- `pumpCalls=134`, `pumpReads=187`, ring minimum publiquement observé en STREAM **14343** échantillons ; `reads=533`, `maxSdReadUs=4165`µs, `slowReadsOver20ms=0`. Déplacements, rotations, porte animée, interactions, dialogues et GUI hub réels couverts, aucune interruption remontée.
- RAM : `[ENTRANCEMUSIC] PRIMING heap8=60920 largest8=45044`, `[ALIVE] heap8` minimum **59676**, `[INTROCLK] PARK heap8=59232` près de la sortie ; `FINAL heap8After=127152 largest8=86004`, `timer=OFF dacPad=OFF file=CLOSED`. `[RESIDENTRESET] ... empty=1`, `[SYSEXIT] MENU-READY`. Aucun crash/WDT dans ce log.

**Verdict : PASS CYD pour le réglage sonore actuel et le transport audio pendant gameplay, en scope testé** ; ni preuve d'écoute du WAV sur les 292,8 secondes, ni de pureté analogique. Aucun code modifié après ce log : ce commit est **docs-only**. À préserver : ring 16Kio, alimentation inter-frame, DAC2/GPIO26, GPIO25 réservé au tactile, `shapeData/mediaTexels == NULL`, WAV exclusivement SD, aucune PSRAM.
