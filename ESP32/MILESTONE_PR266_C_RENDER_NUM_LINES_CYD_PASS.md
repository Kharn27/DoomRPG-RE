# PR #266-C — retrait de Render_t::numLines : PASS matériel classic CYD

**Date : 8 novembre 2026.** Dépôt `Kharn27/DoomRPG-RE`, [PR #266](https://github.com/Kharn27/DoomRPG-RE/pull/266), branche `agent/esp32-render-retire-frac-half-width`. **Dernier SHA de code C réellement testé : `6f888ce8e4aa4f58aa6f565919426d1d5fbcdb7a`** (implémentation `4aa33449be5a473be0959ff2c3f26cd7fb41b219`, harnais corrigé `6f888ce8e4aa4f58aa6f565919426d1d5fbcdb7a`). Log réel `device-monitor-261108-210342.log` : **1 279 lignes / 153 664 octets**. Validation utilisateur « Ca marche niquel si c'est ok tu peux fusionner » ; fusion expressément autorisée après contrôle des invariants.

## Changement de frontière, permanent et borné

- `Render_t` en production ESP32 **148 → 144 octets** (-4), observés dans `Engine structs: Render=144 Game=0 Canvas=44 Total=948 bytes` et `[RENDERCORE] INIT owner=esp-native-bridge bytes=144`. Depuis le début de PR #266, total **160 → 144 octets** pour les jalons A/B/C.
- Le champ desktop/oracle `Render_t::numLines` sort des builds ESP32 normal, bringup et trace. L'identifiant **de ligne BSP source**, nécessaire pour les erreurs murales/span (`FIRST_FRAME_FAIL_SPAN_SCREEN/NEGATIVE/OOB/WALL_DEPTH/WALL_RASTER`), est transporté dans `FirstFrameWork.diagnosticLineIndex` : mémoire transitoire de première frame, pas un état Render persistant.
- Snapshots `RenderScratch` et `Scratch` BSP/sprite dépouillés de copies du compteur sans propriétaire. Desktop et oracle géométrique préservent leurs champs et le comportement historique. Aucun changement intentionnel de projection, ordre BSP/sprite, PAK, RNG, input, tours, HUD ou combats ; pas de nouveau framebuffer, ZIP runtime ni allocations persistantes.
- Le test hôte de texels muraux `native_wall_boundary_inline_host.c` utilise la vraie fonction extraite `sampleWallSpan()` et vérifie désormais explicitement le diagnostic de ligne `161` lors de quatre erreurs fail-closed. La correction est pré-test matériel.

## CI pré-matérielle de C

- [CI push #42441414866](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42441414866) **SUCCESS, 99/99 étapes**.
- [CI PR #42441419274](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42441419274) **SUCCESS, 99/99 étapes**.
- Hôte et firmwares `esp32-cyd` normal, `esp32-cyd-bringup`, trace compris. Aucun nouveau changement de code sur la branche depuis le SHA testé jusqu'au présent commit **docs-only**.

## Preuve sur le véritable CYD (firmware normal)

- ESP32-D0WD-V3, 240 MHz, 4 Mo flash, **PSRAM 0**, PAK indexé **241 entrées**, framebuffer logique **160×120 RGB565 / 38 400 B**, écran 320×240 ×2 ; `[RENDERCORE] SETUP view=160x80@0,20 arrays=1280B`. Scratch `[NATIVEFRAME] SCRATCH owner=heap-transient bytes=2996 stackRender=696 lifetime=one-render` au START et au LOAD.
- NEW GAME Entrance : prologue, `/intro.bsp` CRC `623f34e4`, runtime arena `14095` B / FNV `c3882516`, première frame `71ca7465` ; déplacements/strafe/tours, caisses transformées, pick-ups, dialogues (événement 88 annulé, événement 89 opcode8→19, événement79 opcode26→19), porte275 ouverte puis fermée après tour des monstres, arme ramassée et dialogue d'aide.
- Première sortie vers MENU_MAIN : `RESIDENTRESET empty=1` et `SYSEXIT MENU-READY frame=1945fafa session=off resident=empty saveWrite=no`.
- LOAD V11 slot04 `/DoomRPG-ESP32-slot04.sav` : **5604 B**, map Entrance ; reprise exacte du scope checkpoint ressources/script/lignes/caisses/automap/monstres/topologie/positions/activation/drops, signatures `beaeaccc/50e16fe3/e2a9d90a/df419482`. `[ENGINESESSION] RESUME-VISIBLE map=1` puis `READY shapeData=0x0 mediaTexels=0x0`.
- Porte113 et CROSS sprite324 ; Malwrath sprite322 deux attaques de **trois projectiles 204** avec 9 frames de projectile par probe, dégâts commit seulement après animation : `playerHP=34→30` et `30→22`, armure `23→19` et `19→11`, impact sprite184 frames0/2–2/2 et red flash. Trois attaques joueur `hp=11→7→5→0`, XP et death settle/corpse. Porte113 et fermeture différée ; secret event15, **deux lignes 112/111** ouvertes et `[SECRET] FOUND ... xp=5` ; deuxième EXIT propre.
- Deux réserves RAM `advisory=HEADROOM_OK` (`margin8=53444` et `52628`), `largest8=86004` en jeu ; deux `RESIDENTRESET ... released=18008 ... empty=1`, deux `SYSEXIT MENU-READY` ; état final `heap8=155912 largest8=110580`.
- Toutes les frontières instrumentées `shapeData=0x0` et `mediaTexels=0x0`, zéro `[NATIVEFRAME] FAILED`, panic, Guru, WDT, watchdog, abort ni backtrace dans les 1 279 lignes.

## Limites et publication

**PASS matériel du jalon C** pour ce parcours précis. Le gain `-4 B` est celui de la **structure**, pas une démonstration de `+4 B` heap. Cette capture ne couvre pas Sector 1, une parité image pixel-perfect exhaustive, un benchmark A/B, l'audio, ni un stress test de plusieurs heures. Les jalons A et B ont leurs propres rapports matériels distincts.

**Le code C reste gelé au SHA `6f888ce8e4aa4f58aa6f565919426d1d5fbcdb7a`.** Le présent rapport et les deux entrées docs sont le commit docs-only post-test. Au moment de préparer le merge, `main` avait avancé par PR #265 jusqu'à `1bb06f2112e624442430cf920f3a26647256d366` (aide native, fichiers sans conflit avec #266). Si une intégration de `main` est requise, la considérer comme *apport extérieur* distinct de C, revalider le combiné par CI ; ne pas inventer de nouvelle validation matérielle. Le SHA final `main` après merge doit être relevé sur GitHub.
