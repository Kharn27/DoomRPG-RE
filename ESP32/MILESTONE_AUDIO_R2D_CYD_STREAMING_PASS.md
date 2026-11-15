# AUDIO-R2d — transport SD/ADPCM/DAC2 : PASS matériel, qualité finale à parfaire

**Date :** 15 novembre 2026 — classic CYD ESP32-2432S028R sans PSRAM, firmware normal `esp32-cyd`.
**Branche :** `agent/esp32-audio-r2a-entrance-sd-pcm`, [PR #347](https://github.com/Kharn27/DoomRPG-RE/pull/347) (draft, pas de fusion automatique).
**Commit source matériellement testé :** `efae363b8a9c9b2976a4dd5f1f353b699c92c3f3`.
**CI source :** [push 43769305837](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43769305837) **175/175 SUCCESS** et [PR 43769309516](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43769309516) **SUCCESS**.

## Preuve CYD

Log original **hors Git** : `platformio-device-monitor-261115-233102.log`, **1657 lignes**, SHA-256 `d16afa4a15ec1d8c2ccfbd963b4f9142cc5488ba22dcba52634669b7ca1e818c`.

- **Deux** lectures du WAV utilisateur inchangé `/doomrpg/music/opl/M_STALKS.wav` (nouvelle partie Entrance, puis sauvegarde V11 chargée et gameplay). `WAV_OK` : IMA-ADPCM mono 22050 Hz, blockAlign 1024, samplesPerBlock 2041, 6455296 frames, durée totale théorique 292757 ms.
- **Témoin bip 440 Hz supprimé** : `PLAY witness=none source=SD-ring` lors des deux lectures. Le préflight horloge DAC OFF est conservé, avec `CLOCK measuredHz=21941` puis `21929` (mesures de polling de préflight).
- **AUDIO-R2d élimine les sous-alimentations observées en R2c** : première session `frames=1564383` (environ 70,95 s à 22050 Hz), deuxième `frames=800251` (environ 36,29 s), **0 underrun, 0 recovery, 0 maxSilentFrames, 0 gapsOver90us** pour chacune. `maxGapCycles=20961` / `21642` (≈87,34 / 90,18 µs à 240 MHz), `maxIsrCycles=475` / `469` (≈1,98 / 1,95 µs). Légère réserve : le compteur gap `>22000` ne signifie pas « absence de toute irrégularité sous 90µs ».
- Le transport entre images a réellement servi : `pumpCalls=195` / `148`, `pumpReads=301` / `222`. Ring 16384B, minimum publié en STREAM `minQueued=11378` première session (≈516ms), `14154` deuxième (≈642ms). Ce sont les minima observés par la boucle service, pas une mesure ISR à chaque tick.
- Flux SD : `reads=770` / `396`, `maxSdReadUs=4256` / `4153`, `slowReadsOver20ms=0`. Environ 10.8–10.9 kB/s compressés théoriques, puis DAC à ~22 kHz. Le préfixe musique 4096 codes est identique producteur ↔ ISR `ffec5814` dans les deux sessions.
- **Sorties propres** : deux `[SYSEXIT] MENU-READY`, deux `[ENTRANCEMUSIC] FINAL reason=SESSION_EXIT ... timer=OFF dacPad=OFF file=CLOSED`, et `[RESIDENTRESET] ... empty=1`. Le retour menu final montre `heap8=127152 largest8=86004`, identique après les deux arrêts audio. Pas de panic/WDT dans les 1657 lignes.
- **RAM** : avant allocation musique, `[RAMBUDGET] LAZY_POST heap8=82508` / `81212` et `largest8=61428`; après `[ENTRANCEMUSIC] PRIMING heap8=60920` / `60432`, `largest8=45044`; en STREAM minimum `heap8=56808`, à l'ALIVE minimum `heap8=57252`. Les 16Kio du ring sont bornés et internes. Les anciens libellés `audioI2SDMA` dans RAMBUDGET sont des cibles estimatives héritées et **ne prouvent pas qu'I2S/DMA soit utilisé** : R2d utilise le timer DAC2 direct.
- **Gameplay physique** : déplacements, rotations, porte animée, ramassages, dialogues, HUD/HUB, save/exit/reload ; séquence d'attaque Malwrath et projectiles présente dans la deuxième session. Pas de slowdown ressenti. `shapeData=mediaTexels=NULL` conservés et PAK natif intact.

## Retour acoustique utilisateur (distinct des compteurs)

« Pour 100% il manque encore un poil » ; « pas de saturation ou autre mais toujours un petit bruit de moirage, très léger par rapport à la musique mais perceptible » ; « bien fluid », gameplay « carré ».

**Statut strict : PASS CYD transport / absence d'underruns / cohabitation gameplay / 2 × session exit / RAM raisonnable ; qualité sonore subjective non finalisée.** Le DAC8 a transmis `isrRingMinMax=27..235`, `pcm16MinMax=-1689..2252`, `softKneeFrames=844` sur ~1,57M samples produits en première session, donc la source originale reste faible mais le digital atteint déjà un haut de plage. Le log ne prouve **ni absence de bruit analogique, ni origine** du léger moirage, ni saturation sur la totalité de la piste (292,76s), ni endurance sur toutes les cartes.

## Décision de conservation / piste AUDIO-R2e

1. **Ne pas modifier R2d au sein de son verdict de référence** : gain PCM16/16, soft knee ±96→±112, volume session 100%, ring 16Kio et pompe inter-images validés sur matériel.
2. Une **petite** majoration de niveau en prochain jalon ne doit pas se confondre avec un gain libre/rail DAC : marge de pic observée jusqu'à code 235/255. Vérifier le bruit et l'absence de saturation avec une comparaison A/B sur CYD avant de conserver un nouveau gain (envisager ~+1dB seulement, avec soft knee borné).
3. **Identifier le bruit** par observation acoustique distinguant signal, SD/écran/USB/alimentation, sans supposer la cause. Le DAC 8 bits, la quantification et le WAV ADPCM sont des hypothèses, pas des conclusions tirées du Serial. Ne pas introduire de dither ou de gros filtre sans témoignage A/B qui le justifie. Pour une qualité source meilleure, une nouvelle conversion offline à partir du PCM original demanderait un milestone distinct, sans publier les assets musicaux protégés.
4. Pas de PSRAM, de lecture ZIP runtime, de mix SFX ni de tâches SD concurrentes ; préserver GPIO25 tactile. Pas de merge sans instruction explicite.
