# ARCH-RC2 — première interposition active remplacée par deux hooks natifs explicites

**16 novembre 2026 — PR [#359](https://github.com/Kharn27/DoomRPG-RE/pull/359)**. Base initiale contrôlée sur GitHub : `main=721e3fe8759f443c30bd7ebcceb1220340e79871` (fusion #357). Branche `agent/esp32-arch-rc2-resident-stats-hook`. Issue [#340](https://github.com/Kharn27/DoomRPG-RE/issues/340), audit [ARCH-RC0](MILESTONE_PR354_ARCH_RC0_WRAP_ELF_INVENTORY_CI_PASS.md), garde [ARCH-RC1](MILESTONE_PR355_ARCH_RC1_DORMANT_BRINGUP_WRAP_RETIREMENT_CI_PASS.md).

## Frontière remplacée, comportement préservé

ARCH-RC1 prouvait la liaison ELF et un témoin d'appel direct de `__wrap_EspAssetPack_residentResetStats`. L'interposition dans `ESP32/src/esp_asset_pack_perf_probe.cpp` encapsulait la vraie API PAK, avec cet ordre strict :

1. En TRACE uniquement, `reportIoSample("resident-reset")` ; aucune émission si aucun appel de lecture logique échantillonné.
2. `EspAssetPack_residentResetStats()` : zéro de `residentStats`, `mapFlashRuntimeReadMicros` et, lorsque `DOOMRPG_RA2B_CAPTURE`, `ra2bSdRangeMicros`.
3. En TRACE uniquement, `invalidatePhysicalBaseline()`.

Cette PR ajoute le petit header permanent `ESP32/include/esp_asset_pack_io_probe.h` et deux hooks **C explicites** `EspAssetPackIoProbe_beforeResidentStatsReset` et `EspAssetPackIoProbe_afterResidentStatsReset`, appelés par le vrai propriétaire `esp_asset_pack.cpp` autour de l'opération inchangée. L'implémentation d'observation reste dans `esp_asset_pack_perf_probe.cpp`. Un seul `-Wl,--wrap=EspAssetPack_residentResetStats` disparaît de `platformio.ini` ; les deux noms de linkage `__wrap_`/`__real_` correspondants disparaissent aussi. Aucune nouvelle allocation, cache, copie PAK, lecture ZIP, mutation gameplay, texture atlas ni changement LCD/audio.

## Preuves hôte, ELF et CI

- `ESP32/test/native_resident_stats_reset_boundary_host.py` compile des corps C++ **extraits des sources réelles** : 4 cas positifs INFO/TRACE × RA2b 0/1, ordre avant/zéro/après vérifié, 6 négatifs d'omission/réordonnancement. Ces fixtures sont ciblées : elles ne simulent pas tout le PAK ni un appareil physique.
- `native_wrap_linkage_inventory_host.py` interdit désormais la restauration du flag `--wrap=EspAssetPack_residentResetStats` dans tout profil et échoue si un symbole `__wrap_` retraité est à nouveau lié. Les deux anciennes sondes bringup retirées en RC1 restent interdites.
- Dans les **quatre vrais ELF compilés**, inventaire déclaré / owner lié / témoin direct : `esp32-cyd 44/44/44`, `esp-cyd-inverted 44/44/44`, `esp32-cyd-bringup 47/47/47`, `esp32-cyd-trace 44/44/44`. Les 3 wrappers de logs du toolchain sont reconnus séparément. L'ELF normal/inverted garde un inventaire identique ; un appel direct statique ne prouve pas une invocation runtime.
- **Code testé** : `4856581dae98037c5cbf61f9ad30603783196c32`. Double CI code `SUCCESS`, tous les tests et quatre builds : [PR #43951075947](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43951075947) et [push #43950975480](https://github.com/Kharn27/DoomRPG-RE/actions/runs/43950975480). Recontrôler la double CI de la tête documentaire avant merge.

## Retour du vrai CYD — PASS de la frontière RC2

Utilisateur : « Ca marche pas mal :)))) » ; fichier Serial `platformio-device-monitor-261116-223920.log`, **SHA-256 `2d2bfe5d1e1e8b959acaa3a016bbdcde3dfed2124f07f9230c4f530e6666f0ad`**, 1 403 lignes. Test sur classic CYD ESP32-D0WD-V3, 4 MB flash, **PSRAM=0**, profil déclaré dans le log :

