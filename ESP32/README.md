# Doom RPG — port ESP32-native sur classic CYD

> Guide d'usage **mis à jour au 19 octobre 2026**, sur le code
> `0482f13c2f62b9fc79e650a0b80bfc9695d55bd4` de
> [PR #221](https://github.com/Kharn27/DoomRPG-RE/pull/221) (PASS CYD ciblé).
> Pour les futurs changements, le dépôt GitHub et
> [PORTING_STATUS.md](PORTING_STATUS.md) font foi, pas les anciens jalons.

## Démarrer

Carte cible : **ESP32-2432S028R classic CYD**, ESP32-D0WD-V3 240 MHz,
4 Mo Flash, **sans PSRAM**, ILI9341 320×240, tactile XPT2046, microSD.
Le moteur dessine en **160×120 RGB565 (38 400 octets)** puis présente
en nearest-neighbor ×2. Pas de framebuffer 320×240 supplémentaire.

La microSD doit fournir **`/DoomRPG-ESP32.pak`** à sa racine.
Le fichier `/DoomRPG.zip` n'est **pas lu en runtime** : il est uniquement
une source de référence, notamment pour générer le PAK hors carte.

Depuis la **racine du dépôt**, pour créer le PAK à partir des données de jeu
possédées par l'utilisateur :

```sh
python3 ESP32/tools/build_asset_pack.py /path/to/DoomRPG.zip /path/to/DoomRPG-ESP32.pak
```

Copier le `DoomRPG-ESP32.pak` obtenu à la racine de la microSD.
Le dépôt ne fournit pas les assets originaux du jeu.

Depuis le dossier `ESP32/`, compiler, flasher et afficher le Serial :

```sh
pio run -e esp32-cyd -t upload -t monitor
```

Le port de téléversement configuré pour cette installation est
`/dev/ttyUSB0` ; il peut varier sur un autre PC. Le firmware de
référence est **`esp32-cyd` normal** : `esp32-cyd-bringup`
ajoute des diagnostics qui changent les mesures RAM, et
`esp32-cyd-trace` altère le timing des logs.
Ne pas confondre succès de CI et PASS sur la vraie carte.

## Utiliser le jeu

Le menu de démarrage possède quatre cartes :
**Start Game**, **Load Game**, **Options**, **Help/About**.
Sur les cartes du menu principal, un tap sélectionne et arme la carte
rouge ; un **deuxième tap après relâchement** confirme.

- **Start Game** : nouvelle partie niveau 1, intro animée avec
  `More` / `Continue`, puis transition de chargement et arrivée
  dans Entrance (`/intro.bsp`). Pendant l'intro, toucher l'écran
  révèle le texte en cours ou passe à la suite ; la dernière
  confirmation libère les assets de l'intro avant de charger la map.
- **Load Game** : liste native de **10 slots sur 2 pages**, armement
  puis confirmation de la sauvegarde choisie. Reprend directement
  carte et état natif sans rejouer l'intro. Les saves **V11**
  incluent notamment ressources, scripts, lignes, caisses,
  automap, monstres, positions, activation et drops. Les versions
  antérieures V1–V10 restent lisibles via compatibilité documentée.
- **Options → Video → Luminosité** : préréglages 25 / 50 / 75 / 100 %
  sauvegardés dans `/DoomRPG-ESP32.cfg` sur la SD
  (fichier indépendant du PAK et des `.sav`).
  `Input` et `Sound` demeurent différés.
- **Help/About** : aide native depuis le PAK, **83 lignes**,
  huit visibles par page, boutons fixes **BACK**, **UP**, **DOWN** ;
  `BACK` retourne au dashboard sans reconstruire de menu BSP.

En jeu, les **12 zones tactiles invisibles** de navigation/
actions (plus une zone dédiée aux armes dans le HUD inférieur)
gèrent mouvements, strafe, rotations, sélection/attaque et menu.
Le bandeau supérieur permet de passer un tour ; les rotations
n'avancent pas le tour des monstres. Le HUD et l'affichage monde
utilisent le même framebuffer natif.

**Accès aux armes :** taper **l'icône munitions/boussole en bas à droite**
du HUD (x100..159 et y100..119 logiques, soit x200..319 et
y200..239 sur l'écran physique). Cette zone ouvre directement la
**grille 4×3 des 12 armes** : l'arme équipée est surlignée, les
armes possédées sont en couleur, les autres sont grisées.
**Un seul tap sur une arme possédée l'équipe et referme immédiatement
la grille**, sans tour de monstre ; **BACK** ferme sans changer l'arme.
Les icônes des familiers 9–11 manquent aux données sources et restent
en présentation fail-closed. Le feedback tactile lumineux sur la
zone HUD est facultatif : l'absence d'overlay ne bloque pas l'action.

Le menu du haut ouvre le **HUB à trois onglets**, sans ancien onglet
`WPN` :

| Onglet | Usage |
|---|---|
| `INV` | Notebook, objets transportés, clés possédées ; plusieurs usages d'objets restent différés |
| `STAT` | Santé, armure, niveau, XP, attributs, crédits et clés |
| `SYS` | `SAVE`, `LOAD`, `EXIT TO MENU`, avec confirmation avant action |

`EXIT TO MENU` libère la session et retourne au menu sans sauvegarder.
Une sauvegarde en cours ne doit pas être implicitement modifiée par EXIT.
Le combat et les événements sont natifs, avec plusieurs familles encore
**fail-closed** (comportement non encore pris en charge). L'audio
dispose d'intentions natives mais **pas de lecture audible**
actuellement. Ne pas considérer le port comme une parité complète.

## Vérifier le firmware et la mémoire

Témoins Serial utiles sur le firmware normal :

```text
Engine structs: Render=1532 Game=0 Canvas=44 Total=2336 bytes
[CORE] Game           used=0
[CORE] Desktop Game.c retired bridge=absent exports=0
[DOOMCANVASBRIDGE] INIT exports=4
[DATA] Native PAK indexed, entries=241
[MAINOPAQUE] DASHBOARD ... finalFNV=0f2c9fc3
[ENGINESESSION] READY ... shapeData=0x0 mediaTexels=0x0
[ALIVE] ... heap8=113816 largest8=86004
```

Les deux dernières valeurs RAM sont des **témoins matériels
observés** sur Entrance / jeu resident après chargement, pas
des seuils valables pour toutes les maps. Le log complet du
dernier lot ne contient pas sa ligne de boot `exports=5` ;
elle est contrôlée au niveau du source/CI et doit être vérifiée
lors d'un prochain boot pour un témoin matériel direct.
`Game=0` est validé sur la carte dans le lot antérieur.
Dans tous les cas, `shapeData` et `mediaTexels` doivent
rester `NULL`; **pas de décompression ZIP ni de tableau
texels map-wide en runtime**.

En cas d'incident, conserver le contexte exact Serial
(`[ALIVE]`, `[NATIVEFRAME]`, `[MONSTER...]`,
`[NATIVESAVE]`) et distinguer **faits observés**,
tests host/CI et couverture matérielle.
Voir [DOCUMENTATION.md](DOCUMENTATION.md) pour les procédures
de validation détaillées et [ARCHITECTURE.md](ARCHITECTURE.md)
pour les propriétaires natifs et règles d'évolution.

## Développer / reprendre la branche

1. Relire le SHA réel de `main`, le
   [statut](PORTING_STATUS.md), puis
   [l'architecture](ARCHITECTURE.md) et
   [le recovery](NATIVE_ENGINE_RECOVERY.md).
2. Prendre le comportement desktop comme **spécification exécutable**,
   pas comme propriétaire de structures ESP32.
3. Un seul milestone cohérent sur `agent/*`, avec API native
   bornée, tests hôte et garde fail-closed.
4. Commit + push ; faire tester le binaire normal sur le
   vrai CYD ; documenter **seulement** les PASS Serial observés.
5. Ne jamais merger `main` automatiquement.

Les anciens jalons d'intro et leurs anciennes empreintes mémoire
sont conservés sous [archive/](archive/) et ne sont **pas**
la documentation d'usage actuelle. Les valeurs exactes
de l'ancienne PR #219 A–D sont dans
[le journal archivé](archive/PR219_DESKTOP_RETIREMENT_MILESTONES.md).
