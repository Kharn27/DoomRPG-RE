# PR #239 — Jalon A : retrait du buffer/curseur BSP desktop — validation matérielle non attribuable

Campagne matérielle : **29 octobre 2026**. Branche `agent/esp32-render-retire-legacy-bsp-buffer`, PR https://github.com/Kharn27/DoomRPG-RE/pull/239. Base **main** `f605f855f4d9f9e3b1b6edf47072bb8a3cea6196` (merge #238). **Code candidat pour test : `11d0ec6cfc25a08b4c77247fdde5021dbc864f7f`** ; **non validé matériellement** au 29 octobre : le boot complet transmis après la session montre la disposition #238B (Render 1364 B), pas le layout compilé #239 (1356 B). Le binaire ne publie aucun fingerprint SHA.

## Périmètre code A

- `src/Render.h` : `Render_t::ioBuffer` (pointeur) et `Render_t::ioBufferPos` (curseur) deviennent **desktop-only** via `#ifndef DOOMRPG_ESP32`. L'original `src/Render.c` et son format BSP restent inchangés.
- `ESP32/src/engine_stubs.c` : initialisation du curseur désormais exclu ; `ESP32/src/esp_native_startup.c` : test `ioBuffer == NULL` devenu structurellement sans objet, sans retirer les autres protections des ressources legacy ni les gardes du PAK.
- `ESP32/src/render_startup_bridge.c` : assertion statique `sizeof(Render_t)==1356U`, **−8 B attendus** par rapport au 1364 de #238B. Cette taille est **prouvée par compilation CI**, pas par la partie de boot capturée ici.
- `ESP32/test/native_render_raster_abi_retirement_host.py` : garde d'absence de ces champs et de leurs accès dans les propriétaires ESP32. `ESP32/test/native_render_init_retirement_host.py` : mock constructeur synchronisé, fingerprint `33583415` calculé sur le constructeur réel sans ancien curseur.
- Aucun changement intentionnel : BSP compact immuable, `DoomRPG-ESP32.pak`, raster natif / BSP leaf cache, framebuffer 160×120 RGB565, RNG, entités, gameplay, save, animation, audio et `PlatformVideo_present()`.