`[VIDEO] CYD display profile alternate-panel inversion=off gamma=ILI9341-driver-E0E1-trial BGR=unchanged resampling=nearest framebuffer=untouched`

C'est **le profil `esp-cyd-inverted`**, dont la polarité physique LCD INV est *off*, à ne pas confondre avec un profil normal : `platform_video.cpp` associe cette chaîne uniquement au define `DOOMRPG_CYD_ALTERNATE_PANEL_INVERSION=1`. Le log ne grave pas explicitement le SHA de firmware ; le lien au commit testé repose sur la branche annoncée pour le flash et sur la double CI du code, sans prétendre à une attestation cryptographique de l'image flashée.

- SD montée et PAK indexé avec **241 entrées**, BSP `/intro.bsp` CRC32 `623f34e4`, arena de `14 095 B`, FNV `c3882516`. Deux sessions Entrance : nouvelle partie puis LOAD V11 depuis `/DoomRPG-ESP32-slot04.sav` (`5604 B`), restauration de joueur, monstres, topology, positions, script et overlays ; deux `[ENGINESESSION] READY`.
- Deux `[MAPFLASH] REUSE HIT` pour la même carte vérifiée, `rebuild=no` ; deux `ARM ... resident=1`, cache résident `owner=23592 B` / `payload=19456 B` ; les deux chargements atteignent `[TRANSITIONLOAD] RELEASE ... progress=100`.
- Actions visibles : marche/rotation, caisse transformée, collectibles, porte animée, dialogues opcode 26 et 8 avec continuation opcode 19, HUB/SYSTEM, exit-to-menu confirmé deux fois, LOAD avec sprite/monster state. Les sorties sont `[RESIDENTRESET] ... released=18008 ... after=0/0/0/0/0/0/0 empty=1` **deux fois**, et `[SYSEXIT] MENU-READY ... resident=empty` deux fois.
- `shapeData=0x0` et `mediaTexels=0x0` au démarrage du jeu, chargement natif, `ENGINESESSION READY` fresh et resume, et retour au menu. Aucun panic/Guru Meditation/WDT/assertion signalé dans les 1403 lignes.
- Heap8 min **57 180 B** dans les lignes `[ALIVE]`, `largest8=45 044 B` pendant gameplay, `heap8After=127080` sur **les deux** fins d'audio. Pas de fuite évidente sur les deux cycles observés. Ce n'est pas un benchmark de gain RAM/flash.
- Musique IMA-ADPCM : `FINAL` fresh `frames=846121`, resume `frames=536835`, **0 underrun, 0 recovery, 0 gap>90 µs** sur les deux, `timer=OFF dacPad=OFF file=CLOSED`. Douze `[SFXP1] MIX` observés (`5046` ×1, `5060` ×11) ; deferred pickup `5060` repris après fermeture du dialogue.
- Les captures `[NATIVEFRAME] BOUNDARY-INLINE` montrent des successeurs logiques de l'atlas compact : ce test **ne prouve pas** la parité desktop full-frame et ne débloque **pas** le chantier sampler R-B.
- **Limites** : les deux sessions concernent **Entrance** et une carte déjà présente en flash (`REUSE HIT`). Aucun `MAPFLASH STAGE` d'une autre carte, aucun A/B pixel desktop full-frame, aucun matériel normal `esp32-cyd` pour *ce commit précis*, aucun échantillon TRACE de `reportIoSample` (le binaire inverted est INFO), aucune preuve d'exécution runtime exhaustive. Le passage C++ hôte teste explicitement les effets TRACE. Une anomalie éventuelle propre à l'audio ou aux projectiles relève d'un autre milestone, sans changement ici.

**Verdict : PASS matériel limité mais suffisant pour ARCH-RC2**, parité fonctionnelle observée sur l'ESP-CYD inverted, en complément de la double CI sur quatre profils. Commit(s) post-PASS **docs-only**. Ne pas fusionner dans main sans autorisation explicite ; vérifier CI finale et possibilité de merge avec le main courant, qui a avancé sur la branche audio.

