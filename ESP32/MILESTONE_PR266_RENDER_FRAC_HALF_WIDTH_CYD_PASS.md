# PR #266 — retrait de Render_t::fracHalfScreenWidth : PASS matériel classic CYD

**Date : 8 novembre 2026.** Branche `agent/esp32-render-retire-frac-half-width`, [PR #266](https://github.com/Kharn27/DoomRPG-RE/pull/266) dérivée de `main=bb3c03fde1bcafd83656f40893de142c094e3416`. **Dernier SHA de code testé : `03bd64d70fa20104c80eed29cdecdbeb98361bea`**. Capture matérielle fournie : `device-monitor-261108-033922.log` (1213 lignes), accompagnée de l'acceptation utilisateur **« niquel ! »**. Les commits postérieurs à ce SHA doivent rester **exclusivement documentaires**.

## Frontière native réalisée

- `Render_t` en production ESP32 : **160 → 156 octets** (gain structurel 4 octets), `Engine structs: Render=156 Game=0 Canvas=44 Total=960 bytes` et `[RENDERCORE] INIT owner=esp-native-bridge bytes=156` mesurés au boot.
- Retrait du champ hérité `fracHalfScreenWidth` du `Render_t` ESP32 normal, bringup et trace, conservé uniquement pour le **desktop et l'oracle géométrique**. Son calcul `((screenWidth / 2) << 16) - 0x8000` est reconstruit exactement depuis la largeur à l'aide de la fonction pure `EspNativeRenderGeometry_fracHalfScreenWidth`.
- Les **12 lecteurs natifs réels** ont été migrés : visibilité BSP (3), première frame (3) et sprites (6). L'API de projection native reçoit toujours le même scalaire ; pas d'arrondi inventé, de nouveau cache, de framebuffer ou d'allocation.
- Les tests différentiels (oracle desktop), les largeurs paires/impaires 0..160, le corps du setup et les fixtures CROSS exploitent la primitive native réelle, sans modifier la sémantique du rendu ou le contrat de présentation. Les effets visuels, la logique tour par tour, PAK et RNG ne changent pas intentionnellement.
- **Limite de mesure :** 4 octets de taille d'objet ne prouvent pas un gain net de 4 octets de heap libre : allocations, fragmentation et bruit de boot peuvent masquer la différence.

## Validation CI sur le code éprouvé

- [CI push #42438971302](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42438971302) et [CI PR #42438975212](https://github.com/Kharn27/DoomRPG-RE/actions/runs/42438975212) : **SUCCESS**, 99 étapes par job, **zéro échec**, y compris tests hôte et builds `esp32-cyd` normal, `esp32-cyd-bringup` et trace.
- Les échecs intermédiaires ont permis de repérer les usages de géométrie native, les fixtures de comparaison et une substitution BSP incorrecte ; ils ont été corrigés sur cette même branche **avant le SHA de test**. Aucun code n'a été modifié depuis ce SHA.

## Validation du véritable classic CYD (firmware normal demandé)

- **Matériel :** ESP32-D0WD-V3 dual core 240 MHz, flash 4 MiB, sans PSRAM ; framebuffer logique partagé **160×120 RGB565 / 38400 octets**, écran physique 320×240 nearest-neighbor ×2, PAK indexé 241 entrées depuis SD. `[RENDERCORE] SETUP ... view=160x80@0,20 arrays=1280B`.
- **Démarrage NEW GAME :** intro → Entrance `/intro.bsp`, BSP CRC `623f34e4`, arena native `14095` octets / FNV `c3882516`, première frame `71ca7465` ; `[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0`. Déplacements, rotations, tirs de caisses, pickups, ouverture puis fermeture différée de porte 275, automap et dialogue event 89 avec deux reprises de script `EV_NEXTSTATE` opcode 19.
- **HUB et sortie :** fermeture inventaire `exact=yes exactHud=yes exactBottom=yes`, puis `[RESIDENTRESET] ... released=18008 ... empty=1`, `[SYSEXIT] MENU-READY ... session=off resident=empty` (parcours répété deux fois dans la capture).
- **LOAD V11 :** sauvegarde `/DoomRPG-ESP32-slot04.sav` (5604 B) restaurée avec script, lignes, automap, monstres, positions, activation et topology `exact=yes`, `[ENGINESESSION] RESUME-VISIBLE` ; porte 113 ouverte puis refermée, sprite CROSS index 324 (4 copies), combat contre Malwrath sprite 322, trois salves de projectile 204 (9 frames), riposte séquencée puis commit `playerHP=34->29` / `armor=23->18`, `PLAYERIMPACT` frames 0/2→2/2, puis monstre tué `hp=6->0`. HUB inventaire et grille 12 armes : sorties `exact=yes exactHud=yes exactBottom=yes`.
- **Budget RAM normal :** au NEW GAME après armement résident `heap8=110776`, puis échantillons ALIVE `heap8=109964` / `largest8=86004` ; après LOAD `heap8=109960` puis `109920` / `largest8=86004` pendant combat ; après dialogue du premier parcours `108820` (journal de 1128 B). Les deux `[RAMBUDGET] RESERVE` sont **`advisory=HEADROOM_OK`** : marge `margin8=53432` au START et `52616` au LOAD.
- **Fiabilité :** au terme de cette capture, aucune trace Guru Meditation, panic, WDT ni échec d'assertion ; `shapeData` et `mediaTexels` sont NULL à toutes les frontières instrumentées. Retour testeur **« niquel ! »** après avoir été invité à flasher le profil normal `esp32-cyd`.

## Limites et clôture

Le Serial ne contient pas de SHA Git embarqué ; l'association au commit `03bd64d70fa20104c80eed29cdecdbeb98361bea` découle de la demande de flash de cette révision et du retour de test correspondant. Ce PASS est **fonctionnel et matériel pour les parcours observés**, pas une preuve de parité pixel-perfect absolue, de performance A/B, de consommation mémoire sur toutes les cartes, d'endurance ni de son (backend toujours silent).

**Jalon matériel PASS, code gelé au SHA indiqué.** Suites : commits docs-only, CI finale et vérification que la PR ne contient aucun code post-test ; passage en merge-ready si vert, **aucun merge de `main` sans demande explicite**.
