# PR #219 — journal historique des lots A→D (18 octobre 2026)

> Archive des preuves de chaque frontière et de leurs anciens statuts intermédiaires.
> Des remarques comme « prochain Game_init » ou « exports=6 » décrivent **leur
> état à ce moment-là**, pas l'état final. Pour l'état courant, voir
> [PORTING_STATUS](../PORTING_STATUS.md) et le [guide d'usage](../README.md).
> Le dernier lot E, postérieur aux preuves ci-dessous, est récapitulé
> dans PORTING_STATUS avec son témoin matériel.

## PR #219 — générateur C desktop retiré, SDL packed et parité préservés (18 octobre 2026)

### Quatrième frontière : blitter glyphes Canvas privé — PASS CYD (18 octobre 2026)

**Branche et PR conservées** :
`agent/esp32-retire-generated-doomrpg-oracle`,
[PR #219](https://github.com/Kharn27/DoomRPG-RE/pull/219)
ouverte en draft ; pas de merge dans `main`.
**SHA code du lot D testé sur la branche** :
`461d5d917a630b96cd08b34bf28713c65fd60d8f`.
[CI SUCCESS #39161846989](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39161846989)
(55 étapes, hôte + normal/bringup/trace, compiledb/ELF).

- `DoomCanvas_drawImageSpecial` n'a plus d'export ABI externe.
  Il est devenu `static EspDoomCanvas_drawImageSpecial` dans
  `esp_legacy_doomcanvas_bridge.c`, appelé exclusivement depuis
  `DoomCanvas_drawString1`. **Le corps entier de dessin/clipping/
  tiling est inchangé** et verrouillé par le test hôte
  `doomcanvas_private_blitter_host.py`
  (FNV-1a du corps `4c4a9a93`).
- **Cinq exports Canvas restent compilés** :
  `DoomCanvas_init/free/drawString1/setState/startup`.
  Les audits ELF des trois profils interdisent
  `DoomCanvas_drawImageSpecial` et les 21 TU desktop
  continuent d'être exclues.
- Mesures CI : normal RAM statique **49 488 B**, Flash
  **820 945 B** (−216 B par rapport au lot C) ; bringup
  **49 640 / 841 573 B**, trace **49 536 / 837 569 B**.
  Le build local de l'utilisateur donne
  **49 488 B RAM / 820 961 B Flash** ; écart de Flash **+16 B**
  par rapport à CI, non expliqué par la trace.
  Le firmware binaire installé représente **821 328 B**
  sur flash, ce qui ne correspond pas directement à la jauge
  PlatformIO des sections utilisées : ne pas confondre les métriques.

**PASS hardware ciblé sur le CYD normal, logs transmis par l'utilisateur :**

1. Upload `platformio ... -e esp32-cyd` réussi (hash flash vérifié),
   ESP32-D0WD-V3 240 MHz sans PSRAM, SD/PAK 241 ressources,
   configuration native restaurée à luminosité **75 %** ;
   `Engine structs: Render=1532 Game=0 Canvas=44 Total=2336`.
   `[CORE] Game used=0` et `bridge=absent exports=0`.
   L'init Canvas affiche encore `exports=6` :
   **diagnostic obsolète uniquement**, pas un sixième export
   ELF (audit CI : cinq).
2. `[MAINOPAQUE] DASHBOARD` FNV **`0f2c9fc3`**
   et `heap8=154908`, `largest8=110580` durant six
   `[ALIVE]` initiaux. Menu `Help/About` :
   83 lignes parsées, pages 0→8→16→8, fonds/panneaux
   `5f22cf6b`, `d0788359`, `9213df95`, puis
   BACK redessine exactement le menu FNV `0f2c9fc3`.
   Les textes sont exercés via le blitter interne.
3. `Options` : FNV **`0acffb35`**, cards Back/Video
   actives, Input/Sound explicitement différées, BACK
   retourne FNV `0f2c9fc3`, état et heap inchangés.
   Pas d'ouverture du sous-menu luminosité dans cet extrait.
4. `START` : nettoyage natif `154908→163348`, intro
   packed 4 images, première frame FNV **`ade0195d`** ;
   sept taps, `FINAL-TEXT-PRESENTED` et
   `FINAL-CONTINUE`, `[INTRODISP] READY`
   `heap8=129292→163348` (**34 056 B libérés**).
5. `/intro.bsp` CRC `623f34e4`, arena 14 095 B
   FNV `c3882516`, `[ENGINESESSION] FRESH-VISIBLE`,
   `[RESIDENTGAMEPLAY] READY`. Deux `[ALIVE]`
   consécutifs à **heap8=113816**, **largest8=86004** ;
   `shapeData=0x0 mediaTexels=0x0`.
   Aucune erreur/crash mémoire visible, mais cet extrait
   **ne contient ni LOAD, ni combat** sur le lot D.

**Dette repérée par ce test** : warning compilateur
`DoomCanvas_drawString1(char*)` appelé avec littéral
`"Loading..."` `const char*` ; la correction de
const-qualification et la chaîne de diagnostic
`[DOOMCANVASBRIDGE] INIT exports=6` doivent appartenir à
un **commit distinct**. Ne pas modifier rétroactivement
le lot D testé, ni lui attribuer un résultat hardware
d'un changement de code futur.

**Conclusion** : lot D **PASS CYD ciblé** sur les glyphes et
menus exercés, intro/Entrance et stabilité heap. Les invariants
mémoire restent tenus. Aucun SHA n'est imprimé dans le
runtime : l'empreinte flashée n'est pas indépendante du
workflow rapporté. Branche et PR restent ouvertes,
sans merge.

---

### Troisième frontière : retrait du dernier Game_init / Game_t alloué — PASS CYD (18 octobre 2026)

**Branche et PR inchangées** : `agent/esp32-retire-generated-doomrpg-oracle`,
[PR #219](https://github.com/Kharn27/DoomRPG-RE/pull/219) en cours, **aucun merge dans main**.
**Dernier SHA de code testé en CI** :
`33835b6dadef03aafe10bdb7874459c287cfeb09`.
[CI SUCCESS #39160572323](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39160572323)
(54 étapes : host, normal/bringup/trace, compiledb, garde des symboles ELF).

- `ESP32/src/esp_legacy_game_bridge.c` **supprimé**.
  Aucun export `Game_init`, `Game_activate`, `Game_unloadMapData` dans
  les ELF ; aucune unité `src/Game.c` desktop compilée.
- L'instance heap de `Game_t` a disparu. Le **champ structurel**
  `doomRpg->game` reste provisoirement compatible mais est
  **invariablement NULL**, y compris dans les gardes de session,
  intro, chargement, sortie et le teardown fatal.
  `DoomRPG_getEngineMetrics` annonce `game=0 B` ;
  l'étape `DOOMRPG_CORE_GAME` reste une étape de diagnostic de 0 B.
  Le démontage de la structure racine n'est **pas** entrepris ici.
- Test `game_inert_abi_retirement_host.py` : métriques extraites du vrai
  code natif, absence d'appels Game_* dans les sources ESP32, refus de
  tout pointeur Game non NULL. `doomrpg_root_owner_host.py` a exécuté
  **4096 cas de teardown simulé**, sans allocation Game.
- CI mémoire statique : normal **49 488 B**, bringup **49 640 B**,
  trace **49 536 B** (inchangé) ; Flash normal **821 161 B**,
  bringup **841 789 B**, trace **837 781 B**. Ces tailles ne sont
  pas une preuve de parité binaire ELF.

**PASS matériel ciblé, vrai CYD classic, normal `esp32-cyd`, logs utilisateur
du 18 octobre 2026** (l'extrait commence au tap START et ne contient pas
l'en-tête `[CORE]` ; aucun SHA embarqué n'est fourni : rattachement au
flash de la branche annoncé, pas fingerprint vérifié indépendamment) :

1. MENU_MAIN → **START** : `[MAINSTART] Begin` valide avec
   `shapeData=0 mediaTexels=0` et `heap8=154908` ; nettoyage
   `154908→163348` (**+8440 B**) sans structures legacy.
   Nouveau joueur niveau 1 HP30/30, `stateFNV=e745fce9`,
   quatre images d'intro adoptées en packed indexed.
2. **Intro** : frame initiale FNV `ade0195d`,
   sept taps acceptés, `FINAL-TEXT-PRESENTED`,
   `FINAL-CONTINUE`, `[INTRODISP] READY` :
   `heap8=129292→163348` (**34 056 B récupérés**),
   `largest8=98292→110580`, ressources NULL,
   chargement natif sans retour `DoomCanvas_loadMap`.
3. **Nouveau jeu Entrance** : `/intro.bsp`, CRC
   `623f34e4`, arena `14095 B` FNV `c3882516`,
   spawn `544,1824`, angle 64 ;
   `[ENGINESESSION] FRESH-VISIBLE` et
   `[RESIDENTGAMEPLAY] READY`, `shapeData/mediaTexels=0`.
   Après initialisation, `[ALIVE]` avec
   `heap8=113816`, `largest8=86004`.
4. **EXIT depuis le HUB/SYSTEM**, sans sauvegarde :
   `[SYS] EXIT-CONFIRMED` suivi de
   `[RESIDENTRESET]` before=1/1/1/1/1/1/1 →
   after=0/0/0/0/0/0/0, `empty=1`,
   libération résidente **18 008 B**.
   Menu opaque disponible (`[SYSEXIT] MENU-READY`),
   `heap8=159752`, `largest8=110580`.
5. **LOAD slot09 V11 5 604 B** depuis ce même menu :
   `[NATIVESAVE] LOAD` restaure ressources, scripts, lignes,
   automap, état/topologie/position/activation monstres et drops,
   `[ENGINESESSION] RESUME-VISIBLE`,
   `[RESIDENTGAMEPLAY] READY`. Trois `[ALIVE]` consécutifs
   à **heap8=113816**, **largest8=86004** :
   pas de fuite observée sur la durée de cet extrait.
   Aucune attaque n'est montrée après cette reprise dans **ce**
   log ; ne pas lui attribuer de PASS de combat nouveau.

**Comparaison matérielle sur le même parcours** :
le lot B affichait `heap8=154892` avant START et
`heap8=113800` en jeu ; le lot C affiche respectivement
**154908** et **113816**, soit **+16 octets libres**,
ce qui est cohérent avec la disparition de l'allocation heap
de la shell Game_t de 4 B et de son overhead allocateur.
Le `largest8` reste à 86004 B en jeu.
`shapeData==NULL`, `mediaTexels==NULL` sont préservés.
Ne pas extrapoler cette mesure à toutes les maps, aux tests de
défaillance SD ou à l'audio (backend silent).

**Bilan** : zéro unité C desktop liée, zéro `Game_*` lié,
zéro objet `Game_t` alloué ; champ historique du root
provisoirement NULL. **Frontière #219 lot C matériellement PASS**.
La PR reste ouverte sur cette même branche pour un éventuel
jalon suivant, séparément testé ; **ne pas merger dans main**.

---

### Deuxième frontière sur la même PR : retrait des deux Game_* inertes — PASS CYD (18 octobre 2026)

**Dernier SHA de code de ce lot** :
`e3b02d86963523c89824dc757ac3a8161d50776e`,
[CI SUCCESS #39145231343](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39145231343)
(host + trois builds `esp32-cyd`, bringup, trace + compiledb/ELF).
Ce lot suit le retrait du générateur C et a été testé **sur la même
branche #219**, sans merge.

- `esp_legacy_game_bridge.c` ne fournit plus que **`Game_init`**.
  Les exports `Game_activate` et `Game_unloadMapData` ont été retirés :
  ces anciens corps étaient des no-op sans mutation du monde ni
  libération de map. L'unique invocation résiduelle du second était
  dans `native_main_menu_start_action.c`, après `Render_freeRuntime()` ;
  elle est supprimée. Le vrai cleanup Render et les vérifications
  strictes NULL / cache inactif restent inchangés.
- Nouveau test hôte `game_inert_abi_retirement_host.py` : compile
  le vrai bridge, teste l'allocation et la réutilisation de `Game_init`,
  confirme l'absence des deux autres définitions, et qu'un appel
  réintroduit échoue au lien. Garde CI `--forbid-prefix` des
  deux symboles dans chaque ELF.
- Résultats CI du lot : RAM statique normal **49 488 B** (inchangée),
  Flash normal **821 273 B** (−12 B vs premier lot) ; bringup
  **49 640 / 841 901 B**, trace **49 536 / 837 893 B**.
  Aucun contrôle strict d'identité binaire ELF inter-commits revendiqué.

**Témoins matériels de l'utilisateur, firmware normal sur branche #219 :**

1. **START nouveau jeu** : `[MAINSTART]` preflight PAK réussi ;
   `[MAINMENU] Runtime cleanup` heap8 **154892→163332 (+8440 B)**
   avec `nodes/lines/mapSprites/mappings/shapeData/mediaTexels=NULL` ;
   `[PLAYERSTATE] READY` niveau 1, HP 30/30, arme 2,
   `[INTROSTATE] READY` images=4/texts=3 ; first frame FNV `ade0195d`.
2. **Intro réelle** : sept taps acceptés, texte révélé/continué,
   `FINAL-TEXT-PRESENTED` puis `FINAL-CONTINUE` ; 50 ms clock
   parked ; `[INTRODISP] READY` récupère **34 056 B**
   (heap8 129276→163332, largest8 98292→110580), owner/images/texts
   libérés, pas de `DoomCanvas_loadMap` desktop.
3. **Chargement frais** : `/intro.bsp` CRC `623f34e4`, arena 14 095 B,
   FNV `c3882516`, spawn 544,1824 angle 64 ;
   `[ENGINESESSION] FRESH-VISIBLE` et `[RESIDENTGAMEPLAY] READY` ;
   heap8 **113800**, largest8 **86004** stables lors de plusieurs
   `[ALIVE]`. État immuable `shapeData=0 mediaTexels=0`.
4. **EXIT sans sauvegarde** : `[SYSEXIT] MENU-READY` et
   `[RESIDENTRESET]` before=1/1/1/1/1/1/1 → after=0/0/0/0/0/0/0,
   `empty=1`, released 18008 B. Cleanup menu ne réintroduit pas
   de structures desktop, saveWrite=no.
5. **LOAD depuis le même menu**, slot09 V11 **5604 B** :
   overlays, topologie et activation restaurés exactement, session
   `RESUME-VISIBLE` ; nouveau retour au gameplay avec
   heap8 **113800**, largest8 **86004**, rotation, strafe,
   changement d'armes, attaque sur sprite242, animation et riposte
   ratée `[MONSTERRETAL] MISS-COMMIT`, `rngRollback=yes` au
   préflight et `gameplayRngCommitted=yes` au commit.

**Évaluation** : PASS CYD ciblé pour cette deuxième frontière,
START/INTRO/DISPOSAL/EXIT/LOAD/gameplay. Pas de crash, fuite progressive,
réintroduction ZIP/desktop ou divergence RNG visible dans les traces fournies.
Le log ne contient pas d'identifiant SHA compilé ; l'empreinte du firmware
n'est donc pas indépendamment prouvée. Cette validation ne vaut pas
couverture exhaustive des autres maps ou des scripts.

**Branche #219 encore ouverte** ; ne pas merger dans `main`.
Prochaine frontière éventuelle : `Game_init` et la shell `Game_t` de
**4 B** (pointeur parent uniquement), avec audit de tous les consommateurs
lifecycle avant tout retrait. Ce prochain code ne bénéficie **pas**
automatiquement du PASS matériel décrit ci-dessus.

---

**Base `main` post-merge #218 vérifiée** :
`6390a6d42e50a058a05ebc0f4483cb02894f065f`.
**Branche** : `agent/esp32-retire-generated-doomrpg-oracle`,
[PR #219](https://github.com/Kharn27/DoomRPG-RE/pull/219).
**Dernier SHA de code vérifié par CI** :
`04b696ffbb6cfd76b7c2ef00c1260a202752e979`.
[CI PR SUCCESS #39130682162](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39130682162) :
tests hôte, trois builds `esp32-cyd` / bringup / trace et audits compiledb/ELF.

### Frontière exacte retirée

- Les **21 unités C desktop originales** restent strictement exclues.
  `src/DoomRPG.c` demeure une référence comportementale utilisée par
  les tests hôte, **sans copie C générée** ni compilation dans le firmware.
- `ESP32/scripts/build_engine.py` passe de **781 à 332 lignes**.
  Il ne réécrit plus `DoomRPG.c`, purge une éventuelle copie obsolète
  dans le répertoire de génération et conserve le contrôle fail-closed
  de l'inventaire des 21 sources desktop.
- Le **générateur SDL packed reste inchangé sur ses 8 017 caractères**
  depuis le `main` de base, ainsi que les chemins de génération,
  le sampler indexé 1/4/8 bpp, les palettes et le transfert d'ownership
  surface→texture. La copie C++ SDL adaptée demeure la seule source
  moteur générée admise au build ; elle **n'est pas** une TU desktop.
- Tests primitives/math/binary, images/PAK, présentation/clip,
  temps, RNG, config et keybindings lisent désormais directement la
  référence desktop et les propriétaires natifs, sans exiger les
  marqueurs du générateur supprimé. Les CRC, parités des corps et
  tests différentiels restent présents.
- Nouveau `doomrpg_oracle_retirement_host.py` : le vrai pré-script
  SCons est exécuté avec un `DoomRPG.c` volontairement invalide,
  contrôle la génération du seul SDL, l'effacement d'une ancienne
  copie C et l'échec explicite sur dérive d'inventaire/SDL.
  La CI vérifie également qu'aucun des trois builds ne produit la
  copie `doomrpg_patched_sources/DoomRPG.c`.

### Résultats et portée

| Profil | RAM statique | Flash |
| --- | ---: | ---: |
| `esp32-cyd` normal | 49 488 B | 821 285 B |
| `esp32-cyd-bringup` | 49 640 B | 841 901 B |
| `esp32-cyd-trace` | 49 536 B | 837 889 B |

Ces six valeurs sont **identiques** aux logs de la
[CI #39127502174](https://github.com/Kharn27/DoomRPG-RE/actions/runs/39127502174)
du code #218. Les audits confirment `compiledOriginal=0`,
`compiledPatchedC=0`, `linkedInput=0` et les propriétaires natifs
attendus. Les étapes CI post-correctif sont toutes vertes.
**Ce n'est pas un comparatif strict octet par octet des sections
ALLOC de deux ELF** : ne pas déclarer le firmware bit-identique.
**PASS CYD normal rapporté le 18 octobre 2026 pour ce nettoyage**, sur
le firmware flashé par l'utilisateur depuis la branche #219. La trace ne
contient **pas de SHA firmware embarqué** ; on ne lui attribue donc pas
une empreinte de code exacte ou une identité binaire non démontrée.

Parcours et témoins Serial réellement observés : boot classic ESP32-D0WD-V3
240 MHz, sans PSRAM ; SD 240 MiB et PAK 241 entrées ; restauration primaire
`/DoomRPG-ESP32.cfg` 12 B à **75 %** (LEDC GPIO21 duty=191) ; écran
160×120 RGB565 / présentation x2 ; menu opaque interactif ; LOAD slot09
version **V11 5 604 B** sur **Entrance MAP1 /intro.bsp** (source CRC
`623f34e4`, arena FNV `c3882516`), overlays/monstres/position/activation
restaurés. Reprise gameplay et première image visibles, rotations animées,
PASS_TURN, attaque de monstre et riposte commise avec `rngRollback=yes`,
`rngExact=yes` à la frontière visuelle, attaques au corps à corps et
à distance, mort de monstres et FX, LEVELUP **1→2**, fermeture de la carte
sans tour supplémentaire, déplacement, strafe, drop puis ramassage
`Got Bullet Clip` avec munitions 17→21. Le log affiche un
`MONSTERTURN SKIP` pendant LEVELUP, conformément à la sémantique déjà
identifiée, **pas une panne AI nouvellement découverte**.

En jeu, après reprise et cache primé : trois `[ALIVE]` successifs montrent
`heap8=113800` et `largest8=86004` stables ; avant LOAD, `heap8=154892`,
`largest8=110580`. Le `ENGINESESSION READY` atteste
`shapeData=0x0 mediaTexels=0x0`; aucun ZIP runtime ou backend desktop
réactivé. Aucune exception, erreur de transaction ou dérive RNG n'est
visible dans cet extrait. Ce PASS est **ciblé** ; il ne valide pas toutes
les maps, tous les scripts, les permutations SD, l'identité ELF ni
l'audio audible (toujours différé).

Le code gameplay, RNG, rendu, SD/PAK, sauvegarde et les structures
résidentes n'a pas été modifié dans le premier lot. **Clôture du lot
oracle uniquement** : les commits de documentation suivent le SHA code
`04b696ffbb6cfd76b7c2ef00c1260a202752e979` sans firmware modifié.
**La branche #219 reste ouverte pour un nouveau lot** de désengagement
`Game_t` ; chaque nouveau commit de code ouvre une nouvelle frontière
nécessitant ses propres tests et, si code chargé modifié, un PASS CYD
spécifique. Merge par l'utilisateur uniquement.

---
