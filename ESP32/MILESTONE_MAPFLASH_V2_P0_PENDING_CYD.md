# MAPFLASH-V2 — P0 audit de capacité flash, en attente du CYD

Issue [#476](https://github.com/Kharn27/DoomRPG-RE/issues/476). Branche unique `agent/esp32-mapflash-v2`, basée sur `main=d1f25ba03688364efba753604f59aa75eb2eaadc` au démarrage. **Statut : P0 code read-only, PAS encore de cache V2 ni de PASS matériel.**

## Source exacte relue

- `ESP32/PORTING_STATUS.md`, `ESP32/DOCUMENTATION.md`, `ESP32/MILESTONE_LOADING_PROGRESS_DEFERRED_FLASH_PENDING_CYD.md`.
- `ESP32/src/esp_asset_pack.cpp`, `ESP32/include/esp_asset_pack.h`, `ESP32/src/esp_map_catalog.c`, `ESP32/tools/build_asset_pack.py`, `ESP32/partitions_cyd_raw_pak.csv`.
- `main` source : cache legacy `DMF2` version 1, secteur 0 commité en dernier ; index source copié à `0x1000`, payload compact sans les 12 autres BSP, FNV1a de l'index et payload vérifiés en flash sur HIT. Un changement d'ID map invalide le HIT ; stage efface toute la partition.
- Partition : app0 `0x10000..0x150000` (0x140000 B), cache brut `0x150000..0x3F0000` (0x2A0000 = **2 752 512 B**, 672 secteurs de 4 KiB), coredump `0x3F0000..0x400000` (0x10000 B). **Aucun changement de partition n'est nécessaire pour P0**.
- Index PAK v2 : header 24 B ; entrée 20 B `nameHash,offset,size,crc32,flags` ; hash trié ; payload non compressé, offsets absolus. Les 13 noms BSP sont dans `esp_map_catalog.c`. L'index est global, de même que les fichiers graphiques/audio non-BSP : **ne surtout pas supposer une nomenclature de textures locale par BSP**. Conserver tous les octets non-BSP comme ressources communes jusqu'à un audit de dépendances prouvant une autre sélection.
- Le cache L1 résident de 15 KiB et les fonctionnalités audio et Save/Load restent inchangés.

## Audit MAPFLASHV2 ajouté au firmware normal

Dans `mapFlashBuildSourcePlan()`, le scan existant en lecture seule enregistre pour chacun des 13 BSP du catalogue : présence, sourceOffset, size ; contrôle les overlaps ; calcule `allBspBytes`, `maxBspBytes` et `commonBytes = (packBytes - dataOffset) - allBspBytes`. **Aucune lecture additionnelle de payload SD**, aucune allocation heap, aucune écriture flash supplémentaire et aucune modification de la translation V1.

Au premier plan de PAK (puis à chaque changement pack/index FNV), nouvelles lignes :

- `[MAPFLASHV2] AUDIT scope=plan-only ... common=... allBsp=... maxBsp=... commonOffset=... bspOffset=... slotReserve=... headroom=... singleFits=... allFitsLowerBound=...`
- `[MAPFLASHV2] BSP id=1..13 name=... source=... bytes=... present=...`

Proposition **théorique**, non activée : secteur 0 header A (common), secteur 1 header B (BSP), index dès `0x2000`, payload commun dès `align4096(0x2000 + indexBytes)`, slot BSP dès `align4096(commonOffset + commonBytes)`, réserve `align4096(maxBspBytes)`. Chaque header devrait être validé/commité séparément. `singleFits` doit être vrai pour que cette organisation simple soit possible **dans la partition existante**. `allFitsLowerBound` suppose au mieux un stockage contigu de tous les BSP sans overhead de manifest/transactions : FALSE exclut cette variante sans compression/réduction ; TRUE **ne prouve pas** que la variante est sûre.

Le PAK et les 13 tailles réelles ne sont pas versionnés dans le dépôt : ils doivent provenir du véritable log ou d'un pack local. `python ESP32/test/mapflash_v2_audit_host.py --pak /chemin/DoomRPG-ESP32.pak --check-crc` donne aussi le bilan détaillé, CRC inclus, sur PC. Le script sans argument exécute seulement les cas synthétiques. Le code P0 **ne réalise pas la migration V1→V2** : ce contrat sera abordé au jalon d'écriture, pour éviter un flash partiellement valide.

## Hypothèses et limites de sécurité à résoudre avant activation V2

1. Zone A : invalidation **avant toute réécriture** et publication du header commun **après** readback intégral index + payload ; aucune conservation de l'ancien cache en cas de changement de PAK.
2. Zone B : invalider **son** header avant effacement BSP, ne jamais toucher aux secteurs A sur BSP MISS, puis publier seulement après CRC/FNV complet et readback. Interruption sur B => A conservée, B fail-closed/reconstruite au prochain prepare. Activer le lease runtime **uniquement après double validation**.
3. Traduction : index par ordinal original, non-BSP = offset source compact corrigé de tous les BSP précédents, BSP courant = slot BSP et delta source, autres BSP = strict miss sans SD. Tout range traversant une frontière de segment doit être refusé.
4. Coût : vérifier si la validation complète de A sur chaque transition est dominante. Ne jamais accepter le simple header/identité à la place d'un check d'intégrité sans un autre mécanisme démontré.
5. Journaliser SD bytes, flash bytes, secteurs effacés, prepare/common verify/BSP stage, heap8/minHeap8/largest8 ; ces compteurs seront conçus avec la transaction V2, pas simulés en P0.

## Ancien étalon matériel (non comparable directement)

`MILESTONE_LOADING_PROGRESS_DEFERRED_FLASH_PENDING_CYD.md` : transition Entrance→Junction `staged=2 247 971 B`, `buildUs=18 942 433`, transition totale `19 143 ms` ; LOAD Junction→Entrance `20 453 ms`. Ce sont des observations de l'ancien code/ancien firmware, **pas** des mesures V2 ni un benchmark apparié.

## Validation P0 demandée

Firmware **normal** `esp32-cyd`, source code du HEAD communiqué après CI. Démarrer avec la vraie SD et `/DoomRPG-ESP32.pak` actuel, New Game→Entrance : capturer une ligne AUDIT et les 13 lignes BSP, ainsi que `[MAPFLASH] PLAN/REUSE/READY`, `ENGINESESSION READY`, `shapeData/mediaTexels`, `RAMBUDGET` et audio. Changer Entrance→Junction→Sector1 si accessible ; vérifier que les transitions V1 restent identiques, puis retour/LOAD si possible. Ne **pas** provoquer de corruption ou de coupure d'alimentation pour P0 : aucun protocole transactionnel V2 n'est actif. Aucune revendication de PASS matériel avant réception des logs.

`ESP32/test/mapflash_v2_audit_host.py` : fixture synthétique couvrant la géométrie des 13 BSP, les bornes, l'ordre de hash, les chevauchements, le CRC et la troncature. L'exécution est ajoutée à `.github/workflows/esp32-cyd.yml`.

**Suite après les tailles réelles :** décision documentée entre slot BSP et cache multi-BSP, puis seulement écrire V2 et tests de coupure/transactions avant un nouveau firmware.
