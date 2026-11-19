# ARCH-RC19 — getLine/view directs, test matériel CYD borné (19 novembre 2026)

## Portée et provenance

- [PR #398](https://github.com/Kharn27/DoomRPG-RE/pull/398), branche `agent/esp32-arch-rc19-dynamic-line-reads`, base initiale `fedc80b3e66814f5d21c09008da781fbf6341f65`.
- HEAD firmware/code livré au testeur avant correctif du harnais host : `39ef6bc4b592a4ac5d14f504e120d868d377c269` (attribution par conversation, **le Serial ne contient ni SHA Git ni environnement PlatformIO**).
- Le retrait groupé de `--wrap=EspMapRuntime_getLine` et `--wrap=EspMapLineState_view` réduit le budget GNU **10→8** production et **13→11** bringup. `EspMapRuntime_getLineBase` lit la donnée BSP immuable ; `EspMapLineState_viewBase` rend l'overlay canonique ; les deux APIs publiques préservent l'adaptation dynamique seulement dans le scope rendu. **`--wrap=EspNativeGameplayFrame_renderTurn` est conservé** pour la transaction porte/frame.
- Host RC19 : compilation de six vrais corps C et douze mutants, avec oracles historiques inchangés. Le premier CI de cette PR a échoué sur un **harnais indépendant devenu périmé**, `paid_interaction_host.c`, qui définissait les noms publics et appelait toujours `__wrap_EspMapRuntime_getLine`/`__wrap_EspMapLineState_view` ; correctifs hors firmware dans le harnais + workflow (`3bafd105f8817c17bcb7e4e5ad6781b7d230e494`, `5347e93a2cbb96623772a47ae41673e2ee5d124b`). Ne pas présenter cet ancien run en échec comme un PASS. La validation finale dépend d'un nouveau double CI vert sur HEAD intégré.

## Capture hardware fournie par le testeur

`platformio-device-monitor-261119-230528.log` : **1 994 lignes, 265 803 octets**, SHA256 **`6bd26164941420518d1a3d63fa84dc912d07f5c457a8eaac7aafffad30d1aa82`**. Vrai `ESP32-D0WD-V3`, **4 Mo flash / 0 PSRAM**, framebuffer logique 160×120/38 400 B.

Scénarios directement observés :

1. BOOT → menu → **NEW GAME → INTRO → Entrance** ; `MAPRT READY` arena=14 095 B, carte = 223 nodes / 480 lines / 344 sprites / 93 events ; session native, caisses, pickups, dialogues SELECT 8 et MOVE 26, déplacement/rotation/strafe, HUB System EXIT.
2. **LOAD V11 slot04, 5 604 B**, restauration exacte du world overlay, monstres, topologie, positions, activation ; nouvelle session et gameplay incluant combats, déplacements, portes, HUB System EXIT.
3. **44 `VIEWANIM END`**, 4 `DOORANIM ARM` / **4 `COMPLETE`** (lignes 275 et 113, ouvertes et fermées), **2 HOLD→RELEASE→POST-MONSTER-COMPLETE**, 2 `DOORSFX OPEN-START` 5063 / 2 `CLOSE-START` 5064, **3 DIALOG CLOSE**, **23 MONSTERACTIVESEQ COMPLETE**, 3 MONSTERRETAL COMMIT.
4. Deux fois `HEADROOM_OK`, marges `heap8` annoncées **24 464 B** puis **23 204 B** et `largest8` demandé 16 384 B. **2 `RESIDENTRESET empty=1 released=18008`**, **2 `SYSEXIT MENU-READY resident=empty`**.
5. **5 audits AudioIntent** : `queueOverflow=0 consumerCursorGap=0 audibleSfxDropped=0 pending=0`. Les cinq `ENTRANCEMUSIC FINAL` ont `underruns=0 recoveries=0 maxSilentFrames=0` ; **une mesure `gapsOver90us=1`** sur la deuxième fin M_STALKS, donc ne pas déclarer « aucun gap >90µs ». Aucune ligne `SFXGEN DROP`.
6. `shapeData=0x0 mediaTexels=0x0` aux frontières résidentes, aucun `VIEWANIM FALLBACK`, `DOORANIM FAILED`, `Guru Meditation`, panic, Backtrace ou WDT observé.

## Verdict et limites

**PASS matériel RC19 borné aux scénarios ci-dessus**, compatible avec le contrat de lecture dynamique et la persistance SD/flash. Cela ne prouve pas isolément le diff binaire, une campagne exhaustive, l'injection de faute dans les getters, les cas `WAIT_STATS`/CHANGEMAP, toutes les variantes EV_UNLOCK, ni le fonctionnement du panneau inversé. La preuve stricte des branches scope-only, raw immuable, fail-closed et mutants relève des tests host/CI. **Aucun nouveau firmware flashé après les corrections host-only.**

**Conditions de fusion** : harnais corrigé, nouveau double CI SUCCESS, main GitHub réévalué avant merge ; ensuite créer RC20 depuis le SHA exact du nouveau main.
