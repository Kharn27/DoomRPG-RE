# ARCH-RC5 — direction HUD et projection joueur par hook natif explicite : PASS CYD

**17 novembre 2026 — [PR #366](https://github.com/Kharn27/DoomRPG-RE/pull/366), [issue #340](https://github.com/Kharn27/DoomRPG-RE/issues/340)**. Branche `agent/esp32-arch-rc5-hud-direction-posthook` repartie du vrai `main` après [ARCH-RC4 #363](MILESTONE_PR363_ARCH_RC4_READ_RANGE_OBSERVER_CYD_PASS.md), SHA `174a22fed5a5d254fdeb6a9f576be8fd8d955953`. **Dernier code/test flashé présumé : `4390537a0d6802cee80b4e23a3c8ecac28dbef7c`** ; le firmware Serial n'imprime pas un SHA Git, donc son identité est fondée sur la branche/commit soumis à l'essai utilisateur, et non sur une attestation embarquée.

## Frontière unique retirée

`-Wl,--wrap=EspNativeGameplayHudDirection_render` a été retiré de `ESP32/platformio.ini`. L'ancien wrapper de `esp_native_gameplay_projection_overlay.c` appelait d'abord le peintre natif de la boussole, puis `repaintPlayerHud(angle, outStats)` en préservant `[PLAYERHUD] REFRESH-FAILED` et un résultat fail-closed. Ce post-traitement devient `EspNativeGameplayProjectionOverlay_afterHudDirectionRender(angle, outStats)` déclaré dans le nouveau header C `esp_native_gameplay_projection_overlay.h` et explicitement appelé depuis `EspNativeGameplayHudDirection_render()` **après** fermeture éventuelle de la lease PAK, propagation des statistiques et `free(scratch)`, uniquement si le rendu initial a réussi. Rendu boussole, projection live-player, conditions de rafraîchissement et erreur de projection inchangés. Pas de nouvel objet heap, de modification de gameplay, PAK/flash, audio, matrice LCD, framebuffer, sampling mural R-B ou mixage SFX.

Les candidats plus risqués (`EspAssetPack_residentBegin`, `Esp32PlatformVideo_present`, wrappers de lignes animées et `PlatformInput_setTapCallback`) restent hors périmètre.

## Host / CI avant test

**Double CI sur le même commit code `4390537a0d6802cee80b4e23a3c8ecac28dbef7c` :**
- [push 43992997441](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43992997441) **SUCCESS** ;
- [PR 43993006010](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43993006010) **SUCCESS**.

L'oracle `ESP32/test/native_hud_direction_posthook_host.py` extrait le **vrai C++/C source** (rendu direction, cinq helpers et posthook) et le lie à des doubles bornés de PAK/BMP/platform : **12 scénarios** dont chemin normal, post-traitement refusé, malloc/PAK/bitmap/blit/input invalides, cleanup, plus **12 mutations adversariales** repoussées. Il ne mesure pas une parité de pixels avec le desktop, et n'est pas un test physique. Inventaire des **ELF Xtensa réels** (declared/linked/directCallWitness, pas preuve d'exécution) : normal **41/41/41**, inverted **41/41/41**, bringup **44/44/44**, trace **41/41/41**. Le moteur compilé par les deux profils release est identique hormis réglages LCD. Trois interpositions de logging toolchain distinctes.

## Test réel CYD — PASS matériel borné

Fichier complet utilisateur `platformio-device-monitor-261117-021636.log`, **1762 lignes, 241112 octets**, SHA-256 **`036ba6168c57dc1984ece9d3a31046a154a401f68d2c5dbedaf42cc18f321af0`**. ESP32-D0WD-V3, dual-core 240 MHz, **4 MB flash, aucune PSRAM**, écran ILI9341 320×240, framebuffer partagé RGB565 **160×120 / 38400 B**, `[VIDEO] ... alternate-panel inversion=off` (profil alternatif compatible inverted). Le log ne cite pas le nom d'env PlatformIO au boot ; son identification `esp-cyd-inverted` repose sur le build demandé et le profil écran.

**Deux sessions sur Entrance /intro.bsp** : 
1. Menu -> nouvelle partie -> intro -> loading 8 frames, `TRANSITIONLOAD RELEASE progress=100` après **5501 ms**, `LOADAUDIO-L1 PRESTART ready=1 beforeWorldPresent=1`, `ENGINESESSION FRESH-VISIBLE` puis `ENGINESESSION READY map=1 angle=64 ... shapeData=0x0 mediaTexels=0x0`. Réutilisation de map-flash `REUSE HIT`. Nombreux `RA2B FRAME` et HUD natif en jeu. **Cinq rotations cardinales** en jeu : 64→0, 0→64, 64→128, 128→64, 64→0, toutes `committed=yes`, avec animation intermédiaire. Gameplay réellement exercé : déplacements, casse de caisse, pickup d'armure/medkit/munitions/Fire Ext, dialogues opcode 8 et 26 et chaîne 19/11, portes avec animations ouverture et fermeture, HUB puis EXIT sans SAVE.
2. Menu -> LOAD slot `/DoomRPG-ESP32-slot04.sav` **V11, 5604 B**, restore joueur/world/lines/script/crates/monsters/topology, `TRANSITIONLOAD RELEASE progress=100` après **4011 ms**, map-flash `REUSE HIT`, `ENGINESESSION READY map=1 angle=0 ... shapeData=0x0 mediaTexels=0x0`. Une rotation 0→64 `committed=yes`, mouvements, changements d'arme/HUD, portes (dont passage secret), pickups, dégâts joueur / retal sur monstre, HUB puis EXIT sans SAVE.

**Deux nettoyages complets et symétriques en octets** :
- 1ère session : `[RESIDENTRESET] heap8=88192->106200 released=18008 before=1/1/1/1/1/1/1 after=0/0/0/0/0/0/0 empty=1`, puis `[SYSEXIT] MENU-READY ... resident=empty saveWrite=no checkpoint=unchanged`.
- 2ème session : `[RESIDENTRESET] heap8=88204->106212 released=18008 ... after=0/0/0/0/0/0/0 empty=1`, puis second `[SYSEXIT] MENU-READY ... resident=empty saveWrite=no checkpoint=unchanged`.

En fin de chaque session, `[ENTRANCEMUSIC] FINAL reason=SESSION_EXIT` pour `M_STALKS.wav`, respectivement **655674** et **663390** frames, **0 underrun**, **0 recovery**, **0 gap>90µs** ; `timer=OFF dacPad=OFF file=CLOSED`. `heap8After=127052 largest8=86004` **identiques après les deux stops audio**. Intro `M_IN_CIT.wav` : 110263 frames et zéro underrun/recovery. `[RAMBUDGET] RESERVE ... HEADROOM_OK` deux fois ; minimum **échantillonné** par `[ALIVE]` en gameplay : **59564 B**, puis **60236 B** (`largest8=45044`), pas une preuve de pic RAM absolu. Zéro occurrence de `[PLAYERHUD] REFRESH-FAILED`, panic/Guru Meditation/WDT/stack canary/fatal `TURNFRAME DIAG` dans la capture ; **12 SFXP1 MIX** observés, sans allocation heap dédiée.

**Conclusion : PASS matériel RC5** pour ce scénario INFO/inverted, avec les directions effectivement exercées, les interactions et le teardown répété. Les angles testés ne couvrent pas de façon exhaustive les quatre orientations cardinales dans une même session ; pas de pixel-golden desktop, pas de flash MISS/re-stage, autre carte, échecs PAK runtime, ou validation d'un écran physique par comparaison d'image. `shapeData` et `mediaTexels` restent NULL dans les checkpoints tracés. Aucun post-traitement d'erreur forcée sur CYD : la voie fail-closed est justifiée par l'oracle hôte.

Après test : **documentation seulement** via ce milestone + premiers paragraphes de `PORTING_STATUS.md` et `DOCUMENTATION.md`, ne pas modifier le binaire/test flashé. Vérifier par comparaison Git le diff docs-only et **double CI SUCCESS** sur le HEAD documentaire avant fusion autorisée par l'utilisateur. Après fusion, relire `main` GitHub + SHA exact avant toute branche ARCH-RC6.
