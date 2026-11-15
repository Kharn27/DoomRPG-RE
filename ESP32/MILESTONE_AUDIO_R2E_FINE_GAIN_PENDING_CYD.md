# AUDIO-R2e — réglage fin du volume musical (+1,16 dB), test CYD en attente

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
