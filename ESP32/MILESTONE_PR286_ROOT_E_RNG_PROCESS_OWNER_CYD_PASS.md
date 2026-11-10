# Root-E — propriétaire RNG de processus 132 B : PASS matériel classic CYD

**Date : 10 novembre 2026** · [PR #286](https://github.com/Kharn27/DoomRPG-RE/pull/286) · Repo `Kharn27/DoomRPG-RE`

## Code et CI faisant foi

- Base Root-E : `main=10e42397fab36082b6fa62ac8a90d73157abac46` après fusion Root-D #282. Intégration de SDL-B6 dans le SHA final **`477eb1aea2e615c7a89b27e6b79a0485bc2f0939`** à partir du `main=0828a245b36762ccb1797da615cf152ffabd5691`.
- Firmware **normal `esp32-cyd`** attribué à ce SHA par la session de flash de l'utilisateur ; le Serial ne contient pas de SHA binaire signé. [CI de la combinaison #42724675309](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42724675309) **SUCCESS**, 2 checks, builds normal/bringup/trace, tests hôtes et audits.
- `Random_t` 128 octets `randTable` + 4 octets `nextRand` = **132 B** déplacés de `DoomRPG_t` vers le process native static/BSS. `EspNativeProcessRng_live()` renvoie une identité de pointeur stable (pas de read RNG), servant de référence aux leases de `EspNativeRngReplayGuard`. **177 usages dans 15 TUs** migrés. `DoomRPG_t` sur Xtensa **28 B** au lieu de 160 B ; ABI desktop/oracle reste intacte.
- Le **générateur d'origine et sa cadence** restent inchangés : seed à la création du root natif, états cachés `_seed/resetRand` process-lifetime, table jamais réinitialisée à LOAD/EXIT ; snapshot/rollback par valeur même `Random_t*`. Les tests hôtes contrôlent notamment refill, rollback, parité de fonctions et ABI. **BSS +132 B et heap alloué root −132 B** ; ne pas revendiquer un gain de RAM totale sans mesures séparées.

## Log utilisateur

**`device-monitor-261110-010800.log`**, **1385 lignes / 165855 octets**. Deux anciennes lignes `ALIVE`, puis un vrai `POWERON_RESET` nouveau firmware : ESP32-D0WD-V3, 240 MHz, 4 MB flash, **PSRAM 0**, PAK 241 entrées et vidéo unique 160×120 RGB565 38400 B.

- Boot : `Engine structs: Render=112 Game=0 Canvas=40 Total=676 bytes` ; `[CORE] DoomRPG used=44` ; `[CORERNG] SEEDED next=0 bytes=128 nonZero=128 tableFNV=80c007bb`. L'empreinte n'est pas une constante entre boots : dépend du seed temporel. Initialisation une fois observée.
- Premier parcours : START → prologue → Entrance, caisse détruite/transformée avec `rngCombat=2 rngConsequence=1`, pickups et miniatures PAK, dialogue 8 à reprise 19, porte275, dialogue 26 à reprise 19, HUB → EXIT. Premier menu `SYSEXIT MENU-READY frame=3721ecf7`.
- Reprise : LOAD V11 slot04, **5604 B** ; état des ressources, scripts, lignes, caisses, automap, monstres, topologie, positions, activation et drops restauré ; `MONSTERSTATE RESTORE rngCalls=0` (aucune RNG consommée). Malwrath sprite322 porte113 ouverte ; **3× MEMBER-ATTACK-PROBE rngRollback=yes playerExact=yes**, chacun avec **3 projectiles204** purgés `MALWRATHPROJECTILE CLEAR rngExact=yes`, puis **3× MONSTERRETAL COMMIT rollback=closed** (PV 34→28→22→16 ; armure 23→17→11→5). Trois tirs/attaques joueur **MONSTERCOMBAT COMMIT** contre 322 : **hp 11→7→3→0**, drop `MONSTERDROP COMMIT`, puis pickup dynamique. Second HUB→EXIT propre `SYSEXIT MENU-READY frame=3721ecf7`.
- Mémoire : **2× `RAMBUDGET RESERVE HEADROOM_OK`** ; `margin8=28396` puis 27596 B ; marge DMA 69356 puis 68556 B, `largest8=61428` pendant gameplay. Nettoyage **`RESIDENTRESET released=18020 empty=1`**, puis **`released=18008 empty=1`**. Heap8 menu après chaque sortie = **130876 B**, sans dérive. Invariants `shapeData/mediaTexels=0x0`. Aucun panic, watchdog, reset inattendu ou erreur fatale.
- **5× `NATIVEFRAME BOUNDARY-INLINE`** (indices logiques 15, 49, 62 ×3), déjà observés sur d'autres branches, à suivre séparément sans attribution à Root-E. Pas de test physique volontaire de refill à 127→0, mort/RETRY, HELP/OPTIONS ni SAVE neuf.

## Frontière et clôture

**PASS ciblé Root-E** sur code SHA `477eb1aea2e615c7a89b27e6b79a0485bc2f0939`. Ne jamais remplacer ce SHA par celui d'un commit documentaire ou d'un merge qui ne fut pas flashé. À partir de ce test, conserver le code candidat immuable : tous commits ultérieurs de documentation seulement. Si `main` avance (SDL-B7 #284 a effectivement avancé pendant le test), intégrer ses changements indépendants avec prudence, vérifier le diff des 28 fichiers Root-E par rapport au nouveau `main`, puis CI de combinaison avant autorisation de merge. Le résultat combiné nouveau n'est **pas physiquement retesté par ce log**.

Ne pas revenir à l'archive ZIP au runtime ; préserver `shapeData=NULL`, `mediaTexels=NULL`, framebuffer 38400 B, réserves mémoire et état RNG permanent.

