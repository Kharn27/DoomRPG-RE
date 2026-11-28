# RES60-P22 — audit d'admission du gameplay 192x144 (host-only, pas de firmware)

**Branche `agent/esp32-res60-p1`, [PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364).** P20 + P21 sont PASS sur CYD : 192x144 *preview BSP*, 44 rotations chaudes sur P21, ~107ms N64 frame complète, toutes empreintes FNV stables. Le **vrai gameplay 192x144 est encore désactivé explicitement**. Ce milestone ne doit pas se déguiser en démonstrateur de jeu et ne propose aucun firmware matériel à flasher.

## Pourquoi ne pas enchaîner les micro-optimisations

Dans les 44 frames P21 la part `raysUs` N64 est 22.28ms (contre ~60ms P20). Les autres coûts mesurés subsistent : `mapUs` ~19.2ms de validation de lease avec métadonnées PAK/arena, `PRESENT` ~36.9ms. Réduire davantage le raycast n'apporterait pas mécaniquement le support du gameplay (sprites, HUD, IA, audio, input, sauvegarde). En particulier **ne jamais relâcher la validation PAK par frame** pour supprimer arbitrairement `mapUs` : cela casserait la sémantique fail-closed de P13.

## Vraies frontières relues dans le main GitHub `e84f2f9498fca0c2172af4c0ef03742e1d701b18`

| Propriétaire source | Contrat de production actuel | Conséquence directe |
|---|---|---|
| `include/platform_video_config.h` | framebuffer expérimental **192x144** avec `DOOMRPG_RES60_P1_DIAGNOSTIC=1`, mais `DOOMRPG_LOGICAL_WIDTH/HEIGHT` et viewport production restent **160x120, 160x80@0,20** | allocation vidéo seule n'autorise pas la rasterisation gameplay |
| `src/main.cpp` | `setup()` et `loop()` bifurquent dans la voie RES60, avec `BSP_PREVIEW_ONLY gameplay=NOT_STARTED` et `return` avant `initializeEngineCore` | pas de session, jeu, son ni entités RES60 |
| `src/esp_native_first_frame.c` | `MAX_SCREEN_WIDTH=160U`, rendu natif réel avec travail transitoire, PAK et map runtime | le renderer monde permanent ne peut pas produire 192 colonnes aujourd'hui |
| `src/esp_native_gameplay_frame.c` | préflight exige `DOOMRPG_VIEWPORT_Y==20` et `render->screenWidth==160`, `screenHeight==80` | coordinateur viewport/planes/sprites/HUD/present doit être adapté comme un bloc cohérent |
| `src/esp_native_plane_renderer.c` | admission stricte `screenWidth==160, screenHeight==80` | textures sol/plafond de la vraie scène ne sont pas portées |
| `src/esp_native_sprite_renderer.c` | `SCREEN_W=160, SCREEN_H=80` ; plusieurs chemins visuels assument 160x120 | sprites et effets/armes ne sont pas validés en 192 |
| `src/esp_native_gameplay_hud.c` | garde de compilation 160x120, éléments et hitboxes dépendants des coordonnées | HUD 24+96+24 du preview n'est pas un HUD de gameplay |
| `src/esp_res60_world_preview.c` | un **preview géométrique cardinal plat**, avec un inset texturé PAK diagnostique | son oracle FNV ne démontre ni les murs texturés complets ni les sprites gameplay |

**Budget incompressible constaté :** framebuffer 160×120×2 = 38 400B ; 192×144×2 = **55 296B** soit **+16 896B** / +44%. Le viewport 160×80 = 12 800 pixels passe à 192×96 = **18 432 pixels**, +44%. Le CYD a **0 PSRAM**. Le heap P21 `113924` et `largest8=86004` sont mesurés dans le *preview*, sans les allocations du jeu, du HUD réel, des sprites, de l'audio et de l'IA : **il est interdit de conclure que le vrai jeu tient en RAM à partir de ces seuls chiffres**.

## P22 livrable / critères

Ajouter `ESP32/test/res60_p22_gameplay_boundary_host.py` au workflow. C'est un **sentinel structurel** avec sources réelles (pas de mock du moteur) : vérifie la séparation de l'initialisation P1/production, les sept contrats 160 explicites, l'exclusion des `DOOMRPG_RES60_P*` du build normal et le budget mathématique. Il échoue si l'un de ces contrats change sans mise à jour de l'audit. Il ne lit aucun PAK ni source SD runtime, ne modifie pas le framebuffer, n'ajoute **aucune allocation, aucune ligne Serial et aucune définition** aux binaires. Les deux CI doivent passer ; **aucun test matériel P22** ne peut être revendiqué, car le firmware est identique à P21.

## Suite cohérente, à traiter en milestones bornés

1. **Frontière de configuration vidéo/raster :** abstraire largeur, pitch, viewport et limites de colonnes dans le *renderer natif permanent*, en préservant à l'octet les 160x120 et leur CI ; pas de gameplay 192 activé dans cette étape.
2. **Géométrie + plans réels :** valider 192 colonnes et le vrai raycast/map runtime, la palette et les murs texturés, sans nouveau framebuffer, avec probes RAM et images de référence.
3. **Sprites, arme et HUD :** traiter leurs gardes et 24/96/24 ensemble, touch, clipping, masques et textures, avec oracles du 160x120.
4. **Session/économie RAM :** réintroduire gameplay, input, musique et événements vers les propriétaires natifs existants, mesure du CYD normal, impossible de déclarer GO avant test vrai matériel.

**Décision : P21 conclut la phase micro-optimisation du preview. P22 est un audit de frontières, pas un prétendu gameplay 192x144.** Le chantier reste DRAFT, sans merge dans main.

## Résultat des contrôles de code P22 — double CI SUCCESS (24 novembre 2026)

**Commit exact du code P22 `1a114b48daca6874e2c3137b343bd9db3cdbc22b`**, sans aucune modification du programme compilé (ajout d'un test Python, d'une documentation, et d'une étape CI seulement). [Workflow push #45372593782](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45372593782) : **SUCCESS, 263 étapes**. [Workflow PR #45372607423](https://github.com/Kharn27/DoomRPG-RE/actions/runs/45372607423) : **SUCCESS, 263 étapes**. Le test de garde `RES60-P22 static gameplay resolution ownership and safe preview boundary` passe dans les deux. Compilations 160x120 et profils RES60 classic/inversé passées. Le rapport est donc **PASS HOST seulement** : **aucun nouveau test CYD n'est pertinent ni revendiqué sur ce commit sans modification du firmware**. Pas de baseline perf P22 ; P21 reste la référence matérielle.
