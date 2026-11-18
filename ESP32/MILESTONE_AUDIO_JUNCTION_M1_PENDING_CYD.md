# AUDIO-JUNCTION-M1 — correction du BSP loadMapId distinct du catalogue (PENDING CYD)

**Dépôt** : Kharn27/DoomRPG-RE. **PR** : #378, branche `agent/audio-junction-m-dead-20261118`. Base après synchronisation ARCH-RC10 : `main=ab79f2ece50ab297f49718a459e4744ee1c59768`.

## Échec réel observé et diagnostic

Extrait Serial communiqué par l'utilisateur le **18 novembre 2026** après LOAD V11 vers Junction (map catalogue 9) ; capture fournie inline, **aucun fichier source/log complet ni SHA firmware dans l'extrait**. Séquence :

- Ancienne piste Entrance `M_STALKS.wav` terminée avant staging : `FINAL reason=LOAD_START_BEFORE_BSP ... underruns=0 ... timer=OFF ... file=CLOSED`.
- BSP : `[BSPREAD] HEADER name='Junction' loadMapId=2` ; `[NATIVESAVE] REPRIME-HUD map=9 gameplayLoadMapId=2`, puis `LOAD ... map=9 gameplayLoadMapId=2`.
- Flash : `MAPFLASH READY map=9 ... verified=yes` ; `RAMBUDGET HEADROOM_OK` ; `SFXP3 END ... invalid=0`.
- **Échec avant SD.open** : `[LOADAUDIO-L1] PRESTART map=9 route=M_DEAD.wav ready=0 reason=owner_or_map_not_ready pakOpen=0 resident=1 flash=1 beforeWorldPresent=1`.
- La session Junction atteint tout de même `RESUME-VISIBLE map=9` et `ENGINESESSION READY map=9 ... shapeData=0x0 mediaTexels=0x0`. L'audio n'est pas lancé.

Cause dans le code précédent : le préstart exigeait `view->gameplayLoadMapId==targetMapId`, et le service principal exigeait `view->targetMapId==view->gameplayLoadMapId`. Ce sont deux identifiants **distincts** : carte catalogue native `/junction.bsp = 9` contre discriminateur interne du header BSP `loadMapId=2`. L'ancienne condition fonctionne fortuitement sur Entrance (1/1) et bloque Junction (9/2). Le log ne prouve **pas** que le fichier WAV est présent ni que son format est valide : aucune ouverture SD n'a eu lieu.

## Correctif minimal et tests

- Sélection explicite des paires `(catalog=1,bsp=1) -> M_STALKS.wav`, `(catalog=9,bsp=2) -> M_DEAD.wav` via `EspAudioLoadingRoute_forView`, sans allocation, sans élargir la sélection des autres maps.
- Le préstart **et** le service normal utilisent le même prédicat. Les mauvais couples, y compris `(9,9)`, `(1,2)` et `(10,2)`, restent fail-closed. Le log de refus expose maintenant `viewMap` et `bspLoadMapId` pour les futurs diagnostics.
- Oracle hôte C11 : matrice exhaustive de 256 IDs catalogue × 256 IDs BSP = **65 536 couples**, plus les tests L1 et les oracles historiques, CI normale/inverted/trace/bringup. Aucun rendu, RNG, map mutation, PAK, SDL, buffer, ISR ni wrapper ne doit changer.
- Piste SD externe `/doomrpg/music/opl/M_DEAD.wav` (fallback `/M_DEAD.wav`) : WAV **IMA-ADPCM mono 22 050 Hz**, blocs compressés au plus 1 024 octets.

## Prochain contrôle matériel obligatoire

En `esp32-cyd` ou `esp-cyd-inverted` suivant la carte utilisée, LOAD du slot V11 Junction (map=9, bsp=2) :
1. `[LOADAUDIO-L1] PRESTART map=9 route=M_DEAD.wav ready=1 ... beforeWorldPresent=1`, précédé de `[ENTRANCEMUSIC] OPEN ... M_DEAD.wav` et `WAV_OK` si la carte SD contient un fichier conforme ;
2. `[ENTRANCEMUSIC] PLAY ... track=JUNCTION` **avant** `RESUME-VISIBLE map=9` ;
3. absence d'underruns/recoveries anormaux et écoute humaine ; relance via LOAD/EXIT avec `file=CLOSED ring=FREE timer=OFF` avant BSP.

**État** : correction code et preuves CI à consigner avant **test réel** ; **pas de PASS CYD** tant que le Serial et l'écoute ne sont pas confirmés. Ne pas fusionner sans accord utilisateur.
