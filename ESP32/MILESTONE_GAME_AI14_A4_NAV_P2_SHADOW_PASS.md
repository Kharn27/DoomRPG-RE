# GAME-AI-14 A4-NAV-P2 — CYD shadow PASS (bounded)
 
- Tested candidate: d72766c76a8ab325096d8db6afa5d8af58519fe1 (contextual; not embedded).
- Capture: platformio-device-monitor-261128-120356.log, 162115 bytes, 1157 splitlines, SHA256 a15b36a66691df6350243eeefc04cc1b7179fa8dd8a263dd5b296364a0e7c73f.
- Alternate-panel CYD, Sector 1 slot05, arena 82d2ee25. Both candidate CI runs passed.
- At turn 8: 904 cardinal queries, 96 cache hit comparisons, 1570 sprite checks, 1573 line checks, zero mismatches; 52 line index builds, zero failures. All modes shadow. These are cumulative sampled counters, not counts for the entire capture.
- 15/15 ordered turns completed, 22/22 three-goal chains completed (11 each sprites 83/202), 44/44 continuation commits, no continuation DEFER. All 22 chains reverse at least once, 12 ABAB, no Phantom attack. Subtype10 defers are existing separate scope. Return to menu without writing checkpoint.
- No panic. Native-only owners preserved. This proves optimized vs native reference parity for sampled queries only, not original C parity or a navigation fix.
- Restore fast defaults. P3 captures bounded radius-three live topology, line overlay and block flags outside goal timing; no AI policy changes. Geometry records are native observations and must be compared with original rules. Capture caps: four plans per planner/session, max 64 lines and 64 linked sprites per sample, explicit errors/truncation.
