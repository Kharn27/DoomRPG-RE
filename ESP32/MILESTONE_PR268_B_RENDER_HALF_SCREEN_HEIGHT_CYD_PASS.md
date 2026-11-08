# PR #268-B — retrait de Render_t::halfScreenHeight : PASS matériel classic CYD

**Date : 8 novembre 2026.** Dépôt `Kharn27/DoomRPG-RE` ; [PR #268](https://github.com/Kharn27/DoomRPG-RE/pull/268) (Draft), branche `agent/esp32-render-retire-skipcull-state`, `main=77649ebf992e57aa5f57ff6efd138b45d871c9c2`. **SHA exact du code et des tests B flashé : `7aa475e9a9206e43efa235194409043eb20e76f0`**. Testeur : « Si c'est ok on continue sur la branche ». Capture réelle `device-monitor-261108-234151.log` : **1 328 lignes, 157 400 octets**.

## Frontière technique permanente

- Avant B, `Render_t` ESP32 vaut **140 octets** (PASS A, `skipCull` retiré). Après B, la structure vaut **136 octets** ; diagnostic boot `Engine structs: Render=136 Game=0 Canvas=44 Total=940 bytes` et `[RENDERCORE] INIT owner=esp-native-bridge bytes=136`.
- Le stockage de `halfScreenHeight` n'existe plus en firmware normal, bringup ni trace. `Render_setup` desktop calcule historiquement `screenHeight/2` après normalisation des hauteurs impaires. L'ESP32 utilisait cette valeur constante dans trois modules : raster mural (`esp_native_first_frame.c`), sprites (`esp_native_sprite_renderer.c`) et plans (`esp_native_plane_renderer.c`). Les trois utilisent dorénavant la petite primitive inline allocation-free `EspNativeRenderGeometry_halfScreenHeight(render->screenHeight)`. Le legacy et l'oracle conservent le champ et la routine d'origine.
- Audit des **178 sources native ESP32** : unique écriture dans le setup, trois consommateurs actifs, aucune mutation en session. Pas de changement voulu de culling, perspective, topologie, caches PAK, animations, affichage, combat, RNG ni framebuffer.
- Nouveau `native_render_half_height_retirement_host.py` : profil négatif pour les trois firmwares, profil positif desktop/oracle, trois lecteurs exacts, 513 hauteurs normalisées et leurs translations verticales ; `render_geometry_host.c` étendu. Garde ABI `sizeof(Render_t)==136`, anciens tests setup/raster adaptés. Les deux correctifs de garde historique et d'un commentaire d'en-tête ont précédé le flash.

## CI de B (avant capture matérielle)

- [Push #42528141591](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42528141591) : **SUCCESS, 101/101 étapes**.
- [PR #42528150108](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42528150108) : **SUCCESS, 101/101 étapes**.
- Compilations `esp32-cyd`, `esp32-cyd-bringup`, `esp32-cyd-trace`, tests hôte géométriques, audit de symboles et artefacts ELF.

## Preuves matérielles du log du firmware normal

- Chip `ESP32-D0WD-V3`, deux cœurs 240 MHz, 4 Mo flash, **PSRAM 0**. PAK 241 entrées ; framebuffer logique **160×120 RGB565 / 38400 B** (sortie 320×240 ×2), viewport render **160×80@0,20**, tableaux `1280B`.
- NEW GAME /intro.bsp, Entrance, CRC `623f34e4`, arène native `14095B` FNV `c3882516`, première frame `71ca7465`. Déplacements/tours intermédiaires avec `[VIEWANIM] ... gameplayStable=yes`, collecte de deux armor shards puis medkit, porte275 ouverte et refermée 4 frames après tour, entrée dialogue `event=79 opcode26`, séquence FASTFORWARD/PAGE/CLOSE et reprise `opcode19`. Retour HUB puis EXIT : `[RESIDENTRESET] ... released=18008 ... empty=1`, `[SYSEXIT] MENU-READY frame=1945fafa`.
- Navigation Options→Video→Theme puis choix **PHOSPHORE (index2)** et sauvegarde `[NATIVECONFIG] SAVE result=committed version=3 brightness=25 orientation=NORMAL theme=2`. Retour MENU_MAIN rapide et cohérent (FNV `3721ecf7`).
- LOAD V11 `/DoomRPG-ESP32-slot04.sav`, 5604 octets, même map Entrance, restauration ressources/scripts/lignes/caisses/automap/monstres/topologie/positions/activation/drops, `[ENGINESESSION] RESUME-VISIBLE`, `shapeData=0x0 mediaTexels=0x0`. Deux changements d'arme sans tour, pickup, porte113 ouverture/fermeture à quatre frames, rendu CROSS sprite324. Malwrath sprite322 **3 attaques successives, chacune 3 projectiles204/9 frames de trajectoire**, **3 `[MONSTERRETAL] COMMIT`** après animation ; impact PAK sprite184, pain/flash/border, dégâts et critique, 3 attaques joueur au shotgun ; `[MONSTERCOMBAT] DEATH-SETTLE sprite=322`, drop medkit ramassé. Secret `[SECRET] FOUND event=15 doors=2 xp=5`. Deuxième EXIT : `[RESIDENTRESET] ... empty=1` et `[SYSEXIT] MENU-READY frame=3721ecf7`.
- RAM **2× `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK`** (marges `margin8=51340` et `50328`), plus gros bloc 8 bits en jeu **86004 B**. **2× `[RESIDENTRESET] empty=1`**, **2× `[SYSEXIT] MENU-READY`**. `shapeData=0x0 mediaTexels=0x0` observés aux frontières instrumentées ; aucune `[NATIVEFRAME] FAILED`, panic/Guru/WDT/backtrace/abort dans les 1328 lignes.

## Portée et continuation

**PASS matériel ciblé de B** sur cette capture ; aucune conclusion de performance A/B, d'économie heap nette strictement égale à 4 octets, de parité pixel-perfect ou de stabilité multi-heures. Le code C/CPP/h du jalon B reste **gelé au SHA `7aa475e9a9206e43efa235194409043eb20e76f0`**. Le présent document, et les deux entrées d'index, sont un **commit exclusivement docs après test**. La demande utilisateur autorise **une suite C sur la même branche**, mais **pas de fusion de PR #268 dans `main`**. C nécessitera sa propre CI et son propre log réel.
