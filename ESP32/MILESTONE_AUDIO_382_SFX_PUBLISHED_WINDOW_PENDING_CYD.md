# AUDIO-382-P1 — SFX génériques: mixer toute la fenêtre publiée (CYD LOG PASS / AUDIBLE PENDING)

22 novembre 2026. Issue [#382](https://github.com/Kharn27/DoomRPG-RE/issues/382), notamment le témoin [5069 Sector 1](https://github.com/Kharn27/DoomRPG-RE/issues/382#issuecomment-7084247185). Branche `agent/audio-382-sfx-published-window`, base exacte `main=d9896fcf5f07ab8d41935565002792b608a41333`. Aucun résultat matériel inventé.

## Observation et mécanisme ciblé

Sur le vrai CYD, Sector 1: `[SFXGEN] DROP id=5069 reason=reader-caught-up mixed=17058/20286`: **3228 frames non mixées (~146ms)** alors que `M_E1M3.wav` termine avec **0 underrun et 0 recovery**. RC11/RC12 ont aussi vu des drops 5046/5099 et, séparément, des underruns musicaux. Ne pas attribuer ces défauts à une même cause sans mesure.

Le producteur générique mixait au plus **8192 frames** par appel, même si une plus grande plage de PCM musical futur était *déjà publiée* dans le ring partagé de **16384 frames**. Une longue animation pouvait ensuite laisser une tranche non mixée sous la tête de lecture DAC. Le garde `reader-caught-up` rejetait correctement le reste, mais un plafond logiciel arbitraire contribuait à cette fenêtre de risque.

## Correction P1: minimale, à vérifier sur CYD

`EspNativeSfxPcm_publishedSpan()` calcule `min(writer-first, framesRestantes)` sans plafond artificiel de 8192, refuse tout dépassement de la capacité physique du ring, et laisse intact le **garde vivant de 128 frames** avant écriture PCM. Appel réel dans `mixGenericSfx()`. Aucune allocation, seconde queue, tâche, lecteur SD/PAK, copie SFX map-wide, ISR ou cadence modifiée. Ring 16KiB constant, décodeur PCM PAK flash par tranches 512 octets, musique ADPCM prioritaire conservée.

**Risque à mesurer**: mixer davantage de samples PCM lors d'un même passage peut augmenter le temps du producteur et retarder le service musical. Ce P1 ne garantit pas 0 DROP sous toute charge et ne corrige pas les 852/513 underruns musicaux historiques.

## Validation matérielle du 23/11/2026 — PASS des témoins SFX observés

L'utilisateur a confirmé **le 24/11/2026** être sur `agent/audio-382-sfx-published-window` (tracking `origin/agent/audio-382-sfx-published-window`) lors de ce run. Le Serial ne donne **ni SHA ELF ni identifiant de branche** : l'association au code candidat `348bc71ea54a1881d8ddeed78c0b575766d45962` repose sur cette confirmation, et non sur un fingerprint compilé. Environnement revendiqué comme profil normal `esp32-cyd` lors du contrôle de test.

Capture `platformio-device-monitor-261123-235648.log`, **9223 lignes / 1138450 octets**, SHA256 `e74ff1c1c134ca9c9ea510939dc504d1d26cbe8cb58190686d9552157c2558f4`. LOAD V11 slot01 -> Junction -> Sector 1.

- **SFX:** `[SFXGEN] 130 START / 130 END / 0 DROP`. Témoins `5069: 12/12 END (20286 frames)`, `5099: 2/2 END (19404 frames)`, `5046: 2/2 END (11466 frames)` via mix générique. Aucun `SFXP1/SFXP2 DROP/PARTIAL`, ni `AUDIOINTENT OVERFLOW` dans la capture. Les deux `5046 STREAM-FALLBACK` sont suivis d'un `SFXGEN END`, et ne comptent pas comme pertes.
- **Musique:** `M_DM2TTL.wav` et `M_DEAD.wav` ont tous deux `FINAL underruns=0 recoveries=0` (lignes 179, 1724). Dernier rapport `M_E1M3.wav` ligne **9188**, à **651137 ms / 14338656 frames / 2 loops**, `underflows=0 recoveries=0 maxSilentFrames=0 minQueued=12728`; `maxPublishGapMs=534 gapsOver500Ms=2 maxSdReadUs=4351 slowReadsOver20ms=0`. La musique ne décroche pas dans cette capture malgré quelques longs intervalles de publication.
- **RAM:** `HEADROOM_OK margin8=18680` au budget Sector 1 (ligne 2927), dernier `ALIVE heap8=50576 largest8=32756` ligne 9202. Aucun panic, WDT ni erreur d'allocation relevé.
- **Limites:** capture arrêtée pendant gameplay (ligne 9223). **Pas de FINAL ni AUDIT de Sector 1**, pas de sortie EXIT->MENU et **aucun retour auditif/latence explicite** du testeur à ce stade. La réussite observée ne prouve pas que *tout* scénario chargé passera, ni ne clôt les anciens underruns #382.

**Statut matériel:** `PASS` pour les critères mesurés dans cette session (SFX 5046/5099/5069 complets, musique sans sous-alimentation observée, RAM saine). **PENDING** pour l'appréciation auditive/tactile et le cycle EXIT. Le correctif est candidat au merge une fois ces points confirmés; garder la PR DRAFT et ne pas fusionner sans décision utilisateur. Les deux CI initiales sur le SHA code `348bc71` sont `SUCCESS` (push et pull_request).

## Oracle et critères matériel

`python ESP32/test/audio_sfx_382_published_window_host.py` compile le **helper de production C**, simule les trois tailles réelles 5046=11466, 5099=19404, 5069=20286 frames, un ring de 16384, un initial `queued=12500`, une latence illustrative **420ms**, trois voix concurrentes et le refill musical borné existant. L'ancien cap conduisait à une collision avec le lecteur après la pause; les trois voix atteignent leur dernière frame avec la nouvelle politique. Test de fenêtre invalide/non publiée et wrap uint32. Ce n'est pas un oracle du mix complet PAK, du CPU ni du CYD.

**Test matériel demandé (environnement normal `esp32-cyd`, sans modifier SD/PAK):** trajet de combat Sector 1 reproduisant le SFX **5069**; laisser `M_E1M3.wav` jouer, conserver Serial complet, noter la perception auditive. Chercher `[SFXGEN] START/END/DROP id=5069`, progression totale `20286/20286`, `[AUDIOINTENT] AUDIT audibleSfxDropped=0 sfxRejectedTrack=0`, `[ENTRANCEMUSIC] STREAM/FINAL underruns=0 recoveries=0`, `HEADROOM_OK`, aucune panique/WDT; comparer impression de latence et fluidité tactile. Vérifier également EXIT et retour menu. Si une autre cause apparaît, conserver le témoin dans #382 et ouvrir P2 ciblé plutôt que de masquer le DROP.

**État**: CYD LOG PASS sur 130/130 SFX et musique sans underflow observé; appréciation auditive et EXIT PENDING. Fusion uniquement après décision utilisateur. Ne pas confondre avec résolution totale de #382.
