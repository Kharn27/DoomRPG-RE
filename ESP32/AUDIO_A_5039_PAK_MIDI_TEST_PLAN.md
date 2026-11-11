# AUDIO-A — MIDI 5039 original directement depuis le PAK (candidat, NON PASS CYD)

**Objectif accepté : aucune conversion préalable de MIDI vers WAV/ADPCM.**
La piste `5039.mid` du PAK natif est l'unique source de données musicales.
Le test réel du classic CYD reste nécessaire avant de fusionner.

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