La première CI du commit `0e9e663fe58a49c951e8c866b1ee676f10c5e835` échouait sur le **fingerprint historique** du constructeur hôte ; correction ciblée de ce fixture puis code final `11d0ec6cfc25a08b4c77247fdde5021dbc864f7f`. **CI code final SUCCESS, 75/75 étapes chacune** : [push #40930116152](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40930116152) et [PR #40930129599](https://github.com/Kharn27/DoomRPG-RE/actions/runs/40930129599), tests hôtes et builds normal, bringup, trace.

## Observations fonctionnelles — attribution à #239 non confirmée

Source gameplay : `device-monitor-261029-231601.log`, **1047 lignes** (capture depuis `[ALIVE] uptime=30004`, sans le boot initial). Deux sessions consécutives **LOAD slot04 → EXIT → START → EXIT**, fonctionnelles. **Boot complet transmis ensuite par l'utilisateur** : `Engine structs: Render=1364 Game=0 Canvas=44 Total=2168 bytes`, `[RENDERCORE] INIT ... bytes=1364`, `[CORE] Render used=1380`, `[CORE] READY ... used=2332`, `PSRAM: 0 bytes`, flash `827312 bytes`, hash vérifié. **Écart bloquant** : le code candidat #239 contient `_Static_assert(sizeof(Render_t)==1356U)` dans le build normal, tandis que le boot montre **1364 B, exactement la taille de #238B**. Impossible d'attribuer cette session gameplay au code #239 ; source binaire/branche locale ou artefact de flash à vérifier. La CI prouve seulement la compilation #239, pas l'identité du firmware physique.

### LOAD V11 slot04 → combat/secret → EXIT

- `/DoomRPG-ESP32-slot04.sav` V11, **5604 B**, restauration spatiale `exact=yes`. BSP `/intro.bsp` **21823 B**, CRC `623f34e4`, arène **14095 B**, FNV `c3882516`, 223 nœuds, 480 lignes, 344 sprites, 93 événements. Monstres `30/beaeaccc`, topologie `50e16fe3`, positions `e2a9d90a`, activations `df419482`; `MAPFLASH REUSE HIT rebuild=no`.
- Cache LOAD `LARGE-WARM` frame `92d59b31`, `spriteUs=33760 µs`, `total=81256 µs` **avec instrumentation** `SPRITEORDER/PHASE/FLASH` sur les 16 premiers échantillons. Premier MOVE après NEXT_WEAPON `114702 µs` ; ce sont **des observations**, pas une preuve de gain induit par −8 B. Le code #238B mesurait en séance distincte `spriteUs=33910`, `total=81621`, MOVE `115111` ; contexte non rigoureusement contrôlé.
- Porte113 ouverture **4 frames** ; sprite324 `copies4 draws4→2→0` suivant distance/clip ; Malwrath322 HP **11→7→3→0** en trois attaques, deux ripostes triple projectile204 (chaque `CLEAR` avant le `MONSTERRETAL COMMIT`). Drop Health Vial `tile284` rendu et ramassé, Flak Jacket, secret event15 portes112/111 avec +5XP. Fermeture différée porte113 `HOLD→RELEASE→POST-MONSTER-COMPLETE`.
- HUB System EXIT double-confirm, `saveWrite=no`, `RESIDENTRESET heap8=139964→157972 released=18008 empty=1`, menu `0f2c9fc3`.

### START intro/Entrance → EXIT

- Nouvelle partie reset player `e745fce9`, intro première image `ade0195d`, progression et libération `INTRODISP recovered=34056`.
- `NATIVEBOOT RESIDENT` / `MAPRT READY` même CRC/arène, première frame `ENGINESESSION FIRST_FRAME frame=71ca7465 walls=8 pixels=4430`.
- Mouvement, strafe, rotations, caisse127 transformée en Armor Shard, trois Armor Shards ramassées, porte275 ouverture 4 frames puis fermeture différée `HOLD→RELEASE`, dialogue **event79 opcode26** ouvert/fermé avec `DIALOGCHAIN RESUME ... state=1` (opcode19) et prise du Sm Medkit. **Les dialogues 88/89 ne figurent pas dans cette capture**.
- HUB EXIT, `DIALOGCHAIN OWNER-RELEASE recovered=1144`, second `RESIDENTRESET heap8=139964→157972 released=18008 empty=1`, même menu `0f2c9fc3`, `saveWrite=no`. Les transitions observées conservent `shapeData=0x0 mediaTexels=0x0`.

### RAM / signaux non résolus

- Heap8 en gameplay LOAD après cache `112028/largest8=86004`, en START après prime `112848/86004`, START avec journal dialogue `110884/86004`, menu après chacun des EXIT `157972/110580`. **Valeurs identiques aux frontières relevées #238B**, cohérentes avec le layout 1364 du boot. Aucune mesure RAM #239 attribuable ; ne pas réclamer +8B de heap effectif. Deux `RAMBUDGET ... advisory=HEADROOM_OK`.
- Zéro `ERROR`, `FAILED`, panic, WDT ou fallback dans le fichier ; deux `NATIVEFRAME BOUNDARY-INLINE` (préexistant) et abscisses projetiles `+0x40000000` (préexistant), non corrigées. Le `HUB CLOSE exactHud=NO` n'est pas exploré (EXIT seulement).
- Pas de boot complet / empreinte binaire, autre carte, SAVE neuf, endurance, pixel parity exhaustive ni ressenti visuel indépendant. **Parcours fonctionnel observé sur firmware dont l'identité est incompatible avec le code #239 ; aucun PASS #239.**

## Décision

**Code A candidat inchangé `11d0ec6cfc25a08b4c77247fdde5021dbc864f7f`. STATUT : NON VALIDÉ sur matériel.** Les commits qui suivent ce constat ne doivent corriger que les documents, sans altérer le candidat. Prochaine étape obligatoire : se placer sur `agent/esp32-render-retire-legacy-bsp-buffer`, reconstruire proprement le profil normal `esp32-cyd`, flasher, vérifier **`Engine structs: Render=1356` et `[RENDERCORE] INIT bytes=1356`** dans un nouveau boot, puis refaire un parcours matériel ciblé (au minimum LOAD/EXIT et START/EXIT). Comparer heap/timings uniquement sur ce firmware identifié. Ne pas engager le jalon B ni mettre la PR ready-for-review avant ce PASS. Branche/PR conservées ; merge à l'utilisateur.
