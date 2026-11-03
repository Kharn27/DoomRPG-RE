# PR #247 — désengagement de 1 024 octets de Render_t vers EspNativeTrig (PASS matériel — code gelé)

## Source de vérité, lignée et portée

- Dépôt : `Kharn27/DoomRPG-RE`. Branche unique `agent/esp32-render-extract-native-sintable`, PR [#247](https://github.com/Kharn27/DoomRPG-RE/pull/247), basée sur le **main initial** `653cae0695bc0a9cabd6fa12cfa31cfde0147764` (merge PR #246). **Firmware matériel testé** sur le commit de code `ebf893280db1f91dc81584894b1b41145fa97809`. Aucun merge de #247 effectué par l'agent.
- Durant le test, **main a évolué** vers `b9c49239857309308051196d7ad95f1b02140a99` (merge indépendant de la PR #248 orientation). Ne pas présenter le code combiné après un éventuel merge comme ayant été revalidé par ce log ; il faudra un smoke test ciblé si nécessaire. Pas de rebase du code testé pour cette clôture.
- Frontière : `src/Render.h` met `sinTable[256]` derrière `#ifndef DOOMRPG_ESP32`, préservant la référence desktop et supprimant le champ du Render_t CYD (normal, bringup, trace). `ESP32/src/esp_native_trig.c` possède l'unique table de 256 × int32 en BSS, chargée depuis les **mêmes 1 024 octets little-endian de `/sintable.bin` PAK**. `EspNativeTrig_view()` est NULL avant installation et les trois consommateurs first-frame, BSP visibility et sprite renderer refusent l'absence de table (fail-closed). Pas de calcul trigonométrique substitutif, pas de ZIP runtime ni de miroir map-wide.
- Taille `Render_t` compilée sous assertion statique : **1228 → 204 octets (−1024, −83,39 %)**. La table est transférée en BSS ; **ne pas prétendre un gain net de RAM de 1024 octets**. Le log utilisateur commence après le boot : il **ne montre pas** la ligne `Engine structs: Render=204` ni `CORE used`, bien que le build ait passé l'assertion de taille. `Game=0` et `Canvas=44` sont des références au dernier boot antérieur, pas des valeurs directement relues dans cet extrait.

## CI du candidat de code

- [Push #41545296838](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41545296838) : SUCCESS.
- [PR #41545347712](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41545347712) : SUCCESS.
- Builds normal `esp32-cyd`, bringup, trace ; tests hôte d'intégrité `EspNativeTrig` (4096 cycles exacts, données little-endian, invalides fail-closed), startup Render, ABI native et audits des exports legacy tous passés. Cela n'est pas une validation de performance hardware.

## Test sur vrai classic CYD, firmware normal — PASS (3 novembre 2026)

L'utilisateur a flashé le candidat et fourni le log Serial START + LOAD slot04 V11, puis déclaré : « wow 83% !!! Et bah on le sent meme pas !!! Trop bien ». **Pas de perte perceptible de nervosité** ; aucune comparaison instrumentée de latence de rendu ne doit être prétendue.

- Menu : `heap8=153264 largest8=110580`. `MAINSTART` valide `framebufferFNV=fec57f30` et `shapeData=mediaTexels=NULL`. Intro complète, libération de l'état transitoire `33884 B`, passage à Entrance ; `MAPRT arenaBytes=14095 arenaFNV=c3882516` et **`ENGINESESSION FIRST_FRAME frame=71ca7465`** inchangés.
- **START** : caisses et pickups (bullet clip, armor shards, Fire Ext + aide), flash blanc pulsé et expiry, mouvement axial et strafe, rotations 2 frames, dialogues événements **88, 89 et 79** avec `DIALOGCHAIN RESUME opcode=19`, porte275 ouverture puis fermeture différée après tour, CROSS `copies=4 accounting=1`, HUB/SYS/EXIT. Aucun événement 84 n'apparaît dans **ce** log : ne pas l'inventer.
- **LOAD slot04** : lecture V11 `5604 B`, BSP `c3882516`, état monstre/topologie/position/activation restauré exactement **`beaeaccc / 50e16fe3 / e2a9d90a / df419482`**. Porte113, Malwrath sprite322 (subtype6) reçoit **11→7→3→0 HP**. **Deux** ripostes observées, chacune trois tirs projectile204 avec `MALWRATHPROJECTILE CLEAR` avant `MONSTERRETAL COMMIT`; dégâts joueur et RNG commit après animation. Mort + XP6, **drop medkit effectivement visible puis ramassé**, secret event15 +5XP, HUB/EXIT.
- **Mémoire observée** : `heap8 menu/START prime/LOAD prime/EXIT = 153264 / 112976 / 112160 / 158108 B`, contre #246C `153264 / 112984 / 112172 / 158108 B` : delta **0 / −8 / −12 / 0 B**. Largest8 jeu `86004`, menu `110580` inchangés. `RAMBUDGET ... HEADROOM_OK` aux deux démarrages de session. Pas d'inférence de gain/perte de FPS à partir de trajets différents.
- **Cycle propre** : **deux** `RESIDENTRESET released=18016 empty=1`, **deux** `SYSEXIT MENU-READY frame=0f2c9fc3 session=off resident=empty saveWrite=no`. Absence de WDT/panic dans les lignes fournies, shapeData/mediaTexels NULL au START/ENGINESESSION/EXIT. Pas de nouveau SAVE écrit ; backend audio encore silent.
- `NATIVEFRAME BOUNDARY-INLINE` et les coordonnées `1073743xxx` du projectile204 restent des diagnostics déjà connus ; ce jalon ne les corrige pas. Aucun test de toutes les cartes, endurance, orientation #248 après merge, ni SAVE neuf.

## Clôture et garde de merge

**PASS matériel du seul commit de code `ebf893280db1f91dc81584894b1b41145fa97809`** ; aucun correctif firmware après test. Ce rapport, `ESP32/PORTING_STATUS.md` et `ESP32/DOCUMENTATION.md` forment une clôture **docs-only**. Recontrôler push/PR CI du SHA de docs et vérifier diff `ebf893280db1f91dc81584894b1b41145fa97809..HEAD` limité à ces trois chemins ; alors marquer #247 prête pour revue, **sans merger**. Vérifier la mergeabilité avec le main #248 (pas de rebase silencieux), et refaire un smoke test après combinaison si souhaité.
