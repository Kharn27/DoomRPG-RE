# GAME-AI-14 A4-P3 — continuation player sentinel (pending host/CYD)

Stacked on #468 docs-only PASS 1f4e5d25d41ab32603a3228bfbd647a7a91e4342. Main stays untouched; merge #468 first, then retarget this PR to main. Original Game_trace checks player destination tile on every visited cell if mask & 0x100, including source. First planner already protects destination; continuation lacked any explicit sentinel. Even if adjacent shortcut prevents many such cases, hypothetical calcPath can score a path through the player. Add settled player preflight and source/destination rejection in continuation only, no allocations or retained state. Attack masks without 0x100 unchanged; fail closed if player unavailable or unsettled. Trace cache already scoped per plan. Source-line exception and other first-planner rules unchanged.

Host: preserve A4-P2 4480 plane cases, add 48 actual Game_trace differential sentinel scenarios (4 directions × 3 player locations × 2 masks × 2 coordinate conventions), three not-ready/unsettled checks and one mask-excluded check. Historical SDL-F26 exact reversal checked against parent. Pending CI, no local firmware build.

CYD after CI: repeat Sector1 save slot05, keep sprite202 alive, move/back/pass adjacent to Phantom, verify no monster shares player tile, attack shortcut and ordinary movement remain functional, send Serial. Scope only continuation player sentinel; no full legacy parity or new save claim.

## GAME-AI-14 A4-P3 — continuation player sentinel, PASS CYD non-régression (26 novembre 2026)

PR #469, candidat `6407ae825a828b2f344653ab1e1bb62def49a5f8` (attribution contextuelle, SHA absent du Serial), oracle 4480 plane cases + 48 sentinel Game_trace differential + 3 failClosed + 1 maskExcluded PASS, build classic CYD PASS in both jobs; push CI 45898504525 SUCCESS, PR CI 45898507499 in_progress/pending. Capture `platformio-device-monitor-261126-223127.log`, 237614 B, SHA256 `afac90dc7a99e4771d09684aa51195e231351f6c3c0000a5069258d8823b853b`. Sector1 slot05 V11: 26/26 tours, 173 membres délivrés, 26 chaînes (21 sprite83, 5 sprite202), 50/50 continuation plans/commits, 25 fins à trois goals + 1 attaque anticipée (l.921–955, 3 shots, RNG rollback/commit), zéro DEFER continuation. 0/50 destTile égale tile du joueur calculée depuis target= du PLAN. 28 AI14PLANE, 244 clear/13 blocked (sprite183 16, sprite202 12). Pas de compteur sentinel spécifique : preuve de la protection par oracle, CYD non-régression et absence d'overlap observé seulement. 20 NO-MOVE et 21 subtype10 defer historiques. Heap8 preaudio 68988, marge 11644, NULL invariants, 0 underruns/queueOverflow/audibleSfxDropped, 3 handoffDiscarded menu, saveWrite=no. Aucun nouveau SAVE, subtype13 non testé. Aucun changement code après test.

---

