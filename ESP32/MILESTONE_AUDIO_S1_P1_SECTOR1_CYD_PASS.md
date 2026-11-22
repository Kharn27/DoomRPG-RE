# AUDIO-S1-P1 — Sector 1 M_E1M3 et bruitages natifs (PASS matériel borné)

Date : **22 novembre 2026**. [Issue #427](https://github.com/Kharn27/DoomRPG-RE/issues/427), [PR code-only #428](https://github.com/Kharn27/DoomRPG-RE/pull/428). Base GitHub main `1588b6983cc5724f7028ac0d2c18f96688aba990`. Dernier HEAD **code** attribué au test par le contexte utilisateur : `319b340024e03c1d568350342569205a474174d2`. Le Serial n'imprime pas l'empreinte du binaire : aucune preuve indépendante de SHA firmware.

## Frontière de code P1

Le map catalogue **2** est `/level01.bsp`, avec **BSP loadMapId 3**. `EspAudioLoadingRoute_forView(2,3)` choisit `M_E1M3.wav` ; mauvais couples fail-closed. Chemin SD primaire `/doomrpg/music/opl/M_E1M3.wav` et fallback `/M_E1M3.wav` si le primaire manque. Contrôle IMA-ADPCM mono 22 050 Hz à l'ouverture. Routes Entrance (1,1)/M_STALKS, Junction (9,2)/M_DEAD, intro et menu intactes.

Réutilisation du lecteur SD unique, du ring de **16 384 B INTERNAL/8BIT**, du DAC2/GPIO26 et du mixeur FIFO/SFX existants. Pas de deuxième producteur, moteur/thread, buffer audio, lecture ZIP runtime ou changement du rendu. Tests C11 avec **65 536 paires catalogue/BSP**, fixtures WAV invalides et tests audio historiques. CI originale code-only [push SUCCESS #45011325457](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45011325457) : **234 étapes**, builds `esp32-cyd` et `esp-cyd-inverted`. Double CI après ajout temporaire des WAV [push SUCCESS #45011748790](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45011748790) / [PR SUCCESS #45011755012](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45011755012) (234 étapes chacune). CI finale après cette documentation à vérifier séparément.

## Logs matériels utilisateur

- **Principal** : `platformio-device-monitor-261122-111340.log` — **1 570 lignes, 193 618 octets, SHA256 `8532cc2abc4a2abe0f524bf24ff480b644065e56e98b1ad09d0875b067dcb7ae`**. Véritable classic CYD ESP32-D0WD-V3 240 MHz, 4 MB flash, PSRAM 0 ; profil normal `esp32-cyd` selon protocole du test.
- **Secondaire** : `platformio-device-monitor-261122-112136.log` — **167 lignes, 12 855 octets, SHA256 `e00e0d3432a6a454bee43572fd532df938e20de567c6bd264cfd888fb5f84529`** ; uniquement boot/menu, pas de LOAD Sector 1.
- Menu : `M_DM2TTL.wav` WAV_OK/PLAY. LOAD V11 slot02 (5 604 B) : map catalogue **2**, `/level01.bsp` header `loadMapId=3`.
- Sector 1 : `OPEN M_E1M3.wav` ligne 398, `WAV_OK` 399, `PRIMING ringBytes=16384 heap8=50116 largest8=32756` 400, `PLAY track=SECTOR1` 404 et **`PRESTART ready=1 beforeWorldPresent=1 failClosed=0 elapsedMs=1106`** 405 ; avant la première image jouable (407).
- Déplacement, portes OPEN **5063** / CLOSE **5064**, ramassage **5060**, arme **5057**, tirs/monstres : **12 SFXGEN START + 12 END**, **10 SFXP1 MIX**. Clôture FIFO ligne 1547 : **`consumed=23 sfxPlayedTrack=23 sfxRejectedTrack=0 queueOverflow=0 consumerCursorGap=0 audibleSfxDropped=0 handoffDiscarded=3 pending=0`**. Trois intentions de transition purgées intentionnellement, sans événement audible comptabilisé perdu.
- Cinq `STREAM` (10/20/30/40/50 s) : **`underflows=0 recoveries=0 maxSilentFrames=0`**. `FINAL` Sector 1 ligne 1546 : **1 162 305 frames (~52,7 s)**, `underruns=0 recoveries=0 gapsOver90us=0 maxPublishGapMs=447 gapsOver500Ms=0 maxSdReadUs=4295 slowReadsOver20ms=0`, fichier `CLOSED`, timer et DAC arrêtés.
- Budget mémoire : `HEADROOM_OK margin8=13548` ligne 290 ; environ 50 Ko heap8 libre pendant la musique ; `shapeData=0x0 mediaTexels=0x0` ; `RESIDENTRESET ... empty=1` ligne 1540, musique menu relancée ligne 1555 et `SYSEXIT MENU-READY` ligne 1570. Aucun panic/WDT dans la capture. L'utilisateur confirme l'écoute satisfaisante de la musique originale ; test acoustique limité aux actions jouées.

## Actifs externes et limites

L'utilisateur a ajouté **15 WAV**, soit **36 054 402 octets / 34,38 MiB**, après le test. Ils sont conservés sur **`agent/esp32-audio-s1-wav-assets`**, commit `2eac3ad75c4abff91a523762c791ef6b3ba89c81`, et **exclus de la PR #428 code-only** (pas d'ajout au PAK). Audit de format et décodage complet [GitHub Actions SUCCESS #45012162032](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45012162032) : **15/15** compatibles IMA-ADPCM mono 22 050 Hz ; aucune preuve acoustique de l'absence de souffle DAC. Les fichiers originaux ne doivent pas être diffusés via main sans vérification des droits.

**P1 ne corrige pas le mode SFX-only.** Lorsque SOUND OFF, piste absente, invalide ou désactivée, les bruitages peuvent encore être silencieux : **AUDIO-S1-P2 est explicitement reporté** à la demande utilisateur, sans test matériel artificiel ni retouche du firmware. Aucun essai de panne SD, SOUND OFF ou multi-map endurance sur CYD revendiqué ; comportement négatif couvert en host.

**Verdict : PASS matériel borné de P1**, aucune anomalie dans les mesures audio/ROM/RAM/EXIT observées. Après ce PASS, commits **documentaires uniquement**. Fusion autorisée par l'utilisateur après isolation des WAV, sous réserve CI finale et contrôles de contenu de PR.
