# PR #236 — Jalon B : 7 flags desktop Render_t retirés, comportement PASS, latence en investigation

Date : 27 octobre 2026. PR [#236](https://github.com/Kharn27/DoomRPG-RE/pull/236), branche `agent/esp32-render-retire-desktop-handles`; base `main=3635b17fbab573ac67343034c1cdbb8408244623`. Code B flashé par l'utilisateur : `5efbf3966f06d18ebdac761945e84e53e48131c6` selon la branche/version demandée, pas de SHA binaire dans les logs. Code A précédent testé : `5a90de29184f0cf28ce88d2cdecc6618d6b886c3`; documentation A : `9a11ae92b8583575b67282ca94f021d5bbff22a3`.

## Frontière du code B

Sept champs utilisés seulement par l'ancien moteur desktop passent sous `#ifndef DOOMRPG_ESP32` dans `src/Render.h` : `skipStretch`, `unk4`, `skipBSP`, `skipLines`, `unk5`, `skipSprites`, `skipViewNudge`. `skipCull` demeure dans `Render_t` natif, consommé par `ESP32/src/esp_native_first_frame.c`. `spanMode`, nécessaire à la visibilité BSP/sprites, est intact. Sept écritures à zéro dans le constructeur natif sont supprimées, sans altérer `SDL_memset(render,0,sizeof(Render_t))`. `sizeof(Render_t)` attendu 1428, vérifié en compilation et sur CYD, contre 1456 pour A ; `Game=0 Canvas=44 Total=2232`. Le desktop conserve sa structure. Garde hôte interdisant sept références natives, FNV du constructeur ré-épinglé à `09ca8937`. Diff du code B limité à `src/Render.h`, `ESP32/src/engine_stubs.c`, `ESP32/src/render_startup_bridge.c`, `ESP32/test/native_render_init_retirement_host.py` et `ESP32/test/native_render_raster_abi_retirement_host.py`. Ni `esp_native_sprite_renderer.c` ni `esp_native_gameplay_frame.c` modifiés.

CI code B [push #40576934369](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40576934369) et [PR #40576943390](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40576943390) : **SUCCESS** (tests hôte, normal, bringup, trace).

## Témoignage matériel — fonctionnel PASS

Vrai classic CYD, 4 MB flash, PSRAM 0, profil `esp32-cyd` normal. Boot `Render=1428 Game=0 Canvas=44 Total=2232`, `[RENDERCORE] INIT bytes=1428`, framebuffer partagé 160×120×RGB565 38400 B, `shapeData/mediaTexels=NULL` aux frontières. Entrée START / intro `ade0195d`, disposal `recovered=34056`, BSP `crc=623f34e4`, arène `14095/c3882516`, première frame `71ca7465` (8 murs, 4430 pixels). Caisse127 : `BREAK_REMOVE`, non explosive ; tirage différent de A (`CORERNG tableFNV fbefe05e` vs `b3392759` dans le boot A), pas de raison d'exiger un Armor Shard. Dialogue88 `EV_DIALOG(8)` → `EV_NEXTSTATE(19)`, stateMutation=1 ; dialogue89 via BACK avec `stateMutation=no`. Déplacements/rotations/strafe, deux Armor Shards (tiles839/838), porte275 ouverture 4 frames, fermeture `HOLD/RELEASE` après monster turn, caisse56 → Armor Shard ramassée. EXIT sans save : `[DIALOGCHAIN] OWNER-RELEASE recovered=1144`, `[RESIDENTRESET] 141972→159980 released=18008 empty=1`, menu `0f2c9fc3` stable. Fermeture intermédiaire du HUB inventaire `exactHud=NO`, `worldRedraw=yes`, pas de panne observée mais noter comme limite.

LOAD V11 slot04 : 5604 B, 30 monstres `beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activation13→14 à la porte113, sprite324 copies4, clipping draws4→2→0. Malwrath322 HP 11→8→4→0 ; deux ripostes de 3 projectiles204 chaque fois CLEAR avant `MONSTERRETAL COMMIT`, joueur HP34→30→25 armor23→19→14, drop Health Vial au tile284 puis HP25→29, Flak Jacket armor14→23 au tile253, secret 112/111 +5 XP. EXIT sans save : second `[RESIDENTRESET] 141972→159980 released=18008 empty=1`, `[SYSEXIT] MENU-READY frame=0f2c9fc3`. Heap8 `114044/largest86004` en gameplay, `159980/largest110580` au menu, +40/+28 B libres versus code A. Aucun crash, allocation map-wide, RNG replay mismatch ou reset partiel visible.

## Performance — NON VALIDÉE, alerte ouverte

L'utilisateur rapporte gameplay moins nerveux. Comparaison basée sur les logs CYD A (campagne précédente) et B (actuelle), **pas** sur un benchmark A/B alterné avec répétitions. Quelques témoins :

| Scène et phase | A (µs) | B (µs) | Note |
| --- | ---: | ---: | --- |
| START première translation | 111465 | 136046 | +22 % |
| START première rotation (2 intermédiaires) | 204088 | 252335 | +24 % |
| LOAD première translation | 93858 | 116296 | +24 % |
| LOAD `ENGINECACHE SMALL-WARM` world | 35132 | 35481 | stable |
| LOAD `ENGINECACHE SMALL-WARM` sprites | 37952 | 71700 | +89 % |
| LOAD `ENGINECACHE SMALL-WARM` total | 88177 | 122265 | +39 % |
| LOAD `ENGINECACHE LARGE-WARM` world | 24471 | 24939 | stable |
| LOAD `ENGINECACHE LARGE-WARM` sprites | 34117 | 67693 | +98 % |
| LOAD `ENGINECACHE LARGE-WARM` total | 70522 | 104583 | +48 % |
| LOAD premier tir Malwrath total | 133351 | 164771 | +24 % |
| LOAD premier tir Malwrath sprites | 45715 | 79133 | +73 % |

Parité témoins LOAD warm : `frame=6f93d442` dans A et B ; lecture LARGE-WARM `physical 18/30744B (A)` versus `19/30776B (B)`. `world`, `hud`, `present` similaires ; surcoût concentré sur `EspNativeSpriteRenderer_render()`, incluant lectures pack dans cette phase. `MAPFLASH REUSE HIT` : données utiles servies par **partition raw flash** et cache, non par SD directe à ce stade. La suppression des flags n'a aucune référence/lecture native dans les routines sprites ; aucun mécanisme direct démontré. Hypothèses à distinguer : variations du coût d'accès flash, disposition code/données et cache, environnement/logging ; **ne pas conclure à une cause sans mesure contrôlée**, ni optimiser la présentation. `BOUNDARY-INLINE` et l'écart de log `MALWRATHPROJECTILE.x=TRANSIENT.x+0x40000000` sont préexistants.

## Décision

**PASS fonctionnel B / performance à élucider**, PR #236 demeure DRAFT, pas merge-ready. Le commit de cette note ne change aucun code. Recommander un redémarrage à froid B identique puis un témoin A et un B à scène et cadence comparables, sans logs TRACE ; comparer `ENGINECACHE SMALL-WARM/LARGE-WARM time world/sprites/hud/present`, volumes/frames FNV, et `VIEWANIM totalUs`. Si le surcoût B persiste de manière reproductible, ouvrir un diagnostic borné des lectures de la partition raw flash / coût renderer avant de nouvelles suppressions de champs. Ne jamais prétendre que toutes cartes, endurance, nouveau SAVE, binaire SHA ou parité exhaustive sont validés.
