# PR #248 — orientation matérielle native configurable (PASS classic CYD, 3 novembre 2026)

## Lignée, frontière et gel

Repo `Kharn27/DoomRPG-RE`. Vrai `main` après merge #246 : `653cae0695bc0a9cabd6fa12cfa31cfde0147764`. **Une seule branche** `agent/esp32-native-display-orientation`, **une seule PR** [#248](https://github.com/Kharn27/DoomRPG-RE/pull/248), créée Draft. **HEAD code et fixtures soumis au test réel : `c2e4340acc5fd6aed071afe66dab835595beddf3`**. La présente clôture ne modifie **que la documentation**. L'utilisateur réalisera lui-même le merge ; aucune fusion automatique.

Objectif atteint pour le périmètre observé : retourner physiquement le classic CYD de 180° avec la même interface logique 160×120, sans faire connaître la rotation au renderer Doom RPG, aux événements, au gameplay ou aux sauvegardes.

- `ESP32/src/platform_video.cpp` est seul propriétaire de `TFT_eSPI::setRotation(1/3)` : NORMAL=1, INVERSE=3 ; validité des dimensions **320×240**, retour à l'ancienne rotation si dimensions erronées, reprovisionnement du gamma inversé ILI9341, RGB565 inchangé. Le framebuffer logique **unique** reste 160×120 RGB565 = **38 400 octets**, présentation nearest-neighbor ×2, aucune image de retournement supplémentaire.
- `ESP32/include/esp_native_orientation.h` expose un domaine borné (0 NORMAL / 1 INVERSE), l'état matériel est publié par le propriétaire vidéo, et le tactile n'a pas de copie indépendante. `ESP32/src/platform_input.cpp` calibre les valeurs XPT2046 comme en rotation 1 puis applique **une seule fois** `x=319-x`, `y=239-y` en INVERSE, en amont de toutes les zones tactiles normales. Les tests hôte couvrent les quatre coins et le centre.
- La configuration microSD existante `/DoomRPG-ESP32.cfg` garde la longueur **12 octets** et le magic `DRCF`. Le codec v2 réutilise l'octet 6 pour l'orientation, garde le checksum FNV-1a des huit premiers octets, refuse versions, réservés, valeurs, longueurs et checksums invalides. Les records **v1** valides sont chargés avec la luminosité d'origine et l'orientation NORMAL, sans écriture automatique au boot. Nouvelle écriture v2 seulement sur sauvegarde d'un réglage. Le propriétaire SD conserve fichier temporaire, backup, promotion et vérification de lecture. Le menu sauvegarde **avant** de changer le TFT, avec rollbacks en cas d'échec matériel/redraw.
- Dans Options, l'ancienne carte inactive INPUT devient **ORIENTATION**. Le sous-menu natif **BACK / NORMAL / INVERSE** est tactile, redessiné dans le rail droit protégé, sans BSP, PAK lu à nouveau, rendu desktop ni modification de la carte. LUMINOSITE et SOUND restent à leurs places fonctionnelles respectives.
- Hors périmètre et inchangés : BSP / `EspMapRuntime` / RNG, PAK, architecture native, combat, framebuffer, réglages de rendu du monde, SAVE V11, audio différé ; `shapeData==NULL` et `mediaTexels==NULL`.

## CI du code réellement testé

