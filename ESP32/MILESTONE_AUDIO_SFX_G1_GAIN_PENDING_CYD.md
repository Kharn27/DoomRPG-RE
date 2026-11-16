# AUDIO-SFX-G1 — légère hausse du volume SFX relatif (CYD PENDING)

**16 novembre 2026 — issue [#261](https://github.com/Kharn27/DoomRPG-RE/issues/261).** Demande utilisateur après validation P3 sur le vrai classic CYD : « J'augmenterai bien encore un peu le volume SFX par rapport à la musique ». Toujours **une seule branche / PR active**, [#358](https://github.com/Kharn27/DoomRPG-RE/pull/358), `agent/audio-sfx-p3-original-wav-catalog`.

## Baseline matérielle confirmée

Le probe P3 exécuté sur `esp32-cyd`, source `45668f16804126632ff89f81ac572cc0a2414ff5`, log `platformio-device-monitor-261116-211513.log`, confirme les **92/92 WAV PCM16 mono 8000 Hz**, aucun invalid, delta heap8=0. Les six `MIX` 5046/5060 fonctionnent avec une musique ayant 0 underrun/0 recovery sur les 51 secondes observées. La capture s'arrête pendant le gameplay : pas de test de sortie. Le PASS matériel concerne **P3 uniquement**, pas G1.

## Modification G1, bornée

Les trois seuls effets jouables sont augmentés nominalement de **8/7 (14,3 % d'amplitude, ≈1,16 dB)** en multipliant leurs diviseurs P2 par 7/8 :

| Son | Avant P2 | Nouveau G1 | Effet |
|---|---:|---:|---|
| `5046` pistolet / caisse | 320 | **280** | ×8/7 |
| `5060` ramassage | 128 | **112** | ×8/7 |
| `5045` extincteur | 256 | **224** | ×8/7 |

Le gain est **seulement dans `EspNativeSfxPcm_gainDiv()`**, calculé lors de l'injection SFX en main task ; le PCM musical, le volume musique, le soft-knee, le timer DAC2, l'ISR, le ring 16 KiB, les sources WAV/PAK/SD et le gameplay sont **inchangés**. Le clamp sécurité DAC **16..240** reste actif ; davantage d'échantillons peuvent atteindre le rail. Les IDs non jouables (`5081` dégâts joueur, autres monstres) restent exclus de la whitelist et gardent leur diviseur de repli **512** ; G1 **ne les rend pas audibles**.

### Validations

- Tests host C11 P2 remis aux diviseurs actuels ; test G1 supplémentaire contrôlant les rapports 7/8, le clamp sur des milliers de combinaisons PCM et l'absence d'appel au mixer depuis l'ISR, intégré au workflow CI.
- Builds et audits multi-profils `esp32-cyd`, `esp32-cyd-inverted`, trace et bringup via GitHub CI ; ne déclarer PASS qu'après résultats.
- **Test utilisateur restant** : flasher l'env normal `esp32-cyd`, écouter pistolet/ramassage/extincteur à volume musique identique ; vérifier que c'est mieux équilibré **sans son écrêté désagréable**, et les diagnostics `[SFXP1] MIX`, `[SFXP2] MIX id=5045 totalMixed=17640`, `[ENTRANCEMUSIC] STREAM underruns=0`, `[SFXP2] PARTIAL` absent. Une sortie-menu avec `FINAL reason=SESSION_EXIT` est utile si possible.

**Statut G1 : CYD PENDING**. Le P4 des sons ennemis est un jalon séparé après l'intégration validée : 68/83 nouveaux WAV durent >697ms, nécessitant le même modèle de streamer segmenté que l'extincteur.
