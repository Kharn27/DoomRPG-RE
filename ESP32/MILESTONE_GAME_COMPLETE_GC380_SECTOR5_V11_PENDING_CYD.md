# GAME-COMPLETE GC380-P1 — SAVE V11 Sector 5, 133 topologies dans 816 octets

**Issue :** [#380](https://github.com/Kharn27/DoomRPG-RE/issues/380).
**Base GitHub vérifiée :** `main=d1f25ba03688364efba753604f59aa75eb2eaadc` (après fusion #471, intégration de PERF-GAME P3 et HUMAN #463).
**Branche :** `agent/esp32-gc380-sector5-topology-v11`.
**Statut : PENDING CI + PENDING CYD, aucun PASS matériel revendiqué.** Rebasage GC380-P2 depuis main du 28 novembre 2026.
La découverte vient de [GAME-COMPLETE-A0, PR #376](https://github.com/Kharn27/DoomRPG-RE/pull/376), audit hôte des 13 BSP : `/level05.bsp`, CRC32 `58fbefae`, 504 sprites, 85 ennemis + 48 destructibles = **133** ennemis+destructibles (le nombre d'humains sur Sector 5 n'a pas été vérifié dans cet audit). Cet audit est une preuve de corpus et de cardinalité, **pas** une exécution de sauvegarde/chargement sur CYD ni une preuve de budget RAM.

## Problème précis et périmètre

Jusqu'ici `EspMapSpriteTopology_snapshotMonsters` parcourt **tous** les ennemis et destructibles, y compris cachés ou morts, et refuse dès le 101e : `SNAPSHOT-FAILED reason=capacity cap=100`, puis `V11-CAPTURE-FAILED stage=monster-topology`. Pourtant, le propriétaire des monstres logiques a sa **propre** limite de 100 : les 85 monstres du Sector 5 y entrent. Ce sont 85+48 **topologies**, et non 133 monstres.

Ce milestone **ne modifie pas** les opcodes, la mort, l'audio, les SFX, le rendu, les transitions, le sous-système SD, le menu ni le gameplay. Il modifie uniquement le codec de snapshot de topologie et ses deux lecteurs sémantiques SAVE V11.

## Encodage sans changement de taille ni version

Le format du fichier existant conserve la version **11**, la longueur **5 604 octets**, le `NativeSaveRecordV9Tail` **3 508 octets** et son snapshot topologie **816 octets**. Aucun layout d'en-tête ne change. `recordBytes` dans les 816 octets **discrimine** deux codages :

| Encodage | Admission | Enregistrement | Poids |
|---|---|---|---|
| **legacy8** (historique) | 0 à 100 entités suivies | `spriteIndex:16, linkState:16, linkOrder:16, visual:8, reserved:8` | 100 × 8 = 800 octets |
| **packed6** (nouveau, uniquement si nécessaire) | 101 à 133 entités suivies (ennemis, destructibles et humains) | `spriteIndexLow:8, linkStateLow:8, linkStateHighLow6 + spriteIndexHigh2, linkOrder:16, visual:8` | 133 × 6 = **798 octets** |

L'état de lien natif utilise uniquement les bits **0..13** (`TILE_MASK=0x03ff`, flags `0x0400..0x2000`). Les deux bits 14..15, **réservés et toujours nuls dans le vrai propriétaire**, transportent donc les bits 8..9 de l'identifiant sprite. Cette conversion est **sans perte** pour les indices sprites **0..1023**. Au-delà, la capture refuse explicitement : pas de troncature ou de renumérotation. Chaque record compact stocke **son propre index complet**, il ne dépend pas de la séquence des types ni de l'ordre reconstruit au chargement. Les deux octets de payload inutilisés (798–799) doivent être zéro.

Le FNV de topologie est calculé sur les **enregistrements sémantiques décodés** (index réel, état sans bits codec, ordre, visuel), identique au contrat historique pour `legacy8`. Les métadonnées source-arena, count, reserved, ordre strict, flags de lien, `nextLinkOrder`, padding et hash sont validés **avant** l'application. La restauration `packed6` vérifie aussi que l'ensemble des indices correspond **exactement** aux ennemis et destructibles présents dans la topologie vivante, sinon refus sans mutation. Les sauvegardes anciennes `legacy8`, y compris les anciens états enemy-only V9, gardent leur sémantique/replay. Le CRC du fichier V11 continue de couvrir tous les octets de la section inchangée.

La voie `EspNativeGameplayMonsterState` garde ses **1612 octets** et sa capacité 100 monstres ; la position **812 octets**, l'activation **268 octets** et les drops V11 **144 octets** restent inchangés. Dans `esp_native_gameplay_save.cpp`, la réconciliation logique, le cross-check spatial, la lecture précoce de sauvegarde et le restore lisent les deux codages par la **même petite API C native** `EspMapSpriteTopology_monsterSnapshotRecordAt`. Pas de mutation pendant le préflight et pas de nouvel allocateur.

## Tests hôte et sécurité

`ESP32/test/game_complete_gc380_topology_v11_host.py` **extrait, compile et exécute les vrais corps C de production** (codec, FNV, shapeValid, snapshot et restore) avec un propriétaire minimal déterministe qui possède la **cardinalité vérifiée Sector 5 : 504 sprites, 85 ennemis et 48 destructibles**, et des indices répartis sur plus de 256 sprites. Ce témoin n'est **pas** un chargement du PAK réel ; l'exécution du vrai PAK + sauvegarde V11 sur CYD reste à faire.

Couvre : `133 → packed6`, dernier index 396 (bits hauts réellement sollicités), restaurations exactes `SNAPSHOT → reset → RESTORE → SNAPSHOT` et `SAVE → EXIT/reset → LOAD → SAVE`, mutations morts/cachés/déliés, shape FNV/CRC préservé, `100 → legacy8` avec **octets identiques à l'ancien layout**, ancien enemy-only V9, padding, doublons, ordre, identité corrompue, marqueur/compte incohérent, ordre de lien corrompu, mauvais arena, index supérieur à 1023, **134 entités fail-closed** et owner absent. Ce test est intégré à `.github/workflows/esp32-cyd.yml`. Les builds firmware normaux/trace/inversé/bringup et les tests existants doivent rester verts.

## Protocole matériel — environnement normal `esp32-cyd`

Après **double CI SUCCESS** de la PR, flasher la branche `agent/esp32-gc380-sector5-topology-v11` en `esp32-cyd` sur un classic CYD sans PSRAM, SD avec `/DoomRPG-ESP32.pak` d'origine, sans ZIP runtime.

1. Ouvrir ou rejoindre **Sector 5 / `level05.bsp`**, carte ID6. Vérifier le bon CRC `58fbefae`, le nombre de sprites **504**, ennemis **85**, destructibles **48**, `[MAPCHECKPOINTTOPO]` construit et `[RAMBUDGET] ... advisory=HEADROOM_OK`. Si le jeu ne permet pas d'accéder à cette carte sans modifier le gameplay, le déclarer et ne pas inventer un PASS ; fournir la trace du blocage.
2. Dans le HUB, faire une **SAVE V11** sur un slot libre puis vérifier : `[MAPCHECKPOINTTOPO] SNAPSHOT tracked=133 enemies=85 destructibles=48 codec=packed6 recordBytes=6 sectionBytes=816 allocation=no`, puis `[NATIVESAVE] SAVE ... version=11 bytes=5604`, sans `V11-CAPTURE-FAILED`.
3. Modifier des états (ennemis/destructibles/portes si possibles) → **SAVE** → **EXIT** → **LOAD du même slot** → **SAVE à nouveau**. Observer `[NATIVESAVE] READABLE-SPATIAL ... result=valid`, `[MAPCHECKPOINTTOPO] RESTORE ... tracked=133 ... codec=packed6 ... mutation=tracked-topology-exact`. Consigner les **FNV** topologie et monstres et comparer avant/après. Vérifier qu'aucun ennemi mort/destructible supprimé ne réapparaisse. Conserver une sauvegarde ancienne V11 (Entrance) pour test de rétrocompatibilité `codec=legacy8`.
4. Vérifier enregistrement/chargement, audio sans underruns, HUD, `heap8/largest8`, `shapeData=0x0 mediaTexels=0x0`, retour EXIT/menu avec `RESIDENTRESET empty=1`, sans WDT/panic ni `FAIL-HEAP`. Un checkpoint corrompu doit être refusé en hôte ; ne pas abîmer volontairement la SD de jeu.

**Gates de fusion :** tests hôte réels + double CI firmware + preuve Serial **Sector 5** sur vrai CYD + contrôle rétrocompatibilité V11 + addendum docs-only après PASS + accord explicite. **Ne pas merger avant cela.**

## GC380-P2 — réalignement après main / PR #463 HUMAN

Le code actuel suit **enemy + destructible + human** ; le correctif historique de #425 ne suivait que les deux premières familles. Réintégration sélective du codec sur `main` sans revenir sur les propriétaires/refresh P3l, ni sur les lectures V11 human-inclusive de #463. Le préflight de restore vérifie l'ensemble canonique correspondant au scope sélectionné ; les formats `legacy8` human-inclusive, anciens `legacy8` enemy+destructible et enemy-only restent admis selon les mêmes règles qu'avant. Les deux lectures sémantiques C++ de V11 décodent packed6 par une seule API C ; aucun accès direct à `records[]` n'est conservé pour la topologie. Les 816B / 5604B restent inchangés.

**Limite fail-closed explicite :** packed6 contient 133 records maximum **toutes familles confondues**. Le corpus GC-A0 a attesté 133 enemy+destructible sur Sector 5 mais n'a pas fourni le nombre de sprites HUMAN. Si Sector 5 contient des humains additionnels, `tracked>133` sera refusé proprement ; ne pas annoncer la résolution complète sans la preuve Serial `enemies=85 destructibles=48 humans=0` ou un traitement générique documenté de la cardinalité supplémentaire. Tests hôte ajoutés : 133 avec HUMAN, legacy8 avec six HUMAN, et 134 incluant HUMAN refusé.

Le jalon reste strictement **PENDING CYD**, firmware normal `esp32-cyd`, double CI du nouveau HEAD requis avant test.
