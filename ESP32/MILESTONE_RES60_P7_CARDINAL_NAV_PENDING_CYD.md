# RES60-P7 — rotation cardinale native + commandes tactiles (PASS matériel CYD borné)

**Repo** `Kharn27/DoomRPG-RE`, [PR #364](https://github.com/Kharn27/DoomRPG-RE/pull/364), branche `agent/esp32-res60-p1`, 19 novembre 2026. Ne pas merger en version jouable 60 %. Cette étape suit le **P6 PASS matériel borné** (START deux taps et retour, PAK natif validé).

## Frontière exacte

Le micro-état de prévisualisation possède uniquement `uint8_t res60QuarterTurns=0..3`. Pour `/intro.bsp`, le spawn original `angle=64` donne vue nord. `EspRes60Nav_resolveAngle` ajoute `64*turns` modulo 256 (0 E, 64 N, 128 W, 192 S). LEFT augmente `turns`, RIGHT le diminue ; aucune translation, collision, monstre ou script gameplay n'est activé. Le compact `EspMapRuntime` de 14 095B est reconstruit depuis `DoomRPG-ESP32.pak` pour chaque image demandée, utilisé pour lire les 480 lignes originales puis relâché AVANT `present()`. Le modèle P3 original `EspRes60WorldProbe_renderIntro` reste callable à `turns=0`, son FNV brut historique ne doit pas varier.

Le painter `EspRes60Nav_paint` n'écrit que dans les 24 premières et dernières lignes du framebuffer 192×144 RGB565. Il remplace les bandes rouges/jaunes de debug par un fond sombre et les vraies plaques du skin industriel natif (3 boutons LEFT / MENU / RIGHT en bas, direction EAST/NORTH/WEST/SOUTH en haut). Comme P6, il utilise un **simple pointeur de vue 160×120 avec stride=192**, aucun second framebuffer, aucune texture de carte ni ZIP, aucune allocation/lecture PAK pour l'overlay. Le rayon 192×96 reste octet pour octet celui de P3 avant bande de contrôles. Les trois hitboxes logiques de bas de page sont **x20..67, 72..119, 124..171**, **y120..141** ; conversion tactile physique `floor(x*192/320), floor(y*144/240)`, commune au dessin et à la hitbox. Les touches hors zone sont ignorées. Menu → monde : toujours 2 appuis START ; ensuite LEFT/RIGHT nécessitent un seul appui (debounce déjà assuré par `PlatformInput`), MENU revient au menu. Une rotation n'est commitée qu'après `PlatformVideo_present()` réussi.

## Sécurité, CI et limitations

Contrats de compilation P7 requiert P6/P3 ; aucun symbole nouveau en build 160×120. `ESP32/test/res60_p7_navigation_host.py` compile vrai `esp_res60_nav.c` avec le vrai `esp_main_menu_skin.c`, valide quatre angles, transitions, hitboxes, canaries du framebuffer, refus des paramètres invalides avant écriture, conservation de la bande 3D, FNV déterministe, compilations P7=0 et P7 sans P6 rejetée. Builds Xtensa PIO des deux profils RES60 et profils 50 % dans la CI. État final de CI à renseigner seulement après consultation de GitHub ; **aucun test de matériel P7 à ce jour**.

P7 conserve **rendu géométrique flat-shaded** : `textureSamples=0, sprites=0`, pas de gameplay, pas de mouvement, pas de vrai HUD. Le raycaster P3 fait `192 * 480 = 92 160` tests de lignes par orientation ; il s'agit d'un **oracle/prototype** non du renderer natif permanent. Les futures familles doivent remplacer le raycaster avant gameplay réel, ne pas sur-optimiser le scanout LCD.

## Protocole matériel sur classic CYD au panneau alternate

```sh
cd ESP32
pio run -e esp32-cyd-res60-inverted -t upload -t monitor
```

Carte SD avec `/DoomRPG-ESP32.pak` inchangé. Vérifier : au boot les quatre cartes MENU P6, puis START → relâcher → START. Attendre `[RES60P7] RASTER ... turns=0 angle=64 ... rawP3FNV=ed9b68c4`, `[RES60P7] FRAME ... mapRuntime=RELEASED pak=closed`. Taper **LEFT** en bas (bouton gauche) : `TURN direction=LEFT from=0 to=1`, `angle=128` et nouvelle image. **RIGHT** en bas : `from=1 to=0`, `angle=64` et FNV brut P3 à nouveau `ed9b68c4`. **MENU** en bas au centre : retour menu P6 `fnv=c093a13b` attendu sous PAK/skin identiques. Tester au moins quatre quarts de tour (vue ouest/sud/est/nord), puis retour. Capturer log complet et photo avec les nouvelles bandes sombres. Vérifier absence de panic/heap anomalie/leaks ; minHeap8 P6 observé jusqu'à 115140B mais P7 n'a pas encore de RAM matérielle mesurée.

**Validation exigée** : Serial + avis visuel ; rapport PASS et documentation post-test docs-only ensuite. PR reste draft sans fusion.

## Résultat du test physique P7 — 19 novembre 2026

## RES60-P7 — rotations cardinales natives sur Entrance 192×96 : **PASS matériel CYD borné** (19 novembre 2026)

[PR #364](https://github.com/Kharn27/DoomRPG-RE/pull/364), [rapport P7](MILESTONE_RES60_P7_CARDINAL_NAV_PENDING_CYD.md). **Capture Serial utilisateur** après upload du profil `esp32-cyd-res60-inverted` sur vrai CYD ESP32-D0WD-V3 240 MHz, 4 MiB flash, **PSRAM=0**. PAK `/intro.bsp` **CRC `623f34e4`**, arène compacte **14095B FNV `c3882516`** identiques à P3/P6. Premier START en deux appuis, puis rotations RIGHT `0→3→2→1→0→3→2→1→0`, LEFT `0→1→2→3`, bouton MENU → retour P6 **FNV `c093a13b`**. Orientations : **N angle64 `rawP3FNV=ed9b68c4`, W angle128 `f43a2791`, S angle192 `2697640d`, E angle0 `48ae8442`** ; FNV final avec bandes de navigation N `d68d97c8`, W `ed6e5b1b`, S `80af0b36`, E `ec6061cb`, reproductibles à chaque retour. Deux touches hors zones `TOUCH IGNORED` correctes. Chaque vue `rayTests=92160`, `us≈392–393k` de raster, `FRAME totalUs≈440–441k` (presentation ≈36.9ms), `heap8=139012`, `largest8=86004`, `minHeap8=115156`, `mapRuntime=RELEASED`, `pak=closed`, sans panic/WDT ni dérive heap8 entre rotations. Menu restauré avec PAK refermé et FNV identique. **PASS strictement borné :** vérification Serial des quatre orientations, de navigation et du nettoyage ; pas de photo/saisie d'une commande arbitraire invalide, ni textures, sprites, monde mutable ou vrai gameplay 60 %. Serial ne contient aucun SHA binaire ; attribution au code P7 par contexte d'upload et empreintes, pas par identification cryptographique du firmware. Code rebasé sur `main=b1c6660d55f39623c4e24d874b049cd9b452ad47`, commit `b75fb5966af7824a9368570f97d2ecfb7cdf155e`, double CI **SUCCESS 229/229** ([push 44597678028](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44597678028), [PR 44597700310](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44597700310)). P7 matériel PASS, PR toujours draft, pas de fusion.

---

Les mentions « CYD PENDING » et commandes ci-dessus représentent l'état et le protocole **avant** le test : elles ne concernent plus P7 pour les rotations cardinales et le retour menu. P7 reste distinct du portage jouable 60 %.
