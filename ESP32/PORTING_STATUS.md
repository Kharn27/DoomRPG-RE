# Doom RPG ESP32 — état actuel du port

## Render solid background — REAL-CYD NON-REGRESSION PASS (2026-10-08)

Tested code SHA `0503b08b7bd89a7a50706f9d18cda51fd75679e0`; normal esp32-cyd CI [37790502396](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37790502396) **SUCCESS**. `Render_renderFloorAndCeilingSolidBG` is now defined by the permanent `esp_render_geometry_primitives.c`; generator keeps desktop/bringup source and checks exact legacy function parity. No extra allocator, backing store, or mutable world ownership.

Real classic CYD boot: Render/Game/Canvas = 1532/4/44 B; `shapeData=0x0 mediaTexels=0x0`, MAPRT `c3882516`, first gameplay frame `71ca7465`, cache SMALL-COLD `a9b263f5` and SMALL/LARGE-WARM `20c09fe4`. FORWARD midpoint, both TURN previews, crate transform and Armor Shard pickup, event 88 dialogue/opcode-19 resume, HUB pages and double-confirm SYS EXIT all worked. `[RESIDENTRESET] released=18008 ... empty=1`; final MENU_MAIN `522dc605`, heap8 `164184`, largest8 `110580`, `saveWrite=no checkpoint=unchanged`. These traces prove the **session non-regression only**; no direct invocation of the migrated solid BG function was instrumented, no LOAD or live-monster test, and no pixel-level equivalence proof beyond reported FNVs. Future Render changes need a separate CI and CYD PASS.


