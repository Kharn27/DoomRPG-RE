# ARCH-RC1 — retrait de deux interpositions bringup historiquement dormantes

**16 novembre 2026 — [PR #355](https://github.com/Kharn27/DoomRPG-RE/pull/355)**. Départ vérifié du véritable `main` GitHub **après fusion de #354** : `34b9ddce42354fa65cbcf1e7b81aae1568a4ac6f`. Référence architecturale : [issue #340](https://github.com/Kharn27/DoomRPG-RE/issues/340) et [ARCH-RC0](MILESTONE_PR354_ARCH_RC0_WRAP_ELF_INVENTORY_CI_PASS.md).

## Constat indépendant sur quatre vrais ELF (RC0)

L'inventaire préalable RC0 a obtenu, dans les quatre vrais `firmware.elf` Xtensa, **45 wrappers métiers liés et avec témoin d'appel direct** dans le normal, l'inverted et le trace ; pour le bringup : **50 flags déclarés, 48 corps `__wrap_*` liés, 48 témoins directs**. Deux flags bringup seulement n'avaient pas de propriétaire lié :

- `-Wl,--wrap=DoomRPG_probeNativeMenuSpriteFrame`
- `-Wl,--wrap=DoomRPG_probeNativeMainMenuOverlay`

Il s'agit de vieux ponts de sondage entre menu et dashboard. L'absence de corps `__wrap_*` lié est une **preuve de composition ELF**, pas à elle seule une preuve d'absence de toute exécution comportementale. La présente étape ne retire que les **flags linker inopérants** (pas les fonctions, pas les vrais propriétaires de compositing).

## Modification exactement bornée

1. Dans `ESP32/platformio.ini`, supprimer seulement ces deux lignes bringup et mettre à jour leur commentaire. Les trois flags bringup encore actifs restent : `EspNativeFirstFrame_route`, `EspNativeGameplayInteractionInventory_log`, `EspNativeResidentGameplay_service`.
2. Dans `ESP32/test/native_wrap_linkage_inventory_host.py`, passer de **45+5** à **45+3**, épingler la liste des trois survivants, **refuser l'ajout ultérieur** de chacun des deux anciens `--wrap` dans n'importe quel environnement, et **échouer fermé si un `__wrap_*` retiré apparaît dans un vrai ELF**.
3. Pour les **quatre vrais ELF**, renforcer l'invariant de cohérence : toute frontière `--wrap` encore déclarée doit avoir un propriétaire lié. Les trois wrappers `__wrap_esp_log_write`, `__wrap_esp_log_writev` et `__wrap_log_printf` restent identifiés à part comme appartenant au toolchain ESP32, non aux interpositions métier.
4. Renommer les deux étapes CI d'audit en ARCH-RC1, conserver les rapports `native_wrap_inventory.json` avec les artefacts des quatre firmwares. Les tests hôte adversariaux (doublons, malformations, ressuscitation, divergence normal/inverted) restent exécutés **avant** les builds.

## Résultats mesurés sur le vrai code candidat

| Environnement | Déclarations natives `--wrap` | Wrappers réellement liés | Avec témoin direct `call/j` | Résultat |
| --- | ---: | ---: | ---: | --- |
| `esp32-cyd` | 45 | 45 | 45 | PASS |
| `esp-cyd-inverted` | 45 | 45 | 45 | PASS |
| `esp32-cyd-bringup` | 48 | 48 | 48 | PASS |
| `esp32-cyd-trace` | 45 | 45 | 45 | PASS |

Code testé, immuable ensuite : `7c7e429c29e117840fbf6016d559ddb2d7b8210b`.

- [CI PR #43782887871](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43782887871) — **SUCCESS**, quatre builds, parité du moteur release, tests historiques et audit ARCH-RC1.
- [CI push #43782850481](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43782850481) — **SUCCESS**, mêmes étapes. Le log du push confirme `esp32-cyd-bringup declared=48 linked=48 directCallWitness=48` et `PASS ... common release engine wraps=45`.
- **Validation CI, pas validation matérielle :** aucun nouveau log Serial du CYD. Une définition wrapper liée et un appel direct statique ne signifient pas qu'une branche de gameplay l'exécute en pratique.
- La double CI **du HEAD documentaire final** doit de nouveau être verte avant fusion.

## Limites et suite

Aucune source `ESP32/src` ou `src` de l'ancien desktop n'est modifiée. Aucun `--wrap` de production ou inverted retiré ; aucun changement des 45 flags de la release, du toucher, du framebuffer, du rendu, de l'audio R2e, des maps, de la RNG, du PAK/ZIP ou de la RAM allouée. La modification est **limitée au bringup diagnostique et aux protections de CI**. Aucun gain de SRAM, flash, ni performance n'est revendiqué.

**R-C2** doit maintenant examiner la première **interposition liée réellement active** avec ses consommateurs (par exemple, l'inventaire RC0 signale `EspAssetPack_residentResetStats` avec un témoin direct : à étudier, pas à supprimer automatiquement). Conserver le contrat de l'appel, l'ordre des effets et les oracles avant migration de la composition linker vers une vraie interface native. Si le firmware runtime change, faire tester `esp32-cyd` sur le vrai CYD et ne documenter un PASS matériel que sur les logs exacts.

En parallèle, **R-B sampler mural** reste conditionné au vrai golden original full-frame, sans regrouper le desktop `mediaTexels` ni lire un faux voisin physique du PAK. Une famille, un milestone, un rollback clair. **PR non fusionnée sans consentement explicite.**
