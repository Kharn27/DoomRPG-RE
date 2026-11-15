# AUDIO-R2a — M_STALKS.wav 22,05 kHz IMA-ADPCM, test CYD en attente

**15 novembre 2026 — nouvelle branche expérimentale :** 
`agent/esp32-audio-r2a-entrance-sd-pcm`

**Base exacte :** AUDIO-R1l validé sur hardware, `1f5cb1fa8f7128c83748b296aaca9f50f59503dc`
(PR #342 toujours draft). Ce travail est une **PR empilée** sur la branche audio
et non un merge dans `main` ; le main GitHub observé au départ
est `c69844364118a56b30503065aeed95fbfea70a27` et a pu évoluer.

## Correction impérative du choix musical

**Dernière précision utilisateur, prioritaire sur l'ancien plan :**
le fichier destiné à Entrance **s'appelle exactement `M_STALKS.wav`**,
et c'est **la version 22**, c.-à-d. le **WAV IMA-ADPCM mono 22,05 kHz**
mentionné dans l'issue #261 (pas le PCM16 mono 16 kHz,
pas `entrance.wav`). Ce fichier est un dérivé d'une composition de
Doom II, fourni par l'utilisateur ; **ne pas committer le WAV ni son MIDI**.

Copier le fichier **tel quel** sur la carte microSD, soit :
- `/doomrpg/music/opl/M_STALKS.wav` (emplacement prioritaire) ;
- `/M_STALKS.wav` (fallback racine pour essais).

Aucun renommage, export vers 16 kHz ou synthèse MIDI embarquée.
Le lecteur vérifie l'en-tête du WAV réel au runtime ; **le fichier original
n'a pas été fourni aux outils de compilation**, donc il ne peut pas être
déclaré préalablement compatible sans son véritable en-tête.

## Frontière technique

- À la première session **Entrance uniquement** (`EspPlayerView` map ID 1),
  et à condition que SD et PAK aient déjà démarré, ouvrir un seul fichier
  WAV séparé de `DoomRPG-ESP32.pak`.
- Parser RIFF strict jusqu'à 512 octets, `fmt=0x11`,
  mono 22050 Hz 4 bits, extension `samplesPerBlock`, bloc
  ≤1024 octets, `fact` valide et `data` borné.
  Refuser PCM16 16 kHz, stéréo, format extensible et fichiers malformés.
- Un propriétaire SD sur la boucle jeu : lire **un seul bloc compressé par
  service** (≤1024 octets), décoder dans un scratch statique
  ≤2041 échantillons et publier dans un ring de **8192 octets** de RAM
  interne. Pas de morceau chargé intégralement en RAM, aucune musique dans
  le PAK ou le map-flash staging.
- Convertir PCM16 décodé en DAC8 à **amplitude initiale prudente ±64 codes**,
  fondu d'entrée 200 ms ; échantillons émis par **timer matériel TG1/T1**,
  APB/2 puis alarme 1814 ticks (~22050,7 Hz), vers DAC2/GPIO26.
  ISR IRAM, seulement accès RAM/registre DAC, **aucune lecture SD/I²S/DMA**.
- Préflight cadence **DAC OFF** ~1 s puis fail-closed hors [20900,23200] Hz ;
  sous-alimentation ring=son 128 et arrêt immédiat au service suivant,
  lecture courte=arrêt, manque mémoire=arrêt. Aucune réinitialisation après
  erreur dans la même session ; arrêt + libération intégrale à la sortie
  du gameplay ou du niveau.
- Une musique bouclée ; **pas de SFX mixés, volume options, GM, ADPCM multicanal,
  Bluetooth, ZIP, AUDIO-R1l 440 Hz ni lecture sur Intro**.
  Interactions jeu/tactile/rendu restent prioritaires.
- Mémoire au premier essai : garde heap8 >= 61440 et plus grand bloc >=24576
  avant lecture ; après ring heap8 >=57344 et plus grand >=16384.
  Mesures `[ENTRANCEMUSIC]` : OPEN/WAV_OK/PRIMING/PREFLIGHT/CLOCK/PLAY/STREAM/FINAL,
  heap8, appels SD, durée max de lecture, underruns, gaps ISR et boucles.
- **À vérifier sur CYD réel avant PASS :** compatibilité de l'en-tête du
  WAV personnel, lecture audible sans crépitement ni lenteur, tête de RAM
  pendant Entrance, beaucoup de déplacements/porte/tactile, stop menu
  pendant lecture, éventuels désaccords SPI entre PAK et SD, et endurance
  au-delà de huit secondes. Aucune validation acoustique ni de streaming
  réel anticipée à partir de la CI.

**État initial R2a : implémentation et CI alors validées, écoute matérielle encore en attente. Les observations et la suite supersèdent cet état initial ci-dessous.**

## AUDIO-R2a — premier retour CYD et preuve différentielle (15 novembre 2026)

- Log réel `platformio-device-monitor-261115-221553.log` : `WAV_OK` pour le fichier **personnel inchangé** `/doomrpg/music/opl/M_STALKS.wav`, mono IMA-ADPCM 22050 Hz, 3238912 octets ADPCM, 6455296 échantillons, 292757 ms ; `CLOCK measuredHz=21906`, `PLAY` atteint.
- `SIGNAL at=3s` : témoin synthétique **440 Hz 2 s** écrit en ISR, `witness=44100`, DAC envoyé sur une plage 64..192 ; les premiers 4096 octets DAC8 produits/consommés ont un FNV identique `cbd79e2b`.
- `STREAM` vers 20 s : `frames=440552`, `ringConsumed=396625`, `underflows=0`, `gapsOver90us=0`, `maxSdReadUs=3839`, `heap8=68644`. Pour la musique **avant tout correctif de gain**, `pcm16MinMax=-1689..2252`, `isrRingMinMax=125..132` et **aucun** code à plus de ±8 du neutre. Le ring et l'ISR sont donc fonctionnels, mais le signal music DAC8 est trop faible.
- Retour acoustique utilisateur, distinct de la CI : après débrancher/rebrancher complètement USB, la **branche R1l a retrouvé son bip**, puis sur R2a le **bip de 2 s est audible et la musique est « HYPER faible »**. Ne pas attribuer la réinitialisation requise au seul logiciel sans nouvelle preuve électrique. Le bip audible exclut une panne DAC2 permanente dans ces conditions.

## AUDIO-R2b — correctif de niveau, en attente de reflash réel

- **Changement ciblé de quantification** : `pcm16/512` → `pcm16/64` (**gain fixe ×8**, ~18 dB), et **limiteur** ±64 codes DAC avant fondu 200 ms. Aucune modification du WAV, du décodeur nibble/step, du ring 8192 B, du timer 22050 Hz, du DAC2 GPIO26, des API SD, de la musique de Doom II ou de l'I²S.
- Ajout `gainLimited` : nombre de PCM16 ayant dépassé le seuil correspondant au limiteur, dans les logs `BLOCK0`/`SIGNAL`. C'est un essai prudent et mesurable, **pas** une normalisation de tout le morceau ni une preuve de non-saturation sur 292 s.
- Branches : **#342 fusionnée** dans `main` au SHA `a0a730306987806ff03b1a8730a29f1fe5f9e557`, #347 **rebasée sur ce SHA**, toujours draft. Relecture complète de la CI du HEAD R2b obligatoire avant flash.
- **À tester** sur firmware normal `esp32-cyd` : bip 2 s, volume réel de `M_STALKS.wav`, distorsion éventuelle, `gainLimited`, sous-alimentation, RAM/tactile et sortie menu. Le code/CI ne remplacent pas le PASS acoustique.
