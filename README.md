# Doom RPG ESP32 CYD

[English](#english) · [Français](#francais)

[![ESP32 CYD Build](https://github.com/Kharn27/DoomRPG-RE/actions/workflows/esp32-cyd.yml/badge.svg)](https://github.com/Kharn27/DoomRPG-RE/actions/workflows/esp32-cyd.yml)

> Active work in progress, developed and validated on a real classic CYD. This
> repository does not contain the original Doom RPG game data.

---

<a id="english"></a>

## English

### About

This project is a purpose-built Doom RPG engine for the classic
**ESP32-2432S028R Cheap Yellow Display (CYD)** with 4 MB of flash and no PSRAM.

It is not a blind rebuild of the desktop engine. The reverse-engineered
DoomRPG-RE code remains an executable reference for the original game behavior
and file formats, while the permanent ESP32 runtime is compact, data-driven and
designed around explicit memory ownership.

```text
original Doom RPG data and behavior
 -> ESP32-native parsers and catalogs
 -> compact immutable map runtime
 -> small explicit mutable state owners
 -> native event and gameplay engine
 -> native renderer, HUD, dialog and touch input
 -> 160x120 RGB565 framebuffer
 -> exact nearest-neighbor 2x presentation on the 320x240 display
```

The central design rule is: **a new BSP is data, not a new engine**. Supporting
another map must not require a map-specific renderer, allocator or gameplay
implementation.

### Current status

The current hardware-validated native path includes:

- PAK-backed map loading, compact immutable map state and raw internal-flash
  backing for the requested map;
- native wall, floor/ceiling and sprite rendering with bounded caches;
- calibrated touch controls, movement, strafing, turning, selection and HUB UI;
- collision, events, dialogs, dynamic doors and mutable line state;
- player resources, pickups, hazards, weapons and direct combat;
- player attack damage text, blood feedback and gib visual preservation;
- native monster state, activation, retaliation, movement and bounded attack
  families;
- destructible subtype-2 crates, including their exact RNG-driven conversion
  into pickups;
- checkpoint save/load V6, including player state, consumed resources, script
  state, line state, action-owned removals and transformed crates;
- compatibility reads for the earlier V1 to V5 save formats.

The current V6 crate-transform save path has passed both CI and real-CYD
testing. The exact tested commit, memory figures, fingerprints and remaining
boundaries are recorded in
[`ESP32/PORTING_STATUS.md`](ESP32/PORTING_STATUS.md).

This is not yet a complete gameplay-parity release. In particular, the complete
`CHANGEMAP` level-exit route still awaits its dedicated hardware validation.
Audio, some advanced weapons and monster behaviors, the player-death path and
several HUB features also remain intentionally incomplete or fail-closed.

### Target hardware

| Component | Target |
| --- | --- |
| Board | ESP32-2432S028R classic CYD |
| MCU | ESP32-D0WD-V3, dual core at 240 MHz |
| Flash | 4 MB |
| PSRAM | None |
| Display | ILI9341, 320×240 |
| Touch | XPT2046 |
| Storage | microSD |
| Logical framebuffer | 160×120 RGB565, 38,400 bytes |
| Presentation | Nearest-neighbor 2× |

The current board pinout and touch calibration live in
[`ESP32/src/board_config.h`](ESP32/src/board_config.h). The firmware is built for
the classic CYD; ESP32-S3 CYD-like boards are different targets.

### Required game data

You must legally supply the original Doom RPG data. Game assets are not bundled
and must not be committed to this repository.

The current firmware needs two files at the root of a FAT32 microSD card:

```text
/DoomRPG.zip
/DoomRPG-ESP32.pak
```

`DoomRPG.zip` is generated from the original `doomrpg.bar` with the upstream
`BarToZip` tool. Do not merely rename the BAR file. The ZIP is still required by
transitional legacy startup, menu and HUD paths.

Generate the native PAK from that ZIP at the repository root:

```bash
python3 ESP32/tools/build_asset_pack.py \
    /path/to/DoomRPG.zip \
    /path/to/DoomRPG-ESP32.pak
```

Copy both generated files to the root of the microSD card. The firmware creates
`/DoomRPG-ESP32.sav` when a native checkpoint is saved.

### Build and flash

Install [PlatformIO Core](https://platformio.org/install/cli) or use the
PlatformIO extension for Visual Studio Code, then run:

```bash
cd ESP32
pio run -e esp32-cyd
```

To flash and open the serial monitor:

```bash
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

The repository configuration currently defaults to `/dev/ttyUSB0` at 921600
baud for upload and 115200 baud for monitoring. Change `upload_port` and
`monitor_port` in [`ESP32/platformio.ini`](ESP32/platformio.ini) or pass the
appropriate port explicitly if your CYD appears elsewhere.

For additional bring-up probes and touch overlays:

```bash
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

Use the normal `esp32-cyd` environment for canonical RAM measurements and
hardware validation: diagnostic instrumentation can change the memory layout.
A successful CI build proves compilation only; real-CYD serial logs and visual
behavior remain the runtime authority.

### Documentation

- [`ESP32/README.md`](ESP32/README.md) — ESP32 build and engine overview
- [`ESP32/PORTING_STATUS.md`](ESP32/PORTING_STATUS.md) — authoritative tested
  boundary and current limitations
- [`ESP32/NATIVE_ENGINE_RECOVERY.md`](ESP32/NATIVE_ENGINE_RECOVERY.md) — short
  development-session recovery checklist
- [`ESP32/DOCUMENTATION.md`](ESP32/DOCUMENTATION.md) — documentation and
  milestone index
- [`ESP32/ARCHITECTURE.md`](ESP32/ARCHITECTURE.md) — permanent native engine
  rules and ownership model

### Lineage, legal notice and license

This project would not exist without the DoomRPG-RE reverse-engineering work by
Erick Vásquez García / [GEC] and its contributors:

- [Erick194/DoomRPG-RE](https://github.com/Erick194/DoomRPG-RE)
- [Doomworld: Doom RPG Reverse Engineering](https://www.doomworld.com/forum/topic/129997)

Their work recovered Doom RPG behavior and formats and provides the desktop
reference used during this port. This repository does not claim authorship of
that inherited work. The new work here is the ESP32-native CYD runtime,
including its embedded storage, memory ownership, rendering, input, gameplay
and persistence architecture.

DOOM and Doom RPG are trademarks or properties of their respective owners.
This independent community project is not affiliated with or endorsed by id
Software, Bethesda or ZeniMax.

The repository is distributed under the existing [GNU GPL v3](LICENSE). See
the repository history and upstream project for detailed authorship of inherited
source material.

---

<a id="francais"></a>

## Français

### Présentation

Ce projet développe un moteur Doom RPG conçu spécifiquement pour le
**Cheap Yellow Display ESP32-2432S028R classique (CYD)**, équipé de 4 Mo de
flash et dépourvu de PSRAM.

Il ne s'agit pas d'une simple recompilation forcée du moteur de bureau. Le code
issu de la rétro-ingénierie de DoomRPG-RE reste une référence exécutable pour
reproduire le comportement et les formats du jeu original, tandis que le moteur
ESP32 définit sa propre architecture compacte, pilotée par les données et adaptée aux fortes
contraintes mémoire de la carte.

```text
données et comportement du Doom RPG original
 -> parseurs et catalogues natifs ESP32
 -> représentation compacte et immuable de la carte
 -> petits owners d'état mutable explicitement délimités
 -> moteur natif d'événements et de gameplay
 -> rendu, HUD, dialogues et commandes tactiles natifs
 -> framebuffer RGB565 en 160x120
 -> agrandissement exact x2 sur l'écran 320x240
```

La règle centrale est : **un nouveau BSP est une donnée, pas un nouveau
moteur**. La prise en charge d'une autre carte ne doit pas nécessiter un rendu,
un allocateur ou une implémentation de gameplay propre à ce niveau.

### État actuel

Le chemin natif actuellement validé sur le vrai CYD comprend notamment :

- le chargement des cartes depuis le PAK, un runtime de carte compact et
  immuable, ainsi qu'une copie de la carte demandée dans la flash interne brute ;
- le rendu natif des murs, sols/plafonds et sprites avec des caches bornés ;
- les commandes tactiles calibrées, déplacements, pas latéraux, rotations,
  sélection et interface HUB ;
- les collisions, événements, dialogues, portes animées et états de lignes
  mutables ;
- les ressources du joueur, pickups, dangers, armes et combats directs ;
- le texte des dégâts infligés, les effets de sang et la préservation visuelle
  des gibs ;
- l'état, l'activation, la riposte, les déplacements et plusieurs familles
  d'attaques des monstres ;
- les caisses destructibles de sous-type 2, y compris leur transformation en
  pickups déterminée par le RNG original ;
- les sauvegardes V6, qui conservent l'état du joueur, les ressources ramassées,
  les scripts, les lignes, les suppressions possédées par le moteur d'action et
  les transformations de caisses ;
- la lecture des anciennes sauvegardes V1 à V5.

La sauvegarde V6 des transformations de caisses a été validée par la CI et sur
le vrai CYD. Le commit exact testé, les mesures mémoire, les empreintes et les
limites restantes sont consignés dans
[`ESP32/PORTING_STATUS.md`](ESP32/PORTING_STATUS.md).

Le projet n'est pas encore une version complète avec parité totale du gameplay.
En particulier, la transition de fin de niveau `CHANGEMAP` attend encore sa
validation matérielle dédiée. L'audio, certaines armes et IA avancées, la mort
du joueur et plusieurs fonctions du HUB restent également incomplets ou
volontairement bloqués lorsqu'ils ne sont pas encore sûrs.

### Matériel cible

| Composant | Cible |
| --- | --- |
| Carte | ESP32-2432S028R CYD classique |
| Microcontrôleur | ESP32-D0WD-V3, double cœur à 240 MHz |
| Flash | 4 Mo |
| PSRAM | Aucune |
| Écran | ILI9341, 320×240 |
| Tactile | XPT2046 |
| Stockage | microSD |
| Framebuffer logique | RGB565 160×120, 38 400 octets |
| Affichage | Agrandissement x2 au plus proche voisin |

Le brochage actuel et la calibration tactile se trouvent dans
[`ESP32/src/board_config.h`](ESP32/src/board_config.h). Le firmware cible le CYD
classique ; les cartes similaires basées sur un ESP32-S3 constituent des cibles
différentes.

### Données de jeu nécessaires

Vous devez fournir légalement les données du Doom RPG original. Elles ne sont
pas incluses dans ce dépôt et ne doivent pas y être ajoutées.

Le firmware actuel nécessite deux fichiers à la racine d'une carte microSD en
FAT32 :

```text
/DoomRPG.zip
/DoomRPG-ESP32.pak
```

`DoomRPG.zip` doit être généré depuis le fichier original `doomrpg.bar` avec
l'outil `BarToZip` du projet amont. Renommer simplement le BAR en ZIP ne
fonctionne pas. Le ZIP reste temporairement nécessaire pour certains chemins
hérités de démarrage, de menu et de HUD.

Depuis la racine du dépôt, générez ensuite le PAK natif :

```bash
python3 ESP32/tools/build_asset_pack.py \
    /chemin/vers/DoomRPG.zip \
    /chemin/vers/DoomRPG-ESP32.pak
```

Copiez les deux fichiers générés à la racine de la carte microSD. Le firmware
créera `/DoomRPG-ESP32.sav` lors de la première sauvegarde native.

### Compilation et flash

Installez [PlatformIO Core](https://platformio.org/install/cli) ou utilisez
l'extension PlatformIO pour Visual Studio Code, puis exécutez :

```bash
cd ESP32
pio run -e esp32-cyd
```

Pour flasher la carte et ouvrir le moniteur série :

```bash
pio run -e esp32-cyd -t upload
pio device monitor -e esp32-cyd
```

La configuration du dépôt utilise actuellement `/dev/ttyUSB0` par défaut, avec
un débit de 921600 bauds pour le flash et 115200 bauds pour le moniteur série.
Modifiez `upload_port` et `monitor_port` dans
[`ESP32/platformio.ini`](ESP32/platformio.ini), ou indiquez explicitement le bon
port, si votre CYD est détecté ailleurs.

Pour activer les probes de mise au point et les overlays tactiles :

```bash
pio run -e esp32-cyd-bringup -t upload
pio device monitor -e esp32-cyd-bringup
```

Utilisez l'environnement normal `esp32-cyd` pour les mesures de RAM et les
validations canoniques : l'instrumentation de diagnostic peut modifier
l'organisation mémoire. Une CI verte garantit uniquement que le firmware
compile ; les logs série et le comportement observé sur le vrai CYD restent
l'autorité finale à l'exécution.

### Documentation

- [`ESP32/README.md`](ESP32/README.md) — compilation et vue d'ensemble ESP32
- [`ESP32/PORTING_STATUS.md`](ESP32/PORTING_STATUS.md) — frontière réellement
  testée et limites actuelles
- [`ESP32/NATIVE_ENGINE_RECOVERY.md`](ESP32/NATIVE_ENGINE_RECOVERY.md) — courte
  checklist pour reprendre une session de développement
- [`ESP32/DOCUMENTATION.md`](ESP32/DOCUMENTATION.md) — index de la documentation
  et des jalons
- [`ESP32/ARCHITECTURE.md`](ESP32/ARCHITECTURE.md) — règles permanentes et modèle
  de propriété du moteur natif

### Origine, mentions légales et licence

Ce projet n'existerait pas sans le travail de rétro-ingénierie de DoomRPG-RE
réalisé par Erick Vásquez García / [GEC] et ses contributeurs :

- [Erick194/DoomRPG-RE](https://github.com/Erick194/DoomRPG-RE)
- [Doomworld : Doom RPG Reverse Engineering](https://www.doomworld.com/forum/topic/129997)

Ce travail a permis de retrouver le comportement et les formats de Doom RPG et
sert de référence de bureau pendant ce portage. Ce dépôt ne revendique pas la
paternité de ce code hérité. Le nouveau travail développé ici est le moteur
natif ESP32 pour CYD : stockage embarqué, propriété mémoire, rendu, commandes,
gameplay et architecture de sauvegarde.

DOOM et Doom RPG sont des marques ou propriétés de leurs détenteurs respectifs.
Ce projet communautaire indépendant n'est ni affilié à, ni approuvé par id
Software, Bethesda ou ZeniMax.

Le dépôt est distribué sous la [GNU GPL v3](LICENSE) existante. Consultez
l'historique du dépôt et le projet amont pour le détail des auteurs du code
hérité issu de la rétro-ingénierie.
