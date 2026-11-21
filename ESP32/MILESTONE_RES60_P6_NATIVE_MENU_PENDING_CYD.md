# RES60-P6 — logo original + cartes MENU_MAIN tactiles en 192×144 (PASS CYD borné)

**PR** [#364](https://github.com/Kharn27/DoomRPG-RE/pull/364) — branche parallèle `agent/esp32-res60-p1`. Date : 19 novembre 2026. Environnements PIO : `esp32-cyd-res60` et `esp32-cyd-res60-inverted`. **Ne pas fusionner comme portage 60 % jouable.**

## Oracle et première preuve physique

Source de vérité : `c.bmp` (fond étoilé), `j.bmp` (logo 108×74) depuis `/DoomRPG-ESP32.pak`; les cartes utilisent **le moteur procédural existant `EspMainMenuSkin_paint`**, dont les labels START / LOAD / OPTIONS / HELP et les dimensions 54×24 viennent de `native_main_menu_160x120_layout.h`. Aucun rendu TFT direct hors `PlatformVideo_present`, aucune deuxième surface, pas de ZIP runtime, pas de gros `shapeData` ou `mediaTexels`.

P5 réellement éprouvé par `platformio-device-monitor-261119-140453.log` sur ESP32-D0WD-V3 sans PSRAM (capture 34 lignes) : original `c.bmp/192×128` + `j.bmp/108×74`, framebuffer 192×144 RGB565/55 296 B, `fnv=cd98ab54`, `reads=153 bytes=18204 rasterUs=1189994`, scanout 320×240 `us=36864`, `heap8=191572 largest8=110580 minHeap8=181828`, SD/PAK refermé, idle. Aucune ligne `TOUCH` ou photo : **seul le succès Serial P5 est établi**, ni menu interactif ni fidélité visuelle démontrée.

## P6 — nouvelle frontière bornée

- `DOOMRPG_RES60_P6_MENU=1` s'ajoute **uniquement** aux profils RES60, hérité par l'inverted ; les builds `esp32-cyd` et `esp-cyd-inverted` restent en gameplay 160×120 inchangé.
- `EspRes60TitlePreview_renderAt` conserve une seule lease PAK, décode en tuiles bornées via le flux BMP natif, refuse les coordonnées erronées avant de peindre. `EspRes60TitlePreview_render` P5 garde son comportement centré historique.
- Logo original à `(12,35)`; les quatre cartes industrielles `EspMainMenuSkin_paint` sont peintes dans une **vue pointer-offset (32,12) de 160×120**, mais avec `stride=192` et des retours bornés. Positions physiques logiques finales des cartes : **x136..189**, y20..43, 47..70, 74..97, 101..124. Les hitboxes consultent exactement les constantes de cette peau 160×120 translatées.
- `EspRes60Menu_onTap` consomme une coordonnée physique XPT2046 déjà calibrée : `floor(x×192/320)` et `floor(y×144/240)`. Un premier appui ARM (réécrit les cartes via la même surface, **sans PAK** et sans framebuffer supplémentaire) ; deuxième appui sur le même slot : START → `PlatformVideo_showRes60WorldPreview` (**projection géométrique uniquement**) ; LOAD/OPTIONS/HELP → `[RES60P6] ACTION BLOCKED`. Toucher hors zone désarme, toucher sur le BSP retourne au menu. Aucune action 160×120 de menu/gameplay n'est appelée par ce profil.
- Le dessin est à la demande après edge tactile (callback normal `PlatformInput`, debounce existant). Aucune nouvelle tâche, ISR, FPS fixe, boucle de redraw, mutation world/entity ou audio. Le PAK est fermé avant chaque `present`.

## Gardes, tests et protocole CYD

Test `python ESP32/test/res60_p6_menu_host.py` : compile **les vraies fonctions C** `esp_res60_title_preview.c`, `esp_res60_menu.c`, `esp_main_menu_skin.c` ; mocks PAK/BMP exigent open/close exact, échec logo sans écriture, canaries autour du framebuffer 55 296 B, FNV déterministe après repaint sans SD, quatre hitboxes et transitions deux-tap ; verrou prod `P6=0` et compilation illégale `P6=1/P5=0` refusée. P1–P5, profils de diagnostic/histoire et builds CI classiques/RES60 doivent rester verts sur le SHA final. Pas encore de capture physique P6 : **aucun PASS matériel P6 déclaré**.

Sur le vrai CYD au profil LCD alternatif :

```sh
cd ESP32
pio run -e esp32-cyd-res60-inverted -t upload -t monitor
```

Avec `/DoomRPG-ESP32.pak` sur la microSD, contrôler :
1. `[RES60P6] BOOT mode=MAIN_MENU`, `[RES60P6] MENU source=PAK ... cards=4 ...`, `[RES60P6] FRAME ... pak=closed`, `[RES60P6] IDLE` ; confirmer visuellement le logo et quatre cartes (photo, couleurs/panel).
2. **Un toucher puis relâcher** START : `[RES60P6] SELECT before=-1 after=0` ; **deuxième toucher** START : `[RES60P6] ACTION START target=MAP_INTRO_BSP_PREVIEW`, `[RES60P3] RASTER` ; la valeur `ed9b68c4` dépend du PAK et de la projection P3 inchangés.
3. Toucher le BSP : `[RES60P6] TOUCH BSP-preview->MAIN_MENU`, rechargement PAK et retour quatre cartes. Sélectionner LOAD/OPTIONS/HELP, toucher à nouveau : refus sans lancement de jeu. Tester toucher hors des cartes et désarmement.
4. Fournir logs reset→idle→taps→retour + photo. L'observation Serial ne prouve pas le SHA exact du firmware sans fingerprint embarqué. Relever heap8, minHeap8, absence de panic/WDT.

**Gate** : P6 attend vérification visuelle/tactile CYD ; la migration effective du rendu 3D texturé/entités/HUD/jeu et des vrais callbacks de MENU_MAIN reste un jalon ultérieur. PR #364 draft ; ne pas fusionner sans décision explicite.

## Validation physique après le candidat P6 — 19 novembre 2026

## RES60-P6 — vrai menu original à 192×144, **PASS matériel CYD borné** (19 novembre 2026)

[PR expérimentale #364](https://github.com/Kharn27/DoomRPG-RE/pull/364), [milestone P6](MILESTONE_RES60_P6_NATIVE_MENU_PENDING_CYD.md). **Capture de l'utilisateur dans la conversation, 19 novembre 2026**, après upload `esp32-cyd-res60-inverted` (Serial sans SHA firmware). Le vrai classic CYD ESP32-D0WD-V3 4MB flash, **0 PSRAM**, monte SD 240 MiB et rend les assets PAK : `[RES60P6] BOOT mode=MAIN_MENU touch=TWO_TAP`, `MENU source=PAK bg=c.bmp/192x128 logo=j.bmp/108x74 cards=4 selected=-1 titleFNV=8c671ed4 fnv=c093a13b reads=153 rasterUs=1217593`, `[RES60P1] PRESENT ... us=36885`, `FRAME ... heap8=139136 largest8=86004 minHeap8=129392 pak=closed`. Deux vraies touches calibrées `260,46` puis `272,48` déclenchent `SELECT before=-1 after=0 fnv=a043e5ec` puis `ACTION START target=MAP_INTRO_BSP_PREVIEW`. P3 charge `/intro.bsp` CRC `623f34e4`, arena 14095B FNV `c3882516`, `RASTER width=192 world=96 rayTests=92160 hitColumns=192 wallPixels=6349 fnv=ed9b68c4 us=391653`. Une troisième touche `122,139` retourne à `MAIN_MENU` avec **le même FNV `c093a13b`**. Heap8 après retour 139036, minimum enregistré 115140 ; aucun reboot/panic/PAK actif apparent. L'utilisateur confirme « Ça marche bien... ça fait du bien de voir une meilleure résolution ». **PASS physique borné** : rendu qualitatif rapporté mais sans photo ni calibration colorimétrique ; seulement START et retour testés, pas LOAD/OPTIONS/HELP ni résistance à des fautes injectées. P6 ne lance pas le gameplay 60 %.

---

Les étapes qui restent marquées « PENDING » ci-dessus décrivent l'état du protocole **avant** réception de la capture ; le PASS borné ci-dessous prévaut pour START/retour uniquement. Aucun test physique LOAD, OPTIONS, HELP ni capture photographique.
