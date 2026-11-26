"""Exact reversal of #439 A4-P1 for historical SDL-F26 source witness."""


def restore_pre_game_ai14_a4_p1(src: bytes) -> bytes:
    pairs = (
        (b"                               int32_t destY,\n                               int tileOrigin) {\n    EspMapSprite sprite;",
         b"                               int32_t destY) {\n    EspMapSprite sprite;", 1),
        (b"    /* Entity_calcPath passes tile origins (x << 6), unlike the initial\n     * Entity_aiGoal_MOVE center-to-center LOS. Tile membership is unchanged;\n     * the special-plane crossing must use the original endpoints. */\n    if (tileOrigin) {\n        sourceX -= THREEGOAL_TILE_CENTER;\n        sourceY -= THREEGOAL_TILE_CENTER;\n        destX -= THREEGOAL_TILE_CENTER;\n        destY -= THREEGOAL_TILE_CENTER;\n    }\n",
         b"", 1),
        (b"/* Return 1 blocked, 0 clear, -1 not ready. For calcPath's single-tile\n * subtrace, evaluate type14/15 with the legacy tile-origin endpoints. */",
         b"/* Return 1 blocked, 0 clear, -1 not ready, -2 unsupported legacy special\n * crossing. This is the exact continuation-goal subset of the proven movement\n * planner and deliberately keeps the same fail-closed special-plane boundary. */", 1),
        (b"            specialBlocks = specialEntityBlocks(i, sourceX, sourceY, destX, destY,\n                                                strictSpecial);",
         b"            if (strictSpecial) return -2;\n            specialBlocks = specialEntityBlocks(i, sourceX, sourceY, destX, destY);", 2),
        (b"    /* Only calcPath's single cardinal step is proven for tile-origin mode. */\n    if (strictSpecial && distance != 1U) return -2;\n",
         b"", 1),
    )
    for added, original, count in pairs:
        assert src.count(added) == count, ("A4-P1 reverse drift", added[:100])
        src = src.replace(added, original)
    return src
