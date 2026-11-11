# AUDIO-A — MIDI 5039 original directement depuis le PAK (candidat, NON PASS CYD)

**Objectif accepté : aucune conversion préalable de MIDI vers WAV/ADPCM.**
La piste `5039.mid` du PAK natif est l'unique source de données musicales.
Le test réel du classic CYD reste nécessaire avant de fusionner.

## Contrôle visuel du PCB fourni par l'utilisateur — défaut audio confirmé (11 novembre 2026)

Photo réelle du dos de la carte classique double-USB `ESP32-2432S028` : sérigraphie du réseau **U5 (amplificateur)** lisible et codes CMS observés directement :

| Référence sérigraphiée | Code CMS de cette carte | Valeur de cette carte | Correction documentée sur cette variante |
|---|---|---|---|
| R4 | `472` | 4,7 kΩ | conservée |
| **R7** | `0` | **0 Ω** | **47 kΩ** (code 473) |
| **R8** | `0` | **0 Ω** | **22 kΩ** (code 223) |
| **R9** | `683` | **68 kΩ** | **47 kΩ** (code 473), selon la correction partagée dans la discussion CYD |

**Attribution : il s'agit de marques PHYSIQUEMENT VISIBLES sur la photo utilisateur**, pas d'un diagnostic inféré du modèle de carte ou d'un test électrique. Sans mesure au multimètre, les codes restent l'indication de composants montés, mais correspondent exactement au réseau fautif documenté dans [CYD Mods for better Audio #356](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/discussions/356), et corroboré par [hexeguitar/ESP32_TFT_PIO](https://github.com/hexeguitar/ESP32_TFT_PIO) et [Embedded Kiddie](https://embedded-kiddie.github.io/2025/04/24/).

La discussion indique explicitement **R7 0 Ω, R8 0 Ω et R9 68 kΩ = configuration incorrecte**, et **R7 47 kΩ, R8 22 kΩ et R9 47 kΩ = configuration audio améliorée**. Attention à la distinction : le schéma 2022 indiquait parfois R9=68 kΩ avec R7/R8 normaux, tandis que le correctif communautaire destiné à l'audio propre adopte R9=47 kΩ.

Le test trois états avait montré `DAC-OFF` silence, `DAC-MIDPOINT` craquement, `MIDI` overdrive important, HP 8Ω. Ces indices joints au marquage des CMS étayent **fortement** la cause de gain / charge d'entrée analogique. Cela n'exclut pas d'autres sources de bruit, et un test musical après modification reste obligatoire.

**Conduite :** ne plus gaspiller de cycles à compenser ce défaut en abaissant le PCM 8 bits. L'utilisateur peut choisir une réparation micro-soudure **0603** par une personne équipée : carte éteinte, USB et HP déconnectés, vérifier sérigraphie, remplacement des trois résistances seulement, contrôle des ponts/courts-circuits avant remise sous tension. **Aucune intervention physique n'est faite par l'agent.** Une alternative est un module audio externe, projet distinct avec contraintes de pins/RAM. Ne jamais imposer la soudure à un utilisateur non équipé. La PR #301 reste **Draft / NON PASS audio**, même si le matériel en cause est maintenant visuellement identifié.

---

## Validation différentielle réelle — 11 novembre 2026, AUDIO-A FAIL matériel

L'utilisateur a réalisé le test des **trois étapes**, sur le vrai CYD et son HP **8 Ω** :

| Étape du candidat `fe46b49220b7fb32df44b8a2b05e239f43d3955a` | Observation audible |
| --- | --- |
| `DAC-OFF` : GPIO26 désactivé, DMA silence numérique | **« Rien »** |
| `DAC-MIDPOINT` : GPIO26 activé, PCM `0x8000` constant | **« Cracha »** (craquement audible) |
| `MIDI` : fichier `5039.mid` original du PAK | **« La musique est jouée en mode gain overdrive a fond comme d'hab »** |

**Interprétation bornée :** le bruit est déclenché par l'activation du DAC sans note MIDI. Le moteur de fichiers MIDI, le mixage polyphonique et l'écrêtage PCM numérique ne sont pas nécessaires pour produire ce bruit. Néanmoins, la réponse utilisateur ne précise pas si `DAC-MIDPOINT` produit uniquement un *pop* au moment de l'activation ou du crachotement **continu** durant les deux secondes ; ne pas assimiler ces cas. Le MIDI reste reconnaissable mais très saturé. Les mesures précédentes de ce même matériel (I2S `15999–16000 Hz`, `speedPct=100`, `dmaErrors=0`, `pcmPeak8=10..15`, `pcmClipped=0`) excluent un clipping **dans le PCM calculé**, et ne mesurent pas le signal réel GPIO26 ni la sortie SC8002B.

**Hypothèse matérielle forte (NON prouvée sur ce PCB)** : gain excessif de l'étage SC8002B causé par des valeurs d'entrée erronées. Rapport communautaire sur certaines variantes CYD : `R7=0 Ω`/`R8=0 Ω`/`R9=68 kΩ` au lieu du réseau atténuateur, avec signal audio totalement écrêté ; voir [discussion CYD audio #356](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/discussions/356) et [retour d'expérience CYD_MP3Player](https://github.com/embedded-kiddie/CYD_MP3Player). Le schéma ESP32-2432S028R ancien affiche `R7=47 kΩ`, `R8=22 kΩ`, `R9=68 kΩ`, tandis que la discussion cite une version corrigée à `R9=47 kΩ` : **les variantes de PCB et révisions doivent être distinguées**, ne pas donner une valeur R9 universelle à souder. Si `R7=R8=0 Ω`, le gain approximatif `R9/(R4+R7+R8)=68 kΩ/4,7 kΩ≈14,5` rend plausible une saturation même à 25 % de volume PCM. Charge HP 8 Ω normale.

**Suite autorisée** : photographier/identifier le réseau R7/R8/R9 de la **carte réelle**, ou mesurer hors tension si équipé ; confirmer les valeurs avant tout changement CMS. Ne pas souder sans identification, ne pas remplacer le HP par réflexe. Une correction logicielle brutale du volume ne résout pas un étage analogique défectueux et dégraderait la résolution du DAC 8 bits. Ne pas activer l'audio en production sur la seule base de la CI.

**Décision** : #301 reste Draft / AUDIO-A NON PASS matériel, aucun merge. Le test différentiel a atteint son objectif de localisation : prioriser l'étage DAC/entrée-ampli et l'éventuel pop d'activation, plutôt que modifier le tempo MIDI ou ajouter des filtres de mixage à l'aveugle.

---

## Diagnostic différentiel CYD — brouhaha avec HP **8 Ω** (nouveau candidat)

Retour utilisateur, vrai firmware normal, après le correctif Q8 : musique originale audible mais couverte par un très fort brouhaha saturé. Extraits :

- `AUDIOCLK measuredHz=15999 result=PASS`, `PULSE speedPct=100 dmaErrors=0`, `late=0→57` après chargement d'Entrance.
- À volume SD **25 %**, `pcmPeak8=10→15`, `pcmLimited=0`, `pcmClipped=0` : **pas de clipping détecté dans le PCM produit**.
- Intro/Entrance/EXIT, `HEADROOM_OK`, `RESIDENTRESET empty=1`, `shapeData/mediaTexels=NULL` restent fonctionnels.
- HP utilisateur **8 Ω** : impédance prévue pour le SC8002B, ne constitue pas une preuve de surcharge ; ne pas proposer d'emblée un nouveau haut-parleur.
- Le schéma classique CYD propose `R7=47 kΩ`, `R8=22 kΩ`, `R9=68 kΩ`. Des variantes de carte ont été rapportées avec R7/R8 de **0 Ω**, augmentant fortement le gain et la distorsion ; **la révision physique exacte n'est pas établie par le Serial**. Aucune intervention CMS/fer à souder demandée avant la confirmation du diagnostic par étapes.

**Nouveau test firmware non musical, une seule fois au premier START :** maintenir le même flux DMA I2S 16kHz/2×16 bits et le MIDI original depuis le PAK, mais insérer avant ses premières notes :
1. `[AUDIODIAG] stage=DAC-OFF` — **2 secondes**, les deux DAC désactivés, I2S envoie `0x8000`. Écouter si le brouhaha est déjà présent.
2. `[AUDIODIAG] stage=DAC-MIDPOINT` — **2 secondes**, seul GPIO26 DAC activé ; PCM **0x8000 constant**, donc silence numérique. Écouter le grésillement indépendant du MIDI.
3. `[AUDIODIAG] stage=MIDI` — chanson originale `5039.mid` à **sample 0**, inchangée, après les deux étapes. Mesurer `[AUDIO] PULSE speedPct≈100`, `dmaErrors=0`, `pcmPeak8` et `pcmClipped`.

Les états sont publiés par le cœur audio et annoncés dans la boucle principale. **Aucune fréquence de test synthétique, aucun second firmware, aucune source WAV/ZIP, aucun GPIO25, aucune allocation map-wide, aucune SD sur cœur 0.** Les quatre secondes de décalage au démarrage servent uniquement à isoler la sortie matérielle et ne représentent pas un ralentissement de tempo.

**Consignes de sécurité :** faible volume initial (25 % déjà enregistré), ne pas mettre le haut-parleur contre l'oreille ; si la distorsion est violente, écourter l'écoute. Fournir pour chacun des trois états « silence / bruit / craquement / musique ». Aucun PASS matériel ni changement physique de résistances tant que l'écoute n'a pas identifié la frontière.

---

## Deuxième écoute CYD 11 novembre 2026 — APLL cadence PASS, **audio FAIL bloquant**

Le log fourni sous l'intitulé « PR #305 » correspond à la branche **AUDIO-A PR #301** : firmware avec `esp_native_audio_midi.cpp`, et non au simple Root-J2 #305 déjà fusionné.

- `[AUDIOCLK] VERIFY source=APLL targetHz=16000 measuredHz=16000 elapsedUs=800000 frames=12800 result=PASS`.
- `[AUDIO] PULSE resource=5039 timeMs=11280 wallMs=11216 speedPct=100 fifo=24 late=8 dmaWrites=1127 dmaErrors=0`.
- `[AUDIO] MIDI-START resource=5039 file=5039.mid bytes=17163 tracks=3 ppqn=60 loop=1` depuis PAK ; volume utilisateur 25 %.
- Retour d'écoute utilisateur : **grésillement, sifflement aigu initial avant la musique et impression d'hyper-ralenti**. Le hardware n'est **pas** PASS pour AUDIO-A, malgré le PASS de débit DMA, `HEADROOM_OK` et les invariants natifs.
- L'ancienne lecture à `speedPct=547` était anormalement rapide. Le retour à 100 % peut expliquer une *impression comparative* de ralentissement, pas prouver que les événements MIDI eux-mêmes ont une cadence erronée. Ne pas multiplier artificiellement la vitesse du morceau.
- Le pré-test émet 8+80 tampons PCM au milieu du DAC (128) avant lecture. Un sifflement à cet instant, sans notes synthétisées, implique d'examiner l'étage DAC→SC8002B, l'alimentation/le gain et le flux PCM réellement reçu par le DAC, séparément des harmoniques synthétiques FM-lite. L'horloge mesurée via backpressure DMA ne mesure pas la sortie analogique.
- **État : PR Draft, NON PASS, ne pas fusionner.** Prochaine itération audio séparée et bornée : diagnostic d'un signal volontairement silencieux, d'une seule voix MIDI réelle et de la polyphonie/limiteur avec nouvelle validation par écoute sur le classic CYD. Aucun changement aux fichiers MIDI, au PAK, à GPIO25 ni au gameplay.

---

## Retour matériel 11 novembre 2026 — FAIL et correction candidate

- Le vrai CYD a confirmé `[AUDIO] MIDI-START resource=5039 file=5039.mid bytes=17163 tracks=3 ppqn=60` et les événements du PAK : 270 notes, 84 frappes de batterie et 8 changements de programme après la première séquence.
- **FAIL de cadence (ancien SHA)** : `[AUDIO] PULSE timeMs=79670 wallMs=14540 speedPct=547 dmaWrites=8015 dmaErrors=0 late=51`. Il ne s'agit PAS d'une musique à 16 kHz, malgré `i2s_get_clk=16000`.
- Diagnostic du pilote ESP-IDF v4.4.7 : la branche DAC utilise `I2S_LL_AD_BCK_FACTOR=2`, soit `mclk=sampleRate*2*16=512 kHz` à 16 kHz, mais l'horloge 160 MHz et le diviseur maître matériel 6 bits (max 63) ne peuvent fournir cette fréquence correctement. `i2s_get_clk` ne fait que retourner la demande logique, ce n'est pas une lecture de la cadence matérielle.
- **Correctif candidat** : `cfg.use_apll=true` pour générer l'horloge bas débit via l'APLL ; démarrage de la tâche audio sur cœur 0 avec une calibration **silencieuse de 8+80 buffers PCM** en réutilisant son unique tampon 640B, sans lecture SD ni second tampon audio.
- Le firmware n'active ensuite la synthèse que si `[AUDIOCLK] VERIFY source=APLL targetHz=16000 measuredHz=... elapsedUs=... frames=12800 result=PASS` indique entre **14 et 18 kHz mesurés** ; sinon `FAIL-CLOSED`, pas de lecture accélérée. **Le timestamp / la cadence doivent être confirmés sur le vrai CYD**, pas par la CI.
- Pas de changement de MIDI, d'instrument, du nombre de voix, de la topologie PAK, du DAC GPIO26, du volume ni du gameplay. Les timbres FM-lite restent une limite musicale réelle ; ce correctif ne prétend pas produire l'AdLib/General MIDI.
- Pendant la calibration (~0,8 s attendue), le cœur principal continue les menus et le tactile ; les 80 buffers sont exclusivement émis en silence sur le cœur audio.


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
