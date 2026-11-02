# Post-#244 — retirer six pointeurs de monde desktop de Render_t (candidat à tester)

## Base et provenance vérifiées

- Base : `main` GitHub après merge #244, SHA exact `2ec2353ddcbc83203a3012d854d25a7c2b6f539f`.
- Branche : `agent/esp32-render-world-pointers-post244`, indépendante de l'ancienne PR #243 fermée sans merge.
- Source du retrait : branche archivée `agent/esp32-render-retire-legacy-world-pointers` au SHA `e7ef6d8b549da2a34a8e9a6c3501285e519625a8` ; réintégration de **16 fichiers source/tests** à leurs blobs Git inchangés. Pour chacun, le blob du nouveau main était encore identique à celui de l'ancienne base `fe3bf6cf005d0f87d534b51daf938c3473b6cdb6` : aucun écrasement des modifications #244.
- L'optimisation #244A admission BSP précoce et #244B mutualisation des quatre frames CROSS reste dans le nouveau main ; **ce cumul n'a pas encore été validé sur le vrai CYD**.

## Frontière propriétaire

Les six pointeurs `Render_t::{nodes,lines,mapSprites,tileEvents,mapByteCode,mapStringsIDs}` sont désormais exclus du layout ESP32 **normal**, mais retenus pour le desktop et l'environnement de probes bringup. Le parser et le runtime natifs utilisent les arènes compactes, overlays et backing PAK ; les mirrors heap desktop n'y ont aucun propriétaire. `nodesLength`, `linesLength`, `numMapSprites`, `mapStringCount` et les réelles tables de mappings encore nécessaires restent. `shapeData` et `mediaTexels` doivent rester NULL ; pas d'allocation/copie carte entière ni nouveau cache.

Le constructeur racine cesse d'écrire ces pointeurs absents, les gardes init/intro/first-frame/release contrôlent le propriétaire effectif, et le cleanup ne libère les anciennes listes qu'en bringup. Le test hôte extrait les véritables fonctions C de construction et cleanup et vérifie l'ABI 32-bit, l'ownership du framebuffer partagé, les cycles de destruction, le comportement des guard et la taille. Aucune modification prévue du chemin sprite #244, des actions joueur, de la temporisation, de `PlatformVideo_present()`, du gameplay/RNG, du save ou de l'audio.

**Signature attendue du code candidate :** `sizeof(Render_t)=1308` B (vs `1332` B sur #244), `CORE READY used=2276` B (vs 2300, sous les hypothèses de layout constant). Gain de heap8 **à mesurer**, pas à inventer. L'ancien jalon #243 avait confirmé +24 B de heap8 et fonctionnalité sur sa propre base **sans #244** ; cela ne constitue PAS une preuve pour ce cumul.

## Validation obligatoire

1. CI : tests hôte d'ABI, ctor, cleanup 4096 cycles, raster, gameplay, ainsi que compilation des trois firmwares `esp32-cyd`, `esp32-cyd-bringup`, sprite-trace. Vérifier le vrai SHA des jobs avant toute revendication PASS.
2. Flasher l'environnement **normal** `esp32-cyd` de cette branche après clean build. Capturer le boot complet : `Render=1308 Game=0 Canvas=44 Total=2112`, `RENDERCORE ... bytes=1308`, `CORE READY used=2276`, PSRAM zéro, PAK 241, `shapeData/mediaTexels=NULL`. Baselines uniquement attendues, non encore observées.
3. START intro/Entrance, rendre des scènes CROSS (4 billboards / 1 accounting), caisses/pickups flash pulsé, dialogues 88/89/79 et event60, portes/automap puis EXIT. Première frame et arène attendues `71ca7465` / `c3882516`. Vérifier la fluidité ressentie **par rapport à #244B**, pas seulement le boot et la RAM.
4. LOAD slot04 V11/Malwrath : porte113, animations CROSS index324, deux ripostes triples projectile204 drainées avant commit, mort/drop/pickup/secret+5XP puis EXIT. Deux `RESIDENTRESET released=18008 empty=1`, menus `0f2c9fc3`, `HEADROOM_OK`, no panic/WDT/unsupported ; vérifier mémoire et absence d'écriture save inattendue.
5. En cas de baisse de nervosité, conserver la réserve UX et faire comparaison contrôlée : captures de `SPRITEORDER.scanUs`, `VIEWANIM FRAME.totalUs`, `MONSTERCOMBAT FRAME`, `PLAYERRES FRAME` dans scènes comparables. Le retrait de 24 B de pointeurs ne prouve à lui seul ni gain ni perte de performance. Après PASS réel et validation du ressenti : code gelé, commits post-test docs-only, puis review-ready. Merge **utilisateur uniquement**.

**État présent : source transposée, validation logicielle CI et matériel en attente. Aucun test local ni PASS matériel du cumul #244+#243 revendiqué.**
