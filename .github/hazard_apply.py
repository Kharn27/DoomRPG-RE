from pathlib import Path
import re


def replace_one(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected 1 match, got {count}")
    return text.replace(old, new, 1)


action = Path("ESP32/src/esp_native_gameplay_action_engine.c")
s = action.read_text()
s = replace_one(
    s,
    "#define ACTION_FEEDBACK_PICKUP ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PICKUP\n",
    "#define ACTION_FEEDBACK_PICKUP ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PICKUP\n#define ACTION_FEEDBACK_DAMAGE ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE\n",
    "damage feedback define",
)
s = replace_one(
    s,
    "#define FEEDBACK_DYNAMIC_TEXT_BYTES 24U\n",
    "#define FEEDBACK_DYNAMIC_TEXT_BYTES 24U\n#define FEEDBACK_DAMAGE_RED565 0xb800U\n",
    "damage red define",
)
s = replace_one(
    s,
    "    uint16_t viewportFlashDurationMs;\n    uint8_t viewportFlashPending;\n",
    "    uint16_t viewportFlashDurationMs;\n    uint16_t viewportFlashColor565;\n    uint8_t viewportFlashPending;\n",
    "flash color owner",
)
s = replace_one(
    s,
    "    if (feedback != ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PICKUP || text == NULL ||\n",
    "    if ((feedback != ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_PICKUP &&\n         feedback != ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE) || text == NULL ||\n",
    "dynamic feedback gate",
)
s = replace_one(
    s,
    "    if (viewportFlashMs != 0U) {\n        actionState.viewportFlashPending = 1U;\n        actionState.viewportFlashDurationMs = viewportFlashMs;\n    }\n",
    "    if (viewportFlashMs != 0U) {\n        actionState.viewportFlashPending = 1U;\n        actionState.viewportFlashDurationMs = viewportFlashMs;\n        actionState.viewportFlashColor565 =\n            feedback == ESP_NATIVE_GAMEPLAY_ACTION_FEEDBACK_DAMAGE\n                ? FEEDBACK_DAMAGE_RED565 : 0xffffU;\n    }\n",
    "dynamic flash color",
)
s = replace_one(
    s,
    "    if (actionState.viewportFlashVisible == 0U) {\n        actionState.viewportFlashDurationMs = 0U;\n    }\n",
    "    if (actionState.viewportFlashVisible == 0U) {\n        actionState.viewportFlashDurationMs = 0U;\n        actionState.viewportFlashColor565 = 0U;\n    }\n",
    "cancel flash color",
)
s = replace_one(
    s,
    "    if (feedback == ACTION_FEEDBACK_PICKUP && actionState.feedbackText[0] != '\\0') {\n",
    "    if ((feedback == ACTION_FEEDBACK_PICKUP ||\n         feedback == ACTION_FEEDBACK_DAMAGE) &&\n        actionState.feedbackText[0] != '\\0') {\n",
    "dynamic feedback text",
)

s, n = re.subn(
    r"static int visitViewportBorder\(uint16_t\* framebuffer,\s*int restore\) \{",
    "static int visitViewportBorder(uint16_t* framebuffer,\n                               int restore,\n                               uint16_t flashColor) {",
    s,
    count=1,
)
if n != 1:
    raise SystemExit(f"visitViewportBorder signature: {n}")
count = s.count("framebuffer[index] = 0xffffU;")
if count != 4:
    raise SystemExit(f"border paint sites: expected 4, got {count}")
s = s.replace("framebuffer[index] = 0xffffU;", "framebuffer[index] = flashColor;")
s = replace_one(
    s,
    "    if (!visitViewportBorder(framebuffer, 0)) return 0;\n",
    "    if (!visitViewportBorder(framebuffer, 0,\n                            actionState.viewportFlashColor565 != 0U\n                                ? actionState.viewportFlashColor565 : 0xffffU)) return 0;\n",
    "paint flash color",
)
s = replace_one(
    s,
    "        !visitViewportBorder(framebuffer, 1)) {\n",
    "        !visitViewportBorder(framebuffer, 1, 0U)) {\n",
    "restore flash signature",
)
s = s.replace("[PICKUPFLASH] FAILED phase=paint", "[VIEWFLASH] FAILED phase=paint")
s = s.replace("[PICKUPFLASH] FAILED phase=restore", "[VIEWFLASH] FAILED phase=restore")

old_paint = (
    '        printf("[PICKUPFLASH] PAINT color=white565/ffff viewport=0,%u,%u,%u thickness=%u pixels=%u durationMs=%u snapshot=bounded present=caller\\n",\n'
    '     (unsigned int)FEEDBACK_VIEW_Y,\n'
    '     (unsigned int)DOOMRPG_LOGICAL_WIDTH,\n'
    '     (unsigned int)FEEDBACK_VIEW_HEIGHT,\n'
    '     (unsigned int)FEEDBACK_BORDER_THICKNESS,\n'
    '     (unsigned int)FEEDBACK_BORDER_PIXELS,\n'
    '     (unsigned int)actionState.viewportFlashDurationMs);\n'
)
new_paint = (
    '        printf("[VIEWFLASH] PAINT color565=%04x viewport=0,%u,%u,%u thickness=%u pixels=%u durationMs=%u snapshot=bounded present=caller feedback=%u\\n",\n'
    '     (unsigned int)actionState.viewportFlashColor565,\n'
    '     (unsigned int)FEEDBACK_VIEW_Y,\n'
    '     (unsigned int)DOOMRPG_LOGICAL_WIDTH,\n'
    '     (unsigned int)FEEDBACK_VIEW_HEIGHT,\n'
    '     (unsigned int)FEEDBACK_BORDER_THICKNESS,\n'
    '     (unsigned int)FEEDBACK_BORDER_PIXELS,\n'
    '     (unsigned int)actionState.viewportFlashDurationMs,\n'
    '     (unsigned int)feedback);\n'
)
s = replace_one(s, old_paint, new_paint, "flash paint log")
old_expire = (
    "    actionState.viewportFlashVisible = 0U;\n"
    "    actionState.viewportFlashShownAtMs = 0U;\n"
    "    actionState.viewportFlashDurationMs = 0U;\n"
    '    printf("[PICKUPFLASH] EXPIRE elapsedMs=%u targetMs=%u restored=viewport-border-only\\n",\n'
    " (unsigned int)elapsed, (unsigned int)duration);\n"
)
new_expire = (
    '    printf("[VIEWFLASH] EXPIRE elapsedMs=%u targetMs=%u color565=%04x restored=viewport-border-only\\n",\n'
    " (unsigned int)elapsed, (unsigned int)duration,\n"
    " (unsigned int)actionState.viewportFlashColor565);\n"
    "    actionState.viewportFlashVisible = 0U;\n"
    "    actionState.viewportFlashShownAtMs = 0U;\n"
    "    actionState.viewportFlashDurationMs = 0U;\n"
    "    actionState.viewportFlashColor565 = 0U;\n"
)
s = replace_one(s, old_expire, new_expire, "flash expire log")
action.write_text(s)

resources = Path("ESP32/src/esp_native_gameplay_player_resources.c")
r = resources.read_text()
r = replace_one(
    r,
    '#include "esp_native_gameplay_frame.h"\n#include "esp_native_gameplay_hud.h"\n',
    '#include "esp_native_gameplay_frame.h"\n#include "esp_native_gameplay_hazard_touch.h"\n#include "esp_native_gameplay_hud.h"\n',
    "player resources include anchor",
)
anchor = (
    '    if (!ensureOwner(afterView->targetMapId)) {\n'
    '        printf("[PLAYERRES] DEFER tile=%u reason=owner-not-ready mutation=no\\n",\n'
    '               (unsigned int)afterTile);\n'
    '        return 1;\n'
    '    }\n'
    '    memset(candidates, 0, sizeof(candidates));\n'
)
replacement = (
    '    if (!ensureOwner(afterView->targetMapId)) {\n'
    '        printf("[PLAYERRES] DEFER tile=%u reason=owner-not-ready mutation=no\\n",\n'
    '               (unsigned int)afterTile);\n'
    '        return 1;\n'
    '    }\n'
    '    {\n'
    '        EspNativeGameplayHazardTouchStatus hazardStatus =\n'
    '            EspNativeGameplayHazardTouch_processMove(doomRpg, beforeView, afterView);\n'
    '        if (hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_FATAL) {\n'
    '            resources.view.fatal = 1U;\n'
    '            return 0;\n'
    '        }\n'
    '        if (hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_COMMITTED ||\n'
    '            hazardStatus == ESP_NATIVE_GAMEPLAY_HAZARD_TOUCH_DEFERRED) {\n'
    '            return 1;\n'
    '        }\n'
    '    }\n'
    '    memset(candidates, 0, sizeof(candidates));\n'
)
r = replace_one(r, anchor, replacement, "player resources hazard anchor")
resources.write_text(r)

Path(".github/workflows/temp-hazard-touch-apply.yml").unlink()
Path(".github/hazard_apply.py").unlink()
