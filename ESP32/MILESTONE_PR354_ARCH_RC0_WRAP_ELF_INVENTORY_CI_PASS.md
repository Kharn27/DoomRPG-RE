# ARCH-RC0 — vraie cartographie des interpositions natives `--wrap` dans quatre ELF

**16 novembre 2026 — [PR #354](https://github.com/Kharn27/DoomRPG-RE/pull/354)**, [issue #340](https://github.com/Kharn27/DoomRPG-RE/issues/340). Base `main` GitHub exactement vérifiée **après fusion de #351** : `4844d4da3c65e7b4fd9ea4117a76ac676d23e764`.

## Motivation et frontière

Le firmware n'embarque plus SDL et ne compile plus les 21 TUs desktop originaux ([audit post-SDL](ARCH_POST_SDL_AUDIT.md)). Son intégration native utilise pourtant de nombreuses interpositions GNU ld `-Wl,--wrap=...`. La suppression de ces flags sur la seule foi de leur présence textuelle dans `platformio.ini`, ou de la disparition d'un symbole, serait dangereuse.

La prochaine extraction du vrai sampler mural R-B nécessite toujours une preuve comportementale *full-frame* avec les ressources originales, distincte des témoins R-A2a/R-A2b déjà acquis. **ARCH-RC0 est donc un inventaire R-C préalable et indépendant, sans refactoring des appels.** Il permet de choisir de façon factuelle la première frontière de composition native à clarifier plus tard.

## Réalisation

- Nouveau test permanent `ESP32/test/native_wrap_linkage_inventory_host.py` : lit les sections réelles `ESP32/platformio.ini`, refuse une déclaration `--wrap` malformée, doublée ou inattendue ; protège quelques frontières essentielles comme le rendu, la lecture PAK et la RNG. Les extensions d'environnement sont inventoriées séparément.
- Pour **chacun des quatre firmwares réellement construits**, lit `xtensa-esp32-elf-nm -S --defined-only firmware.elf` et `xtensa-esp32-elf-objdump -d firmware.elf`. Rapproche les flags `--wrap` des corps `__wrap_*` **effectivement liés**, puis liste les *call/jump sites directs visibles* comme témoins supplémentaires.
- Un wrapper défini dans l'ELF **ne prouve pas qu'il s'exécute en gameplay** ; l'absence de call site direct visible **ne prouve pas qu'il est dormant** (indirection, optimisations). Les rapports font explicitement cette distinction. L'inventaire n'attribue aucun gain RAM/flash, aucun appel runtime ni aucune équivalence sémantique non mesurée.
- Le test `--compare-release` refuse une différence de frontières déclarées ou d'admission des symboles liés entre `esp32-cyd` et `esp-cyd-inverted`. Il ne compare pas artificiellement des compteurs de calls, qui peuvent dépendre du binaire de la dalle.
- Les trois wrappers système `__wrap_esp_log_write`, `__wrap_esp_log_writev`, `__wrap_log_printf` sont présents dans les **quatre ELF** et relèvent du toolchain ESP32 : ils ne font **pas** partie des 45 `--wrap` configurés pour Doom RPG. Ils sont explicitement reconnus ; **tout autre wrapper lié non déclaré** échoue fermé.
- La CI lance le self-test adversarial avant les builds et l'inventaire des quatre ELF après les audits desktop. Quatre rapports JSON `native_wrap_inventory.json` sont conservés dans les artefacts avec les firmwares : ce sont des **métadonnées de linkage**, sans assets privés ni capture de gameplay.

## Mesures effectives du code testé

| Firmware | Flags natifs déclarés | Wrappers liés | Frontières avec témoin call/jump direct | Diagnostics absents |
| --- | ---: | ---: | ---: | --- |
| `esp32-cyd` | 45 | 45 | 45 | 0 |
| `esp-cyd-inverted` | 45 | 45 | 45 | 0 |
| `esp32-cyd-bringup` | 50 | 48 | 48 | 2 |
| `esp32-cyd-trace` | 45 | 45 | 45 | 0 |

Les deux sondes bringup déclarées mais non liées sont `DoomRPG_probeNativeMenuSpriteFrame` et `DoomRPG_probeNativeMainMenuOverlay`, déjà commentées « dormant » dans `platformio.ini` ; c'est **une absence de définition finale**, pas une preuve d'un comportement retiré.

Le corpus normal prouve que **les 45 frontières ne sont pas de simples options mortes** : chaque wrapper métier est réellement lié et possède au moins un appel direct visible dans le désassemblage. Exemples de plusieurs consommateurs dans le rapport : `Esp32PlatformVideo_present`, `EspMapRuntime_getLine`, `EspAssetPack_readRange`. Cela justifie de ne **pas** les supprimer en groupe.

**Commit code final testé :** `74a3ab8531f6beb574f436fa5cc0ed7e7b6a519a`. Double CI :
- [PR run #43781252701](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43781252701) — **SUCCESS**, tests hôte, quatre builds et audits ELF ;
- [push run #43781245851](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43781245851) — **SUCCESS**, mêmes garde-fous.

Les premiers runs du code avant renforcement de l'allowlist étaient également verts ([PR 43780626959](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43780626959), [push 43780597595](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43780597595)). Ils ont permis de constater indépendamment la présence des trois adaptateurs du toolchain. **Recontrôler la double CI après les commits purement documentaires avant toute fusion**.

## Suite de qualité native

- **R-B (sampler mural)** : obtenir d'abord une preuve originale full-frame reproductible, puis extraire le petit contrat de bloc 4-bpp/cache borné et de successeur sans modifier les pixels. Ne pas réintroduire `mediaTexels` map-wide ; les vrais 7/37 voisins PAK physiques divergents démontrent que l'adjacence naïve est incorrecte.
- **R-C1 (composition)** : choisir **une** frontière observée dans les JSON, examiner ses vrais consommateurs et l'ordre des effets, puis migrer vers un appel/interface directe si les oracles C et un test CYD prouvent strictement la parité. Un seul wrapper à la fois, avec rollback fail-closed ; aucune suppression « parce qu'il n'est plus SDL ».
- **R-D (headers historiques)** : retirer progressivement les dépendances aux structures desktop selon les consommateurs réels, sans détruire les oracles.

**Aucune source firmware `ESP32/src`, aucun `src` desktop, aucun flag dans `platformio.ini`, aucun pixel, audio R2e, cache PAK, SRAM ou profil LCD n'a été modifié.** ARCH-RC0 est un milestone **host/CI-only**. Pas de flash demandé, pas de PASS matériel inventé. Le vrai CYD devra tester la future PR modifiant un owner ou une interposition. **Ne pas merger #354 sans accord explicite.**
