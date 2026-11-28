"""Exact inverse of PERF-GAME-P3k normal-profile UART changes."""

def restore_pre_p3k_goal(value: bytes) -> bytes:
    edits = (
        ("#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_LOG_LEVEL < DOOMRPG_LOG_TRACE\n    printf(\"[MONSTER3GOAL] ARM chain=%u sprite=%u subtype=%u firstTile=%u->%u\\n\",\n           (unsigned int)threeGoalView.observedChains,\n           (unsigned int)spriteIndex,\n           (unsigned int)monster->subtype,\n           (unsigned int)sourceTile,\n           (unsigned int)destTile);\n#else\n    printf(\"[MONSTER3GOAL] ARM chain=%u sprite=%u subtype=%u firstTile=%u->%u legacyGoalCount=3 frameTime=0 firstGoalAlreadyCommitted=yes continuationGoals=bounded-2 attackLoops=3-native-probe/simultaneous-fail-closed\\n\",\n           (unsigned int)threeGoalView.observedChains,\n           (unsigned int)spriteIndex,\n           (unsigned int)monster->subtype,\n           (unsigned int)sourceTile,\n           (unsigned int)destTile);\n#endif".encode("utf-8"),
         "    printf(\"[MONSTER3GOAL] ARM chain=%u sprite=%u subtype=%u firstTile=%u->%u legacyGoalCount=3 frameTime=0 firstGoalAlreadyCommitted=yes continuationGoals=bounded-2 attackLoops=3-native-probe/simultaneous-fail-closed\\n\",\n           (unsigned int)threeGoalView.observedChains,\n           (unsigned int)spriteIndex,\n           (unsigned int)monster->subtype,\n           (unsigned int)sourceTile,\n           (unsigned int)destTile);".encode("utf-8")),
        ("#if defined(ARDUINO) && defined(DOOMRPG_ESP32) && DOOMRPG_LOG_LEVEL < DOOMRPG_LOG_TRACE\n                printf(\"[MONSTER3GOAL] COMPLETE sprite=%u subtype=%u goal=3/3 tile=%u distance2=%u sameTurn=yes additionalRng=0\\n\",\n                       (unsigned int)spriteIndex,\n                       (unsigned int)monster->subtype,\n                       (unsigned int)currentDest,\n                       (unsigned int)distance2);\n#else\n                printf(\"[MONSTER3GOAL] COMPLETE sprite=%u subtype=%u frameTime=3 goal=3/3 tile=%u distance2=%u adjacentCardinal=no attack=no movementChain=complete sameTurn=yes mutation=no additionalRng=0\\n\",\n                       (unsigned int)spriteIndex,\n                       (unsigned int)monster->subtype,\n                       (unsigned int)currentDest,\n                       (unsigned int)distance2);\n#endif".encode("utf-8"),
         "                printf(\"[MONSTER3GOAL] COMPLETE sprite=%u subtype=%u frameTime=3 goal=3/3 tile=%u distance2=%u adjacentCardinal=no attack=no movementChain=complete sameTurn=yes mutation=no additionalRng=0\\n\",\n                       (unsigned int)spriteIndex,\n                       (unsigned int)monster->subtype,\n                       (unsigned int)currentDest,\n                       (unsigned int)distance2);".encode("utf-8")),
    )
    for new, old in edits:
        assert value.count(new) == 1, "P3k exact historical inverse anchor"
        value = value.replace(new, old, 1)
    return value


def restore_pre_p3k_sprite(value: bytes) -> bytes:
    edits = (
        ("#if !defined(ARDUINO) || !defined(DOOMRPG_ESP32) || DOOMRPG_LOG_LEVEL >= DOOMRPG_LOG_TRACE\n    printf(\"[NATIVESPRITE] CROSS index=%u id=%u resource=%u copies=4 draws=%u near=%u clip=%u accounting=1\\n\",\n           (unsigned int)parent->index,\n           (unsigned int)parent->logical,\n           (unsigned int)resourceLogical,\n           (unsigned int)subDraws,\n           (unsigned int)subNear,\n           (unsigned int)subClip);\n#else\n    (void)subClip; /* Diagnostic only; logical accounting above is unchanged. */\n#endif".encode("utf-8"),
         "    printf(\"[NATIVESPRITE] CROSS index=%u id=%u resource=%u copies=4 draws=%u near=%u clip=%u accounting=1\\n\",\n           (unsigned int)parent->index,\n           (unsigned int)parent->logical,\n           (unsigned int)resourceLogical,\n           (unsigned int)subDraws,\n           (unsigned int)subNear,\n           (unsigned int)subClip);".encode("utf-8")),
    )
    for new, old in edits:
        assert value.count(new) == 1, "P3k exact historical inverse anchor"
        value = value.replace(new, old, 1)
    return value
