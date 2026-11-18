# ARCH-RC11 — retrait AMBIENT/COMPOSE des GNU `--wrap` : CI PASS + campagne CYD étendue PASS (frontière bornée)

**18 novembre 2026 — PR [#379](https://github.com/Kharn27/DoomRPG-RE/pull/379)**. Branche `agent/esp32-arch-rc11-ambient-compose-scope` basée sur le vrai `main` vérifié après merge RC10 : `ab79f2ece50ab297f49718a459e4744ee1c59768`. **HEAD de code testé par contexte : `faf79e68b13dda0742b79aa7037dde8ddea01881`.** Le journal Serial **n'intègre aucun SHA firmware ni identifiant exact PlatformIO**, donc cette association ne prouve pas de manière cryptographique quel ELF a été flashé.

## Frontière du code

- Supprimé des flags de linker `--wrap=EspNativeGameplayFrame_renderAmbient` et `--wrap=EspNativeGameplayFrame_composeTurn`. Inventaires firmware passant de **32→30** (normal, inverted, trace), et **35→33** (bringup), **sans changer** les interpositions `renderTurn`, `EspMapRuntime_getLine`, `EspMapLineState_view`.
- `esp_native_resident_gameplay.c` redirige l'appel unique de fond ambiant vers `EspNativeDynamicLineRender_renderAmbient` ; `esp_native_gameplay_event_chain.c` redirige l'unique capture WAIT script `composeTurn` vers `EspNativeDynamicLineRender_composeTurn`. Dans les deux cas `beginDynamicPreview → leaf canonique inchangée → endDynamicPreview`, sans mutation gameplay ni publication illégale. État/texture de porte ouvert et protection non-réentrante restent la responsabilité du propriétaire de lignes dynamiques.
- Le renderer cardinal, `shapeData` et `mediaTexels`, PAK, framebuffer 160×120, allocations, RNG, animation de portes, MONSTER et audio demeurent hors modification fonctionnelle.

**Double CI SUCCESS sur le code ci-dessus** : [push #44335784126](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44335784126) et [pull_request #44335793288](https://github.com/Kharn27/DoomRPG-RE/actions/runs/44335793288), SHA identique. **178** véritables unités C/C++ inspectées, vrai code C de begin/end/ambient/compose compilé et exécuté, **14 mutants adversariaux** rejetés, oracles historiques F18/F24/F27/F28 sur blobs Git originaux exacts ; vrai test `paid_interaction_host.c` de WAIT + automap + présentation et test ambiant adaptés aux API explicites. **Quatre vrais ELF** : `esp32-cyd=30/30/30`, `esp-cyd-inverted=30/30/30`, `esp32-cyd-trace=30/30/30`, `esp32-cyd-bringup=33/33/33` (déclarés/liés/witness d'appel direct) ; 3 symboles toolchain logging séparés.

## Capture du vrai CYD

`platformio-device-monitor-261118-171645.log` : **3418 lignes, 449042 octets**, SHA256 **`4e53f3dc67d1e2d92842443af6c542892666e5b13337e76ba478ed01441c16cc`**. `ESP32-D0WD-V3` rev3, dual-core 240 MHz, flash 4194304 octets, **PSRAM=0**, ILI9341 320×240, framebuffer RGB565 logique 160×120 = 38400 B, inversion d'écran `off`. Une session NEW GAME Entrance depuis boot, puis HUB→EXIT→MENU_MAIN, ensuite LOAD V11 sauvegarde slot04 (5604 B, map1, positions/monster/automap topologie restaurés) puis deuxième HUB→EXIT→MENU_MAIN.

### Couverture matérielle vérifiée

| Famille | Observations du Serial |
|---|---|
| Frames de preview | **122 × `VIEWANIM END`**, **178 × `VIEWANIM FRAME`** : 39 ROTATE (78 frames), 66 MOVE axial (66), 17 STRAFE (34). **Toutes** `rendered=yes presented=1 gameplayStable=yes` ; `logicalCommit=once` et aucun `VIEWANIM FALLBACK` |
| Portes | **14 × `DOORANIM ARM`**, **14 × `COMPLETE transitions=1 frames=4 state=stable transaction=committed`** sur lines 275 (4), 352 (2), 113 (4), 102 (4) ; **7 × HOLD → RELEASE → POST-MONSTER-COMPLETE** avant fermeture visible, fermeture logique appliquée au bon moment. Aucun `DOORANIM GEOMETRY-FAULT` ni `DYNAMICLINES PREVIEW-DEFER` |
| Dialogues et scripts | **10 × DIALOG OPEN / 10 × CLOSE** dont opcode 8 et 26 puis resume opcode 19 ; `DIALOGCHAIN RESUME`, aucune erreur de reprise |
| Combat / assets | **6 × MONSTERCOMBAT COMMIT**, **5 × MONSTERRETAL COMMIT**, trois `MONSTERCOMBAT DEATH-SETTLE` observés, mouvements multiples `MONSTER3GOAL`, portes, crates, pickup, armes, SFX, HUD et HUB fonctionnels dans les logs |
| Chargement/teardown | **2 × ENGINESESSION READY**, **2 × TRANSITIONLOAD RELEASE progress=100**, **2 × RAMBUDGET RESERVE ... HEADROOM_OK** ; **2 × RESIDENTRESET released=18008 empty=1** (lignes 1734, 3385), **2 × SYSEXIT MENU-READY session=off resident=empty saveWrite=no** (1761, 3416) |
| Mémoire et stabilité | `shapeData=0x0 mediaTexels=0x0` à l'entrée et aux frontières ; `heap8` gameplay ≈ **56252–60264 B** aux ALIVE (largest8=45044), aucun Guru/panic/WDT/brownout identifié, pas de backlog de session après EXIT |

**Limite de preuve directe RC11** : 0 × `[AMBIENT] FAIL` ; 0 × `[AMBIENT] FRAME`, mais le succès utilise **`DRPG_LOGD`** supprimé au niveau de logs de ce firmware, donc absence de `FRAME` n'établit ni absence ni présence d'appels AMBIENT. 0 × `[SCRIPTWAIT] ARM/PRESENT` : **la branche WAIT et donc COMPOSE ne sont pas observées exécutées sur ce CYD**. Elles sont exercées dans le **test hôte de vraies fonctions C** et le test d'intégration réel WAIT. Le PASS matériel doit rester explicitement **non-régression étendue / aucun défaut de scope observé**, pas un PASS exhaustif du chemin WAIT hardware, ni un golden framebuffer. L'évolution de ses paths justifierait un test CYD ciblé à l'avenir s'ils deviennent accessibles.

## Anomalies hors scope : signalées avant fusion, sans correctif RC11

1. **[AUDIO #382](https://github.com/Kharn27/DoomRPG-RE/issues/382)** créée AVANT fusion : la **première session M_STALKS** termine à **0 underrun / 0 recovery** (ligne 1738) ; la **deuxième session LOAD V11** affiche à la fin **852 underruns / 1 recovery / maxSilentFrames 852 / maxPublishGap 594ms / gapsOver500ms=3** (ligne 3393), soit ~38,6ms d'échantillons perdus à 22050 Hz ; aucune lecture SD >20ms. Pendant le combat LOAD, **deux `SFXGEN DROP reader-caught-up`** (lignes 2273, 2274) : id5046 `mixed=11258/11466` et id5099 `mixed=10896/19404`; un `STREAM-FALLBACK loss=no` ligne 2266. Aucun lien de causalité audio/mutation RC11 prouvé.
2. **[AUDIO FIFO #377](https://github.com/Kharn27/DoomRPG-RE/issues/377)** existante et mise à jour AVANT fusion : **55 × `AUDIOINTENT OVERFLOW drop=oldest cap16`** dans cette capture (**2 NEW GAME, 53 LOAD V11**). Un FIFO overflow n'est pas à lui seul la preuve d'un dropout SFX ; les sorties de producteurs sont traitées dans #382.
3. [Tonneaux létaux #371](https://github.com/Kharn27/DoomRPG-RE/issues/371) déjà suivis, pas attribués à RC11 et non retouchés.

**Verdict matériel : PASS de non-régression étendue** et frontières RC11 conservées sur deux sessions CYD, sous la limite explicite de capture directe AMBIENT/WAIT ; double CI de l'implémentation et des deux callsites complets. **Aucun bug RC11 de rendu/portée observé.** Après ce PASS, seules des modifications **documentaires** à cette PR sont acceptées avant fusion. Vérifier que les pipelines CI sur HEAD docs restent SUCCESS.
