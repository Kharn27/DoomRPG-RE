# PR #239 — Jalon B : ownership natif des compteurs de mappings, PASS CYD (31 octobre 2026)

## Décision et provenance

**PASS matériel ciblé du jalon B** sur classic CYD ESP32-2432S028R, ESP32-D0WD-V3, 4 MB flash, **0 PSRAM**, firmware **normal** `esp32-cyd`. Candidat de code **figé avant test** : `f901938c70338bd782364b78e2e8a7845adbc022`, branche `agent/esp32-render-retire-legacy-bsp-buffer`, PR [#239](https://github.com/Kharn27/DoomRPG-RE/pull/239), base `main=f605f855f4d9f9e3b1b6edf47072bb8a3cea6196`. Le développeur a vérifié localement `git rev-parse HEAD=f901938…`, `_Static_assert(sizeof(Render_t)==1348U)` et branche synchronisée. La première capture du 31/10 `device-monitor-261031-000547.log` identifiait encore A (`Render=1356`) ; elle **ne valide pas B**. Un nouveau clean/flash a précédé le fichier B.

**Source matérielle B** : `device-monitor-261031-004416.log`, **1727 lignes de contenu**, boot complet puis deux sessions consécutives `LOAD slot04 → gameplay → EXIT → START → gameplay → EXIT`. Aucune empreinte SHA binaire embarquée : attribution à la campagne utilisateur, au HEAD local contrôlé et à l'assertion/taille réellement journalisées, pas à un SHA extrait du Serial.

## Frontière code B

- `src/Render.h` : les deux champs `Render_t::textureCnt` et `Render_t::spriteCnt` deviennent **desktop-only** (`#ifndef DOOMRPG_ESP32`). Ils restent indispensables dans `src/Render.c` desktop ; ce TU/son ABI ne sont pas modifiés.
- Ils étaient **encore actifs** côté ESP32 dans `ESP32/src/esp_legacy_config_mappings_startup.c`, contrairement à l'hypothèse de départ. La première CI B a justement échoué sur ces références : faux de les qualifier de morts. Correction permanente : garder les comptes dans `MappingPlan_t installedPlan`, propriétaire déjà existant, utiliser ses bornes dans la réutilisation/diagnostic et vérifier les quatre tableaux non-NULL après installation. Pas de nouveau gros owner, pas d'allocation supplémentaire.
- `ESP32/src/render_startup_bridge.c` impose `_Static_assert(sizeof(Render_t)==1348U)`; le garde `ESP32/test/native_render_raster_abi_retirement_host.py` interdit les champs retirés et les accès natifs ; `ESP32/test/native_render_mappings_retirement_host.py` vérifie les comptes du plan natif, témoin de source `4ab5b50f` et la réutilisation sur 4096 cycles hôte.
- **Inchangés intentionnellement** : backing store `/DoomRPG-ESP32.pak`, BSP/arena natifs, géométrie framebuffer 160×120 RGB565 / 38400 B et sortie 320×240 x2, raster et cache, RNG gameplay, gameplay, scripts, sauvegarde, audio, `PlatformVideo_present()`, invariants `shapeData=mediaTexels=NULL`.

**CI code B** verte, avant test : [push #41141371978](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41141371978) et [PR #41141377102](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41141377102), host tests et builds `esp32-cyd`, bringup et trace. Les changements post-PASS sont exclusivement documentaires.

## Preuves boot et mappings

Le boot matériel B affiche explicitement :
```text
Engine structs: Render=1348 Game=0 Canvas=44 Total=2152 bytes
[RENDERCORE] INIT owner=esp-native-bridge bytes=1348
[CORE] Render         used=1364 heap=179424 largest=110580
[CORE] READY stages=5 allocatedObjects=4 heap used=2316 remaining=178912 largest=110580 clip=160x120
[MAPPINGS] Header texelOffsets=592 bitShapeOffsets=1300 textures=152 sprites=252
[MAPPINGS] INSTALLED payload=8376 heap8=153144 largest8=110580 scratch=framebuffer noInflatedHeap=yes
[CONFIGMAP] READY native config handoff and mappings resident
```
Comparaison avec #239A matériel `Render=1356`, `CORE Render used=1372`, `CORE used=2324` : **−8 B** dans le layout Render et dans le root alloué. Le parser utilise toujours 152 textures / 252 sprites et son payload de 8376 B ; aucune régression de chargement mappings n'apparaît dans le test.

## Parcours hardware, ordre et bornes

**Session 1, LOAD V11 slot04 (5604 B) sur Entrance.** `/intro.bsp` CRC `623f34e4`, arène 14095 B FNV `c3882516`, état monstres `beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activation `df419482`, `MAPFLASH REUSE HIT rebuild=no`. NEXT_WEAPON, collision d'une porte bloquée puis SELECT porte113 4 frames, activation Malwrath322 et trois ripostes projetiles204 **chacune `CLEAR` avant `MONSTERRETAL COMMIT`**. Joueur HP `34→28→23→18`; Malwrath HP `11→7→3→0`, drop Health Vial puis pickup; fermeture différée porte113 avec `HOLD→RELEASE→POST-MONSTER-COMPLETE`. Flak Jacket, secret event15/portes112-111 +5XP, crédits et HUB EXIT. Première sortie :
```text
[RESIDENTRESET] heap8=139980->157988 released=18008 ... empty=1
[SYSEXIT] MENU-READY frame=0f2c9fc3 session=off resident=empty saveWrite=no checkpoint=unchanged
```

**Session 2, START après EXIT.** Player reset, intro `INTRO1 frameFNV=ade0195d`, `INTRODISP recovered=34056` ; BSP et arène identiques, `[ENGINESESSION] FIRST_FRAME map=1 angle=64 frame=71ca7465 walls=8 pixels=4430`. Caisse127 transformée en `Shell Clip` et ramassée ; porte275 ouverture et fermeture post-tour ; dialogues events88/89 opcode8, event79 opcode26, tous repris par opcode19 ; Sm Medkit, munitions et Fire Ext avec message autonome ; **chaîne event60** : dialogs successifs, continuations op19, `DIALOGCHAIN RESUME` mutations `show=1` puis `hide=1 unlock=1` (snapshot topology borné 2408 B). Journal `DIALOGCHAIN OWNER-RELEASE recovered=3568` à EXIT. Deuxième sortie reproduit **exactement le même** `RESIDENTRESET heap8=139980→157988 released=18008 empty=1` et `SYSEXIT MENU-READY frame=0f2c9fc3`.

Les **deux** sessions publient `[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0` et `[RAMBUDGET] RESERVE ... advisory=HEADROOM_OK`. Aucun `Guru Meditation`, panic, assert failed, watchdog ou corruption heap observé dans les 1727 lignes. Les traces `[NATIVEFRAME] BOUNDARY-INLINE` sont au nombre de **6** (parcours plus large que A), signal préexistant à suivre séparément, pas une preuve de nouveau défaut mapping ; les positions x des projectiles avec `+0x40000000` sont également préexistantes. Ne pas prétendre qu'elles sont corrigées.

## RAM observée (firmware normal, pas bringup)

| Frontière | #239A | #239B | Différence libre |
| --- | ---: | ---: | ---: |
| `sizeof(Render_t)` | 1356 | 1348 | −8 B de struct |
| `CORE READY used` | 2324 | 2316 | −8 B alloués |
| MENU initial heap8 | 153136 | 153144 | +8 B |
| LOAD cache primé heap8 | 112044 | 112052 | +8 B |
| START cache primé heap8 | 112856 | 112864 | +8 B |
| START avec journal dialogue heap8 | 110900 | 110908 | +8 B |
| EXIT menu heap8 | 157980 | 157988 | +8 B |
| Largest8 gameplay / menu | 86004 / 110580 | 86004 / 110580 | constant |

Les +8 B sont **observés**, ne pas généraliser à toutes les frontières ou inférer un gain de vitesse. Dans B, le warm cache LOAD `spriteUs=46172 µs`, `total=93194 µs`, premier MOVE `127595 µs` ; A mesure une autre exécution, dont les timings ne prouvent ni accélération ni ralentissement imputable au layout. Le gameplay de START B est plus long et exerce davantage de scripts.

## Limites, statut de clôture

PASS **ciblé** pour l'ownership des compteurs de mappings, le boot, chargement natif et ces deux sessions de non-régression. Aucun SAVE neuf/roundtrip, endurance longue, toutes cartes ni parité pixel exhaustive ; aucune validation audio. Aucune refonte renderer/buffer et **aucune modification de code après PASS**. Les preuves et limites A restent dans `MILESTONE_PR239A_RENDER_BSP_IO_BUFFER.md`. Étape de clôture : mise à jour **docs-only** `PORTING_STATUS.md`, `DOCUMENTATION.md`, corps PR, contrôle du diff, CI post-doc, puis Ready-for-review ; **merge exclusivement utilisateur**.
