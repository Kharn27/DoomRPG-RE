# SDL-E1 — raster RGB565 natif : PASS matériel classic CYD (10 novembre 2026)

- **PR** : https://github.com/Kharn27/DoomRPG-RE/pull/291
- **Issue SDL** : https://github.com/Kharn27/DoomRPG-RE/issues/269
- **Base exacte main** : `2d697dbafe6ba5498efcb3140bccf229676c8b81` (merge #289).
- **Candidat de code production** : `6e7451a93890ebdfbd3ee148e69e3a46a7db3d0c` ; le dernier commit de garde `5ebe8d1d9ba16a0cb88eeb7d73164a231c6070e2` ne modifie **que le rapport/test ELF**. Le Serial n'imprime pas de SHA de firmware : attribution par le parcours PR/flash, non fingerprint embarqué.
- **CI finale avant test** : https://github.com/Kharn27/DoomRPG-RE/actions/runs/42905895343 — **SUCCESS** ; normal `esp32-cyd`, bringup, trace, tests hôte/compilation/ELF.
- **Rapports Serial du vrai classic CYD** : `device-monitor-261110-230846.log` (**2047 lignes, 243625 octets**) pour gameplay/LOAD/EXIT et `device-monitor-261110-231208.log` (**168 lignes, 15463 octets**) pour OPTIONS/HELP. Le log principal commence par une ligne ALIVE d'une session précédente, puis contient un `POWERON_RESET` complet. Classic ESP32-D0WD-V3, CPU 240 MHz, flash 4 MB, **PSRAM 0** ; PAK 241 entrées.

## Frontière de code

Le propriétaire permanent `EspNativeTexture` assure désormais aussi `EspNativeTexture_setDrawColor`, `EspNativeTexture_fillRect`, `EspNativeTexture_drawLine` et le clip unique partagé avec les blits indexés. Les cinq fonctions de l'ABI DoomRPG `setClipFalse`, `setClipTrue`, `setColor`, `fillRect`, `drawLine` sont migrées vers ce propriétaire sans nouvel allocation/framebuffer, sans dépendance ZIP runtime, sans modification des états monde, et sans changer la cadence `DoomRPG_flushGraphics → SDL_RenderPresent → PlatformVideo_present`. Le rendu logical demeure **160×120 RGB565 (38400 B), sortie nearest-neighbor ×2**. Les définitions SDL inutilisées peuvent subsister en source ou en hôte ; l'audit se fonde sur les **symboles réellement liés**.

Le différentiel hôte exerce 320 séquences seedées **sur les véritables corps SDL et natifs** et compare intégralement les pixels RGB565, rectangles, lignes Bresenham, couleurs et clipping. L'oracle de présentation teste également les sept corps DoomRPG sur 1600 opérations mixtes. Le dernier ELF CI observe **8→4 fonctions SDL liées, 442→85 B de corps SDL liés**, dans les **trois profils** ; les quatre exports déliés sont `SDL_RenderSetClipRect`, `SDL_SetRenderDrawColor`, `SDL_RenderFillRect`, `SDL_RenderDrawLine`.

### Mesures compilateur, strictement comparables

| Profil | Avant #291 (main après #289) | Après SDL-E1 | Delta |
|---|---:|---:|---:|
| normal flash | 859181 B | 859145 B | **−36 B** |
| normal RAM statique | 80792 B | 80776 B | **−16 B** |
| bringup flash | 881793 B | 881785 B | −8 B |
| bringup RAM statique | 80944 B | 80920 B | −24 B |
| trace flash | 877321 B | 877313 B | −8 B |
| trace RAM statique | 80840 B | 80816 B | −24 B |

Ces deltas ne démontrent ni gain heap8 à l'exécution, ni vitesse de rendu, ni amélioration de fluidité. Ne pas additionner les octets de corps symboliques SDL au gain flash net.

## PASS matériel — gameplay (`230846`)

- Boot depuis `POWERON_RESET`, lancement de la partie depuis le menu, intro/touch, `/intro.bsp` Entrance CRC `623f34e4`, arena immutable `14095 B`, gameplay générique READY, `shapeData=0x0` / `mediaTexels=0x0` aux frontières observées.
- 7 `NATIVEIMAGE DECODE` et 7 `ADOPT` indexed **4-bpp** (menu, fonts, intro/hand) sans retour à l'ancienne texture SDL.
- Déplacement, strafe, rotation avec frames présentées, 8 `DOORANIM COMPLETE` dont fermetures différées, collision, 17 `PICKUPMINI START` et 17 `END`, 9 `DIALOG OPEN` et 9 `CLOSE` y compris opcodes 8/26 et textes longue durée (19 lignes), flash/pain/retaliation.
- Deux monstres tués (sprite 220 et Malwrath 322). **5 `MONSTERRETAL COMMIT`**, dont 3 attaques Malwrath ×3 coups ; 3× `MALWRATHPROJECTILE CLEAR` `rngExact=yes` ; `MONSTERCOMBAT COMMIT` pour Malwrath `hp=11→0` et drop.
- Premier EXIT menu propre, `NATIVESAVE LOAD` checkpoint V11 **slot04, 5604 B**, map/ressources/scripts/lignes/crates/automap/monstres/topologie/positions/activation restaurés, nouvelle session native et second EXIT propre.
- Deux `RAMBUDGET RESERVE advisory=HEADROOM_OK` : marges `heap8=28416/27604 B`, DMA `69376/68564 B`. `largest8=61428` en partie. Deux `RESIDENTRESET heap8=112876→130884 released=18008 empty=1`. Deux `SYSEXIT MENU-READY frame=e687f0c7 session=off resident=empty`. Menu final `heap8=130884 largest8=86004`. Aucun panic/Guru/abort/WDT observé.
- **8 `NATIVEFRAME BOUNDARY-INLINE`** : diagnostics déjà connus, non fatals ; à conserver dans le suivi, sans les attribuer arbitrairement à SDL-E1.

## PASS matériel — OPTIONS/HELP (`231208`)

- Depuis MENU_MAIN, carte OPTIONS armée puis confirmée : BACK/VIDEO (SOUND encore désactivé). VIDEO→LUMINOSITE, réglage **50→75 %** avec `NATIVECONFIG SAVE result=committed version=3`, `BACKLIGHT SET percent=75` ; retour VIDEO, retour OPTIONS puis retour menu via `MAINBACK READY` et `OPTIONBACK FAST ... runtimeFNV=e687f0c7`.
- HELP : `MAINMODEL HELP-NATIVE lines=112`, textures `c.bmp` et `k.bmp` indexed 4-bpp décodées/adoptées ; scène preview originale PAK `ready=1` (map1, texture59), pages de 7 lignes. Parcours **scroll=0→7→14→21→28→35→42→49→56** (les 112 lignes ne sont **pas toutes parcourues**). `MAINBACK READY source=help`, menu restauré `frame=e687f0c7`, touch réarmé. `heap8=130684` et `largest8=86004` identiques avant/après HELP. Aucun panic/WDT observé.

## Verdict et limites

**PASS matériel ciblé SDL-E1** ; les scénarios principaux du rendu natif et du menu sont exercés sur le vrai CYD, en complément des tests hôte exacts. Aucune preuve de perfection visuelle à partir du log seul, et pas d'A/B performance matérielle. Seul le format image 4-bpp a été observé sur CYD ; les formats 1/8-bpp et les échecs forcés OOM restent des validations hôte. Pas de test intégral des 112 lignes HELP ni de tous les niveaux/monstres. **Après PASS : uniquement changements de documentation ; toute modification du code E1 impose un nouveau gate hardware.**
