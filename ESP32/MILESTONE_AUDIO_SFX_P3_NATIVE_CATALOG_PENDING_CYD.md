# AUDIO-SFX-P3 — inventaire complet des WAV originaux et des sons ennemis (CYD PENDING)

**16 novembre 2026 — [issue audio globale #261](https://github.com/Kharn27/DoomRPG-RE/issues/261).** Nouvelle branche unique `agent/audio-sfx-p3-original-wav-catalog` créée à partir du **vrai `main` GitHub** après fusion matérielle [#357](https://github.com/Kharn27/DoomRPG-RE/pull/357), SHA `721e3fe8759f443c30bd7ebcceb1220340e79871`.

## Diagnostic

Le log P2 valide trois sons `5045` complets, les pickups `5060` différés après dialogue, 0 underrun et une sortie propre. En revanche, `5081` publié après les dégâts reçus est encore hors de la liste de lecture `{5045,5046,5060}`. Le combat natif possède aussi les tables `painSounds` et `deathSounds` (p. ex. `5085/5089/5090/5099/5110/5138`) mais ne publie pas encore tous ces sons. Il faut connaître les vraies durées et formats du PAK avant une extension sûre du mixeur.

## Portée strictement bornée

- Les 9 WAV déjà inventoriés par P0 conservent leurs identifiants et logs. P3 ajoute les **83 autres WAV** de la **table desktop exacte de 95 IDs** (`src/Sound.c` : 3 MIDI exclus + 92 WAV au total). Aucune plage artificielle ni duplication.
- Un seul probe **passif** dans la même tâche principale, uniquement si flash PAK actif/vérifié et aucun bail concurrent, ouvre/ferme le PAK par l'API native. Lit des en-têtes RIFF au maximum 512 octets par fichier, sans WAV entier, sans heap, ZIP runtime, musique SD SFX, ISR ni nouvelle tâche.
- Logs `[SFXP3] BEGIN/ENTRY/END` pour les 83 nouveaux IDs : format, fréquence, bits, frames, durée en millisecondes, résultat PCM/IMA/autre/invalid. Ancienne couverture P0 et musique préservées.
- Test host exact de partition 95 = 3 MIDI + 9 P0 + 83 P3 sans doublons, références de douleur/mort des monstres présentes, `5081` distinct (dégâts du joueur), gardes de provenance PAK, liste d'admission du mixeur inchangée. Quatre compilations CI avant test matériel.

## Test CYD normal demandé

Flasher cette seule branche avec l'environnement `esp32-cyd` ; Start Game sur Entrance et enregistrer tout le boot. Vérifier `[SFXP3] END scanned=83 ...`, les lignes `ENTRY id=5081`, `5085`, `5089`, `5090`, `5099`, `5110`, `5138`. Relever les durées/formats réels et `heap8 delta`. Vérifier encore musique `underruns=0` et gameplay normal.

**Pas de son ennemi nouveau dans P3 :** le prochain jalon utilisera les durées mesurées pour étendre le mixeur par familles d'opcodes/intentions, fail-closed, en respectant les 16 KiB de ring, les buffers bornés et la priorité musicale. Aucun PASS matériel inventé.
