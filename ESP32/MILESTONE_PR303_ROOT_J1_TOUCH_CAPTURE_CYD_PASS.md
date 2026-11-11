# Root-J1 — MENU_MAIN touch capture : PASS matériel classic CYD (11 novembre 2026)

- **PR** : https://github.com/Kharn27/DoomRPG-RE/pull/303 ; issue audit : https://github.com/Kharn27/DoomRPG-RE/issues/302
- **Candidat combiné testé par l'utilisateur** : `a4533256539da936b9a6f7800553137ab09a15d6`, basé sur `main=5d92d2236ef00a6175a8bbdc40c986e16f87ceea` (après fusion SDL-F5 #300). Attribution au flash fourni par l'utilisateur ; **le SHA du firmware n'est pas imprimé dans le Serial**.
- **CI exacte du candidat** : [PR #43043389203](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43043389203) et [push #43043377482](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43043377482) **SUCCESS**, trois firmwares PlatformIO, gardes Root-I / Root-J1 / SDL-F5, test hôte du vrai corps de dispatch 4096 cycles et 2 identités, tests existants et exclusions desktop/SDL.
- **Log physique** : `device-monitor-261111-155800.log`, **1 934 lignes / 219 967 octets** (capture de deux démarrages POWERON_RESET), vrai ESP32-D0WD-V3 dual-core, flash 4 Mo, **PSRAM=0**, `Engine structs: Render=108 Game=0 Canvas=36 Total=641 bytes`. Résolution logique 160×120 RGB565 / écran 320×240 ×2.

## Découplage réalisé

`ESP32/src/native_main_menu_touch.c` n'utilise plus `extern DoomRPG_t* doomRpg` pour l'activation tactile. Le token `touchDoomRpg`, déjà capturé dans TouchPrepare et activé dans TouchActivate, assure (1) la confirmation et le dispatch START/LOAD/OPTIONS/HELP, (2) le retour du sélecteur LOAD, (3) la transition ST_MENU à l'enregistrement du callback. Les callbacks sans token valide ou inactifs restent **fail-closed** ; aucune nouvelle allocation, aucune modification des hitboxes, du double tap, du frame ou des assets. Le singleton `DoomRPG_t` **1 octet** et le pointeur processus global restent indispensables à d'autres API et ne sont pas supprimés.

La garde Root-I a été actualisée pour exiger le token explicite ; `root_j_menu_touch_captured_owner_host.py` vérifie une normalisation inverse **octet-pour-octet** du fichier de production vers son blob Git original `7c6ddf89a4bbbd108f4e99b82b6de7fa312efeab`, les trois chemins du callback et 4096 exécutions du vrai dispatch C avec deux identités, action LOAD/non-LOAD, FAIL/recovery et NULL/inactif.

**CI après intégration SDL-F5** : normal **859481 B flash / 80768 B RAM statique**, bringup **882185 / 80912 B**, trace **877601 / 80808 B**. Mesures de compilation, pas comparaison RAM dynamique ni benchmark A/B.

## Incident SD au début du log — séparé du PASS

- **Premier démarrage** : `[sd_diskio.cpp] f_mount failed: (3) The physical drive cannot work`, `[SD] No card or mount failure`, `SD=unavailable PAK=unavailable`. Le boot termine volontairement en mode dégradé, layout et menu désactivés sans crash ; messages ALIVE jusqu'à 15 s.
- **Deuxième démarrage POWERON_RESET**, présent explicitement dans la capture : `[SD] Mounted, size=240 MiB`, `[DATA] Native PAK indexed, entries=241`, `[DATA] HUD resource preflight OK` et toutes les étapes CORE/LAYOUT/PRERENDER/RENDER/MAPPINGS/MENU READY. **Aucune auto-récupération sans reboot n'est démontrée** ; pas de modification SD embarquée sous Root-J1.

## Parcours matériel observé après récupération SD

1. **MENU_MAIN + OPTIONS** : 4 zones tactiles, double confirmation OPTIONS, VIDEO → LUMINOSITE, luminosité **100→50 % persistée** dans la config V3, BACK VIDEO→OPTIONS→MENU_MAIN, dashboard recomposé et touch réarmé. `ST_MENU` synchronisé via token capturé, FNV MENU_MAIN `e687f0c7`.
2. **HELP** : second double tap, 112 lignes, scroll 0→49 (7 pages), BACK et réarmement du menu `e687f0c7`.
3. **START/INTRO** : double tap et transition native, 4 images intro + hand `p.bmp`, 50 ms clock, reveal/continue jusqu'à FINAL-CONTINUE, puis dispose exact : **34 080 B récupérés** et état intro libéré ; début de `Entrance`, fichier `/intro.bsp` CRC32 `623f34e4`, 14 095 B d'arène compacte.
4. **Gameplay** : au total **9 NATIVEIMAGE DECODE / 9 ADOPT**, **5 dialogues OPEN / 5 CLOSE**, **4 portes COMPLETE**, **17 miniatures PICKUP START / 17 END**, 2 ripostes MONSTERRETAL COMMIT et **2 MALWRATHPROJECTILE CLEAR**. Des diagnostics `NATIVEFRAME BOUNDARY-INLINE` apparaissent **7×** ; connus, non fatals. Aucun panic, Guru, abort ni WDT dans la capture.
5. **Sortie et reprise** : HUB → EXIT, menu `e687f0c7` restauré ; LOAD ouvert, slots 10 / 2 pages, slot 04 confirmé ; **LOAD V11 `5604 B`** et `RESUME-VISIBLE` sur Entrance ; nouveau gameplay puis HUB → EXIT. Le retour « cancel sans charger » du sélecteur n'a pas été isolément démontré ; le chemin LOAD et sa réutilisation du token sont validés.
6. **Mémoire et invariants** : **2× `RAMBUDGET RESERVE ... HEADROOM_OK`**, marges heap8 **28 236 / 27 424 B**, marges DMA **69 196 / 68 384 B** ; **2× `RESIDENTRESET released=18008 empty=1`**, **2× `SYSEXIT MENU-READY frame=e687f0c7`** sans sauvegarde parasite ; `shapeData=0x0 mediaTexels=0x0` à **13 frontières**. Pas de regression observée de l'écran/menu, du framebuffer ni du chemin PAK.

## Verdict et suite

**PASS matériel ciblé Root-J1**, avec incident SD transitoire au premier boot et second boot sain. Pas de SHA firmware affiché ni de campagne OOM/fatal ; pas de preuve d'auto-retry SD. **Après ce PASS, le code de production/tests du candidat est gelé : seulement documentation avant merge.**

Après fusion, lire le **vrai SHA de main**, `PORTING_STATUS.md`, `DOCUMENTATION.md`, et réévaluer le reste non-SDL : singleton root 1 B assumé, dernier `render_startup_bridge.c` qui lit encore `extern doomRpg`, `main.cpp` qui passe explicitement le token aux services, et wrappers `DoomRPG_*` encore présents pour compatibilité. **Ne pas supprimer globalement un contrat de durée de vie sans bénéfice vérifié.** PR AUDIO-A #301 parallèle ; audit préalable à toute branche suivante.
