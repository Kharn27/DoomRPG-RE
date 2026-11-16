# AUDIO-S1/I1 — SOUND ON/OFF, DAC idle dès MENU et musique M_IN_CIT pendant ST_INTRO (CYD PENDING)

**16 novembre 2026**, [issue audio #261](https://github.com/Kharn27/DoomRPG-RE/issues/261). Après fusion autorisée de [PR #358](https://github.com/Kharn27/DoomRPG-RE/pull/358) (main exact `d51afd5b72b98ecd549ffd9908b2f2e62c011bf4`), une seule branche nouvelle `agent/audio-sound-menu-toggle-idle-dac`, préparée pour le **firmware normal esp32-cyd**, **aucun PASS matériel sur cette branche à ce stade**.

## Frontière S1 : vrai sous-menu SOUND

Avant : Options -> SOUND ouvrait directement les 5 cartes de volume (BACK/25/50/75/100), runtime uniquement. Après : **Options -> SOUND -> BACK / ON (ou OFF selon état actuel) / VOLUME -> BACK / 25 / 50 / 75 / 100**.

- La carte ON/OFF affiche l'**état courant**, avec la même sélection en **deux taps relâchés** que la page Options ; sa confirmation bascule ON ↔ OFF. BACK retourne à Options. La carte VOLUME ouvre **le vrai écran 5 cartes préexistant**, avec les mêmes 25/50/75/100 et deux taps ; son BACK et sa confirmation retournent désormais à **SOUND**, non directement à Options.
- UI bornée à la colonne de droite, même framebuffer 160x120, même modèle natif `MENU_MAIN_OPTIONS`, aucun nouvel objet desktop, aucun framebuffer ni texture.
- Nouveau `EspNativeAudioIntent_isSoundEnabled/setSoundEnabled` : **ON par défaut au boot**, **OFF coupe complètement le lecteur de musique**, indépendant de `musicVolumePercent` qui garde sa valeur sélectionnée. Ces deux réglages sont **session-only** ; la config native SD V3 (12 B) ne change pas, le prochain reboot remet SOUND ON et music 100 %.
- Laisser DAC2/GPIO26 **ON au point numérique 128** dès `setup()`, sans timer ni ring ni lecture SD : `[AUDIOIDLE] ON dacPad=ON code=128 timer=OFF buffer=0`. Quand SOUND OFF, passer au milieu puis désactiver DAC2 : `[AUDIOIDLE] OFF dacPad=OFF`. Ce maintien de pad ne fait **pas** démarrer plus tôt la musique ; il évite seulement d'activer tardivement l'étage de sortie. Pendant la lecture, l'ISR 22050Hz reste seule propriétaire du DAC.

## Frontière I1 : morceau d'intro natif

- Lorsque la vraie clock `ST_INTRO` est active, le lecteur 22.05kHz choisit `/doomrpg/music/opl/M_IN_CIT.wav` (fallback `/M_IN_CIT.wav`) au lieu de `/doomrpg/music/opl/M_STALKS.wav` (fallback `/M_STALKS.wav`).
- **Le fichier M_IN_CIT n'est PAS dans Git/PAK** : le placer sur la microSD à `/doomrpg/music/opl/M_IN_CIT.wav`. Format exigé exactement **WAV IMA-ADPCM mono 22050Hz**, RIFF+fmt+fact+data, blockAlign <=1024 (généralement 1024) et samplesPerBlock correspondants, vérifié par le même parseur natif fail-closed que M_STALKS. S'il manque ou a un format inconnu : `[ENTRANCEMUSIC] SILENT reason=missing_file|invalid_wav`, **pas** de crash, pas de lecture ZIP.
- Le lecteur réutilise **strictement le même** ring 16384B, les mêmes buffers bornés, la même ISR DAC2, les mêmes garde-mémoire et clock/preflight mesurée. Il n'y a **qu'une seule piste à la fois**. Les intentions SFX PAK sont désactivées pendant le morceau de l'intro.
- **Important frontière SPI/RAM** : au tap `[INTROIN] FINAL-CONTINUE`, `EspEntranceMusic_stopIntroAtHandoff()` libère le timer, la SD `File` et les 16KiB avant `Esp32IntroClock_park("intro-exit-ready")`, et donc **avant** le chargement de `/intro.bsp`, la préparation de la flash map, le cache et la transition. Log `[INTROMUSIC] HANDOFF musicClosed=yes mapLoaderMayStart=yes`.
- Après l'intro, un vrai `M_STALKS.wav` distinct est rouvert à l'entrée dans Entrance, avec le même preflight timer 1s. **La musique ne démarre pas encore pendant l'écran de chargement** : cette option serait un jalon distinct nécessitant une preuve de budget RAM et d'ownership du lecteur lors des phases flash / BSP. Ne pas la mélanger avec S1/I1.
- Le DAC maintenu ON ne retire pas les ~1-2 s de startup musical post-load : clock calibration, priming SD, et scheduling sont conservés.

## Non-régressions et validation

- Aucun changement de jeu, géométrie, rendu/animation, ZIP runtime, native config V3, musique R2e gainDiv=14, gain SFX G1, compteur `SFXGAIN`, ring 16KiB, timer, ISR, occupation SD/PAK pendant gameplay ni deux builds normal/inverted (même code).
- Tests hôtes C11 : SOUND 0/1, invalid-state fail-closed, volume retenu OFF puis ON, reset d'intentions indépendant. Tests statiques : UI SOUND 3 cartes + 5 cartes, peinture de rail, pas de nouveau menu legacy, DAC point silencieux sans timer, WAV intro exact, libération pré-BSP, séparation SFX/intro, API existante de M_STALKS inchangée. CI sur 4 profils.
- **Test CYD normal à effectuer** : depuis MENU_MAIN, Options -> SOUND -> vérifier état ON -> double-tap OFF -> revenir en arrière -> double-tap ON ; ouvrir VOLUME -> choisir 50 % -> vérifier retour à SOUND puis Options, EXIT ; vérifier `[AUDIOSWITCH]`, `[AUDIOIDLE]`, `[OPTIONSSOUND]` et cohérence volume au démarrage du jeu. Avec `M_IN_CIT.wav` conforme sur SD, Start Game : morceau en ST_INTRO, `[ENTRANCEMUSIC] PLAY ... M_IN_CIT.wav`, `FINAL reason=INTRO_EXIT_BEFORE_MAP`, `[INTROMUSIC] HANDOFF`, puis `[BSPREAD]`, `[MAPFLASH]` et enfin `PLAY ... M_STALKS.wav`. Vérifier `underruns=0`, heap8/largest8 et le gameplay normal avec SFX. Faire aussi une séquence Sound OFF -> Start Game -> intro silencieuse -> Entrance silencieuse. Sur `esp32-cyd-inverted`, même logique audio doit compiler/être identique.
- Si la musique de l'intro est absente : le test des gardes/menu reste possible, mais l'audio I1 n'est pas matériellement validé. Si le démarrage pendant les animations de l'intro perd des frames ou la RAM, rollback et isoler un milestone de service.

**Statut : PENDING test réel sur CYD. Ne PAS fusionner avant le retour matériel.**
