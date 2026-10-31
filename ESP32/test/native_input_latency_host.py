#!/usr/bin/env python3
"""Source-level regression guard for the input/feedback ownership boundary."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
r = (root / "src/esp_native_resident_gameplay.c").read_text()
c = (root / "src/esp_native_gameplay_controls.c").read_text()
a = (root / "src/esp_native_gameplay_action_engine.c").read_text()
s = r.split("void EspNativeResidentGameplay_service(", 1)
assert len(s) == 2
s = s[1]
assert "const int inputReady = pending != NULL && pending->pending != 0U;" in s
assert "if (!inputReady && !EspNativeGameplayControls_isExpired()) return;" in s
assert s.index("EspNativeGameplayControls_restore(1, &feedbackStats)") < s.index("EspNativeGameplayInput_consume(&intent)")
assert "EspNativeGameplayControls_restore(0, NULL)" in r
assert "EspNativeGameplayMonsterAttackVisual_isBusy()" in s
assert "EspNativeGameplayMonsterActiveSequence_isBusy()" in s
assert "EspNativeGameplayEventChain_isWaiting()" in s
assert "if (framebuffer[offset] == painted)" in c
assert "feedback.expiresMs = nowMs() + ESP_NATIVE_GAMEPLAY_FEEDBACK_MS;" in c
assert "if (EspNativeGameplayControls_isActive())" in a
assert "viewportFeedbackBlinkLit(elapsed)" in a
print("native_input_latency_host: PASS no 120ms pending-action wait, journal and combat/modal guards kept")
