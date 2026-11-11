# SDL-F1 — services non graphiques natifs : PASS matériel classic CYD (11 novembre 2026)

- **PR** : https://github.com/Kharn27/DoomRPG-RE/pull/294 ; **issue** https://github.com/Kharn27/DoomRPG-RE/issues/269
- **Base de départ** : `d22de91cf3372bc7a39366808d10fb1150d6c313` (SDL-E1 #291 déjà fusionnée).
- **Intégration concurrente Root-G #290** : `main=2a50651fc6d56e311925a5efc026881f0296db59`, conservé comme second parent dans le candidat **`bba5947094d24a6d3c880a12699e2ee3c06351a4`**. Le Serial n'imprime **pas** de SHA firmware : attribution de ce candidat au flash utilisateur, pas fingerprint embarqué.
- **CI finale du candidat combiné Root-G + F1** : https://github.com/Kharn27/DoomRPG-RE/actions/runs/43000674341 — **SUCCESS** (tous tests hôte, normal/bringup/trace, audit ELF). Le firmware normal occupe **860301 octets flash** et **80792 octets RAM statique** sur cette combinaison ; aucune comparaison A/B heap/runtime n'est établie.
- **Serial matériel** : `device-monitor-261111-111223.log` (**1771 lignes, 205956 octets**), préambule ALIVE d'une session antérieure puis **POWERON_RESET** complet, ESP32-D0WD-V3 à 240 MHz, flash 4 MB, **PSRAM=0**, PAK natif 241 entrées.

## Frontière de code SDL-F1

- `EspNativeProcessClock_ticks()` remplace l'appel lié à `SDL_GetTicks` pour les deux API `DoomRPG_GetTimeMS/GetUpTimeMS` et pour le chronométrage de préparation du `p.bmp` de l'intro. `esp_timer_get_time()/1000` conserve les ticks millisecondes modulo 32 bits, le `basetime=0` et la durée de vie processus. Tests hôte : parity CRC des corps legacy, boot tick=0, wrapping, 10 000 pas mixtes.
- La panne d'adoption d'image native ne consulte plus `SDL_GetError` : message explicite, conservation des libérations et rollback.
- `DoomRPG_Error` écrit `[DOOM ERROR] DoomRPG Error: ...` sur Serial sans `SDL_ShowMessageBox`, puis conserve les fermetures/sorties historiques ; chemin fatal non déclenché sur le vrai CYD.
- **Pas de changement de rendu, framebuffer ni cadence** ; `SDL_RenderPresent` reste le dernier symbole SDL lié, toujours pont vers `PlatformVideo_present()`. Les alias de types et fonctions dormantes du shim restent à nettoyer séparément.
- Audit ELF : **4 → 1 symbole SDL lié**, `85 → 40 octets` de corps symboliques SDL, résultat confirmé sur les **trois** profils. Ne pas assimiler cette baisse à un gain flash/RAM total ni à une preuve de fluidité. Le desktop/oracle et `DoomRPG-ESP32.pak` restent inchangés.

## Observations matérielles

- **Menu et options** : OPTIONS → VIDEO → luminosité **75→100%**, `NATIVECONFIG SAVE result=committed version=3`, retour VIDEO → OPTIONS → MAIN ; retour FNV `e687f0c7`. HELP `lines=112`, parcours paginé `0→7→14→21→28→35`, preview de mur PAK `ready=1`, retour MAIN avec touch réarmé. Pas de parcours intégral des 112 lignes.
- **Intro + démarrage** : neuf `NATIVEIMAGE DECODE` et neuf `ADOPT`, tous indexés 4-bpp, `INTROFIT PREPARE ... elapsedMs=40`, intro et destruction sans fuite, `/intro.bsp` Entrance CRC `623f34e4`, arena immutable `14095 B`.
- **Gameplay** : 7 `DIALOG OPEN` / 7 `CLOSE`, dont le dialogue d'aide d'arme autonome, **4 `DOORANIM COMPLETE`**, **3 `CRATE COMMIT`**, **14 `PLAYERRES COMMIT`**, **14 `PICKUPMINI START` / 14 END**, déplacements/strafe/rotations animés ; 2 `MONSTERRETAL COMMIT` (attaques multiples du sprite 322), pas de panic.
- **Cycle** : HUB → EXIT confirmé → `RESIDENTRESET released=18008 empty=1` → `SYSEXIT MENU-READY frame=e687f0c7` ; puis LOAD V11 `slot04` **5604 B**, restaurations script/ressources/lignes/automap/monstres/topologie/position/activation exactes annoncées ; reprise `RESUME-VISIBLE`. Le log s'arrête avec **l'armement** d'un deuxième EXIT ; ce deuxième EXIT n'est **pas** compté comme validé.
- **Mémoire** : deux `RAMBUDGET RESERVE ... HEADROOM_OK` avec marges heap8 **28204 / 27392 B** et DMA **69164 / 68352 B** ; `largest8=61428` en session, `shapeData=0x0` et `mediaTexels=0x0` à toutes les frontières observées. Pas de Guru, panic, WDT, abort ou erreur d'allocation dans le fichier.
- **Diagnostic existant** : 7 `NATIVEFRAME BOUNDARY-INLINE`, non fatal, déjà connu ; ce log ne permet pas d'établir de lien causal avec F1.

## Verdict / portée

**PASS matériel ciblé SDL-F1**, sur le firmware normal attribué au SHA `bba5947094d24a6d3c880a12699e2ee3c06351a4` après CI intégrée SUCCESS. Les scénarios clock/erreur fatale forcée/OOM et BMP 1/8-bpp sont prouvés seulement par tests hôte ou restent non injectés ; ni A/B runtime, ni exhaustivité niveau/monstres, ni second EXIT réussi ne sont revendiqués. **À partir de ce PASS, clôture docs-only ; ne plus modifier le code du candidat testé.** Prochaine étape sur nouveau `main` fusionné : migration de `SDL_RenderPresent` en propriétaire natif, sans optimiser `PlatformVideo_present()`.
