# PR #238 — Jalon B : cinq champs Render opaques desktop retirés — PASS classic CYD

Date de la campagne : **29 octobre 2026**. Deuxième jalon sur la **même branche et la même PR #238**, à la demande explicite de l'utilisateur, après le PASS du jalon A.

## Identifiants et code figé

- Dépôt : `Kharn27/DoomRPG-RE` ; PR : https://github.com/Kharn27/DoomRPG-RE/pull/238 ; branche `agent/esp32-render-retire-desktop-frame-timers`.
- Base main exacte maintenue : `0883682baa32ae218a6369c2e43b5689060d10dc`; **ne pas merger main par l'agent**.
- Jalon A : SHA code flashé selon la campagne antérieure `5fc43a920dcd9c9f7124d9f199d70bc980efb03d`, preuve dans `MILESTONE_PR238_RENDER_DESKTOP_FRAME_TIMERS.md`. Code A et preuve matérielle A restent inchangés.
- **Jalon B, SHA code matériel attribué par la campagne utilisateur : `fd39dee47205b5cd50fe6be7d0353f92773eb0fc`**. Le fichier Serial ne contient pas de SHA du binaire ; ce lien dépend du contexte de flash.
- CI du code B : https://github.com/Kharn27/DoomRPG-RE/actions/runs/40791105911 (push) et https://github.com/Kharn27/DoomRPG-RE/actions/runs/40791111176 (PR), **SUCCESS 75/75 chacune**, tests hôtes et builds `esp32-cyd`, bringup et trace.
- Source matérielle : fichier utilisateur `device-monitor-261029-185143.log`, **1398 lignes / 167182 octets sur disque**, d'une seule session avec LOAD→EXIT→START→EXIT. Le fichier débute à `[SDL] CYD display attached`, donc n'inclut pas les premières lignes de boot (chip, PSRAM, `Engine structs`). Aucune mesure hardware non présente n'est fabriquée.

## Frontière logicielle permanente

- `src/Render.h` rend cinq champs de compatibilité desktop-only via `#ifndef DOOMRPG_ESP32` : `int unk0`, `int viewZOld`, `short unk1`, `int unk2`, `byte unk3`. Desktop garde les cinq membres et leur ABI.
- `ESP32/src/render_startup_bridge.c` : `_Static_assert(sizeof(Render_t) == 1364U, ...)` (vs 1376U pour A).
- `ESP32/test/native_render_raster_abi_retirement_host.py` : vérifie gardes desktop-only, absence d'accès natifs à ces membres via récepteurs Render, propriétés encore live (`animFrameTime`, `skipCull`, compteurs raster), et la taille native. La première tentative du test a fait un faux positif sur `EspPlayerView.viewZOld` dans `esp_native_gameplay_save.cpp` : correction de la détection **pour ne scanner que les récepteurs Render**. Le modèle `PlayerView` et ses snapshots restent intacts.
- Ne touche **ni** raster, palette, framebuffer logique RGB565 160×120 38400B, `PlatformVideo_present()`, asset PAK, map runtime immutable, cache BSP leaf 2048B, musique/audio, RNG, gameplay, save/checkpoint ni surfaces de mutation.
- Le retrait de cinq déclarations hétérogènes `int/short/byte` a diminué la structure de **12 B**, pas de la somme naïve de largeurs en raison de l'alignement. Les allocations réelles ont également leur propre arrondi.

## Résultats matériels vérifiés

### Boot / invariants / RAM

```text
[RENDERCORE] INIT owner=esp-native-bridge bytes=1364
[CORE] Render         used=1380 heap=179408 largest=110580
[CORE] READY stages=5 allocatedObjects=4 heap used=2332 remaining=178896 largest=110580
[VIDEO] Logical framebuffer 160x120 RGB565, 38400 bytes
[DATA] Native PAK indexed, entries=241
```

Contre A : `Render_t 1376→1364` (−12 B) ; `[CORE] Render used=1392→1380`, `[CORE] used=2344→2332`. **Pas de preuve Serial de la ligne `Engine structs: ... Total=2168`** puisqu'elle précède le début du fichier (valeur attendue, pas observée).

| Frontière heap8 / largest8 | Jalon A | Jalon B | Delta heap8 libre |
| --- | ---: | ---: | ---: |
| Gameplay LOAD après cache prime | 112020 / 86004 | 112028 / 86004 | +8 |
| Menu après EXIT (2 fois identique B) | 157956 / 110580 | 157972 / 110580 | +16 |
| START après cache prime | 112832 / 86004 | 112848 / 86004 | +16 |
| START avec journal des dialogues | 110876 / 86004 | 110884 / 86004 | +8 |

Deux `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK`. Plus grande allocation inchangée. Écarts heap8 et taille structure ne sont pas équivalents mécaniquement.

### LOAD slot04 checkpoint V11 → EXIT

