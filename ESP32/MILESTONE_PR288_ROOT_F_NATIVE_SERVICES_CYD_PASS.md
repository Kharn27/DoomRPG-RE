# PR #288 — Root-F : services natifs et sentinelles desktop — PASS CYD

**Date :** 10 novembre 2026. **Matériel :** ESP32-2432S028R classic CYD, ESP32-D0WD-V3, 240 MHz, 4 MB flash, **sans PSRAM**. Firmware normal `esp32-cyd`, monitor `device-monitor-261110-213725.log` (1612 lignes). **Attribution du code :** PR #288 [code Root-F initial intégré à l'ancien main `664b2614abd14c63903db838aadaf5aef57edbb2`](https://github.com/Kharn27/DoomRPG-RE/pull/288) ; le Serial ne contient pas de SHA firmware embarqué : l'attribution est basée sur le parcours utilisateur, les gardes ABI et les métriques de boot. **Intégration post-test de main SDL-C1 #287 :** `e32951b4141b1bf73af6f9614ef70a54daf82012` (merge à deux parents avec `01b4b82dfbecfdb1b857c34e4a6602a01d61c3f3`). Cette version combinée doit être validée par CI ; ce log seul n'établit pas son flash.

## Frontière de code

- ESP32 `DoomRPG_t` : **28 → 12 octets** (trois pointeurs réels conservés : `doomCanvas`, `render`, `menuSystem`), retrait groupé des deux pointeurs sentinelles `game`/`player` qui n'ont plus d'objet natif.
- `graphSetCliping` sort du root vers un état statique de présentation `nativeGraphClipSet` avec reset/accesseur ; `imageMemory` sort du root vers un compteur statique dans le propriétaire des images `nativeImageMemory`. **+8 octets d'état statique ; l'extraction ne prouve pas à elle seule un gain RAM global.**
- ABI desktop et oracle géométrique conservées. Pas de mutation du framebuffer partagé, du RNG, de l'accès PAK, du gameplay ni des trois pointeurs restant réellement propriétaires.
- Après fusion de SDL-C1 dans la branche : conserver le décodeur/indexeur PAK natif et le transfert de texture sans copie de SDL-C1 ; adapter seulement l'état de comptabilisation et la fixture différentielle correspondante. Aucun retour à ZIP ou ancien chemin `SDL_RWops/SDL_Surface` pour le chargement d'image.

## Témoins matériels sur le firmware Root-F

- Boot : POWERON_RESET, PSRAM=0, SD présente, PAK 241 entrées, framebuffer RGB565 **160×120/38 400 octets** ; `Engine structs: Render=112 Game=0 Canvas=40 Total=660 bytes` (Root-E : 676 B). `[CORE] DoomRPG used=28` est le **delta heap du root**, non le `sizeof` (12 B ; l'allocateur ajoute son coût).
- Prologue, Entrance, carte native `/intro.bsp` : arène **14095 B**, empreinte `c3882516`. Gameplay natif : dialogues événementiels 8/26, reprise opcode19, portes animées, caisses/loot, PICKUPMINI non bloquants, HUD, armes et HUB.
- Premier HUB→EXIT : `[RESIDENTRESET] ... released=18012 ... empty=1`, puis `[SYSEXIT] MENU-READY`. LOAD slot04 **V11, 5604 octets**, restauration exacte des états monstres/topologie/position/activation ; deuxième session active.
- Malwrath sprite322 : **4 ripostes** `MONSTERRETAL COMMIT`, **4× `MALWRATHPROJECTILE CLEAR`**, dégâts appliqués seulement après l'animation/projectiles ; trois `MONSTERCOMBAT COMMIT` (PV **11→7→3→0**) et mort + drop, puis nouveau HUB→EXIT.
- **2× `[RAMBUDGET] RESERVE ... HEADROOM_OK`**, marges heap8 **28412 et 27600 octets**, plus gros bloc 8-bit en jeu **61428 B**. Deux `[RESIDENTRESET] released=18012 ... empty=1`, deux `[SYSEXIT] MENU-READY`, heap8 menu final **130884 B**, `largest8=86004`. `shapeData=mediaTexels=0x0` aux frontières consignées ; aucun panic, WDT, fatal ou assertion dans les 1612 lignes ; `[ALIVE]` jusqu'à **162264 ms**.
- **3× `[NATIVEFRAME] BOUNDARY-INLINE`** (déjà connu), non fatal, à suivre distinctement ; coordonnées temporaires `MALWRATHPROJECTILE pos=1073743xxx` également présentes antérieurement et non corrigées dans ce jalon.

## Conclusion et portes de validation

**PASS matériel ciblé Root-F**, sur le chemin réellement observé. Le fichier n'établit ni tous les cas limites ni un benchmark A/B de RAM globale, et ne vérifie pas visuellement tous les BMP 1/4/8-bpp. Le **code Root-F flashé est gelé** après PASS ; seules les documentations ont été ajoutées, à l'exception de l'intégration indépendante de SDL-C1 depuis `main`, qui exige tests hôtes/Xtensa/ELF sur le SHA combiné. Ne pas déclarer un second PASS matériel de ce firmware combiné sans Serial correspondant.

**Prochain jalon Root-G :** démanteler ensemble les **trois derniers pointeurs réels** (`doomCanvas`, `render`, `menuSystem`) dans un propriétaire natif de durée de vie processus, sans allouer un second exemplaire ni libérer le framebuffer partagé, puis supprimer le root natif uniquement si le graphe d'initialisation/reset et tous les appelants le permettent. Maintenir l'ABI desktop/oracle et le fail-closed.
