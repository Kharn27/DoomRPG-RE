# PR #241 — input réactif et pickup blanc pulsé (CANDIDAT)

Base SHA exact : `c23d8925ab0b56f2752e23f9f6c344341d931f31` sur la PR #240 encore ouverte. PR empilée pour garder séparés le retrait de champs Render et le changement de rythme. Le log matériel `device-monitor-261031-021905.log` valide #240 (Render 1332, CORE 2300, 2 sorties propres) mais le ressenti Malwrath reste à améliorer. Le cadre blanc `blink=steady` n'interdisait pas à lui seul les mouvements pendant 500ms.

Ce jalon supprime **l'attente systématique de 120ms** avant dispatch d'un tap prêt : le service restaure et présente d'abord le journal tactile exact, puis consomme la commande sans cooldown. Un tap ultérieur admis remplace l'ancien snapshot. Si aucune commande n'attend, le feedback expire normalement à 120ms. Les gardes combat, script WAIT et modales restent strictes.

Le flash pickup 2px blanc partage les **trois pulses 105ms ON / 65ms OFF** du cadre rouge, sur l'horloge inchangée de 500ms. Le tint directionnel demeure rouge-only. Message de pickup topbar toujours 1200ms. Aucun nouveau timer, mémoire, RNG, mutation de monde, tweak de combat/projectile ou optimisation de PlatformVideo_present().

Tests : vrai compositeur dans `damage_cue_timing_host.c` étendu aux pulses blancs et exact restore, source guard `native_input_latency_host.py`, ancien host `hud_weapon_feedback_host.c`. Attendre CI push normal/bringup/trace/host, puis tester clean/flash normal `esp32-cyd` : pickups et FORWARD rapides, LOAD Malwrath 3 projectiles puis réaction, START intro/dialogue/EXIT et LOAD/EXIT, relever `VIEWFLASH BLINK`, `QUEUE/MOVE`, RAM, FNV et erreurs Serial.

**CANDIDAT — aucun PASS matériel ni promesse de FPS, et aucune fusion automatique.** Merger #240 avant de retargeter la PR empilée sur main.
