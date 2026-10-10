# Doom RPG ESP32 — guide de travail

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

La politique de logs compile-time est définie dans `ESP32/include/doomrpg_log.h` : `DRPG_LOGE/I/D/T`. Normal = INFO, bringup = TRACE. Les messages de succès récurrents sur VIDEO, PAKIO, redraw et tour doivent rester hors du flux INFO ; ne pas masquer les erreurs, transitions, guards et recoveries.

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

## Validation d'une nouvelle frontière

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

Ne pas introduire de runtime propre à une map, de gros index pointer-heavy, ni de nouveaux wrappers universels simplement pour remplacer ceux du desktop. Le PAK reste le backing store, `EspMapRuntime` reste immuable, chaque mutation a un propriétaire explicite. Différer l'optimisation de `PlatformVideo_present()` : Doom RPG est turn-based.

La documentation ne doit plus être un second journal de développement : décrire **le fonctionnement actuel**, son protocole de test et ses limites. L'historique exhaustif vit dans Git.
