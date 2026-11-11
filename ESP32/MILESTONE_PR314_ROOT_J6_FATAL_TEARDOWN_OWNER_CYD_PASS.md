# Root-J6 — Teardown fatal avec identité processus explicite — PASS CYD

**Date :** 11 novembre 2026  
**PR :** [#314](https://github.com/Kharn27/DoomRPG-RE/pull/314)  
**Issue :** [Root-J #302](https://github.com/Kharn27/DoomRPG-RE/issues/302)  
**Branche :** `agent/esp32-root-j6-fatal-teardown-explicit-owner`  
**Candidat attribué au firmware flashé :** `0d14cb81d502d7d659e31cc1a0327169716ab3e3`  
**Base exacte du candidat :** `d819cf41884a8635253c24aa817ae2cf53b5854b` (Root-J5 fusionné)  
**Main après fusion SDL-F11 #313 :** `077e1890e1b567ce8abd7ebbe48e3ca517fae1a2`  
**Intégration Root-J6 sur le nouveau main :** `9ec7a5470216fc198d09ef87d78b60536f8136e3`

## Périmètre

La fonction native `ESP32/src/engine_stubs.c::DoomRPG_FreeAppData(DoomRPG_t* doomrpg)` vérifie désormais `doomrpg != NULL && doomrpg == doomRpg` avant toute libération, au lieu de vérifier seulement le global `doomRpg`. Elle passe le **paramètre explicite** `doomrpg` au destructeur Canvas, au lieu de relire le global.

L'entrée fatale `DoomRPG_Error()` continue de transmettre la véritable identité du processus, et la durée de vie de l'objet identitaire de 1 octet reste intacte. L'ordre des libérations reste **menu → render → canvas**, et les pointeurs propriétaires sont remis à NULL. Aucune création d'allocation, de framebuffer, de cache, de ZIP runtime ou de mutation gameplay ; audio et SDL hors champ. L'objectif est de fermer un chemin dangereux de teardown sur un root étranger, **pas** de récupérer artificiellement 1 octet.

## Validation hôte et CI avant matériel

- Le test ajouté `ESP32/test/root_j6_fatal_teardown_owner_host.py` restitue le **blob Git entier** d'origine de `engine_stubs.c` (**`cb5105f498161d2ce4f4679f4edb84435e7cb68a`**) par inversion des deux changements de production seulement.
- Il extrait et compile le vrai corps C de `DoomRPG_FreeAppData` avec `cc -Werror` et exécute **4096 cas** incluant root live/étranger/nul, identity non initialisée, les huit masques des trois propriétaires, l'ordre menu/render/canvas et les doubles appels. Refus sans mutation pour root incorrect ; libération once-only correcte pour le root live.
- Gardes Root-I/J5 et `ESP32/scripts/build_engine.py` adaptés pour contrôler le passage explicite du root et sa vérification. Tests SDL-F10 et tous les autres oracles conservés.
- CI candidat [PR #43071720850](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43071720850) et [push #43071710278](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43071710278) : **SUCCESS** (host, builds normal/bringup/trace, audits ELF). Cela valide les trois configurations compilées ; pas de mesure de performance runtime isolée.

## Log du vrai classic CYD

Source : **`device-monitor-261111-190156.log`**, **1666 lignes / 194236 octets**, une ligne ALIVE provenant de la session précédente puis un seul `POWERON_RESET`.

- ESP32-D0WD-V3 dual core 240 MHz, flash 4 MB, **PSRAM=0** ; SD montée (240 MiB), PAK natif **241 ressources**.
- CORE prêt (Render=108 B, Game=0 B, Canvas=36 B) ; LAYOUT 160×120 / viewport 160×80 ; PRERENDER et RENDERSTART prêts, framebuffer **38400 B** partagé ; CONFIGMAP prêt, `mappings.bin` 8392 B, payload **8376 B**.
- HELP 112 lignes, défilement jusqu'à 35 puis BACK ; OPTIONS→VIDEO→LUMINOSITE **75→100** sauvegardée sur SD puis retour menu ; START→intro narrative de 50ms, `INTRODISP READY` libérant **34080 B** puis `/intro.bsp` Entrance et gameplay normal.
- Premier gameplay : `3 DIALOG OPEN` (dont 1 standalone) et `3 CLOSE`, `4 DOORANIM COMPLETE`, `11 PICKUPMINI START` et `11 END`, HUB→EXIT sans écriture de sauvegarde. LOAD V11 **slot04, 5604 B**, reprise du gameplay avec **4 MONSTERRETAL COMMIT** et **4 MALWRATHPROJECTILE CLEAR** (chaque attaque terminée) ; second HUB→EXIT ; HELP→BACK après retour.
- Deux réserves mémoire **HEADROOM_OK** : heap8 margin **28268 B / 27456 B**, DMA margin **69228 B / 68416 B**. Les deux `RESIDENTRESET` rendent **18008 B** avec `empty=1`. Les deux `SYSEXIT MENU-READY` retrouvent `frame=1945fafa` et `resident=empty`. **13 confirmations** `shapeData=0x0 mediaTexels=0x0`.
- Aucun panic, Guru Meditation, WDT, abort ni `FAIL` opérationnel. **5 `NATIVEFRAME BOUNDARY-INLINE`** non fatals, diagnostics préexistants. Les mentions `fail-closed` sont des limitations de gameplay différé et non un échec du boot.

**Limites de la preuve :** le firmware ne journalise pas son SHA. Le PASS est attribué au candidat d'après le flash utilisateur, sans authentification binaire indépendante. Le scénario `DoomRPG_Error`/OOM n'a **pas été déclenché sur matériel** : la fermeture des roots étrangers est couverte par le harnais C plutôt que par un crash délibéré sur le CYD. Pas de test de tous les BSP, pas de benchmark A/B.

## Fusion parallèle SDL-F11 et gel post-test

Après ce test, [SDL-F11 #313](https://github.com/Kharn27/DoomRPG-RE/pull/313) était déjà fusionnée, vrai `main=077e1890e1b567ce8abd7ebbe48e3ca517fae1a2`. La branche Root-J6 est reconstruite à partir de ce SHA par le commit `9ec7a5470216fc198d09ef87d78b60536f8136e3`. **Les cinq blobs J6** suivants sont repris octet pour octet du candidat testé : `ESP32/src/engine_stubs.c`, `ESP32/scripts/build_engine.py`, `ESP32/test/root_i_canvas_backref_host.py`, `ESP32/test/root_j5_layout_owner_host.py`, `ESP32/test/root_j6_fatal_teardown_owner_host.py`. Le workflow est composé sur la version SDL-F11 courante : tous les gardes F11 et J6 sont présents.

**Ce n'est pas un deuxième PASS physique conjoint SDL-F11+Root-J6.** Le code J6 est gelé : seul le rapport et les deux documents de statut sont ajoutés après intégration. La CI combinée sur le dernier SHA de documentation doit être **SUCCESS** avant la fusion. Aucun autre changement firmware ne doit être attribué au log J6.

## Suite

Après fusion, relire le **vrai `main`** et son SHA, `ESP32/PORTING_STATUS.md`, `ESP32/DOCUMENTATION.md` et ce milestone. Auditer les dépendances root global et SDL encore réellement utilisées avant de choisir un nouveau jalon borné. Conserver l'identité processus tant que `DoomRPG_Error` en dépend ; ne pas réécrire l'ABI desktop pour retirer 1B symbolique. Pour SDL, reprendre le fil SDL-F12 à partir du dernier main et ne pas chevaucher une PR déjà ouverte. Toujours tester `esp32-cyd` normal et préserver le backing store PAK et les invariants NULL.
