# PR #273 — DoomCanvas renderFloorCeilingTextures desktop-only : PASS CYD

**Date : 9 novembre 2026.** [PR #273](https://github.com/Kharn27/DoomRPG-RE/pull/273), branche `agent/esp32-canvas-retire-floor-ceiling-gate`. **Code candidat flashé et confirmé par son empreinte de structure : `561be44c787ac28c770dc93e0b51429779cd29bf`**, base réelle `main=392173acc21878dfbbb45fa92100c4ae42d4c34d`, après fusion #271 (SDL-B1) et #272 (Render arrays). **Source primaire : `device-monitor-261109-012856.log`, 1 734 lignes / 205 121 octets**, firmware normal `esp32-cyd` classic CYD. Le log n'imprime pas directement un SHA Git ; celui-ci est le SHA fourni pour le test, tandis que `[DOOMCANVASBRIDGE] ... bytes=40` et les effets constatés sont des empreintes comportementales. L'utilisateur autorise explicitement la fusion si PASS.

## Frontière fonctionnelle

- `DoomCanvas_t::renderFloorCeilingTextures` était un booléen desktop initialisé à `true`, lu deux fois dans `src/Render.c` pour le choix de l'ancien rendu de plans. Aucun lecteur raster/gameplay ESP32 dans les TU natifs ; une seule écriture d'initialisation dans `EspNativeEngineRoot_initCanvas`.
- Champ conservé hors ESP32 et dans l'oracle géométrique ; supprimé en normal, bringup, trace. Suppression de la seule écriture native. `DoomCanvas_t: 44→40 B`, `Render_t:112 B`, somme des structures moteur attendue `916→912 B`, sans attribuer 4 B nets de heap : taille structurale seulement. Les données de plans natifs restent lues du BSP/PAK. Aucun changement runtime de la vidéo, framebuffer RGB565 `160×120=38400 B`, palettes, textures, animation, gameplay, SDL-B1 ni HELP.
- `ESP32_DOOMCANVAS_RETIRED_PLANE_GATE_BYTES=4U` comptabilise la suppression dans les empreintes de layout. Le test ABI compile réellement `src/DoomCanvas.h` en normal/bringup/trace pour démontrer l'absence et en desktop/oracle pour démontrer la conservation. Le corps C réel du constructeur est exécuté 4 096 fois pour les deux chemins d'initialisation, plus injection de malloc OOM ; FNV du corps ajustée, autres tests inchangés.
- Branche #273 sans modification des sources de la PR #267 HELP draft.

## CI pré-flash

- [Push #42539767006](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42539767006) et [PR #42539808486](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42539808486) : **SUCCESS** au SHA `561be44c787ac28c770dc93e0b51429779cd29bf`, plus de 100 étapes par pipeline, tests hôte/probes/ABI/ELF/SDL et trois builds `esp32-cyd`, `esp32-cyd-bringup`, `esp32-cyd-trace`. Seul le firmware **normal** a été testé physiquement.

## Preuves du log matériel

- Boot `[DOOMCANVASBRIDGE] INIT ... bytes=40`, `[RENDERCORE] INIT ... bytes=112`, `[RENDERCORE] SETUP ... view=160x80@0,20 arrays=640B`, framebuffer partagé 160×120 RGB565 38 400 B, pitch=320, SD et PAK 241 entrées opérationnels.
- Menu principal et **HELP ROM 77 lignes** avec pagination jusqu'à 70/70, retour au menu, réouverture et retour ; interface native. **NEW GAME / Entrance** : histoire/introduction, `/intro.bsp` CRC `623f34e4`, arena 14 095 octets `c3882516`, frame `71ca7465`, mouvements/rotations `gameplayStable=yes`, caisses, pickups d'armure/medkit/munitions/Fire Ext, dialogues opcode8 (events 82, 89) et opcode26 (event79), reprise opcode19, porte275 animée en 4 étapes et fermeture différée, HUB/EXIT sans save.
- **LOAD V11 slot04**, 5 604 B : restaure ressources/script/lignes/état de caisse/automap/monstres/topologie/positions/activation/drops (FNV `beaeaccc/50e16fe3/e2a9d90a/df419482`), `[ENGINESESSION] RESUME-VISIBLE`, porte113 en 4 frames, Malwrath sprite322 **3 ripostes triples avec projectile204**, trois `[MALWRATHPROJECTILE] CLEAR`, trois `[MONSTERRETAL] COMMIT`, trois attaques joueur puis `[MONSTERCOMBAT] DEATH-SETTLE sprite=322`, impact visuel, loot/jeu/HUB et deuxième EXIT.
- **Deux** `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK` : NEW `margin8=52040`, LOAD `margin8=51228`; pendant gameplay `largest8=86004` ; après second menu `heap8=154508 largest8=110580`. **Deux** `[RESIDENTRESET] released=18008 ... empty=1` et **deux** `[SYSEXIT] MENU-READY frame=e687f0c7`. `shapeData=0x0 mediaTexels=0x0` aux bornes visibles, aucune ligne `ERROR`, `FAILED`, `panic`, `Guru Meditation`, `WDT` ou `abort`.
- Onze `[NATIVEFRAME] BOUNDARY-INLINE` correspondent au diagnostic de frontière déjà connu. Le PASS est **ciblé** ; il ne prouve ni identité pixel par pixel sur toutes les cartes, ni stabilité longue durée, ni gain net de heap ou de FPS.

## Clôture

Le code firmware matériellement testé reste figé à `561be44c787ac28c770dc93e0b51429779cd29bf`. Après PASS, seuls ce rapport et les index `PORTING_STATUS.md` et `DOCUMENTATION.md` sont ajoutés au commit documentaire. La fusion est autorisée explicitement par l'utilisateur ; vérifier CI de ce commit, fusionner #273, puis récupérer le **nouveau SHA exact de main** avant toute nouvelle branche `agent/*`.
