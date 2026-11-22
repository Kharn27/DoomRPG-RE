# AUDIO-382-P1 — SFX génériques: mixer toute la fenêtre publiée (CYD PENDING)

22 novembre 2026. Issue [#382](https://github.com/Kharn27/DoomRPG-RE/issues/382), notamment le témoin [5069 Sector 1](https://github.com/Kharn27/DoomRPG-RE/issues/382#issuecomment-7084247185). Branche `agent/audio-382-sfx-published-window`, base exacte `main=d9896fcf5f07ab8d41935565002792b608a41333`. Aucun résultat matériel inventé.

## Observation et mécanisme ciblé

Sur le vrai CYD, Sector 1: `[SFXGEN] DROP id=5069 reason=reader-caught-up mixed=17058/20286`: **3228 frames non mixées (~146ms)** alors que `M_E1M3.wav` termine avec **0 underrun et 0 recovery**. RC11/RC12 ont aussi vu des drops 5046/5099 et, séparément, des underruns musicaux. Ne pas attribuer ces défauts à une même cause sans mesure.

Le producteur générique mixait au plus **8192 frames** par appel, même si une plus grande plage de PCM musical futur était *déjà publiée* dans le ring partagé de **16384 frames**. Une longue animation pouvait ensuite laisser une tranche non mixée sous la tête de lecture DAC. Le garde `reader-caught-up` rejetait correctement le reste, mais un plafond logiciel arbitraire contribuait à cette fenêtre de risque.

## Correction P1: minimale, à vérifier sur CYD

`EspNativeSfxPcm_publishedSpan()` calcule `min(writer-first, framesRestantes)` sans plafond artificiel de 8192, refuse tout dépassement de la capacité physique du ring, et laisse intact le **garde vivant de 128 frames** avant écriture PCM. Appel réel dans `mixGenericSfx()`. Aucune allocation, seconde queue, tâche, lecteur SD/PAK, copie SFX map-wide, ISR ou cadence modifiée. Ring 16KiB constant, décodeur PCM PAK flash par tranches 512 octets, musique ADPCM prioritaire conservée.

**Risque à mesurer**: mixer davantage de samples PCM lors d'un même passage peut augmenter le temps du producteur et retarder le service musical. Ce P1 ne garantit pas 0 DROP sous toute charge et ne corrige pas les 852/513 underruns musicaux historiques.

## Oracle et critères matériel

`python ESP32/test/audio_sfx_382_published_window_host.py` compile le **helper de production C**, simule les trois tailles réelles 5046=11466, 5099=19404, 5069=20286 frames, un ring de 16384, un initial `queued=12500`, une latence illustrative **420ms**, trois voix concurrentes et le refill musical borné existant. L'ancien cap conduisait à une collision avec le lecteur après la pause; les trois voix atteignent leur dernière frame avec la nouvelle politique. Test de fenêtre invalide/non publiée et wrap uint32. Ce n'est pas un oracle du mix complet PAK, du CPU ni du CYD.

**Test matériel demandé (environnement normal `esp32-cyd`, sans modifier SD/PAK):** trajet de combat Sector 1 reproduisant le SFX **5069**; laisser `M_E1M3.wav` jouer, conserver Serial complet, noter la perception auditive. Chercher `[SFXGEN] START/END/DROP id=5069`, progression totale `20286/20286`, `[AUDIOINTENT] AUDIT audibleSfxDropped=0 sfxRejectedTrack=0`, `[ENTRANCEMUSIC] STREAM/FINAL underruns=0 recoveries=0`, `HEADROOM_OK`, aucune panique/WDT; comparer impression de latence et fluidité tactile. Vérifier également EXIT et retour menu. Si une autre cause apparaît, conserver le témoin dans #382 et ouvrir P2 ciblé plutôt que de masquer le DROP.

**État**: code PENDING CYD. Fusion uniquement après test physique, revue CI et décision utilisateur. Ne pas confondre avec résolution totale de #382.
