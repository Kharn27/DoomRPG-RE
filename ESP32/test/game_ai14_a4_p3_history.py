"""Reverse A4-P3 source change for historical SDL-F26 witness."""


def restore_pre_a4_p3(src: bytes) -> bytes:
    pairs = (
        (b"        if ((flags & ESP_MAP_TILE_WALL) != 0U) return 0;\n        if (tile == playerTile) return 0;\n        lineBlock = closedLineBlocksTile(tile, mask);",
         b"        if ((flags & ESP_MAP_TILE_WALL) != 0U) return 0;\n        lineBlock = closedLineBlocksTile(tile, mask);", 1),
        (b"            if ((sourceFlags & ESP_MAP_TILE_WALL) != 0U) return 0;\n            if (sourceTile == playerTile) return 0;\n            lineBlock = closedLineBlocksTile(sourceTile, mask);",
         b"            if ((sourceFlags & ESP_MAP_TILE_WALL) != 0U) return 0;\n            lineBlock = closedLineBlocksTile(sourceTile, mask);", 1),
        (b"    if (strictSpecial && distance != 1U) return -2;\n\n    /* Game_trace checks the player sentinel on every visited tile, including\n     * the source cell. Topology contains map entities only, so check the\n     * settled player explicitly for movement masks, never attack LOS. */\n    if ((mask & 0x0100U) != 0U) {\n        const EspPlayerViewState* player = EspPlayerView_view();\n        if (player == NULL || player->active != 1U ||\n            player->viewX != player->destX || player->viewY != player->destY ||\n            !tileIndexFor(player->destX, player->destY, &playerTile)) return -1;\n    }\n\n    for (i = 1U;",
         b"    if (strictSpecial && distance != 1U) return -2;\n\n    for (i = 1U;", 1),
        (b"    uint32_t distance;\n    uint32_t i;\n    uint16_t playerTile = UINT16_MAX;\n\n    if (!centeredCoordinate(sourceX)",
         b"    uint32_t distance;\n    uint32_t i;\n\n    if (!centeredCoordinate(sourceX)", 1),
    )
    for added, original, count in pairs:
        assert src.count(added) == count, ("A4-P3 reverse drift", added[:100])
        src = src.replace(added, original)
    return src
