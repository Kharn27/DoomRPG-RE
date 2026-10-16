# Doom RPG ESP32 — guide de travail

## PR #217 — follow-up de code review : Options aligné sur MENU_MAIN (16 octobre 2026)

Après la clôture initiale `7c866696`/docs `909e7e37`,
la review Codex GitHub a identifié un P2 réel :
`paintOptionsBounded()` composait le logo 102×120 à `x=29`
(car il le centrait dans 160px) tandis que le menu principal
l'affiche à `x=0`. Le rail `x=102..159` masquait sa droite.
**Nouveau SHA de code :**
`e4d0d03844ebfbd431318b8b1e0c9f2936c5d961`.
Le painter Options utilise désormais exactement
`DOOMRPG_CANVAS_X + DOOMRPG_ESP32_MAIN_MENU_DASH_LOGO_X`,
comme MENU_MAIN, sans modifier y/w/h ou les quatre boutons.
Le test hôte vérifie explicitement l'égalité des destinations
`logoDst` dans les deux renderers et interdit le recentrage.

Il s'agit d'une **nouvelle frontière de code** : ne pas
présenter les photos précédentes comme un test matériel de
cette variante. Pour validation sur le classic CYD normal,
ouvrir MENU_MAIN → OPTIONS et vérifier que le logo occupe
la même zone gauche `x=0..101`, puis BACK. Les autres
limitations audio et gameplay listées ci-dessous subsistent.
Seul l'utilisateur merge dans `main`.

---

## PR #217 — clôture et mode d'emploi du menu DOOM / terminal (16 octobre 2026)

