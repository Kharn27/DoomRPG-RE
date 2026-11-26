"""Exact A4-P2 reversal, before A4-P1 and older PERF/SDL history."""

def restore_first(src: bytes) -> bytes:
    pairs = (
        (b"    if (originPlaneClear != 0U || originPlaneBlocked != 0U) {\n        printf(\"[AI14PLANE] planner=first sprite=%u goal=%u originClear=%u originBlocked=%u planStatus=%d\\n\",\n               (unsigned int)candidate.monster->spriteIndex, (unsigned int)1U,\n               (unsigned int)originPlaneClear, (unsigned int)originPlaneBlocked, planStatus);\n    }\n    if (planStatus == -3 || probeRng.boundary != 0U) {",
         b"    if (planStatus == -3 || probeRng.boundary != 0U) {", 1),
        (b"    originPlaneClear = originPlaneBlocked = 0U;\n    planStatus = aiGoalPlan(candidate.monster->spriteIndex,",
         b"    planStatus = aiGoalPlan(candidate.monster->spriteIndex,", 1),
        (b"                                                strictSpecial);\n            if (specialBlocks < 0) return -1;\n            if (strictSpecial) {\n                if (specialBlocks) ++originPlaneBlocked;\n                else ++originPlaneClear;\n            }",
         b"                                                strictSpecial);\n            if (specialBlocks < 0) return -1;", 2),
        (b"static EspNativeGameplayMonsterMovementView movementView;\n/* Per-plan origin-plane witness; no retained world state. */\nstatic uint16_t originPlaneClear, originPlaneBlocked;",
         b"static EspNativeGameplayMonsterMovementView movementView;", 1),
        (b"clear = cardinalTraceClear(worldX, worldY, worldDestX, worldDestY,\n                                       attackerSprite, mask, 1);",
         b"clear = cardinalTraceClear(worldX, worldY, worldDestX, worldDestY,\n                                       attackerSprite, mask, 0);", 1),
        (b"    if (distance == 0U || distance > 31U) return 0;\n    if (strictSpecial && distance != 1U) return -2;",
         b"    if (distance == 0U || distance > 31U) return 0;", 1),
        (b"            specialBlocks = specialEntityBlocks(i, sourceX, sourceY, destX, destY,\n                                                strictSpecial);",
         b"            if (strictSpecial) return -2;\n            specialBlocks = specialEntityBlocks(i, sourceX, sourceY, destX, destY);", 2),
        (b"/* Return 1 blocked, 0 clear, -1 not ready; calcPath uses tile origins. */",
         b"/* Return 1 blocked, 0 clear, -1 not ready, -2 deliberately unsupported special\n * crossing during calcPath's tile-origin legacy subpath. */", 1),
        (b"    sprY = (int32_t)sprite.y;\n    /* calcPath uses tile origins, not centers, for special-plane crossings. */\n    if (tileOrigin) {\n        sourceX -= MOVE_TILE_CENTER;\n        sourceY -= MOVE_TILE_CENTER;\n        destX -= MOVE_TILE_CENTER;\n        destY -= MOVE_TILE_CENTER;\n    }",
         b"    sprY = (int32_t)sprite.y;", 1),
        (b"                               int32_t destY,\n                               int tileOrigin) {\n    EspMapSprite sprite;",
         b"                               int32_t destY) {\n    EspMapSprite sprite;", 1),
    )
    for added, original, count in pairs:
        assert src.count(added) == count, ("A4-P2 reverse drift", added[:100])
        src = src.replace(added, original)
    return src


def restore_continuation_witness(src: bytes) -> bytes:
    pairs = (
        (b"    if (originPlaneClear != 0U || originPlaneBlocked != 0U) {\n        printf(\"[AI14PLANE] planner=continuation sprite=%u goal=%u originClear=%u originBlocked=%u planStatus=%d\\n\",\n               (unsigned int)spriteIndex, (unsigned int)goalStep,\n               (unsigned int)originPlaneClear, (unsigned int)originPlaneBlocked, planStatus);\n    }\n    if (planStatus == -3 || probeRng.boundary != 0U) {",
         b"    if (planStatus == -3 || probeRng.boundary != 0U) {", 1),
        (b"    originPlaneClear = originPlaneBlocked = 0U;\n    planStatus = aiGoalPlan(spriteIndex, monster->subtype,",
         b"    planStatus = aiGoalPlan(spriteIndex, monster->subtype,", 1),
        (b"                                                strictSpecial);\n            if (specialBlocks < 0) return -1;\n            if (strictSpecial) {\n                if (specialBlocks) ++originPlaneBlocked;\n                else ++originPlaneClear;\n            }",
         b"                                                strictSpecial);\n            if (specialBlocks < 0) return -1;", 2),
        (b"static EspNativeGameplayMonsterThreeGoalTurnView threeGoalView;\n/* Per-plan origin-plane witness; no retained world state. */\nstatic uint16_t originPlaneClear, originPlaneBlocked;",
         b"static EspNativeGameplayMonsterThreeGoalTurnView threeGoalView;", 1),
    )
    for added, original, count in pairs:
        assert src.count(added) == count, ("A4-P2 reverse drift", added[:100])
        src = src.replace(added, original)
    return src
