# Loading bar — deferred flash staging for fresh Start / checkpoint LOAD

## Statut : PASS matériel ciblé, PR #353 (16 novembre 2026)

- Branche : `agent/esp32-loading-deferred-flash-progress`
- Base initiale : `c3be0e7b9bb1a5b521a3962bc0210065add8214f` ; base effective de PR au dernier contrôle : `4844d4da3c65e7b4fd9ea4117a76ac676d23e764`.
- **HEAD firmware matériellement testé (freeze code)** : `6fb453979e9b73134235c8a5658d59db941fcfe4`.
- Log reçu : `platformio-device-monitor-261116-013749.log`, classic CYD normal `esp32-cyd`, ESP32-D0WD-V3 / 4 MB / sans PSRAM.
- CI code : [push 43780699953](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43780699953) 177/177 SUCCESS ; [PR 43780704682](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43780704682) 178/178 SUCCESS. Les firmwares normal et inverted sont compilés ; inverted **non testé sur cette capture matérielle**.

## Cause et solution

CHANGEMAP présentait déjà les compteurs réels ERASE/COPY/VERIFY. Sur START et LOAD, le chargement natif restaurait d'abord le BSP, runtime et sauvegarde (checkpoints fixes jusqu'à 85%), puis `__wrap_EspAssetPack_residentBegin()` lançait `EspAssetPack_mapFlashPrepare()` : cache miss impliquant erase+SD→flash+verify à une barre artificiellement proche de la fin.

Correction **uniquement de présentation et de branchement du callback** (aucun transfert déplacé) : START et LOAD marquent la barre `flashDeferred`; leurs checkpoints initiaux tiennent dans 0–20%, les callbacks flash réels occupent 20–85%, et les caches/handoff 90–100%. Une réutilisation valide ne simule aucune copie et saute directement à `FLASH-READY=85`. Le callback est annulé après prepare sur succès ou échec. CHANGEMAP et death-route restent sur leurs plages initiales. Format PAK/sauvegarde, gameplay, world, rendu, propriétaire de cache et allocations inchangés.

## Preuves matériel CYD

1. **LOAD depuis menu, flash déjà sur Entrance (slot06 V11)** : `REUSE HIT requestedMap=1 cachedMap=1 rebuild=no`, `FLASH-READY overall=85` puis `CACHE-COLD=90`, `CACHE-LEARN=96`, `READY=100`, `RELEASE progress=100 elapsedMs=4207`, `ENGINESESSION READY map=1`.
2. **CHANGEMAP Entrance→Junction** : `REUSE MISS requestedMap=9 cachedMap=1 reason=world-identity`, ERASE 0/5/10/15/20, COPY 20→80, VERIFY 80→100, `MAPFLASH READY map=9 staged=2247971 buildUs=18942433`, `TRANSITIONLOAD END progress=100 elapsedMs=19143`, `ENGINESESSION READY map=9`.
3. **LOAD en jeu Junction→Entrance (slot04 V11)** : `REUSE MISS requestedMap=1 cachedMap=9 reason=world-identity`, BSP `overall=7`, RUNTIME `14`, RESTORE `20`; ERASE `20/25/30`, COPY `33/38/43/48/53/58/63/68`, VERIFY `72/77/82`, `FLASH-READY=85`, CACHE `90/96`, READY `100`, `RELEASE elapsedMs=20453`, `ENGINESESSION READY map=1`. PAK index/payload vérifiés FNV ; restauration exacte V11 des états (topologie, monstres, drops, lignes, ressources). Le taux d'update affiché est volontairement borné : toutes les étapes flash physiques ne déclenchent pas nécessairement un frame à chaque pas.

Sécurité/ressources : trois `RAMBUDGET RESERVE advisory=HEADROOM_OK`, trois `ENGINESESSION READY`, `shapeData=mediaTexels=0x0`, aucun WDT, Guru Meditation, flash failure ou `LOAD-FAILED` dans la capture. Retour utilisateur : « C'est niquel pour moi ;) ».

## Limites et périmètre

- **Le scénario nouveau START après Junction n'est pas capturé** dans ce log : le code et la compilation de cette route passent, mais pas de PASS Serial distinct revendiqué. Death-route non rejoué.
- La trace indique `[ENTRANCEMUSIC] FINAL reason=FAIL-CLOCK` sur la reprise Entrance, après la validation du chargement ; **anomalie audio séparée** à traiter sur un jalon audio, pas un échec de cette PR. Ne pas confondre avec la progression et ne pas revendiquer audio PASS.
- Après ce PASS, tous les commits sur la branche doivent être **docs-only**. La PR peut être fusionnée à la demande explicite de l'utilisateur.
