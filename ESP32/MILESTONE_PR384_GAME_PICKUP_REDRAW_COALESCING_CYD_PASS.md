# GAME-PICKUP — Redraw coalescing + miniature d'origine : CI SUCCESS et CYD PASS borné

**18 novembre 2026 — [PR #384](https://github.com/Kharn27/DoomRPG-RE/pull/384)** ; branche `agent/gameplay-pickup-redraw-coalescing`, base exacte d'origine `main=82c566488eb35edef6ee1339d29641ac4a920c46` (ARCH-RC12). HEAD **code testé selon le contexte de l'envoi de l'utilisateur** : `1bbdb594575dc3fad343890e3c250ffac0838cbc`. Le firmware Serial ne contient pas de SHA Git ni de nom d'environnement PIO : cette association est contextuelle, non une attestation de binaire. Pendant la validation, `main` a avancé à `5d0e3fd8d2479e7310521f7573e6b8ebe62eb807` avec ARCH-RC13 #385 ; la combinaison de ces deux branches exige une CI sur le PR merge ref actualisé.

## Frontière architecturale bornée

- Les mutations de ramassage restent **immédiates et atomiques** : inventaire/HP/armure/crédits/munitions, bitset de consommation, messages et intention sonore inchangés. Aucun changement du calcul gameplay, de l'ordre des événements ou des tours ennemis.
- Suppression pour les ramassages ordinaires du **second rendu BSP/HUD/LCD synchrone** après le frame de mouvement. Le petit `EspPickupRedraw` (au plus 8 B), avec délai de calme de **600 ms**, est satisfait par un **frame de monde final normalement présenté** ou par **un seul** rattrapage quand le jeu est au repos ; aucun nouveau tour, ISR, timer, framebuffer, alloc map-wide, ZIP runtime ou PSRAM.
- Première acquisition d'arme avec **dialogue d'aide modal**, ainsi que l'automap, restent sur le chemin synchrone strict avec sa sémantique de rollback ; aucune optimisation qui court-circuite ces gardes.
- Miniature de ramassage **24×24**, FIFO bornée à **16**, décodage PAK sécurisé, composition et restauration du fond sans pixels périmés. **Clignotement original conservé** : `MINI_DURATION=500`, `MINI_ON=105`, `MINI_PERIOD=165`, `MINI_PULSES=3`. Quand une miniature attend derrière une autre, le passage peut s'effectuer dès **160 ms** visibles ; le jeu n'est jamais ralenti volontairement par la FIFO. Une commande tactile déjà en attente prend la priorité sur une présentation **cosmétique-only**.
- Traces détaillées PREPARE/COMMIT/FEEDBACK/SOURCE réservées à DEBUG/TRACE ; en `esp32-cyd` normal : COMMIT court avec action+FNV+stats et FEEDBACK lisible. Ce changement vise le coût UART à 115200 bauds et ne supprime ni transaction ni SFX.

## Historique de validation et CI

1. Premier correctif : moins de rendus additionnels, mais utilisateur encore insatisfait de la fluidité sur trois objets alignés.
2. Deuxième passe : traces Serial raccourcies, priorité à l'input et mini fixe ; comparaison utilisateur avec `main` : **déplacements/ramassages sensiblement plus réactifs**, mais la mini fixe est visuellement rejetée.
3. Troisième passe : **seuls** le rythme `litAt()` 3×105ms et le test hôte rétablissent le clignotement d'origine ; coalescing, logs compacts, priorité input et backlog borné sont conservés.

Double CI **SUCCESS** sur le HEAD de code `1bbdb594575dc3fad343890e3c250ffac0838cbc` : [PR #44407422338](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44407422338) et [push #44407410447](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44407410447). Les tests de la **vraie** logique mini hôte vérifient les trois fronts ON/OFF, chaque restauration d'underlay neuf, protection HUD/garde, FIFO, backlog, input et refus de lease PAK ; les oracles historiques SHA-sources restent stricts. Builds Xtensa `esp32-cyd`, `esp-cyd-inverted`, `esp32-cyd-trace`, `esp32-cyd-bringup` inclus dans les CI.

## Capture CYD de la troisième passe — PASS matériel borné

**Fichier envoyé** `platformio-device-monitor-261118-231034.log` : **2423 lignes, 313226 octets**, SHA256 `9aeeecc8fd5413c028c468866721ab79021e78d3350fa0eede76d7a1b0e0ad35`. ESP32-D0WD-V3, 240 MHz, flash 4 MiB, **PSRAM=0**, `shapeData=0x0 mediaTexels=0x0`. Test réel, pas un test hôte.

**Scénario observé :** MENU_MAIN → NEW GAME Entrance et prologue → déplacements rapides, porte, dialogue opcode 26 avec continuation, arme première acquisition+dialogue modal, sélections et interactions, plusieurs items → HUB/System/EXIT → MENU_MAIN → LOAD V11 du slot04 → couloir de plusieurs pickups alignés, déplacements/rotations et interactions → HUB/System/EXIT → retour MENU_MAIN, encore vivant.

- **20 `PLAYERRES COMMIT`** de ressources/arme, chacune avec `PICKUPMINI QUEUE`, **20 `START`** explicitement `blink=3x105ms/500ms`, **20 `END`** `mode=normal` (durées observées **500..687 ms**, dérive d'ordonnancement normale). **Aucun `PICKUPMINI OVERFLOW`** ni perte visible de la FIFO dans les traces. Le Serial ne trace pas chaque bord ON/OFF : la cadence vient du code et du test hôte, l'animation physique est corroborée seulement par le retour utilisateur.
- **19 `REDRAW-QUEUE`** classiques, **13 `REDRAW-SATISFIED`** par un frame de monde déjà prévu, **6 `REDRAW-IDLE`** au repos sans tour ni mutation ; **un seul `FRAME reason=RESOURCE-PICKUP`**, sur **tile 643**, pour `firstAcquire=yes` et `DIALOG OPEN-STANDALONE`, intentionnel et couvert. Aucun rendu BSP synchrone pour les 19 autres commits.
- Ramassages variés : ARMOR, HEALTH, INVENTORY, AMMO, CREDITS, WEAPON, y compris crédits sur `157→125→93`. Pas de transaction dupliquée ou rollback raté dans les logs. `MONSTERACTIVESEQ COMPLETE` livre 0 sur 40 occurrences et 1 sur 3 ; aucune propagation de tour supplémentaire attribuable aux miniatures.
- **2 `SYSEXIT MENU-READY`**, `RESIDENTRESET` propre, LOAD V11 et retour au jeu observés. Plusieurs `ALIVE` avec `heap8≈59–60 KiB` en jeu et bloc libre max `45044 B`, sans panic, WDT ou épuisement heap. Le log finit au menu vivant.
- Audio musique : `ENTRANCEMUSIC STREAM underflows=0 recoveries=0` dans les séquences de la capture ; pas de sous-alimentation détectée. Cela ne valide pas subjectivement toute l'acoustique.

## Observation hors scope

**30 `AUDIOINTENT OVERFLOW drop=oldest capacity=16`**, surtout après le LOAD et pendant la chaîne de pickups, sont **déjà suivis en [issue #377](https://github.com/Kharn27/DoomRPG-RE/issues/377)** (FIFO audio). Ne pas élargir la PR gameplay à la correction audio, ni créer un doublon d'issue. Pas de panne audio transport observée dans cette capture.

## Conclusion / limite du verdict

**PASS CYD borné** pour la suppression du redraw redondant, les mutations de ressources, la réintroduction du clignotement programmé, l'input, le modal, le retour menu/reload et les invariants RAM. Le test ne démontre ni la durée CPU effective passée en UART ni le rendu pixel-à-pixel de tous les fronts de clignotement sur LCD ; il ne couvre pas toutes les cartes et conditions de combat. **Aucune modification du code après la capture ; seulement cette documentation et ses index.** Revalidation CI à effectuer après documentation sur le merge ref actuel intégrant RC13.
