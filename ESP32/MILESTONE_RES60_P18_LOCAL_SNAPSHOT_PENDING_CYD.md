# RES60-P18 — CPU : alias frame-local du snapshot heap (CI/CYD PENDING)

**Branche** `agent/esp32-res60-p1`, [PR #364 DRAFT](https://github.com/Kharn27/DoomRPG-RE/pull/364). Candidat P18 dérivé du code P17 CYD PASS `4b7718054ace14f78b34def20ff7e21c5d57be15`, avec synchro **main** `fb8a031ad5eb3d04efc65dff0da7febc2f21dd0c` via merge non-force `7e873e0094cda5d20c70a84dd6e4d1cc6278e1f8`. Le main UAC actuel est inclus ; aucun merge de cette expérience dans main.

## Mesure matérielle qui motive P18

Le Serial physique `platformio-device-monitor-261122-161354.log`, **576 lignes, 81222 octets**, SHA256 `fe19307c4d92f86bb630969ef299a9a6cd574f133cb0db9250763c04233d2fde`, prouve la réussite P17 : **37 frames, 34 rotations, 3 START, 3 MENU**, 4 FNV P3/P7 + P8/P10 identiques ; `heap8=114564` (-16B vs P16), `minHeap8=101632` (-16B), **`largest8=86004` (+28672B vs P16 `57332`)**. Le framebuffer de 55296B tient à `0x3ffd1ad0` dans la région devenue **59488B** ; le snapshot de 6144B, acquis une seule fois à `0x3ffb2730`, conserve une région séparée avec un bloc de 86004B.

**Régression CPU circonscrite :** moyennes `cache=HIT` P16/P17 : angle 64 N raycast **59.963→64.026ms (+6.78%)**, 0 E **20.865→22.284ms (+6.80%)**, 192 S **1.655→1.745ms (+5.45%)**, 128 W **7.375→7.869ms (+6.69%)**. En revanche `loadUs` baisse de ~3ms, donc `preparedUs` varie moins ou s'améliore ; ne pas confondre temps CPU de raycast et temps total. Les différences peuvent venir du code pointeur ou de sa localisation : l'hypothèse doit être vérifiée sur CYD.

## Changement unique P18

Le flag `DOOMRPG_RES60_P18_LOCAL_SNAPSHOT=1` est **strictement RES60 normal/inverted**, exige P17/P16/P15/P13/P1. Dans le vrai `esp_res60_world_preview.c`, le pointeur global `candidateLines` est copié **une seule fois par frame** dans le pointeur local automatique `frameSnapshot`, après validation du propriétaire. Les copies de 12 octets/candidat et lectures dans la boucle raycast utilisent cet alias afin de rendre un accès stable visible au compilateur. La lecture d'entrée de chaque ligne vérifie aussi que le callback n'a pas changé le propriétaire avant toute écriture. À P18=0, **chemin P17 strictement conservé** comme oracle A/B.

Aucune modification du nombre de lignes, de l'ordre des candidats, des ray tests, de `rayHit`, des tie-break, de P10 UV, des lectures PAK, des régions d'allocation, des 6144B heap, de la BSS, de `PlatformVideo_present`, ni de la production 160×120. La seule trace en plus est au boot : `[RES60P18] ALIAS source=P17-heap localPerFrame=1 allocationsPerRay=0 framebuffers=1`. L'alias peut ou non aider le générateur Xtensa ; **aucune vitesse supposée sans vrai test**.

## Oracle et critères

`ESP32/test/res60_p18_local_snapshot_host.py` reprend le **harness réel P15** : 261 frames full RGB565 55 296B comparant le code C P15 BSS (référence) et P18 heap (candidat), mêmes UV/sampling P10, canaries, égalités de distance, 512 candidats et erreurs. Injection d'échec `malloc`, rendu pré-init et après libération fail-closed, begin/end idempotents, nm BSS 6144B→4/8B, vérification exclusion production et P18 compilable seulement avec P17. Le pipeline conserve le test P17 intact et exécute P18 ensuite ; doivent passer les builds normaux/inversés/trace/bringup et les deux RES60.

Test matériel normal de la branche `pio run -e esp32-cyd-res60-inverted -t upload -t monitor` depuis `ESP32` ; tourner N/E/S/W et refaire START/MENU. Comparer `raysUs`, `preparedUs`, `heap8`, `minHeap8`, `largest8`, mêmes FNV. **Cible exploratoire** : retrouver ~60ms raycast nord sans perdre le bloc ~86K ni baisser sensiblement `minHeap8`. Le nouveau main UAC peut influencer les régions heap ; ne jamais inventer leur stabilité. Si non concluant, rollback P18 tout en conservant P17.

**Statut : CI/CYD PENDING, PR DRAFT. Non jouable en 192×144 ; preview MAP_INTRO seulement.**