**Deux GitHub Actions SUCCESS**, chacune **78/78 étapes** sur le SHA `c2e4340acc5fd6aed071afe66dab835595beddf3` :
- [push run #41545911114](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41545911114)
- [PR run #41545920057](https://github.com/Kharn27/DoomRPG-RE/actions/runs/41545920057)

Les tests hôte incluent codec v1/v2, corruption FNV/version/octet réservé, taille exacte, valeurs non supportées, coins/centre tactiles, SD simulée (incomplète, échec write/promote, backup, rollback, reboot), et contrats des menus. Les **trois** firmwares `esp32-cyd` normal / `esp32-cyd-bringup` / `esp32-cyd-trace` compilent. Aucun build local non exécuté n'est revendiqué.

## Retour hardware normal `esp32-cyd` — PASS ciblé

Le 3 novembre 2026, l'utilisateur a fourni trois extraits Serial du vrai **ESP32-2432S028R classic CYD sans PSRAM** : parcours START/EXIT dans la nouvelle orientation, reboot, puis chargement V11/combats/EXIT. Son verdict : **« Pour l'instant ça marche bien… Les commandes répondent toujours et au bon endroit »**, **« C'est bien conservé après le boot »**, puis **« Pour moi c'est ok »**.

Au **reboot**, témoin direct et fort de la persistance :
```text
[TFT] Ready, physical size=320x240 diagnostics=disabled
[VIDEO] Logical framebuffer 160x120 RGB565, 38400 bytes
[ORIENTATION] SET NORMAL->INVERSE rotation=3 physical=320x240 framebuffer=untouched gamma=preserved
[BACKLIGHT] SET percent=100 duty=255/255 gpio=21 frame=untouched gameplayRng=untouched
[NATIVECONFIG] LOAD source=primary result=valid version=2 bytes=12 brightness=100 orientation=INVERSE primary=valid write=no
```
L'utilisateur confirme visuellement le retournement et la correspondance des touches. Les logs du parcours montrent que les commandes `FORWARD`, `TURN_LEFT/RIGHT`, `STRAFE_LEFT`, `SELECT`, `MENU`, les dialogues 8/26 avec avance/fermeture et reprise opcode19, les portes, le HUB et `EXIT` passent par les mêmes zones logiques. Ces résultats valident la correction tactile **en pratique** sur les scénarios parcourus ; ils ne constituent pas un relevé systématique pixel par pixel des quatre coins.

**START depuis l'intro** : `/intro.bsp` Entrance, CRC `623f34e4`, arène compacte `14095 B` / `c3882516`, première frame `71ca7465`. Caisses (dont **piège déclenché** : `CRATETRAP` 3 frames, blast 15, joueur HP 30→12 et armure 12→0), ressources avec flash blanc pulsé, dialogues events 88/89/79 et aide Fire Ext, porte275 (fermeture différée après tour), HUD et HUB/EXIT. Log `[RESIDENTRESET] ... released=18008 ... empty=1`, `[SYSEXIT] MENU-READY frame=0f2c9fc3`, sans sauvegarde de partie intempestive.

**LOAD V11 existant slot04** : fichier 5604 B ; reprise exacte des FNV monstres/topologie/positions/activation **`beaeaccc/50e16fe3/e2a9d90a/df419482`**, porte113, Malwrath sprite322 (HP 11→7→3→0) et trois ripostes triples projectile204 dont les animations se terminent avant les commits de dégâts ; drop puis pickup et secret event15 **+5XP** ; changement d'arme par HUB, interaction/retour, `[RESIDENTRESET] ... released=18008 ... empty=1` et second menu `frame=0f2c9fc3`. Pas de nouvelle écriture SAVE V11 dans ces extraits.

**RAM réelle** : boot `Render=1228 Game=0 Canvas=44 Total=2032 B`, `[CORE] used=2196 B`, `PSRAM=0`, PAK=241, framebuffer 38400 B. Menu au reboot `heap8=153264`, gameplay START après cache `heap8=112784 largest8=86004`, LOAD après cache `heap8=112172 largest8=86004`. Deux `[RAMBUDGET] ... advisory=HEADROOM_OK`; `shapeData=0x0`/`mediaTexels=0x0` aux frontières relevées. Retour menu propre sans fuite manifeste dans ces séquences. Pas de preuve de benchmark A/B de performance.

Les diagnostics `NATIVEFRAME BOUNDARY-INLINE` et le décalage x des traces du projectile204 `pos=1073743664` sont **antérieurs à #248** et restent hors périmètre, sans régression d'orientation établie. Aucun crash, WDT ou panic dans les extraits. Le son reste backend silent, non testé.

## Limites de preuve et clôture

**PASS hardware ciblé** sur rotation INVERSE + tactile en jeu + persistance reboot + compatibilité V11 + invariants natifs. Le retour de l'utilisateur confirme l'UX. **Pas d'affirmation** de test matériel séparé du retour INVERSE→NORMAL, de l'import d'un véritable ancien fichier v1, de corruption SD volontaire ni d'endurance ; ces cas disposent de tests hôte, pas de témoin CYD dédié dans ce retour. Ce PASS n'implique ni pixel parity exhaustive ni validation de toutes les cartes.

**Code gelé : `c2e4340acc5fd6aed071afe66dab835595beddf3`**. Après réception du PASS matériel, seules les trois sources de documentation `ESP32/PORTING_STATUS.md`, `ESP32/DOCUMENTATION.md` et le présent rapport doivent être modifiées. Contrôler que le diff de clôture est docs-only et que la CI du nouveau HEAD demeure verte avant de convertir **la même PR #248** de Draft en Ready for review. **Ne jamais merger dans main**.