> Source de vérité : **main GitHub + code + logs Serial du vrai classic CYD**. État de référence au 8 octobre 2026, `main` : [`60d34d174bed0d4d4e13137f306070b5018c0fa9`](https://github.com/Kharn27/DoomRPG-RE/commit/60d34d174bed0d4d4e13137f306070b5018c0fa9). Les SHA ci-dessous décrivent des frontières testées, pas nécessairement des mesures du commit documentaire courant.

## Render Berserk desktop ABI retirement — pending CYD validation (2026-10-08)

Source audit: `Render_setBerserkColor` reads/modifies framebuffer pixels and drives legacy `SDL_UpdateTexture` / `SDL_RenderCopy`; known original call sites are in `src/DoomCanvas.c`, a retired production TU. The normal firmware must not retain this desktop presentation API: generator now guards the original body as desktop/bringup-only and checks body drift, so any remaining production caller would be a linker error rather than silent reactivation. **This does not implement or certify original Berserk behavior.** The red-tint behavior remains a functional parity item (deferred) until native ownership, trigger and CYD visual proof are established. No gameplay, death fade, framebuffer size, or sprite renderer is changed by this cut.

## Render PR #205 — CYD PASS après correction des guards (2026-10-08)

**Firmware testé** : code `291f99fcdf2e46536d5f941f13e033b16338639c` (suivi du commit documentaire `a70a5835e3f02a64cb60893fd1c8497ccaa95d2d`, aucun changement code). CI du head pré-test : [37801938309](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37801938309) et [37801946460](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37801946460), **SUCCESS**. La correction P2 rend désormais `Render_renderFloorAndCeilingSolidBG` compilable sous le guard de production ; l'algorithme n'a pas changé. La nouvelle compilation et le parcours matériel passent, **sans preuve d'appel direct à cet ABI**.

Log Serial sur classic CYD : Render/Game/Canvas `1532/4/44 B`, framebuffer partagé `38400 B`, `shapeData=0x0` et `mediaTexels=0x0`. Menu initial et final `522dc605`; MAPRT `c3882516`; première frame `71ca7465`; cache SMALL-COLD `a9b263f5`, SMALL-WARM/LARGE-WARM `20c09fe4`. Parcours réel : START → intro → Entrance → MOVE/TURN → caisse transformée, pickup armor, dialogue événement 88 avec opcode 19, HUB → SYS EXIT double confirmation → menu. `[RESIDENTRESET] released=18008 empty=1` ; menu final `heap8=164184 largest8=110580`, `saveWrite=no checkpoint=unchanged`. Aucune régression sur ce parcours. Non couvert par ce test : LOAD, monstres actifs et mort, appel positif de la fonction solid BG. La compatibilité fade legacy reste retirée, son comportement de mort est possédé par `fadeViewport()`.

**Conclusion :** PASS de compilation normale/PR et PASS matériel de non-régression, sur le code ci-dessus ; documenté sans nouvelle modification de firmware. Une preuve fonctionnelle du solid BG exigerait un appel explicite distinct ; ne pas confondre linkage et utilisation.

## Code review PR #205 (2026-10-08)

- **P1**: newline perdu par le stripping du témoin fade : corrigé au SHA `d145acf45601aa2d35050fc8f82b798fd250f5fb`, CI normale et PR vertes.
- **P2**: `Render_renderFloorAndCeilingSolidBG` était initialement placé par erreur sous `#if FIXED_VERSION != 1`, donc exclu du firmware normal. Corrigé au SHA `291f99fcdf2e46536d5f941f13e033b16338639c` : corps original inchangé, désormais placé dans le guard production `DOOMRPG_ESP32 && !DOOMRPG_ESP32_BRINGUP_PROBES`. **Les anciens PASS matériels n'éprouvaient pas ce symbole** ; attendre CI et nouveau test CYD pour ce correctif.

## Render fade ABI retirement — implementation awaiting new hardware test

Legacy call census: `src/DoomCanvas.c` invokes `Render_fadeScreen` in its desktop animation paths (lines 824, 1556); the production ESP32 `DoomCanvas.c` translation unit is retired and its live death animation calls `fadeViewport()` in `ESP32/src/esp_native_gameplay_player_death.c`. Source and the previous real-CYD death log confirm native death fade, but not ABI invocation. The latest increment **removes the production `Render_fadeScreen` export entirely**. Desktop/bringup retain original code; a newly introduced normal firmware caller must fail at link time, not silently revive legacy framebuffer semantics. The preceding native fade implementation was a temporary migration, not needed for equivalent game behavior. Pending CI and real-CYD non-regression of this *new* commit, do **not** call it tested.

## Render fade call witness — real-CYD death path (2026-10-08)

Tested firmware head `d145acf45601aa2d35050fc8f82b798fd250f5fb`, normal esp32-cyd CI [37796616566](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37796616566) and PR job [37796635096](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37796635096) both **SUCCESS**. The one-shot `[RENDERFADE] ENTRY` witness is present in the native ABI implementation; **no such entry appears in the provided death-session Serial excerpt**. The trace does show `[PLAYERDEATH] ARM`, `PHASE elapsedMs=807 fade=begin`, `READY elapsedMs=3027 fade=0 frames=51 input=death-menu`, after ordered monster attack and lethal player state commit. Therefore the **native death fade is hardware-exercised**, but no positive runtime call to legacy-named `Render_fadeScreen` is established. Do not claim that its migrated body is functionally exercised; assess whether the ABI can be retired after confirming its complete caller census.

This run also covers monster crit/hit/miss, active move, hazard touch/pass, door close held across monster turn, and stable observed idle heap8=118288 B / largest8=86004 B. `[TURNFRAME] DIAG fail=WORLD_RENDER` preview fallbacks recur, including moves around 1376,384/448 and strafe at 1397,480; this known gap remains **OPEN**, not a Render fade regression attribution. No full LOAD-after-death coverage in supplied excerpt. Hardware results apply to this exact code SHA; following commit is documentary only.

## Render fade — REAL-CYD death-sequence PASS (2026-10-08)

Hardware-tested firmware code SHA `2e541ab86824099fe2e6e9b73095826154cd9aa1`; normal CI [37793287250](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37793287250) **SUCCESS**. Real CYD: boot / MAPRT `c3882516`, first frame `71ca7465`, cache `a9b263f5` / `20c09fe4`, MOVE/TURN, crate, pickups, dialogs 88+79, door 275, active monster movement and attack, and lethal PASS_TURN all proceed. Player death logs `[PLAYERDEATH] ARM ... fadeMs=750..3000`, `PHASE elapsedMs=804 ... fade=begin`, then `READY elapsedMs=3013 phase=death-menu-ready fade=0 frames=51`. During initial stable gameplay, heap8 remains 118288 B and largest8 86004 B; death-menu heap8 114828 B, largest8 86004 B. No crash observed. **This proves the native death fade path, NOT direct invocation or pixel parity of the newly owned legacy-named `Render_fadeScreen`**. The exact call path still requires an invocation witness. The old `[TURNFRAME] ... fail=WORLD_RENDER` midpoint fallbacks, `[NATIVEFRAME] LEGACY_GUARD→RETRY→RECOVERED` and lack of LOAD testing are not silently cleared by this PASS.

## Functional parity ledger — original Doom RPG vs native ESP32

Status vocabulary: **native-validated** = original behavior reproduced and exercised on hardware; **native-partial** = live behavior with recorded gaps; **disconnected/deferred** = legacy call removed/stubbed but user-visible behavior still to reproduce; **compat-only** = old ABI retired because a separate native owner replaces it; **unverified** = original function exists/migrated but runtime invocation not yet shown. Never use a linker/build PASS alone as behavior parity.

| Original family / behavior | Native state / owner | Remaining proof or missing feature |
| --- | --- | --- |
| World BSP visibility and rendering | native-partial: compact runtime + native wall/plane/sprite renderer | Packed-wall guard recovery and some VIEWANIM WORLD_RENDER fallbacks still occur; regression-path investigation pending |
| Screen floor/ceiling solid fill (`Render_renderFloorAndCeilingSolidBG`) | **unverified invocation**, implementation migrated unchanged | First-session regression PASS SHA `0503b08b`; direct-call witness not recorded |
| Framebuffer fade (`Render_fadeScreen`) | **compat-only, retired from production linkage**; no native ABI export | Death fade **native-validated** in SHA `2e541ab8`; prove whether this specific ABI is actually called |
| Player death fall + fade + death menu | native-partial: `PLAYERDEATH` | Fall/fade/death menu real-CYD PASS; legacy shake and death sound intentionally deferred; test LOAD/RETRY from death menu separately |
| Monster activation, movement, retaliation, attack animation | native-partial: `MONSTERACT/MOVELIVE/RETAL/ATKVIS` | Live behavior observed, projectile/attack sound/message/pain face/shake and movement interpolation deferred |
| Save/load original world semantics | native-partial: save V11 and read-compatible V1–V10 | LOAD animation fallbacks, loaded live-monster lifecycle and older save coverage remain |
| Original sound/music | disconnected/deferred: `AUDIOINTENT` silent backend | Implement native playback independently; don't revive legacy Sound object |
| Legacy plane-test functions / BSP traversal / map loads | compat-only: production `esp_legacy_render_reject.c`; normal owner native | Reject stubs are not validated positive invocations; any missing original effect must get a native owner, not a fallback |
| Legacy `Render_draw2DSprite` / `shapeData` decoding | disconnected/deferred compatibility; native weapon/sprite assets read packed PAK | Inventory/weapon rendering exercised; don't restore map-wide `shapeData`; audit any original overlay/weapon effects not yet mapped |
| Legacy berserk postprocess (`Render_setBerserkColor`) | **disconnected/deferred**: desktop/bringup original retained; production ABI retired | **Berserk red-tint behavior parity remains OPEN**. Audit original trigger, timing and native HUD/framebuffer owner before claiming reproduction; never silently re-enable legacy SDL texture writes |

Update this ledger whenever a legacy function is unlinked or a corresponding native behavior becomes hardware-validated; historic milestones live in Git, not in 123 separate files.

## Règles fondamentales

- Cible : ESP32-2432S028R classic CYD, ESP32-D0WD-V3 à 240 MHz, flash 4 MB, **0 PSRAM** ; ILI9341 320×240, XPT2046, microSD.
- Framebuffer partagé : **160×120 RGB565 (38 400 B)** ; présentation nearest-neighbor ×2.
- Source des assets runtime : **`/DoomRPG-ESP32.pak`** sur SD. `/DoomRPG.zip` est un input de génération/référence, **pas** un backing store runtime.
- `shapeData == NULL`, `mediaTexels == NULL`. Ne pas réintroduire de décompression carte entière, de tables de texels globales, ni de graphes d'entités desktop.
- Architecture pérenne : catalogues/parsers natifs → `EspMapRuntime` immuable compact → petits états mutables possédés → moteur événementiel → gameplay et rendu natifs.
- Desktop/J2ME = **spécification comportementale**, jamais l'architecture mémoire imposée au CYD.
- Le redraw doit suivre les événements, et non une boucle vidéo fixe optimisée prématurément.
- Lire aussi [`ARCHITECTURE.md`](ARCHITECTURE.md) (contrat durable) et [`DOCUMENTATION.md`](DOCUMENTATION.md) (utilisation/build).

## Frontière hardware actuellement validée

| Élément | Dernier témoin CYD observé |
| --- | --- |
| Build normal | `esp32-cyd` ; dernier CI production P1 : [37775947093](https://github.com/Kharn27/DoomRPG-RE/actions/runs/37775947093) SUCCESS |
| SHA code hardware P1 | `cccd26d1e3560d1948db3004805d4c8d1a8317a2` |
| Structs Render / Game / Canvas | 1532 / 4 / 44 B |
| Main menu framebuffer FNV | `522dc605` |
| MAP_INTRO / native arena FNV | `c3882516` |
| Première frame gameplay FNV | `71ca7465` |
| Sortie SYS, session libérée | `[RESIDENTRESET] released=18008 empty=1`, journal dialogue : 1036 B |
| Heap8 après retour au menu (échantillon P1) | 164184 B ; largest8 110580 B |
| État PAK / mémoire | invariants shapeData/mediaTexels NULL aux frontières observées |

Ces chiffres **ne sont pas** une promesse universelle pour toutes les cartes ou séquences d'interaction. Les FNV du cache renderer ont changé lors du rétablissement de `FIXED_VERSION=1` : SMALL-COLD `d4151456→a9b263f5`, SMALL-WARM/LARGE-WARM `efb3a31b→20c09fe4`. Il n'y a **pas** de preuve d'équivalence pixel à pixel pour ces caches. La graine RNG varie entre boots.

## Frontières natives et propriétaires actuels

### Storage et cartes
- Backing PAK indexé et accès bornés SD/offsets ; catalogues natifs.
- `EspMapRuntime` compact immuable, accessors sémantiques sans allocations ; `EspMapState`, scripts, lignes/textures, automap et topologie sprite possédés explicitement.
- Identité carte via catalogue ; une nouvelle BSP **ne doit pas** nécessiter un renderer ou un moteur dédié par niveau.
- Les scripts/événements mutent des états explicites, avec rejet des cas non supportés et rollback là où requis.

### Gameplay et UI
- Intro et premier rendu, menu principal tactile, Help/Options, START new game, LOAD checkpoint natif, HUD/HUB, interactions, portes, pickups, armes, ennemis, mort joueur, changement de carte et checkpoint natif ont des chemins ESP32-native.
- Sauvegardes : le format V9 inclut la topologie et l'état spatial monstres/destructibles ; la compatibilité V8 est ciblée (ne rejoue pas arbitrairement les scripts).
- Les parties gameplay vivantes ont leurs propres propriétaires : actions joueur, mouvement/attaque des monstres, état du monde, rendu, feedback et save/load. Ne pas rétablir les structures desktop comme autorité.
- Détail des invariants de modules : [`ARCHITECTURE.md`](ARCHITECTURE.md).

### Render — frontière de désengagement en cours
- `Render_t` 1532 B ; aucun retour des gros membres historiques.
- Le normal `esp32-cyd` a les propriétaires permanents `render_startup_bridge.c`, `esp_render_geometry_primitives.c` et `esp_legacy_render_reject.c` pour les familles déjà migrées.
- `Render_initColumnScale`, `Render_cullBoundingBox`, `Render_transform2DVerts`, `Render_clipLine`, `Render_clipVertex`, `Render_projectVertex`, `Render_occludeClippedLine` sont natifs ; calcul fixed-point `FIXED_VERSION=1` restauré pour la parité legacy.
- `Render_findEventIndex`, `Render_relinkSprite`, `Render_renderBSPNoclip` et trois fonctions plane-test sont retirés du production legacy, via des compatibilités **fail-closed** là où nécessaire. `Render_freeRuntime` a un propriétaire natif.
- Le code généré `Render.c` subsiste **pour les autres fonctions legacy encore utilisées** : ne pas le retirer en bloc.
- Le bringup conserve les références legacy nécessaires à ses probes ; les guards bringup du correctif P1 sont revus dans le code, **mais leur compilation n'est pas encore validée**.
- Les validations de retrait sans appel effectif sont des **PASS de non-régression**, pas des preuves que les stubs ont été invoqués.
- `SYS EXIT` éprouve le reset résident natif ; il ne prouve **pas** l'exécution directe de `Render_free` / `Render_freeRuntime`.

## Limites et risques à ne pas effacer

- Le chemin **LOAD → aperçu d'animation de déplacement** possède encore un fallback connu ; la séquence monstres actifs après LOAD demande une validation dédiée.
- Les derniers tests Render/P1 fraîche session **ne couvrent ni LOAD, ni attaque/mouvement de monstres vivants**.
- En cache packed-wall, le chemin `[NATIVEFRAME] LEGACY_GUARD → RETRY → RECOVERED` a été observé ; comportement de récupération bornée, pas preuve de correction complète.
- Le dernier test HUB a rapporté `exactHud=NO` pour bandes haut+bas combinées, `exactBottom=yes` ; recomposition de la barre supérieure reportée au redraw.
- Une réussite CI ne remplace **jamais** un PASS Serial du vrai CYD. `esp32-cyd-bringup` n'est pas la référence de RAM production.

## Prochaine direction

Poursuivre le **désengagement de Render** par **famille de fonctions** : auditer les appels réels, comparer au comportement legacy, assigner un propriétaire permanent, fail-closed pour l'incomplet, instrumenter un probe borné, CI normal et preuve matérielle avant d'étendre la frontière. **Aucune nouvelle famille Render ne doit être activée en bloc.**

## Source de l'historique

Les anciens 123 rapports `MILESTONE*.md` ont été supprimés du HEAD pour éviter duplication et fausses informations courantes. Ils restent dans Git au [commit de base documentaire `60d34d174bed0d4d4e13137f306070b5018c0fa9`](https://github.com/Kharn27/DoomRPG-RE/tree/60d34d174bed0d4d4e13137f306070b5018c0fa9/ESP32) et dans les PR/commits antérieurs. Le présent fichier décrit **ce qui est valide maintenant**, non l'ordre des travaux.
