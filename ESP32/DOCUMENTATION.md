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

## Contrat d'évolution

Ne pas introduire de runtime propre à une map, de gros index pointer-heavy, ni de nouveaux wrappers universels simplement pour remplacer ceux du desktop. Le PAK reste le backing store, `EspMapRuntime` reste immuable, chaque mutation a un propriétaire explicite. Différer l'optimisation de `PlatformVideo_present()` : Doom RPG est turn-based.

La documentation ne doit plus être un second journal de développement : décrire **le fonctionnement actuel**, son protocole de test et ses limites. L'historique exhaustif vit dans Git.
