# PR #236 — Jalon A : retrait des handles Render desktop, PASS matériel classic CYD

Date : 27 octobre 2026. Branche : `agent/esp32-render-retire-desktop-handles`, PR [#236](https://github.com/Kharn27/DoomRPG-RE/pull/236). Base exacte : `main=3635b17fbab573ac67343034c1cdbb8408244623` après merge #235. **SHA final du code testé : `5a90de29184f0cf28ce88d2cdecc6618d6b886c3`**, attribution par branche et procédure de flash ; les logs Serial ne portent aucun SHA/fingerprint firmware. Commit A initial `035ced88d894e09a7cb935f837e6b4bf9dac7426`, suivi du correctif tests-only `5a90de29` (voir CI).

## Frontière retirée

`Render_t::spanFunction` (pointeur de callback du raster span desktop) et `Render_t::piDIB` (texture SDL de présentation desktop) sont **desktop-only**, masqués sous `DOOMRPG_ESP32` dans `src/Render.h`. L'ESP32 partage déjà l'unique framebuffer RGB565 `PlatformVideo` 160×120 (38 400 B). Le startup natif n'initialise/n'inspecte plus de `piDIB`, le destructor privé n'appelle plus `SDL_DestroyTexture(render->piDIB)`. La garde hôte `native_render_raster_abi_retirement_host.py` interdit l'accès natif à ces champs. Les tests des corps privés startup/free restent épinglés sur les nouvelles empreintes FNV (`0ca067c4` et `6ebb123a`), leurs fixtures n'allouent ni ne détruisent une fausse texture SDL. Le corps de cleanup runtime `Render_freeRuntime` n'a pas changé (`8042d34c`). **`Render_t: 1464→1456 B`**, statiquement vérifié et confirmé au boot réel : `Game=0 Canvas=44 Total=2260`, `[RENDERCORE] INIT bytes=1456`.

CI sur le commit initial `035ced88` en échec **avant build** : oracle hôte startup encore figé sur l'ancien corps `f6343a76` ; correction ciblée tests-only `5a90de29`, sans retoucher le code de production. Après correctif : [push #40566333068](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40566333068) et [PR #40566344525](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40566344525) **SUCCESS**, hôte et firmwares normal/bringup/trace.

## START → INTRO → Entrance → EXIT

Boot `ESP32-D0WD-V3`, flash 4 MB, PSRAM 0 ; `Render=1456 Game=0 Canvas=44 Total=2260`. Shared FB `160×120, pitch320, 38400 B`. Menu `0f2c9fc3`. Intro `ade0195d`, disposal `recovered=34056`. BSP `/intro.bsp`, CRC `623f34e4`, arène `14095/c3882516`, première frame `71ca7465` (8 murs / 4430 pixels). MOVE/strafe/rotation, caisse127→Armor Shard, armure `0→4→8→12`; dialogue88 et dialogue89 `EV_DIALOG(8)→EV_NEXTSTATE(19)` avec `stateMutation=1`. Porte275 ouverture 4 frames, fermeture différée `HOLD/RELEASE` après monster turn. Caisse56→Rocket et récupération ammo3 `0→1`. EXIT sans SAVE, journal dialogue `OWNER-RELEASE recovered=1144`, `RESIDENTRESET heap8=141944→159952 released=18008 empty=1`, menu `0f2c9fc3`, `ALIVE heap8=159952 largest8=110580`.

## LOAD slot04 V11 → Malwrath → EXIT

Checkpoint `5604 B` : 30 monstres `beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activation `13→14` après porte113. Sprite324 quatre copies, `draws=4→2→0` en fonction du clipping/near. Malwrath322 `HP 11→6→2→0` (shotgun×2 puis pistolet) ; deux ripostes chacune de trois projectiles204, `MALWRATHPROJECTILE CLEAR` strictement avant `MONSTERRETAL COMMIT`. Joueur `HP 34→27→20`, armure `23→16→9`; drop Health Vial tile284 récupéré `20→24 HP`, Flak Jacket tile253 `9→23`, secret portes112/111 `+5XP`. Porte113 ouverte en 4 frames puis fermée `HOLD/RELEASE`. EXIT sans SAVE : `RESIDENTRESET 141944→159952 released=18008 empty=1`, `MAINOPAQUE finalFNV=0f2c9fc3`, `ALIVE heap8=159952 largest8=110580`.

## Comparaison / limites

Heap8 gameplay hors journal `113988→114004` (**+16 B libres** vs #235B), menu post-EXIT `159936→159952` (**+16 B**), plus gros blocs inchangés `86004/110580`. Réduction du sizeof de 8 B mais ne pas attribuer mécaniquement les écarts de malloc au seul champ. `shapeData=mediaTexels=NULL` aux frontières logguées ; aucun panic, `WORLD_POST fail` ni `RNG_REPLAY_DIVERGED` observé. Le fallback `NATIVEFRAME BOUNDARY-INLINE` et l'écart `MALWRATHPROJECTILE.pos.x - NATIVESPRITE TRANSIENT.pos.x = 0x40000000` persistent, **non diagnostiqués**. Pas de SAVE neuf, endurance, toutes cartes, parité pixels exhaustive, fingerprint du firmware, A/B vitesse ou test de fatal teardown matériel.

**Verdict : PASS ciblé du code A `5a90de291...`. Geler le code A ; présent commit de clôture documentation seule, vérifier diff et CI post-doc. Puis créer un jalon B *distinct* sur la même branche et demander une nouvelle validation CYD. PR #236 reste draft jusqu'au PASS B ; merge uniquement utilisateur.**