Base réelle `main 4434e47a36b70826c401fc17a8b24d371789098c`,
branche `agent/esp32-retire-canvas-sound-abi`,
[PR #217](https://github.com/Kharn27/DoomRPG-RE/pull/217).
**SHA code définitif de ce lot :**
`7c866696f4ce1d263b4a514544d7f83c06146d27`.
[CI du SHA : SUCCESS (51 étapes)](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38877111031) ;
le retour utilisateur du 16 octobre autorise la clôture documentaire
et la préparation au merge, **sans demander à l'agent de merger**.

### Rendu MENU_MAIN : contrat désormais retenu

En logique 160×120, le logo source PAK **108×74** est copié vers
`SDL_Rect{x=0,y=0,w=102,h=120}`. Il est donc aligné **au bord haut**,
occupe toute la hauteur et est **volontairement déformé** ; ne pas
« corriger » son ratio sans nouveau retour visuel. Aucun encadrement
ni montage rouge autour du logo. Le panneau droit, seul habillé,
commence à `x=102` et contient les boutons `START`, `LOAD`,
`OPTIONS`, `HELP` : `x=104..157`, hauteur 20, `top=13+25*i`
pour `i=0..3`. `native_main_menu_160x120_layout.h`
définit la géométrie partagée par dessin, sélections, touch gate,
hitboxes de debug, Options Back et retour `NO SAVE`. La carte
reste neutre jusqu'au premier tap, devient rouge armée, puis
le **second tap après relâchement** confirme ; la logique
d'action/touch existante ne change pas.

### Terminal et code d'accès

L'owner permanent `esp_native_gameplay_password.c` dessine
l'en-tête de question sur deux lignes, un séparateur unique,
un champ de saisie en creux (sans les deux anciens traits
rouges isolés), et un keypad natif 3×4 avec touches différenciées
DEL rouge / VALID vert. Les seules étiquettes déplacées sont
DEL et VALID (`y+5` au lieu de `y+2`) ; rectangles tactiles
inchangés. Un curseur bloc 5×9 (ligne `y=32`) clignote
toutes les 450 ms dans le chemin modal résident, à la suite
du code saisi, sans relecture des glyphes ni tour supplémentaire.
Les 5×9 pixels sont modifiés dans le framebuffer, puis
`Esp32PlatformVideo_present()` est appelé normalement ;
ne pas confondre ce rafraîchissement logique borné avec
un transfert matériel partiel.

### Preuve et test en cas de nouvelle révision

Le dernier firmware avant documentation compile dans les profils
`esp32-cyd`, `bringup` et `trace` ; les tests hôte
`ESP32/test/native_ui_doom_polish_host.py` et
`native_audio_handoff_host.py` ainsi que les gardes
compiledb/ELF passent. L'ancien hardware PASS audio
`b6711e666965dfa7efb2017d18d6cff3ab68b5fc` a prouvé
intro/Entrance/EXIT et les invariants mémoire, mais
**n'est pas un essai matériel du logo final 102×120**.
Le retour visuel utilisateur accepte le jalon pour merge,
sans nouveau Serial/capture indexé au SHA final.

Si le layout doit encore bouger, tester sur carte réelle uniquement
en configuration normale :

```sh
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```

Sur les logs, repérer `[MAINOPAQUE] TARGET` indiquant
`logo=102x120@0,0 stretched=yes unframed=yes`,
puis vérifier les quatre doubles taps et Options→Back,
LOAD `NO SAVE`, le champ code et son curseur. Les FNV
de framebuffer seront nécessairement différents des étapes
historisées. Audio : le nouvel `EspNativeAudioIntent_stopAll()`
et le PLAY 5039 utilisent encore un backend **silent**,
sans FIFO ni sortie sonore réelle ; ce chantier sera testé
ultérieurement. Ne pas réintroduire de ZIP runtime, de
`shapeData` ou `mediaTexels`, ni de source desktop
compilée. Pour tout prochain milestone après merge, relire
le vrai `main` et ses SHA de référence.

---

## PR #217 — handoff audio natif depuis l'intro, test CYD (16 octobre 2026)

Depuis le `main 4434e47a36b70826c401fc17a8b24d371789098c`,
branche `agent/esp32-retire-canvas-sound-abi`,
[PR #217](https://github.com/Kharn27/DoomRPG-RE/pull/217).
Le dernier code firmware normal flashé et observé :
`b6711e666965dfa7efb2017d18d6cff3ab68b5fc`.
[CI SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38824713309),
avec nouveau test hôte `python3 ESP32/test/native_audio_handoff_host.py`,
trois builds et gardes ELF.

L'intro ne passe plus par `Sound_playSound`; le pont Canvas émet
une commande native PLAY `5039 flags=11 priority=5`.
Deux anciens sites `Sound_stopSounds` utilisent désormais
`EspNativeAudioIntent_stopAll()` : `resourceId=0 flags=0x80`,
dans le même record borné à 8 octets. Le backend reste **silent**
et `lastIntent` conserve uniquement la dernière intention ; ne
pas prétendre que les sons successifs ou la musique jouent déjà.
Les autres stubs `Sound_*` sont hors de ce jalon.

Pour la preuve physique, flasher **uniquement le profil normal** :

```sh
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```

Chemin validé par les logs utilisateurs : Start Game → intro complète
→ dispose → Entrance → HUB SYSTEM → EXIT confirmé → menu principal.
On observe `[AUDIOINTENT] STOP_ALL seq=5` puis
`PUBLISH seq=6 resource=5039 flags=11 priority=5`, les 7 taps
d'intro, `INTRODISP READY` récupérant 34 056 octets,
`ENGINESESSION FRESH-VISIBLE` et `RESIDENTGAMEPLAY READY`.
Deux `ALIVE` en session à `heap8=112576 largest8=86004`.
Le reset EXIT vide les owners (`released=18008 empty=1`) et
retrouve le menu de FNV `522dc605`, `heap8=158256`,
`saveWrite=no`, avec `shapeData/mediaTexels=NULL`.

**Ne pas extrapoler :** aucun STOP_ALL observé **pendant EXIT**,
aucune sortie sonore réelle, aucun combat ni nouveau SAVE→LOAD sur
ce code, et pas de comparaison pixel exhaustive. Toute mutation firmware ultérieure exige un nouveau test matériel ;
les commits documentaires qui suivirent ce premier témoin audio
étaient docs-only, puis une passe UI distincte fut conduite sur
cette même PR, clôturée avec son propre code SHA ci-dessus. La cadence UI sous charge
est une dette distincte et reste documentée dans PORTING_STATUS.

---

## PR #216 — état du firmware natif et retour d'impact (16 octobre 2026)

**Firmware normal `esp32-cyd` joué sur CYD :**
`3361cdd1d4208f69c9d9eb12c41207b9016f1ac4`, depuis
`main 0ca15d756c2c6a64a5b4bc6b3e2b0ec89d83b7bd`.
[PR #216](https://github.com/Kharn27/DoomRPG-RE/pull/216) ;
[CI push PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38822809348) /
[PR PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38822814390).
La copie générée de `DoomRPG.c` sert encore d'artefact de
vérification, mais **aucune TU desktop C générée n'est liée** ;
la racine `doomRpg` et les fonctions de sortie/cleanup vivent
dans `ESP32/src/engine_stubs.c` (durée process).

### Frontière native du combat

- `MONSTERATKVIS` présente les poses de tir et revient à idle ;
  la **dernière pose idle réellement présentée** publie
  `completedProbe` dans le même passage, sans bail
  `final-idle` de 500 ms supplémentaire. Pour les familles à trois
  tirs, les phases *intermédiaires* gardent leur cadence ; un
  projectile 204 actif impose une dernière frame de CLEAR réussie.
  Une frame non présentée ou altérant le RNG est refusée/retry,
  sans autoriser prématurément le COMMIT.
- `MONSTERRETAL` reste seul responsable du commit HP/armure,
  des RNG gameplay et de la décision létale ; le `MONSTERTURN`
  maintient un seul probe en vol. Le `DAMAGE` non létal
  regroupe **message + contour rouge + première bande de direction**
  dans une frame physique, après le commit et le traitement de
  mouvement du passage courant. Le début réel de la lease de
  1200 ms du message et de 500 ms du contour est mesuré au présent,
  pas à l'intention. Décès/pièges/barils hors de ce regroupement.
- `MONSTERACTIVESEQ` exécute au maximum **un membre vivant
  entier par service**, et sauvegarde l'ordinal/préfixe ordonné
  jusqu'au prochain passage de loopTask. `isBusy()` bloque
  touches et intentions déjà en attente tant que le tour n'est
  pas clos. Aucun `delay`, aucune nouvelle task, aucun nouveau
  buffer map-wide. Le budget est une limite d'actions complètes,
  pas un plafond en microsecondes.

### Reproduction et preuve

```sh
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```

Sur `/intro.bsp` / Entrance, reprendre le checkpoint près de la
porte `line=396`, SELECT pour l'ouvrir, laisser attaquer
`sprite=157 subtype=0`, puis PASS_TURN. Les logs validés
pour ce firmware montrent le `STEP phase=final-idle phaseMs=0`,
`COMPLETE finalIdleHoldMs=0`, `MONSTERRETAL COMMIT`,
`DAMAGECUE ON redBorder=visible presented=1` **avant**
`MONSTERACTIVESEQ COMPLETE` : le composeur reçoit désormais une
opportunité de rendu entre les autres membres. Préflight du RNG
`rngRollback=yes` ; dégâts réels `HP 30→26→22` et
armure `20→17→15`. Les `[ALIVE]` disponibles donnent
`heap8=112320 largest8=86004` inchangés. L'utilisateur constate
une amélioration nette, mais ne revendique pas une cadence parfaite.

**Défaut encore ouvert :** sous charge `DAMAGECUE DONE`
`onMs=272/120 gapMs=281 delayed=yes` puis
`onMs=321/120 gapMs=237 delayed=yes` ; malgré un
`VIEWFLASH targetMs=500`, expiration à `675/681ms`.
Ne pas reporter `nominalOnMs=85 nominalGapMs=45` comme temps
physiques atteints. Le prochain jalon doit auditer la durée des
transactions de membre/renderer et établir des opportunités de
service UI réellement bornées ; ne pas accélérer arbitrairement
les tirs 3-shot ni retoucher le RNG.

**Limites de couverture :** ce playtest est un PASS ciblé
porte/riposte/ordonnancement/heap, pas un nouveau test
SAVE→LOAD/EXIT sur ce SHA, ni une preuve de parité frame-à-frame
desktop, de projectile Malwrath physique ou de `Config present=yes`.
Le test hôte de `monster_active_sequence_budget_host.c` valide
la politique du budget, tandis que les logs du vrai CYD valident
l'ordre de présentation ; ne pas intervertir leurs preuves.

**Clôture :** après le dernier SHA de code joué, seules des
modifications documentaires sont admises avant merge utilisateur ;
nouveau jalon de timing sur une branche `agent/*` issue du prochain
`main` réellement fusionné. Ne pas merger automatiquement.

---


## Config key bindings natifs : premier lot PR #216 validé sur CYD (15 octobre 2026)

Branche `agent/esp32-own-config-keybinds`, code firmware
`8436de9caa83c8b3efd1d3d39fbc340c5293c89f`,
[PR #216](https://github.com/Kharn27/DoomRPG-RE/pull/216).
Les 3 tableaux process-lifetime `keyMapping`, `keyMappingTemp`,
`keyMappingDefault` (12 × 44 B chacun) et
`DoomRPG_setDefaultBinds` résident désormais dans
`esp_legacy_config_mappings_startup.c` au lieu de la copie
`DoomRPG.c` générée. Le source desktop reste inchangé.

Le script `build_engine.py` rejette toute dérive CRC ou
textuelle du tableau de defaults et du corps de reset.
Le test hôte `python3 ESP32/test/doomrpg_config_binds_host.py`
compile les définitions native et desktop et exerce 8 192
modifications/restaurations de la map. Le contrôle
`tools/audit_desktop_elf.py --require-native-doomrpg-config-binds`
vérifie compiledb et la propriété native, sur trois
environnements. `COMMON` objet GCC Xtensa est légitime pour
les états non initialisés, la **définition finale doit être BSS**
et unique. [CI push PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38747810890)
et [PR PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38747821709).

Le vrai CYD normal a démarré : `Config file present=no`,
`CONFIGMAP READY`, menu `522dc605`, chargement V11 slot07
CRC/runtime/états spatiaux identiques, déplacements/portes/
combat/FX/pickup/retour EXIT opérationnels ;
`heap8=112328 largest8=86004` constants pendant le gameplay.
Exit sans sauvegarde, resident vidé, menu rétabli.
**Ne pas déclarer le chemin `Config present=yes` validé** :
la lecture positive n'a pas été exercée sur la dalle/SD.
Les logs attestent des présentations de frames, mais aucune
comparaison visuelle pixel à pixel. Le correctif CI post-code
n'a pas modifié le firmware. Ce lot est accepté ;
tout nouveau lot de code sur #216 réclame un nouveau PASS
matériel et une clôture documentaire propre, distincte.


## Sprite mural en angle : règles du renderer legacy rétablies (15 octobre 2026)

Code CYD validé `a088b485f3891d515537ae2961a27e0d0a40e5eb` ;
[PR #215](https://github.com/Kharn27/DoomRPG-RE/pull/215).
Après suppression du retry mural, les motifs d'un mur rouge
d'Entrance étaient absents en angle oblique et présents de face.
La comparaison avec `src/Render.c` a révélé des règles visuelles
non encore portées : les sprites avec le flag
`0x00800000` sont décalés d'une unité suivant leur direction
et ont une priorité de tri `INT32_MAX`. Les sprites 184..190
reçoivent la profondeur spéciale `0`, quand le test de flags
précédent ne s'applique pas. Le helper purement visuel
`esp_native_sprite_visual_legacy.h` rétablit ces règles pour
`esp_native_sprite_renderer.c` (projection/tri) et
`esp_native_bsp_visibility.c` (BSP leaf) ; il **ne modifie pas
la carte immutable ni les positions persistantes**.

```sh
python3 ESP32/test/wall_sprite_visual_host.py
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```

Host **25 920** comparaisons/compilation native, CI
[push PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38734975334)
et [PR PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38734982820).
Sur CYD normal, l'utilisateur constate que les motifs restent
dessinés face au mur et de biais dans la pièce auparavant
défectueuse. Log de non-régression : mouvement/rotation,
porte160 animée, monstres6/7 et combat/riposte/FX/drop/pickup
opérationnels ; RNG des probes rollback exact,
`heap8=112328 largest8=86004` stables. Pas de nouveau
problème de rendu remonté.

Conserver le correctif `BOUNDARY-INLINE` précédent, ses tests
et ses propres preuves matérielles. Ce PASS est celui de
**`a088b485f3891d515537ae2961a27e0d0a40e5eb`**, pas une validation pixel parfaite de
tous les sprites du jeu. Ne pas déplacer ces décalages vers
le runtime map immutable ou la sauvegarde. Après les logs
de validation, seuls des changements documentaires sont
autorisés avant le merge utilisateur.



## Renderer natif : frontiere murale sans retry — hardware PASS (15 octobre 2026)

Depuis `main c5384c7664f062a64b0e7ed3c928b2dcde8cd89b`, code
`a71ed3c4cb0b12e2a0ad6ab2e06292637bbb39d5` sur
`agent/esp32-wall-boundary-inline`. [PR #215](https://github.com/Kharn27/DoomRPG-RE/pull/215),
[CI push PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38728936980),
[CI PR PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38729192919).
Le renderer conserve la semantique historique du sampler des murs :
2048 octets de texels packed pour un mur 64x64, avec possibilite
d'un octet provenant de la texture successeur aux limites. La
resolution inline `BOUNDARY-INLINE` utilise mappings.bin et
wtexels.bin via la lease PAK existante, reutilise le garde sur
le meme rendu, et ne refait plus l'image. Quatre gardes BSS
de 16 B max, `SPAN_OOB` fail-closed si indice >2048,
successeur/PAK invalide ou limite depassee. Pas de tableau
`mediaTexels`, pas de changement des objets gameplay/sauvegarde/RNG.

Test de preuve hote (vraies fonctions sampler/resolver compilees) :
```sh
python3 ESP32/test/native_wall_boundary_inline_host.py
```
2 000 cas deterministes, nibble/pixel, garde reutilisee,
octet exact du successeur, refus borne et pixels sentinelles
preserves sur les erreurs. Pour reproduire sur classic CYD :
```sh
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```
LOAD slot07 -> Entrance -> STRAFE_LEFT 310->309. Le CYD a emis
`BOUNDARY-INLINE logical=15 actual=40 source=20480
successorActual=68 successorSource=32768 byte=11` ;
meme temoignage au retour STRAFE_RIGHT 308->309. Aucun
`RETRY/RECOVERED` et pas de `NATIVEFRAME FAILED` visibles.
LOAD V11 identique, portes et combats actifs, RNG guard intact,
RAM residente `heap8=112328 largest8=86004`,
`shapeData/mediaTexels=NULL`. Le test n'etablit pas un hash
de frame identique a la reference desktop ni un delta FPS
mesure. L'ancien probleme SPAN_OOB dans les autres sections
de documentation est historique, pas une anomalie ouverte
observee sur ce cas depuis le correctif. Apres le PASS,
**ne modifier que la documentation**. Merge par l'utilisateur.



## Lecteurs RWops de Config — extraction native (15 octobre 2026)

Code testé `eaa915aacf5171dbd118758ab33bb402f9f9edee`;
[CI PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38717722550).
`src/esp_doomrpg_config_read.c` est maintenant propriétaire
des deux corps identiques au desktop `File_readByte` et
`File_readInt`. Suppression des corps du `DoomRPG.c` généré
avec garde d'empreinte/égalité du générateur et contrôle de
l'ELF (exactement 2 exports dans la TU native, zéro dans le généré).
Preserver `char` signé, `SDL_SwapLE32`, et la sémantique SDL_RWops :
**ne pas prétendre avoir corrigé les lectures courtes ou NULL**.

Tests locaux reproductibles :
```sh
python3 ESP32/test/doomrpg_config_read_host.py
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```
CI : parité hôte 4 096 appels, 8 194 octets, profils
normal/trace/bringup PASS. Vrai CYD normal : boot, absence
de Config traitée correctement, LOAD V11 slot07 via le HUB,
restauration exacte des overlays et activation, gameplay,
attaques et ripostes avec RNG probes intacts, RAM stable,
`shapeData/mediaTexels=NULL`. **La trace indique
`Config file present=no` : aucun fichier Config réel n'a
été lu physiquement.** Ce PASS couvre l'intégration et
l'absence de régression sur ce chemin, pas un parse physique
de Config présent. Si un futur test utilise un Config réel,
valider sa lecture sans modifier le fichier de sauvegarde.
Le renderer signale toujours `LEGACY_GUARD/SPAN_OOB/RECOVERED`,
hors périmètre du lecteur binaire. Pas d'autre changement
de firmware après cette validation.


## RNG provider extracted — hardware PASS (15 October 2026)

Permanent owner: `esp_doomrpg_process_rng.c`. Four exact legacy
functions and process-lifetime hidden globals `_seed` and
`resetRand` moved out of generated DoomRPG.c. Generator pins CRC
and body parity. Byte/int replay guard unchanged.
Code `a7968169a0af2e2ad93322214547089102b2d292`,
[CI PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38714139429):
12,000 RNG operations per scenario across 4 initial ticks,
reservation and rollback, word/byte boundary, three builds and ELF.

CYD PASS: LOAD V11 slot07, hundreds of actions, combat,
multi-hit sprite286 `rngExact=yes`, save slot06,
Entrance full 6/6 secrets and 30/30 monsters; transition to Junction.
Real `MONSTERMOVERNG next=127->0 prepared=1` followed by
`COMMIT reservation=consumed-by-live-move`.
Heap and null legacy map invariants preserved.
Repeated compact renderer `LEGACY_GUARD/SPAN_OOB/RECOVERED`
remains unresolved. This is a scoped, non-exhaustive RNG proof.
Run `python3 ESP32/test/doomrpg_process_rng_host.py` and flash
normal `pio run -e esp32-cyd -t upload -t monitor` for regression.


## Process clock natif : validation CYD (15 octobre 2026)

Base du lot : `main e99286b9dc1c3a120f24f76ed3741ee22cc6be0c`,
branche `agent/esp32-extract-doomrpg-process-clock`, commit de code
`d757502976f3eaaa37e5bf28bc8dd63c7e9c002a`.
`esp_doomrpg_process_clock.c` possède les corps intacts de
`DoomRPG_GetTimeMS` / `DoomRPG_GetUpTimeMS` ainsi que
`basetime` ; le générateur les retire strictement de `DoomRPG.c`.
**Ne pas réinitialiser basetime** à l'ouverture d'une carte ou au LOAD ;
respecter la sentinelle zéro et les ticks SDL Uint32.

Validation reproductible :
```sh
python3 ESP32/test/doomrpg_process_clock_host.py
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```
[Test CI SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38711767983) :
10 000 pas hôte d'horloge en ordre mixte, trois profils compilés,
audit `--require-native-doomrpg-clock` (définitions et BSS).
CYD réel normal : START→intro→Entrance→LOAD slot07, déplacements,
rotations, dialogues, portes, pickup, attaque/riposte de sprite5,
HUB SYS EXIT vers MENU_MAIN `522dc605`, reset résident vide.
LOAD restaure monster/topology/position/activation FNV ; heap8
`112328` stable après LOAD puis `111184` avec contexte dialogue ;
`shapeData/mediaTexels=NULL`. Pas d'écriture SAVE.
Le renderer compact `LEGACY_GUARD/SPAN_OOB` reste un incident
indépendant et ouvert ; ne pas confondre recovery et résolution.
Cette documentation ferme **le lot horloge seulement** ; les commits
RNG ultérieurs imposent une nouvelle validation matérielle.


## Extraction DoomRPG — présentation SDL/clip native (CYD validé le 15 octobre 2026)

La famille de sept méthodes **réellement liées** de présentation
`DoomRPG_setClipFalse/True`, `DoomRPG_setColor`,
`DoomRPG_setFontColor`, `DoomRPG_fillRect`,
`DoomRPG_drawLine` et `DoomRPG_flushGraphics` a un propriétaire
permanent : [`src/esp_doomrpg_presentation.c`](src/esp_doomrpg_presentation.c).
Le script [`scripts/build_engine.py`](scripts/build_engine.py)
contrôle les corps adaptés (CRC + égalité exacte) et enlève ces sept
définitions de la **copie générée** de `DoomRPG.c`.
L'original desktop reste intact. Aucun framebuffer supplémentaire ;
le raster utilise le contexte SDL/CYD existant et le framebuffer
**160×120 RGB565**, avec son clip et les couleurs habituelles.
`drawRect` et `drawCircle` n'étaient pas parmi les exports liés
identifiés : **ne pas les compter comme migrés**.

Vérification hôte :

```sh
python3 ESP32/test/doomrpg_presentation_host.py
```

Le test compare les vraies définitions adaptées aux sources natives
et exerce **1 600 opérations déterministes** de clip, rectangles,
lignes, changement de couleur/font, flush et sentinelles dans un
simulateur SDL/RGB565. Pour le binaire, compiler les trois profils
`esp32-cyd`, `esp32-cyd-trace`, `esp32-cyd-bringup` et utiliser
l'audit `tools/audit_desktop_elf.py` avec
`--require-native-doomrpg-presentation` (intégré en CI).
Dernier code testé `1840009295909088f97d94b806320585ef832e41` ;
[CI SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38693375415).
Pour flasher le **profil RAM de référence** sur classic CYD sans PSRAM :

```sh
cd ESP32
pio run -e esp32-cyd -t upload -t monitor
```

La validation matérielle de l'utilisateur couvre les transitions
UI et MAP_INTRO, les dialogues et le monde, les déplacements/rotations,
l'attaque des monstres, le chain-blast de trois barils, les effets
visuels et SAVE V11 / LOAD. Test post-trigger utile à conserver :
après ouverture de porte `line=87`, activation
`sprite=10 activeCount=21` ; SAVE slot07 à `1632,992 / 192`
(dos au monstre) puis LOAD et un PASS_TURN **sans rotation**.
`activation FNV aca2058f` et `monster FNV 966ac091` sont
restaurés ; le monstre `sprite=10` attaque avec
`DAMAGECUE side=BACK`. `shapeData/mediaTexels` restent NULL et
`heap8=112128 largest8=86004` dans ce passage.

**Portée limitée** : une même activation après LOAD n'établit pas
que tous les monstres SHOW par trigger sont restaurés correctement.
Les protections `NATIVEFRAME LEGACY_GUARD/SPAN_OOB/RECOVERED`
restent un problème de renderer compact distinct. Tests hôte,
CI et playtest ne prouvent pas la parité absolue des pixels,
ni une accélération. La présentation ne doit pas être utilisée
pour réintroduire un framebuffer ou la dépendance ZIP runtime.


## Deuxième extraction DoomRPG — images/PAK (CYD accepté le 15 octobre 2026)

Sur la même branche que l'extraction math/binary,
`esp_doomrpg_image_resources.c` possède maintenant
`DoomRPG_createImage`, `DoomRPG_freeImage` et
`DoomRPG_fileOpenRead`, retirés de la **copie C générée**. Il
préserve strictement les corps **déjà adaptés** pour PAK et
`Esp32Bmp_LoadRW`, sans lecture ZIP runtime ni map-wide
`mediaTexels`/`shapeData`. La source `src/DoomRPG.c` est toujours
une référence, pas une unité compilée telle quelle.

Test hôte depuis la racine :

```sh
python3 ESP32/test/doomrpg_image_resources_host.py
```

Il épingle les CRC des corps d'origine/adaptés et vérifie
l'owner concret : lecture allouée PAK, nom slash, RGB565,
color key, tailles, passage surface→texture, imageMemory,
free et double-free sans texture ; les interfaces BMP/PAK/SDL
sont **mockées**, pas des lectures SD physiques.
Le générateur refuse une différence de corps.
CI commit `e648f2fc` :
[push PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38648524728)
et [PR PASS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38648529981)
sur normal/trace/bringup et vérifications compiledb/ELF.

Le boot utilisateur confirme BMP indexés 144×72 / 108×74,
logo et menu natif, READ du slot V11 07 depuis MENU_MAIN,
entrée /intro.bsp CRC `623f34e4`, viewport, HUD, PAK,
mouvement 552→520, porte ouverte et combat du sprite 106
jusqu'à corpse/drop, avec feedback FRONT et
`heap8=112328 largest8=86004`, `shapeData/mediaTexels=NULL`.
**PASS ciblé du chargement et de la session**, sans test direct de
teardown et sans preuve d'accélération sur le même scénario.
Ne pas interpréter la différence entre `Wrote 809936 bytes`
(esptool, image écrite) et `809565 B Flash` (mesure CI PlatformIO)
comme une incohérence de taille : ce sont deux métriques distinctes.

L'incident dormant/invisible après LOAD reste à reproduire séparément.
Pour toute future migration d'un provider lié, build/test CI puis
flash du profil **normal** et logs réels avant doc de clôture.
Le commit documentaire ne touche pas aux fichiers de firmware.



### Clôture matérielle du lot DoomRPG math/binary — 15 octobre 2026

Depuis `main bb5b599f`, la PR [#212](https://github.com/Kharn27/DoomRPG-RE/pull/212)
sort six helpers stateless de la copie générée `DoomRPG.c` sans déplacer
les RNG, clocks, images ou états persistants. Owner source :
`src/esp_doomrpg_primitives.c`. Test reproductible depuis la racine :

```sh
python3 ESP32/test/doomrpg_pure_primitives_host.py
```

Il compare les corps C desktop pinés et natifs (60 000 vecteurs seeded,
endian, offsets et quelques limites) ; le générateur rejette un drift CRC
ou une divergence exacte de corps. CI sur `9fec89d1` :
[38645127794 SUCCESS](https://github.com/Kharn27/DoomRPG-RE/actions/runs/38645127794),
trois firmwares + tests + gardes compiledb/ELF.

**Log CYD utilisateur sur le firmware normal demandé** : boot/menu PAK
READY ; LOAD V11 slot 07 /intro.bsp ; monde visible après session prime ;
mouvement `552->520` ; porte ouverte, sprite 106 frappé trois fois
(7→5→3→0 HP), retaliation 27→23 HP, feedback directionnel FRONT
superposé au cadre rouge. `shapeData` et `mediaTexels` restent NULL,
`heap8=112328` et `largest8=86004` à la session. Pas de SHA flash
imprimé dans la capture ; tests qualitatifs ciblés, pas tous les états du
jeu ni preuve d'égalité binaire. Les anomalies post-LOAD préexistantes
restent indépendantes et ouvertes.

**Procédure pour le lot suivant :** construire les trois profils, comparer
les références source + ABI, et tester uniquement `esp32-cyd` sur le vrai
CYD avec boot, LOAD, mouvement, combat et Serial. Tant que le provider
lié change, ne pas réutiliser une preuve d'identité ELF du jalon Render.c
pour éviter un reflash. La documentation après PASS est docs-only.



Documentation **opérationnelle** du port classic CYD. Pour l'état exact et les limites hardware : [`PORTING_STATUS.md`](PORTING_STATUS.md). Pour le design de long terme : [`ARCHITECTURE.md`](ARCHITECTURE.md).

## Matériel et stockage

- **ESP32-2432S028R classic CYD**, 4 MB flash, sans PSRAM ; écran ILI9341 320×240, touch XPT2046, carte microSD.
- Framebuffer logique 160×120 RGB565 (38 400 octets), présentation nearest-neighbor ×2.
- Fichier runtime obligatoire sur SD : `/DoomRPG-ESP32.pak`.
- `/DoomRPG.zip` n'est utilisé que comme source/référence pour la génération ; **aucune lecture ZIP en jeu**.
- L'audio est hors périmètre de priorité immédiate. Ne pas déplacer la mémoire graphique vers des buffers map-wide.

## Build, flash et logs

À partir de `ESP32/`, environnement normal (celui à tester sur le vrai CYD) :

```sh
pio run -e esp32-cyd
pio run -e esp32-cyd -t upload -t monitor
```

Selon le poste, PlatformIO peut être invoqué avec `platformio` au lieu de `pio`. La cible d'upload configurée dans le projet peut dépendre du port série (historique : `/dev/ttyUSB0`).

`esp32-cyd-bringup` sert aux diagnostics/probes supplémentaires ; **ne pas** tirer de conclusions RAM production à partir de ce profil. La vérité matérielle vient du boot et des interactions réelles, via Serial.

### Diagnostic d'un incident gameplay

`esp32-cyd-trace` hérite de **la production** : même layout, optimisations
release, stockage et propriétaires. Il active TRACE sans définir
`DOOMRPG_ESP32_BRINGUP_PROBES`, sans overlay tactile et sans probes de startup.
Les écritures Serial modifient le timing : ce n'est ni un benchmark ni une
preuve d'identité du firmware normal. Bringup reste un diagnostic plateforme,
avec d'autres layouts et un démarrage différent, pas un simple « mode debug ».

Les anciens profils `esp32-cyd-story-teardown-probe` et
`esp32-cyd-divider-probe` et leurs blocs firmware sont retirés. Leur couverture
utile est exercée par `test/diagnostic_profiles_host.py` : vrais formatter,
story-hand et `DoomCanvas_free`, I/O image/cleanup intro mockés. Pas de preuve
PAK ou d'équivalence heap ESP32 apportée par ce test hôte.

Pour l'incident zombie actuel, `DOOMRPG_ESP32_TRACE_SPRITE=106` dans
`platformio.ini` sélectionne **l'index immuable du sprite de la carte**, pas
son subtype ni son ordre d'activation. Changer uniquement cette valeur pour
un autre incident, sans créer d'environnement par monstre. `[SPRITETRACE]`
observe les données déjà lues par le renderer : position projetée, tile et
flags topology, pose, caméra, preview, admission hidden/BSP/candidate,
near/backface/clip/frame/span failure ou raster avec nombre de pixels.
`result=rasterized pixels=0` n'est pas une preuve d'affichage visible ; un rejet
ou une frame réussie ne suffit pas à diagnostiquer un rollback gameplay.
Les déplacements commis et décisions de chaque membre deviennent également
visibles via les logs TRACE existants. Aucun buffer ni owner persistant ajouté.

Depuis `ESP32/` :

```sh
pio run -e esp32-cyd-trace
pio run -e esp32-cyd-trace -t upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

Fermer tout autre lecteur Serial avant le monitor. Vérifier au boot
`[TRACEPROFILE] production-path sprite=106 ... bringup=no`, puis charger la
sauvegarde existante près du zombie **sans la réécrire**. Capturer la sortie
depuis LOAD, tous les tirs jusqu'à la disparition, puis le premier mouvement
qui le fait réapparaître. Conserver les lignes complètes, pas seulement les
SPRITETRACE. Une sauvegarde est une reproduction, pas la preuve d'une cause.
Après correction éventuelle, refaire le scénario avec `esp32-cyd` normal.
Ce jalon ajoute l'observabilité ; il ne corrige pas encore la disparition.

Le jalon depuis main `b941048` retire la copie C desktop Render générée dans
les deux profils. L'owner géométrique natif sert maintenant aussi bringup ;
normal reste bit-identique chargé dans la comparaison locale. DoomRPG.c reste
généré pour ses utilitaires/états réellement liés, et le shim SDL packed reste
fonctionnel. Cartographie ELF et priorités : `CONSOLIDATION_AUDIT.md`.

Pour refaire la cartographie sur **un build courant**, depuis ESP32 :

```sh
pio run -e esp32-cyd
pio run -e esp32-cyd -t compiledb
python3 tools/audit_desktop_elf.py .pio/build/esp32-cyd --compiledb compile_commands.json --calls
python3 tools/audit_desktop_elf.py .pio/build/esp32-cyd --compiledb compile_commands.json --require-retired-render
```

Répéter avec bringup et régénérer compiledb pour le profil choisi. Les vieux
Render.o/Render.c de .pio ne prouvent pas une compilation ; SCons/map/ELF sont
croisés. Le CSV b941048 est un snapshot de la base **avant** retrait, pas un
inventaire automatique du HEAD. Un symbole lié ou un call direct ne prouve
jamais un chemin réellement exécuté sur le CYD.

L'ancienne suite graphique `menu.bsp`/ZIP est retirée : son point d'entrée
retournait déjà 0 pour tous les builds ESP32. Le rapport bringup `[MENUBSP]
RETIRED` et `MENU BSP FAIL` reste donc attendu ; il ne teste pas le menu opaque
production. Les diagnostics actifs première frame, CHANGEMAP, opcodes et touch
ne sont pas supprimés avec cette suite historique.

La politique de logs compile-time est définie dans `ESP32/include/doomrpg_log.h` : `DRPG_LOGE/I/D/T`. Normal = INFO, bringup = TRACE. Les messages de succès récurrents sur VIDEO, PAKIO, redraw et tour doivent rester hors du flux INFO ; ne pas masquer les erreurs, transitions, guards et recoveries.

Mouvements de monstres : le normal conserve les résumés BEGIN/COMPLETE,
ORDERED-DISPATCH, les erreurs de mouvement/publication, ainsi que les témoins
attaque-probe/retaliation nécessaires au watcher. Les succès PROBE/ALLOW/COMMIT,
obstacles de pathfinding ordinaires et détails MEMBER/PAUSE/RESUME sont TRACE ;
les mêmes PROBE/prepare/MEMBER restent affichés si rollback/admission/publication
échouent. Utiliser **esp32-cyd** pour juger la latence : bringup garde les logs
exhaustifs. Ce retrait de texte ne change ni le nombre de redraws, ni le RNG,
ni la cadence d'attaque ; pas de gain FPS hardware revendiqué.

Le dernier retour CYD confirme l'absence de gain ressenti et un SAVE V11 slot 3
suivi du retour au jeu/expiration de « Game saved » ; pas de LOAD dans cet
extrait. Le goal 2/3 de sprite 202/subtype 4 reste différé sur une géométrie
special-plane non possédée (premier déplacement déjà commis), code inchangé
depuis la base main. Détails et limites de preuve : `PORTING_STATUS.md`.

Rollback de l'interaction opcode 23 depuis automap : en cas d'échec de
démarrage du script/dialogue, le resident restaure son mode et redessine
l'automap ; depuis WORLD, il redessine le monde. Si le redraw de restauration
échoue, le gameplay se désactive. Le test hôte utilise le vrai routage/mode
resident avec startup/render/assets mockés ; il ne prouve pas un échec I/O
réel sur CYD. Pour un smoke matériel, ouvrir normalement le dialogue depuis
automap ; ne pas retirer/corrompre le PAK pour fabriquer une panne.

## Feedback directionnel des dégâts et réserves post-LOAD

Sur le CYD, un hit monstre non mortel commité affiche le contour rouge
historique de 2 px logiques / 500 ms **et**, pendant cette période lorsque
le compositor publie immédiatement la première impulsion, une bande de
10 px logiques (20 px physiques) dans le côté de provenance réel :
`FRONT` en haut de la vue, `RIGHT` à droite, `BACK` en bas,
`LEFT` à gauche. Deux impulsions de 85 ms avec 45 ms de pause ;
le feedback est distinct du séquenceur IA et ne doit pas retarder
l'attaquant suivant. En INFO, comparer `[MONSTERRETAL] COMMIT`,
`[VIEWFLASH] PAINT/EXPIRE`, `[DAMAGECUE] QUEUE` et
`[DAMAGECUE] ON ... redBorder=visible presented=1`. Le
`concurrent=yes` du log signifie que les deux effets *peuvent* coexister,
pas que chaque impulsion a réellement recouvert le rouge : c'est
`redBorder=visible` qui l'atteste.

Correction de synchronisation après la capture `814a5034...` : le rectangle
était présenté dans la transaction de dégâts, mais la direction ne démarrait
qu'au prochain service, après les autres monstres. Le rectangle expirait
alors avant l'impulsion (818 ms dans le témoin à distance).
L'enqueue **après commit réussi** tente désormais immédiatement la première
impulsion via le compositor existant, sans nouveau rendu monde ni attente
des deux clignotements. La direction est servie avant l'expiration des effets ;
le rouge ne se restaure pas sous une direction active **ou encore en file**.
Les barrières touch/PAK/level-up/transaction et retry/timeout UI restent actifs,
la file reste ordonnée, et un échec du présent UI ne rejoue pas les dégâts.
Ce présent overlay borné peut ajouter le coût d'un transfert écran ; ce n'est
pas une optimisation de performances. Les durées 500/85/45 ms sont nominales :
un loopTask occupé peut allonger l'affichage, sans attendre ni bloquer le tour
logique pour terminer le feedback. Tests hôte du vrai service/compositor
avec clock/assets/présentation mockés. **Acceptation matérielle qualitative
du correctif donnée le 14 octobre 2026** : l'utilisateur n'a pas reproduit la
désynchronisation et accepte le résultat. Aucun nouveau log/SHA de boot/profil
fourni ; ce n'est pas une validation exhaustive des cas ci-dessous ni une
mesure de timings. La preuve logicielle et ce retour matériel restent distincts.

Scénario de non-régression à conserver sur **esp32-cyd normal** : hit zombie à distance avec plusieurs monstres
actifs, puis hit au corps-à-corps ; vérifier la première impulsion sur le rouge,
et l'absence de pixels persistants après un mouvement/changement d'arme.
En Serial, attendre `DAMAGECUE ON ... redBorder=visible` pour le premier hit
avant les traitements suivants, pas `VIEWFLASH EXPIRE` puis `ON ... none`.
Le log `MONSTERRETAL COMMIT` peut suivre le `DAMAGECUE ON` : le commit des
owners player/RNG/frame est déjà clos avant l'enqueue, le texte est imprimé
après. L'incident distinct du zombie invisible reste ouvert/non reproduit.

Pour un tonneau, tester séparément la cible et son explosion.
Un blast **unique** mortel (`chain=1`) peut désormais mettre
`playerHP=0` dans l'API terminale, réserver exactement un octet RNG
de mort après le blast et avant les conséquences monstres, puis armer le
vrai menu de mort une fois la transaction commise. Chercher
`BARRELRADIUS PLAYER-HIT`, `DEATH-RNG-RESERVE`,
`MONSTERRADIUS COMMIT`, `LETHAL-COMMIT`, `PLAYERDEATH ARM/READY`.
Le vrai CYD a validé ce chemin avec deux monstres voisins ; une
**chaîne létale de plusieurs tonneaux** reste explicitement non supportée.

Lorsqu'un monstre né d'un trigger `SHOW` refuse de bouger après
LOAD jusqu'à ce que le joueur se retourne, **ne pas** réactiver tout
le voisinage ou scanner à 360° comme correctif. V9/V11 enregistre déjà
activation + ordre (snapshot 268 B), topologie et positions. Garder
un slot avant le trigger et un autre après apparition/activation,
puis comparer `[MONSTERACT] ACTIVE` avant SAVE,
`[NATIVESAVE] SAVE ... activation=count/FNV`,
`[MONSTERACT] RESTORE ... activeCount/stateFNV` et le premier
`[MONSTERACTIVESEQ] BEGIN/COMPLETE`. Une apparition après rotation
prouve une activation par visibilité BSP, **pas** une perte du snapshot
à elle seule. L'enquête root-cause SAVE/LOAD/SHOW doit rester un
jalon distinct ; détails dans `PORTING_STATUS.md`.

## Playtest libre : captures automatiques des incidents

Pour jouer normalement **sans surveiller le terminal**, depuis `ESP32/` :

```sh
# Python utilisé par PlatformIO a généralement déjà pyserial :
python3 -m pip install pyserial  # seulement si non installé
python3 tools/playtest_watch.py --port /dev/ttyUSB0 --bell
```

Cet outil remplace le moniteur Serial pendant le playtest : **ne pas ouvrir `pio device monitor` simultanément**, car les deux processus se disputeraient le port série. Aucun reflash n'est nécessaire. Les règles explicites reconnaissent les frontières non implémentées ou les anomalies importantes (ex. `[HUB] SELECT-DEFER ... kind=item`, `[RESIDENTGAMEPLAY] SELECT-DEFER`, `[MONSTERMOVE] DEFER`, `[TURNFRAME] DIAG fail=WORLD_RENDER`, `[HUB] CLOSE ... exactHud=NO`). Les simples textes `sound=...-deferred`, `turnAdvance=deferred` et les messages READY ne provoquent **pas** d'alerte.

À chaque incident : une alerte `[WATCH]` et un fichier `playtest-captures/incident-....log` de ~75 lignes précédentes, le déclencheur, puis jusqu'à 20 lignes suivantes. Un cooldown de 45 s par catégorie limite les duplications ; ce n'est **pas** une garantie de capture exhaustive de chaque occurrence. `Ctrl+C` arrête seulement le moniteur PC et écrit `snapshot-....log` contenant les dernières lignes, utile si un problème visuel ne correspond à aucune règle. **Le CYD continue à tourner : aucun arrêt logiciel, watchdog, ou écriture microSD n'est impliqué.** Ne pas confondre capture du contexte et pause du gameplay.

Options : `--echo` réaffiche tous les logs, `--before 120 --after 30` augmente le contexte, `--cooldown 90` réduit les alertes. On peut valider sans matériel avec `python3 tools/playtest_watch.py --self-test`, ou lire un flux enregistré : `python3 tools/playtest_watch.py --stdin < ancien-log.txt`. Les fichiers de capture sont ignorés par Git. Pour une pause du jeu réellement ciblée sur des actions non possédées, ajouter plus tard un mode diagnostic ESP32 **opt-in** avec reprise explicite, jamais un `while(1)` qui déclenche le watchdog.

Cheats : jalon séparé, à auditer par rapport aux commandes réelles du legacy ; ne pas inventer de code secret ni modifier les progressions de sauvegarde dans ce jalon.

## Régression native : projectiles de monstres et tonneaux

Le **sprite logique 204** est une ressource de projectile créée en combat, pas un sprite BSP permanent. Sa paire de mappings est ajoutée *sparsely* au catalogue **avant** le gameplay dès que la topologie contient un Imp (subtype 3) ou un Malwrath/Cacodemon (subtype 6). Ce chemin de présentation est autorisé **uniquement pour l'attaque primaire** de ces deux subtypes : Imp = un projectile, Malwrath = trois. Chaque projectile est un effet monde transitoire, peint aux positions bornées par le renderer, sans ajouter de sprite persistant ni consommer de gameplay RNG. Les autres familles de missiles demeurent hors de ce contrat limité.

Sur le vrai CYD, contrôler `[ENGINESESSION] SPRITES ... monsterProjectile204=primed` si l'une de ces familles est présente ; puis, pour une attaque du Malwrath, `[MONSTERATKVIS] ARM ... projectile=malwrath204-3shots-armed`, les `[MALWRATHPROJECTILE] FRAME ... logical=204 ... presented=1` (trois points par vol) et deux `REARM shot=2/3` / `shot=3/3`. Le `[MONSTERATKVIS] COMPLETE` doit précéder la résolution réelle `[MONSTERRETAL] COMMIT` ou `MISS-COMMIT`; les HP/armure/RNG et le tour restent les propriétaires gameplay existants. Un `projectile=none` sur cette attaque primaire, des `DEFER` de rendu ou un sprite 204 absent ne sont **pas** un PASS. La validation matérielle ciblée du Malwrath est consignée dans `PORTING_STATUS.md`.

Pour les tonneaux, distinguer **visée** (`[ACTIONENGINE] TRACE ... route=BARREL_SUBTYPE1`) et **préflight du rayon d'explosion** (`[BARRELRADIUS] PREFLIGHT` ou `DEFER`). Une ligne liée `type=0` est inerte pour `Game_hurtEntityAt()`, quel que soit son subtype, sans rendre les autres entités destructibles ignorables. Quand plusieurs tonneaux et monstres participent à la chaîne, l'ordre natif est tirage de l'explosion, dégâts/conséquences et RNG des monstres, puis explosion suivante, dans une transaction à rollback borné ; chercher `[MONSTERRADIUS] SEQUENCE-BEGIN`, `[BARRELRADIUS] BLAST` et `[BARRELRADIUS] INTERLEAVED-COMMIT`. **Ne pas qualifier un ancien `DEFER` ou une impression visuelle seule de PASS ciblé** : reproduire le scénario exact et vérifier la consommation d'ammo, les monstres, les drops et l'absence de rollback partiel.

Enfin, si les taps remontent en `[TOUCH]` mais que `[ALIVE] SD=unavailable PAK=unavailable ... MENU=unavailable`, rechercher l'échec du montage SD avant d'incriminer le menu ou les correctifs gameplay. Un firmware qui démarre sans `/DoomRPG-ESP32.pak` ne peut pas initialiser le jeu.

## Validation d'une nouvelle frontière

### Interactions payantes avec dialogue et attente

Le script original ne propose pas de YES/NO : terminer le dialogue d'offre
avance son état, puis une nouvelle interaction contrôle/débite les crédits.
Il est toutefois possible d'interrompre **EV_DIALOG** avec sa carte **BACK**
tactile (en haut à gauche du viewport, au-dessus du texte). Ce tap annule sans
reprise du script ni débit ; ailleurs, le tap reste SELECT (texte/page/suite).
**EV_DIALOGNOBACK**, notamment après paiement, n'a pas cette carte et reste
SELECT-only : aucun remboursement ou nouvelle branche script n'est ajouté.

Le SELECT natif admet maintenant un `EV_REQSTAT` crédits (23, stat 2) suivi
du débit `EV_DECSTAT` correspondant (22), d'un dialogue sans Back (26), des
particules de capture (35, famille 3), d'un son publié comme intent silencieux
(42) et d'une attente (37). Après l'attente : SHOW/HIDE, refresh de vue (36),
note (40) et mutations d'état (11/19/20). Les opérandes et toute cette fermeture
sont préflightés avant le paiement ; SHOW utilise les gardes natives existantes.
Les autres stats, particules et formes de chaîne restent fail-closed.

Fonds insuffisants : texte BSP de l'opcode 23, aucun débit ni continuation.
Paiement : snapshot PlayerState et rollback si l'ouverture du dialogue échoue ;
le débit est commité seulement lorsque le dialogue est présenté. WAIT garde
l'entrée monde fermée, sans sleep ni action différée derrière le timer. Le
timer monotonic démarre après la première frame d'effet réussie, accepte le wrap
32 bits, et reste borné à 10 s. Le fond de l'effet passe par `Frame_composeTurn`
(pas de present/activation/automap/fresh-frame), puis l'effet est ajouté et une
seule frame physique publiée. Ne pas armer PresentGate avant renderTurn : sa
garde refuse cette composition et laisserait le WAIT armé indéfiniment.
Une note est préparée dans le candidate/scratch
NOTE existant et commise après le rendu final ; échec I/O/rendu = rollback du
segment et retry, pas de second paiement. Pas de nouveau fichier moteur ni de
nouveau propriétaire Player/Notebook/topologie/PAK.

**PASS CYD ciblé consigné** : event67/tile675, débit 19→9, effet publié,
HIDE/SHOW/NOTE, activation puis combat/mort du monstre, retour WORLD et aucune
seconde facture dans la trace. Portée précise et réserve d'identité firmware :
PORTING_STATUS. BACK visible est accepté ; son annulation sans débit n'est pas
prouvée par cet extrait. Pour compléter : depuis une sauvegarde pré-paiement,
annuler l'offre via BACK puis réouvrir ; avec <10 crédits, le dialogue de refus
ne doit changer ni crédits ni monde. Vérifier le notebook à l'écran et SAVE/LOAD.
Les logs doivent montrer `SCRIPTWAIT PRESENT` puis `SCRIPTSTAT COMPLETE` ;
`ARM` seul ne vaut pas succès. Garder les logs
`[SCRIPTSTAT]`, `[SCRIPTWAIT]`, `[NOTE]`, et vérifier `shapeData/mediaTexels=0`.
Le timer, le son silencieux et les mocks hôte ne prouvent pas une parité visuelle
ou RNG exhaustive avec le desktop. Ajouter un smoke-test dialogue ordinaire,
HUB/SAVE/LOAD et SHOW/HIDE (les types/formats de sauvegarde restent inchangés).

### Protocole général

1. Lire le vrai SHA `main`, [`PORTING_STATUS.md`](PORTING_STATUS.md) et [`ARCHITECTURE.md`](ARCHITECTURE.md) ; examiner les fonctions legacy concernées dans le repo.
2. Partir **du SHA exact** après chaque merge ; créer une branche `agent/*`. Jamais de merge automatique dans `main`.
3. Choisir **une famille cohérente bornée** ; introduire uniquement le propriétaire natif durable et les guards de parité nécessaires. Les cas non couverts restent **fail-closed**.
4. CI sur `esp32-cyd` ; vérifier symboles, mémoire, allocations, signatures, flux d'appel, invariants. Les tests locals/CI ne sont pas présentés comme tests hardware.
5. Push de la branche, flash du firmware normal par l'utilisateur, collecte des **logs Serial réels**, comparaison avec le comportement attendu. Diagnostiquer, corriger, committer et push sans redemander.
6. Après PASS : geler la frontière du code testé ; commits suivants **docs-only** avant merge-ready. Toute nouvelle modification de code nécessite sa propre validation.
7. Actualiser **seulement** les faits du statut courant dans `PORTING_STATUS.md`. Pas de nouveau fichier `MILESTONE*.md` par micro-changement. Les commits et PR forment l'historique.

### Mini check-list runtime

- Boot normal, SD et PAK prêts, menu stable.
- Identité de la carte et FNV non contradictoires avec l'état attendu.
- `shapeData == NULL`, `mediaTexels == NULL` ; pas de source ZIP runtime.
- Allocations et cache bornés, pas de changement sournois d'owner mutable.
- Tester uniquement les trajets pertinents à la frontière (START, mouvements, rotations, portes, dialogue, HUD, LOAD, monstres, EXIT si concernés). **Ne jamais présenter les autres comme testés.**
- Sur Render, distinguer explicitement appel réel du chemin retiré et simple non-régression d'une session.
- Le résultat d'un test est documenté avec **SHA de code précis** et éventuelles réserves ; ne pas recopier tous les logs.

## Où trouver les spécifications

| Besoin | Référence |
| --- | --- |
| Objectifs/invariants mémoire/architecture native | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Derniers SHA, RAM, FNV, frontières en cours, caveats | [PORTING_STATUS.md](PORTING_STATUS.md) |
| Implémentation exacte | Code source et scripts de génération dans le repo |
| Parité Doom RPG originale | Sources desktop/J2ME retenues dans le repo |
| Historique d'un jalon | Commits/PR GitHub et fichiers historiques au [snapshot du 8 octobre 2026](https://github.com/Kharn27/DoomRPG-RE/tree/60d34d174bed0d4d4e13137f306070b5018c0fa9/ESP32) |

## Présentation du menu de mort

Après la chute et le fondu existants, le menu de mort occupe le framebuffer
entier et reprend la palette industrielle et les polices partagées du HUB et
du bilan de mission : cadre sombre, bandeau `YOU DIED` rouge, sous-titre
`MISSION FAILED`, puis quatre cartes à deux lignes. Aucun asset ni buffer
supplémentaire n'est chargé ; le propriétaire de mort reste inchangé.

`LOAD GAME` ouvre désormais un sélecteur tactile de **10 slots SD** (deux pages de cinq), qui peut charger l'ancien checkpoint via le slot 1. Deux pressions sur un slot lisible confirment LOAD ; les slots vides sont désactivés. En jeu, HUB → SYSTEM → SAVE/LOAD ouvre le même inventaire de slots, avec sauvegarde V11 atomique indépendante par slot. Pagination : gros chevron supérieur **précédent** (actif page 2), inférieur **suivant** (actif page 1) dans une colonne à droite ; numéro de page dans l'en-tête. Style visuel accepté comme provisoire sur CYD, finition Doom à reprendre en fin de port. Le slot AUTO n'est pas implémenté.
`GO TO JUNCTION`, `RETRY SECTOR` et `MAIN MENU` restent visibles et atténués,
avec `NOT AVAILABLE` : leurs backends ne sont pas implémentés par cette refonte.
Le tap unique LOAD, son dispatch différé au service et les autres actions
fail-closed sont conservés. Les cartes font 140×19 pixels logiques, à
`x=10..149`, `y=33..51 / 54..72 / 75..93 / 96..114` ; peinture et hit-test
utilisent les mêmes constantes. Les marges et interlignes ne déclenchent rien.

Ne pas confondre `MAIN MENU` sur cet écran de mort, encore différé, avec
`SYS → EXIT TO MENU` dans le HUB en jeu : ce dernier possède son propre retour
natif fonctionnel. La refonte graphique ne débranche ni ne modifie cette route.

Validation locale : commande du test dans [`test/README`](test/README), build
`esp32-cyd` et inspection du vrai painter avec/sans sauvegarde. Ces preuves
ne remplacent pas le matériel. Sur CYD : mourir avec un checkpoint, vérifier
la lisibilité puis charger et reprendre le gameplay ; répéter sans checkpoint,
vérifier `NO SAVE` et que les options différées ne quittent pas l'écran.

## Contrat d'évolution

L'audit architectural depuis `main 9d203e5` est dans
[`CONSOLIDATION_AUDIT.md`](CONSOLIDATION_AUDIT.md). Son inventaire distingue
sources présentes, unités compilées, sections liées et preuves d'exécution.
Premier lot : retrait de neuf anciens markers post-load Junction inaccessibles,
sans fusion d'états actifs. La comparaison des sections allouées des deux ELF
est stricte (octets/adresses/layout), avec `tools/compare_elf_alloc.py` ; les
tests hôte et la CI n'attribuent aucun nouveau PASS gameplay/matériel.

Deuxième lot sur la même branche : retrait de l'ancien renderer/probes de
menu ZIP (16 sources, 13 headers), quatre directives de wrap inutilisées et
allègement du diagnostic fermé. Après les deux lots : **158 sources C/C++,
152 headers include**. Sections allouées et RAM/flash inchangées dans les deux
profils. Trois fixtures SYS/progress/death-menu sont réalignées et intégrées
à la CI ; elles ne valident pas la restauration SD réelle. Les bridges de menu
et caches encore liés/partagés restent explicitement différés dans l'audit.

Ne pas introduire de runtime propre à une map, de gros index pointer-heavy, ni de nouveaux wrappers universels simplement pour remplacer ceux du desktop. Le PAK reste le backing store, `EspMapRuntime` reste immuable, chaque mutation a un propriétaire explicite. Différer l'optimisation de `PlatformVideo_present()` : Doom RPG est turn-based.

La documentation ne doit plus être un second journal de développement : décrire **le fonctionnement actuel**, son protocole de test et ses limites. L'historique exhaustif vit dans Git.
