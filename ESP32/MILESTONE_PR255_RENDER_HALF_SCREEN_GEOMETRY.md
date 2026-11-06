# PR #255 — retrait des intermédiaires de géométrie desktop : PASS matériel CYD

Date : 6 novembre 2026. [PR #255](https://github.com/Kharn27/DoomRPG-RE/pull/255) ; branche `agent/esp32-retire-render-halfwidth-fracheight` ; main initial exact `fca4118f5efc595519380eadb06bcc18541306ec`.

## Portée validée

Le code natif supprime seulement `Render_t::halfScreenWidth` et `Render_t::fracHalfScreenHeight`, inutilisés par les renderers ESP32 ; champs desktop et desktop `Render_setup` inchangés. `halfScreenHeight` et **`fracHalfScreenWidth = ((screenWidth / 2) << FRACBITS) - 0x8000`** restent présents et conservent le résultat arithmétique antérieur. Aucune altération voulue de projection murs/sprites, cadrage, ordonnanceur, animation, gameplay, RNG, PAK, framebuffer ou save. `sizeof(Render_t)` : **192 → 184 octets**, retrait de huit octets de structure et non benchmark de heap. Source/ABI tests sur `src/Render.h`, `ESP32/src/engine_stubs.c`, `ESP32/src/render_startup_bridge.c`, `ESP32/test/native_render_setup_retirement_host.py` et `ESP32/test/native_render_raster_abi_retirement_host.py`.

Code testé sur CYD : **`334dadd333ffc7f9e6dd4c1e7a70ecb47784704e`**. Premier commit `8a273045e705902cac0bce211bbd2893b22ad41c`, puis correction strictement host-only de l'ordre d'assertions (aucune variation du firmware). Garde setup FNV `dfb8ef59`, 4096 cycles normal/pair/impair/réentrée et trois OOM, scans d'absence de lecteurs natifs, ABI desktop préservée. CI finale avant test matériel [push #42131517844](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42131517844) et [PR #42131524196](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42131524196) : **SUCCESS**, tests hôte + builds `esp32-cyd` normal, diagnostic et trace. L'échec CI antérieur était un test Python faisant référence à `body` avant son initialisation, corrigé avant les deux SUCCESS.

## PASS sur le vrai classic CYD (firmware normal)

Log utilisateur **`device-monitor-261106-005000.log`**, **1 503 lignes** et appréciation directe **« Impeccable... »**. POWERON, ESP32-D0WD-V3/240 MHz, 4 MB flash, PSRAM zéro, PAK 241, framebuffer 160×120 RGB565/38400 B. `Engine structs: Render=184 Game=0 Canvas=44 Total=988 bytes`, `[RENDERCORE] INIT owner=esp-native-bridge bytes=184`, `[CORE] Render used=200`, `[RENDERSTART] READY`, `palettes loaded: entries=3280 bytes=6560`.

START → intro/Entrance `14095 B / c3882516`, première frame `71ca7465`, mouvements/rotation/strafe, pickup, porte275 avec fermeture différée, HUB puis EXIT. LOAD V11 slot06 → Entrance, porte459 → **MISSION COMPLETE** écran `LEVELSTATS PRESENT` et ACK → **Junction `NATIVECHANGEMAP COMMIT sourceMap=1 targetMap=9`** `8867 B / bc432a0f`, déplacements, dialogue, HUB. LOAD V11 slot04 → Entrance, combats contre Malwrath322, projectiles 204 drainés avant les **deux `MONSTERRETAL COMMIT`** sur le joueur, animations et hit-feedback, puis EXIT. **Deux `SYSEXIT MENU-READY frame=13074b1a`** et **quatre `RESIDENTRESET ... empty=1`**. **Quatre `HEADROOM_OK`** ; `shapeData=0x0 mediaTexels=0x0` vérifié aux frontières (11 occurrences). Aucun `Guru Meditation`, `panic`, `WDT`, `FAILED` ni `ERROR` dans ce log.

Heap8 stable : menu initial **153260**, Entrance actif **112156**, après LOAD V11 **112152**, Junction **121776**, retour menu **158104 B** ; largest8 gameplay **86004**, menu **110580**. Aucun nouveau SAVE écrit (EXIT `saveWrite=no`), aucune preuve comparative A/B chiffrée de fréquence de rendu, ni exercice de toutes cartes, dog-familiar, orientation INVERSE ou audio.

## Décision

**PASS matériel ciblé et ressenti utilisateur positifs.** Le code est gelé après le test. Ce rapport est **docs-only**, à faire suivre de CI de clôture avant merge de #255 expressément demandé par l'utilisateur. Ne pas confondre les impacts sur les monstres déjà présents (`HITFX`) avec les impacts des monstres sur le joueur : prochain jalon autonome, inventaire exhaustif des visuels desktop et parité hôte de toutes les branches, sans exiger une session CYD par catégorie de dégâts.
