"""GAME-PICKUP: strict inverse for frozen SDL Git-blob history oracles.

These helpers operate only on in-memory test sources. They are deliberately
anchored to the exact additive pickup-redraw change. Production source, SHA
baselines, asset ownership and gameplay are never rewritten.
"""

def _as_text(source):
    binary = isinstance(source, bytes)
    return (source.decode("utf-8") if binary else source), binary

def _restore_type(source, binary):
    return source.encode("utf-8") if binary else source

def _remove(source, value):
    assert source.count(value) == 1, ("GAME-PICKUP history token drift", value)
    return source.replace(value, "", 1)

def _span(source, first, last, replacement):
    assert source.count(first) == 1 and source.count(last) == 1, (
        "GAME-PICKUP historical span drift", first, last)
    a = source.index(first)
    b = source.index(last, a)
    assert a < b, ("GAME-PICKUP historical span order", first, last)
    return source[:a] + replacement + source[b + len(last):]

def restore_pre_game_pickup_resources(source):
    source, binary = _as_text(source)
    for addition in (
        "#include <esp_timer.h>\n",
        '#include "esp_native_gameplay_pickup_redraw.h"\n',
        "    EspPickupRedraw redraw;\n",
        "        memset(&resources.redraw, 0, sizeof(resources.redraw));\n",
        "    int syncRedraw = 0;\n",
    ):
        source = _remove(source, addition)
    source = _span(
        source,
        "    /*\n     * A normal MOVE has already published its canonical destination frame.",
        "        /* Cosmetic FIFO is committed AFTER the semantic world transaction.",
        '    if (rerender(doomRpg, afterView, "RESOURCE-PICKUP")) {\n'
        '        /* Cosmetic FIFO is committed AFTER the required world redraw.',
    )
    source = _span(
        source,
        "/*\n * A successful full settled WORLD frame is authoritative.",
        "void EspNativeGameplayPlayerResources_sessionReset(void) {",
        "void EspNativeGameplayPlayerResources_sessionReset(void) {",
    )
    # PR384 follow-up removes only high-rate pickup printf traffic from the
    # normal 115200-baud CYD profile. The old body must remain reconstructible
    # byte-for-byte in this historical test.
    source = _remove(source, (
        "        /* Keep normal CYD Serial output short: on a 115200-baud monitor,\n"
        "         * a few KB per pickup can materially delay successive moves. The\n"
        "         * full legacy diagnostic contract remains in DEBUG/TRACE builds. */\n"
        "#if DOOMRPG_LOG_LEVEL < DOOMRPG_LOG_DEBUG\n"
        "        printf(\"[PLAYERRES] COMMIT tile=%u count=%u action=%s player=%08x hp=%u/%u armor=%u/%u credits=%u rollback=closed\\n\",\n"
        "               (unsigned int)afterTile,\n"
        "               (unsigned int)appliedCount,\n"
        "               actionName(applied[0].action),\n"
        "               (unsigned int)playerFNVAfter,\n"
        "               (unsigned int)EspNativeGameplayPlayerState_health(),\n"
        "               (unsigned int)EspNativeGameplayPlayerState_maxHealth(),\n"
        "               (unsigned int)EspNativeGameplayPlayerState_armor(),\n"
        "               (unsigned int)EspNativeGameplayPlayerState_maxArmor(),\n"
        "               (unsigned int)EspNativeGameplayPlayerState_view()->credits);\n"
        "#endif\n"
    ))
    source = _remove(source, (
        "#if DOOMRPG_LOG_LEVEL < DOOMRPG_LOG_DEBUG\n"
        "        printf(\"[PLAYERRES] FEEDBACK tile=%u message=\\\"%s\\\"\\n\",\n"
        "               (unsigned int)afterTile, pickupMessages[0]);\n"
        "        (void)dialogOpened;\n"
        "        (void)pickupMessageStart;\n"
        "        (void)legacyMessagesShifted;\n"
        "#endif\n"
    ))
    for prefix in (
        "        DRPG_LOGD(\"[PLAYERRES] PREPARE",
        "            DRPG_LOGD(\"[PICKUPMINI] SOURCE",
        "        DRPG_LOGD(\"[PLAYERRES] COMMIT",
        "        DRPG_LOGD(\"[PLAYERRES] FEEDBACK tile=%u message",
    ):
        assert source.count(prefix) == 1, ("GAME-PICKUP diagnostic drift", prefix)
        source = source.replace(prefix, prefix.replace("DRPG_LOGD", "printf"), 1)
    return _restore_type(source, binary)

def restore_pre_game_pickup_resident(source):
    source, binary = _as_text(source)
    source = _remove(source, '#include "esp_native_gameplay_player_resources.h"\n')
    source = _remove(source, (
        "    /* Only the resident settled WORLD publisher can acknowledge that its\n"
        "     * canonical image already contains the latest committed pickups. This\n"
        "     * avoids linking PlayerResources into standalone compositor host tests. */\n"
        "    if (frame.active == 1U && frame.finalPresented == 1U &&\n"
        "        !automapActive()) {\n"
        "        EspNativeGameplayPlayerResources_noteWorldPresented();\n"
        "    }\n\n"
    ))
    settled = (
        "        if (!EspNativeGameplayControls_isActive() &&\n"
        "            !EspNativeGameplayPresentGate_isArmed() &&\n"
        "            !EspAssetPack_isOpen() &&\n"
        "            EspNativeSpriteRenderer_ambientDue(nowMs)) {"
    )
    source = _span(
        source,
        "        /* The pickup has already committed its resource/world transaction.",
        settled, settled,
    )
    return _restore_type(source, binary)
