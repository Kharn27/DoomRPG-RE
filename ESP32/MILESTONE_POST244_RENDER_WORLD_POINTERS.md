# Post-#244 — retrait des six pointeurs de monde desktop de Render_t (PASS CYD)

## Real classic CYD — 2 novembre 2026 : PASS fonctionnel, RAM et nervosité

**Code matériel attribué par campagne de flash/branche :** `4aa6eb643e68f81820da5f27dd46e6c9be1baff4` (firmware normal `esp32-cyd` ; le log Serial ne contient pas de SHA binaire embarqué). Source `device-monitor-261102-014011.log` : **1457 lignes** ; utilisateur après test : **« Le jeu est toujours impeccable pour moi :) »**. GitHub Actions du code, avant ce test : [push #41367931341](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41367931341) et [PR #41367989410](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41367989410), **SUCCESS 78/78 étapes chacune** (gardes source + tests hôte Render ctor/free, 4096 cycles, admission BSP/CROSS et builds normal/bringup/trace).

**Boot / ownership :** vrai ESP32-D0WD-V3 240 MHz, flash 4 Mo, **PSRAM=0** ; `Engine structs: Render=1308 Game=0 Canvas=44 Total=2112` ; `[RENDERCORE] INIT ... bytes=1308`, `[CORE] READY ... used=2276`. Le layout normal perd exactement **24 octets** par rapport à #244B (Render 1332→1308, core 2300→2276), sans modifier les données PAK (241 entrées), mappings (152 textures/252 sprites, payload 8376 B), framebuffer logique 160×120/38400 B, ni les véritables owners natifs. `shapeData/mediaTexels=0x0` aux frontières observées. La branche conserve #244A admission BSP avant décodage et #244B réutilisation Frame des CROSS.

**RAM matériel, #244B → #245 :** menu initial `heap8 153160→153184` (+24 B), START cache prime `112880→112904` (+24 B), LOAD prime `112068→112092` (+24 B), menu après chaque EXIT `158004→158028` (+24 B) ; largest8 jeu/menu `86004/110580` B, stable. Deux `RAMBUDGET RESERVE ... advisory=HEADROOM_OK` (marges heap8 55560/54748) ; les deltas heap sont des observations hardware, pas une règle d'allocation universelle.

**Parcours START :** prologue/Entrance, BSP source CRC `623f34e4`, arena `14095/c3882516`, première frame `71ca7465` ; caisses 127/56 → ressources, 7 pickups dont Armor Shards, Bullet Clip et Fire Ext, flash blanc 3 pulses avec expiry, dialogues 88/89/79 + reprises opcode19, porte275 quatre frames et fermeture différée, animations déplacement/rotation/strafe, sprites CROSS 141/77, HUB et EXIT sans SAVE. Le protocole mentionnait event60/automap, mais ce journal ne prouve **pas** le parcours event60 ni l'ouverture de l'automap ; ne pas élargir ce PASS.

**Parcours LOAD :** checkpoint slot04 V11/5604 B, restauration exacte `beaeaccc/50e16fe3/e2a9d90a/df419482` (monstres/topologie/positions/activation), `MAPFLASH REUSE HIT rebuild=no` ; porte113, Malwrath sprite322 HP `11→7→3→0`, **trois** ripostes de trois tirs projectile204 intégralement drainées (`MALWRATHPROJECTILE CLEAR` avant chaque `MONSTERRETAL COMMIT`), drop Sm Medkit pris sur tile285, Flak Jacket, secret event15 `+5XP`, EXIT. 101 traces `NATIVESPRITE CROSS copies=4 accounting=1` (index 141/77/324), clipping/near attendu ; le log ne mesure pas physiquement les lectures PAK par groupe CROSS. Trois `NATIVEFRAME BOUNDARY-INLINE` et l'abscisse des projectiles loggée avec l'offset `0x40000000` sont **préexistants/non résolus**, et n'ont pas été attribués au retrait.

**Teardown / sécurité :** **deux** `[RESIDENTRESET] ... released=18008 empty=1`, **deux** `[SYSEXIT] MENU-READY frame=0f2c9fc3 ... saveWrite=no` ; aucun panic, watchdog, overflow, unsupported positif ou save write dans ce fichier. Les comptes `unsupported=0` restent conformes. Pas de SAVE neuf, toutes-cartes, pixel parity exhaustive, audio ni endurance démontrés.

**Performances / ressenti :** 16 `SPRITEORDER.scanUs`, médiane **3066,5 µs** (une première frame froide à 16279 µs) ; 33 frames MOVE médiane **88145 µs**, 16 frames TURN médiane **83977 µs**, 6 frames combat moyenne **95854,5 µs**, 11 frames pickup médiane **93246 µs**. Pour contexte, #244B (autre session/échantillons) MOVE médiane 87670 µs, combat moyenne 96781 µs : **pas de preuve A/B d'accélération/ralentissement causal**. L'utilisateur confirme une nervosité inchangée et un jeu impeccable, levant la réserve UX historique de #243 **pour cette combinaison précise #244+#245**.

**Décision : PASS matériel ciblé fonctionnel+RAM+UX. Code gelé au SHA `4aa6eb643e68f81820da5f27dd46e6c9be1baff4`. Après ce test, aucun changement source autorisé ; seulement documentation puis contrôle diff et CI post-doc avant Ready for review. Merge réservé exclusivement à l'utilisateur.**

## Base et provenance vérifiées

- Base : `main` GitHub après merge #244, SHA exact `2ec2353ddcbc83203a3012d854d25a7c2b6f539f`.
- Branche : `agent/esp32-render-world-pointers-post244`, indépendante de l'ancienne PR #243 fermée sans merge.
- Source du retrait : branche archivée `agent/esp32-render-retire-legacy-world-pointers` au SHA `e7ef6d8b549da2a34a8e9a6c3501285e519625a8` ; réintégration de **16 fichiers source/tests** à leurs blobs Git inchangés. Pour chacun, le blob du nouveau main était encore identique à celui de l'ancienne base `fe3bf6cf005d0f87d534b51daf938c3473b6cdb6` : aucun écrasement des modifications #244.
- L'optimisation #244A admission BSP précoce et #244B mutualisation des quatre frames CROSS reste dans le nouveau main ; **ce cumul est désormais validé sur le vrai CYD au SHA du code indiqué ci-dessus**.

## Frontière propriétaire

Les six pointeurs `Render_t::{nodes,lines,mapSprites,tileEvents,mapByteCode,mapStringsIDs}` sont désormais exclus du layout ESP32 **normal**, mais retenus pour le desktop et l'environnement de probes bringup. Le parser et le runtime natifs utilisent les arènes compactes, overlays et backing PAK ; les mirrors heap desktop n'y ont aucun propriétaire. `nodesLength`, `linesLength`, `numMapSprites`, `mapStringCount` et les réelles tables de mappings encore nécessaires restent. `shapeData` et `mediaTexels` doivent rester NULL ; pas d'allocation/copie carte entière ni nouveau cache.

Le constructeur racine cesse d'écrire ces pointeurs absents, les gardes init/intro/first-frame/release contrôlent le propriétaire effectif, et le cleanup ne libère les anciennes listes qu'en bringup. Le test hôte extrait les véritables fonctions C de construction et cleanup et vérifie l'ABI 32-bit, l'ownership du framebuffer partagé, les cycles de destruction, le comportement des guard et la taille. Aucune modification prévue du chemin sprite #244, des actions joueur, de la temporisation, de `PlatformVideo_present()`, du gameplay/RNG, du save ou de l'audio.

**Signature prévue (confirmée au boot matériel) :** `sizeof(Render_t)=1308` B (vs `1332` B sur #244), `CORE READY used=2276` B (vs 2300, sous les hypothèses de layout constant). Gain de heap8 **mesuré +24 B** aux quatre frontières comparées, détails ci-dessus. L'ancien jalon #243 avait confirmé +24 B de heap8 et fonctionnalité sur sa propre base **sans #244** ; cela ne constitue PAS une preuve pour ce cumul.

## Protocole de validation initial (historique, désormais exécuté)

1. CI : tests hôte d'ABI, ctor, cleanup 4096 cycles, raster, gameplay, ainsi que compilation des trois firmwares `esp32-cyd`, `esp32-cyd-bringup`, sprite-trace. Vérifier le vrai SHA des jobs avant toute revendication PASS.
2. Flasher l'environnement **normal** `esp32-cyd` de cette branche après clean build. Capturer le boot complet : `Render=1308 Game=0 Canvas=44 Total=2112`, `RENDERCORE ... bytes=1308`, `CORE READY used=2276`, PSRAM zéro, PAK 241, `shapeData/mediaTexels=NULL`. Baselines uniquement attendues, non encore observées.
3. START intro/Entrance, rendre des scènes CROSS (4 billboards / 1 accounting), caisses/pickups flash pulsé, dialogues 88/89/79 et event60, portes/automap puis EXIT. Première frame et arène attendues `71ca7465` / `c3882516`. Vérifier la fluidité ressentie **par rapport à #244B**, pas seulement le boot et la RAM.
4. LOAD slot04 V11/Malwrath : porte113, animations CROSS index324, deux ripostes triples projectile204 drainées avant commit, mort/drop/pickup/secret+5XP puis EXIT. Deux `RESIDENTRESET released=18008 empty=1`, menus `0f2c9fc3`, `HEADROOM_OK`, no panic/WDT/unsupported ; vérifier mémoire et absence d'écriture save inattendue.
5. En cas de baisse de nervosité, conserver la réserve UX et faire comparaison contrôlée : captures de `SPRITEORDER.scanUs`, `VIEWANIM FRAME.totalUs`, `MONSTERCOMBAT FRAME`, `PLAYERRES FRAME` dans scènes comparables. Le retrait de 24 B de pointeurs ne prouve à lui seul ni gain ni perte de performance. Après PASS réel et validation du ressenti : code gelé, commits post-test docs-only, puis review-ready. Merge **utilisateur uniquement**.

**État final : code CI 78/78 et hardware CYD PASS du cumul #244+#245 ; aucune compilation locale revendiquée, code gelé, sortie de Draft seulement après CI du commit documentaire.**