- `/DoomRPG-ESP32-slot04.sav` V11 **5604 octets**, `READABLE-SPATIAL result=valid`, `V9-SPATIAL-STAGE ... exact=yes`; map `/intro.bsp` **21823 B CRC `623f34e4`**, 14095 B arena FNV `c3882516`, 223 nœuds, 480 lignes, 344 sprites, 93 événements.
- Monstres `30/beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activation `df419482`, `legacyReplay=0/0/0`. `MAPFLASH REUSE HIT` et `rebuild=no`.
- NEXT_WEAPON 1→2 sans tour ; MOVE 281→282 ; Health Vial HP33→34, porte113 0→1 sur quatre frames puis fermeture `HOLD/RELEASE/POST-MONSTER-COMPLETE`.
- Malwrath sprite322, mType/subtype6, hp **11→7→3→0** sur trois coups de pistolet ; deux ripostes `loops=3` et trois projectiles204 chacune. Les **deux** `MALWRATHPROJECTILE CLEAR` et `MONSTERATKVIS COMPLETE` précèdent leur `MONSTERRETAL COMMIT`. Le joueur passe 34→29→22HP, armure 23→18→11 avant pickup. Un `COMBAT-INPUT-BLOCK` est conforme à l'animation encore active. `MONSTERDROP COMMIT` type3/20/def91 Health Vial tile284, ramassé `Got Health Vial`, puis Flak Jacket.
- Sprite324 `copies=4 draws=4→2→0` et cull/clip cohérents ; secret event15, portes112/111 `SNAP`, +5 XP. HUB → System → EXIT confirmé, **sans SAVE**.
- `RESIDENTRESET released=18008 empty=1`, `SYSEXIT MENU-READY frame=0f2c9fc3`, heap8 `157972`, largest8 `110580`.

### START depuis le menu → EXIT

- START remplace le player checkpoint par un `PlayerState_resetFresh` FNV `e745fce9`. Intro t=0 FNV `ade0195d`, touch reveal/more/skip/final continue ; `INTRODISP recovered=34056` (varie de 4 B vs A selon allocation), aucune lecture ZIP runtime.
- BSP `/intro.bsp` même CRC / arena ; `ENGINESESSION FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430`.
- MOVE initial tile904→872, crate127 transformée en Armor Shard ; dialogue88 SELECT opcode8→RESUME opcode19 ; dialogues89 SELECT opcode8→19,79 MOVE opcode26→19 ; dialogue84 fermé par MENU en `back-cancel` (aucune mutation). Porte275 4 frames à l'ouverture et `HOLD/RELEASE` de fermeture, autres caisses, pickups, déplacements jusqu'au-delà de tile738, ensuite HUB EXIT.
- `DIALOGCHAIN OWNER-RELEASE recovered=1144`, `RESIDENTRESET released=18008 empty=1`, menu final `0f2c9fc3`, `saveWrite=no`.
- `shapeData=0x0` et `mediaTexels=0x0` aux transitions et `ENGINESESSION READY` : invariants préservés aux frontières de mesure.

## Chronométrie normal `esp32-cyd`, probes actifs

La campagne A avait 16 premières traces `SPRITEORDER/SPRITEPHASE/SPRITEFLASH` activées ; la B a la même instrumentation bornée à 16 échantillons. Témoin de LOAD : **même slot04**, angle0, `NEXT_WEAPON` puis premier FORWARD 281→282. Une seule comparaison par firmware, donc pas un benchmark statistique ni un test causal.

| Temps en microsecondes | A | B | Écart B−A |
| --- | ---: | ---: | ---: |
| `LARGE-WARM spriteUs` | 37775 | 33910 | −3865 |
| `LARGE-WARM totalUs` | 84756 | 81621 | −3135 |
| Premier `VIEWANIM MOVE` après NEXT_WEAPON | 117796 | 115111 | −2685 |
| `SPRITEORDER recordUs` en LARGE-WARM | 15947 | 12255 | −3692 |

START : première MOVE `99869→96049 µs` ; à interpréter comme témoin de non-régression uniquement. Les phases plus tardives sont non instrumentées : **ne pas** comparer A/B aux ~90ms des retests A silencieux. Les améliorations apparentes ne prouvent aucune optimisation imputable au retrait des champs.

## Limites et décision

- `NATIVEFRAME BOUNDARY-INLINE` présent **5 fois**, abscisses `MALWRATHPROJECTILE` avec offset loggé `0x40000000` : phénomènes préexistants, pas attribués ni déclarés résolus ici.
- Aucune ligne `[HUB] CLOSE` dans cette campagne : passages HUB suivis de EXIT. Le `exactHud=NO` connu de campagnes antérieures n'est donc **pas revalidé** ici.
- Aucune ligne d'assert, panic, WDT ou corruption dans le fichier. Le comportement visuel ressenti sur écran, la parité pixel exhaustive, les autres cartes, SAVE neuf et endurance multi-heures ne sont pas attestés.
- **PASS CYD ciblé**, code B figé `fd39dee47205b5cd50fe6be7d0353f92773eb0fc`. Toute suite de la PR jusqu'à Ready-for-review = **docs-only**, contrôlée par diff B→doc, CI post-doc. Merge à l'utilisateur.
- Les captures suivantes peuvent utiliser le filtre PlatformIO `log2file` ; un seul fichier joint, plus de copier-coller interrompant le gameplay.
