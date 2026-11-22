"""Strict reversible GAME-S1 subtype10 P1 delta for frozen source oracles.

Each hunk is inverted verbatim once. Both the P1 production source and the
restored main source are pinned by their Git blob SHA. No old oracle is waived.
The helper is a host-only test utility, never compiled into the firmware.
"""
import hashlib

EXPECTED = {
    "esp_native_gameplay_monster_movement.c": ("72d2ab7ad7b48a50d761adbeaa2d6f24946d12e6", "71adcad9b2e0d48638436755ff02938105bc080c"),
    "esp_native_gameplay_monster_movement_publish.c": ("979cef07b7133f61abd3470fb1248bd0a22c44a0", "0585882c8254a38812adeb7204f5f8d13b9d7d78"),
    "esp_native_gameplay_monster_active_sequence.c": ("2637539668f8a46c37e157772cd73a33101c4b0e", "369339178076eac89e8cc0f7b47d0fb5540619d9"),
    "esp_native_gameplay_monster_turn.c": ("3992a1783f7b5955781b9738a5945b67410eadd8", "3eddfdda0ae1a2ae18d4502bbd8ce65d6d88f978"),
    "esp_native_gameplay_monster_state.c": ("fb4972dd84bd3ab5a934ac1e3381fdff826cbca0", "b8189e585db3658375ca00ecdecbc68e94a0d586"),
}
REVERSE_EDITS = {
    "esp_native_gameplay_monster_movement.c": (
        ("#define MOVE_TYPE_SPECIAL_TRACE 14U\n",
         "#define MOVE_SUBTYPE_SPECIAL_AI 10U\n#define MOVE_TYPE_SPECIAL_TRACE 14U\n"),
        ("        if (monster->alive == 0U || monster->subtype >= 14U ||\n            !EspNativeGameplayMonsterActivation_isActive",
         "        if (monster->alive == 0U || monster->subtype >= 14U ||\n            monster->subtype == MOVE_SUBTYPE_SPECIAL_AI ||\n            !EspNativeGameplayMonsterActivation_isActive"),
    ),
    "esp_native_gameplay_monster_movement_publish.c": (
        ("#define PUBLISH_MOVE_TILE_SIZE", "#define PUBLISH_MOVE_SPECIAL_AI 10U\n#define PUBLISH_MOVE_TILE_SIZE"),
        ("        outCalls == NULL || monster->subtype >= 14U) {",
         "        outCalls == NULL || monster->subtype >= 14U ||\n        monster->subtype == PUBLISH_MOVE_SPECIAL_AI) {"),
    ),
    "esp_native_gameplay_monster_active_sequence.c": (
        ("#include \"esp_native_gameplay_monster_position.h\"\n#include \"esp_native_gameplay_monster_subtype10_policy.h\"\n#include \"esp_native_process_rng_owner.h\"\n#include \"esp_native_rng_replay_guard.h\"\n",
         ""),
        ("\n/* P1 admission: inspect the original single <84 byte without consuming it.\n * When the byte crosses a refill boundary, borrow the already-proven RNG\n * reservation and restore the live Random_t before leaving preflight.\n * This preview deliberately does NOT invent an inactive-monster list order.\n * If at least one viable corpse is found, defer rather than revive the wrong\n * corpse or publish a half-owned weapon18/animation/activation transaction. */\ntypedef enum Subtype10Admission_e {\n    SUBTYPE10_NOT_READY = 0,\n    SUBTYPE10_FALLBACK = 1,\n    SUBTYPE10_RAISE_DEFER = 2\n} Subtype10Admission;\n\nstatic Subtype10Admission subtype10AdmitFallback(\n    uint16_t attackerSprite, uint8_t* outRoll, uint16_t* outCorpse) {\n    Random_t* live = EspNativeProcessRng_live();\n    Random_t saved;\n    uint8_t prepared = 0U;\n    uint8_t roll;\n    const EspNativeGameplayMonsterView* monsters;\n    const EspNativeGameplayMonsterPositionRecord* attackerPos;\n    const EspMapSpriteTopologyView* topology;\n    uint32_t i;\n\n    if (outRoll != NULL) *outRoll = 0U;\n    if (outCorpse != NULL) *outCorpse = ACTIVESEQ_NO_SPRITE;\n    if (live == NULL || outRoll == NULL || outCorpse == NULL ||\n        live->nextRand < 0 || live->nextRand >= RANDTABLESIZE ||\n        !EspNativeRngReplayGuard_beginProbeBoundary(\n            live, &saved, &prepared)) {\n        return SUBTYPE10_NOT_READY;\n    }\n    /* beginProbeBoundary() exposes the reserved new table for this preview. */\n    if (live->nextRand < 0 || live->nextRand >= RANDTABLESIZE) {\n        if (prepared != 0U) {\n            (void)EspNativeRngReplayGuard_endProbeBoundary(\n                live, &saved, prepared);\n        }\n        return SUBTYPE10_NOT_READY;\n    }\n    roll = live->randTable[live->nextRand];\n    if (prepared != 0U &&\n        !EspNativeRngReplayGuard_endProbeBoundary(live, &saved, prepared)) {\n        return SUBTYPE10_NOT_READY;\n    }\n    *outRoll = roll;\n    if (!EspNativeSubtype10_wantsRaise(roll)) {\n        return SUBTYPE10_FALLBACK;\n    }\n\n    monsters = EspNativeGameplayMonsterState_view();\n    topology = EspMapSpriteTopology_view();\n    attackerPos = EspNativeGameplayMonsterPosition_find(attackerSprite);\n    if (monsters == NULL || monsters->records == NULL ||\n        topology == NULL || attackerPos == NULL ||\n        topology->spriteCount > 1024U) {\n        return SUBTYPE10_NOT_READY;\n    }\n\n    for (i = 0U; i < monsters->count; ++i) {\n        const EspNativeGameplayMonsterRecord* corpse = &monsters->records[i];\n        const EspNativeGameplayMonsterPositionRecord* pos;\n        int32_t dx;\n        int32_t dy;\n        uint8_t visual = 0U;\n        uint8_t occupied = 0U;\n        uint32_t j;\n\n        if (corpse->alive != 0U || corpse->spriteIndex == attackerSprite) {\n            continue;\n        }\n        pos = EspNativeGameplayMonsterPosition_find(corpse->spriteIndex);\n        if (pos == NULL || !EspMapSpriteTopology_getVisualState(\n                corpse->spriteIndex, &visual)) {\n            return SUBTYPE10_NOT_READY;\n        }\n        dx = (int32_t)pos->worldX - (int32_t)attackerPos->worldX;\n        dy = (int32_t)pos->worldY - (int32_t)attackerPos->worldY;\n        if (!EspNativeSubtype10_candidateEligible(\n                dx, dy, 1U, 0U, 0U)) {\n            continue;\n        }\n        /* The native renderer knows hidden gib vs corpse, but P1 cannot\n         * establish every legacy 0x1000000/0x2000000 bit (notably after LOAD).\n         * A nearby non-hidden death/corpse must therefore be deferred. */\n        if ((visual & 0x80U) != 0U) continue;\n        if ((visual & 0x0fU) != 2U && (visual & 0x0fU) != 4U) {\n            return SUBTYPE10_NOT_READY;\n        }\n\n        /* Exact 16263 *entity* occupancy mask; links/line overlays not\n         * represented by this topology remain conservatively unsupported.\n         * This is only a negative gate, never authority to resurrect. */\n        for (j = 0U; j < topology->spriteCount; ++j) {\n            uint8_t type = 0U, subtype = 0U;\n            uint16_t linkState = 0U, linkOrder = 0U;\n            if (!EspMapSpriteTopology_getEntity(\n                    j, &type, &subtype, &linkState, &linkOrder)) {\n                return SUBTYPE10_NOT_READY;\n            }\n            if (type < 16U &&\n                (ESP_NATIVE_SUBTYPE10_RAISE_OCCUPANCY_MASK &\n                 (1U << type)) != 0U &&\n                (linkState & ESP_MAP_SPRITE_TOPOLOGY_LINKED) != 0U &&\n                (linkState & ESP_MAP_SPRITE_TOPOLOGY_TILE_MASK) ==\n                    pos->tileIndex) {\n                occupied = 1U;\n                break;\n            }\n        }\n        if (EspNativeSubtype10_candidateEligible(\n                dx, dy, 1U, 0U, occupied)) {\n            *outCorpse = corpse->spriteIndex;\n            return SUBTYPE10_RAISE_DEFER;\n        }\n    }\n    return SUBTYPE10_FALLBACK;\n}\n\n",
         ""),
        ("        uint8_t subtype10Prepared = 0U;\n        uint8_t subtype10OldAlt = 0U;\n        uint8_t subtype10Roll = 0U;\n        uint16_t subtype10Corpse = ACTIVESEQ_NO_SPRITE;\n        Random_t subtype10Before;\n",
         ""),
        ("            Subtype10Admission admission = subtype10AdmitFallback(\n                spriteIndex, &subtype10Roll, &subtype10Corpse);\n            if (admission != SUBTYPE10_FALLBACK) {\n                printf(\"[MONSTERSUB10] %s turn=%u ordinal=%u/%u sprite=%u corpse=%u rollPreview=%u rngConsumed=0 mutation=no reason=%s\\n\",\n                       admission == SUBTYPE10_RAISE_DEFER\n                           ? \"RAISE-DEFER\" : \"DEFER\",\n                       (unsigned int)activeSeq.expandedTurns,\n                       (unsigned int)(ordinal + 1U),\n                       (unsigned int)activationCount,\n                       (unsigned int)spriteIndex,\n                       (unsigned int)subtype10Corpse,\n                       (unsigned int)subtype10Roll,\n                       admission == SUBTYPE10_RAISE_DEFER\n                           ? \"weapon18-revive-transaction-unowned\"\n                           : \"rng-or-corpse-preflight-not-ready\");\n                continue;\n            }\n            /* Legacy consumes this byte BEFORE toggling the alternate\n             * attack flag, even if no inactive monster is reanimable. */\n            subtype10Before = *EspNativeProcessRng_live();\n            if (DoomRPG_randNextByte(EspNativeProcessRng_live()) !=\n                    subtype10Roll) {\n                *EspNativeProcessRng_live() = subtype10Before;\n                printf(\"[MONSTERSUB10] DEFER sprite=%u reason=preview-live-roll-mismatch rngRollback=yes mutation=no\\n\",\n                       (unsigned int)spriteIndex);\n                continue;\n            }\n            subtype10OldAlt = monster->alternateAttack;\n            if (!EspNativeGameplayMonsterState_toggleAlternateAttack(\n                    spriteIndex, subtype10OldAlt)) {\n                *EspNativeProcessRng_live() = subtype10Before;\n                printf(\"[MONSTERSUB10] DEFER sprite=%u reason=alternate-state-not-owned rngRollback=yes mutation=no\\n\",\n                       (unsigned int)spriteIndex);\n                continue;\n            }\n            subtype10Prepared = 1U;\n            DRPG_LOGI(\"[MONSTERSUB10] FALLBACK turn=%u ordinal=%u/%u sprite=%u roll=%u raiseRoll=%u eligibleCorpse=none rngCalls=1 alt=%u->%u next=ordinary-ai\\n\",\n                      (unsigned int)activeSeq.expandedTurns,\n                      (unsigned int)(ordinal + 1U),\n                      (unsigned int)activationCount,\n                      (unsigned int)spriteIndex,\n                      (unsigned int)subtype10Roll,\n                      (unsigned int)EspNativeSubtype10_wantsRaise(subtype10Roll),\n                      (unsigned int)subtype10OldAlt,\n                      (unsigned int)(subtype10OldAlt ^ 1U));\n",
         "            printf(\"[MONSTERACTIVESEQ] MEMBER-DEFER turn=%u ordinal=%u/%u sprite=%u subtype=%u cause=special-ai-unowned mutation=no rngConsumed=0\\n\",\n                   (unsigned int)activeSeq.expandedTurns,\n                   (unsigned int)(ordinal + 1U),\n                   (unsigned int)activationCount,\n                   (unsigned int)spriteIndex,\n                   (unsigned int)monster->subtype);\n            continue;\n"),
        ("            if (subtype10Prepared != 0U) {\n                /* Member-probe never published: undo the special-prefix\n                 * mutable record and live logical RNG before aborting. */\n                const int altRestored =\n                    EspNativeGameplayMonsterState_toggleAlternateAttack(\n                        spriteIndex, (uint8_t)(subtype10OldAlt ^ 1U));\n                *EspNativeProcessRng_live() = subtype10Before;\n                printf(\"[MONSTERSUB10] ROLLBACK sprite=%u altExact=%s rngRestored=yes reason=member-probe-invalid\\n\",\n                       (unsigned int)spriteIndex,\n                       altRestored ? \"yes\" : \"NO\");\n            }\n",
         ""),
    ),
    "esp_native_gameplay_monster_turn.c": (
        ("        printf(\"[MONSTERTURN] READY arena=%08x ownerBytes=%u enemies=%u mode=probe+rollback schedule=MOVE+PLAYER_ATTACK+PASS_TURN rotation=legacy-no-turn attackFamily=stationary-cardinal-generic traceMask=%04x playerDamage=prospective movementPositions=deferred activationOrder=fail-closed subtype10AI=admission-guarded-fallback/raise-deferred mutation=no\\n\",\n",
         "        printf(\"[MONSTERTURN] READY arena=%08x ownerBytes=%u enemies=%u mode=probe+rollback schedule=MOVE+PLAYER_ATTACK+PASS_TURN rotation=legacy-no-turn attackFamily=stationary-cardinal-generic traceMask=%04x playerDamage=prospective movementPositions=deferred activationOrder=fail-closed subtype10AI=deferred mutation=no\\n\",\n"),
        ("    }\n    if (!EspMapSpriteTopology_getEntity(monster->spriteIndex,\n",
         "    }\n    if (monster->subtype == TURN_SUBTYPE_SPECIAL_AI) {\n        return 1;\n    }\n    if (!EspMapSpriteTopology_getEntity(monster->spriteIndex,\n"),
    ),
    "esp_native_gameplay_monster_state.c": (
        ("int EspNativeGameplayMonsterState_toggleAlternateAttack(\n    uint16_t spriteIndex, uint8_t expectedBefore) {\n    EspNativeGameplayMonsterRecord* record;\n    if (expectedBefore > 1U || !EspNativeGameplayMonsterState_isReady()) {\n        return 0;\n    }\n    record = EspNativeGameplayMonsterState_findMutable(spriteIndex);\n    if (record == NULL || record->alive == 0U ||\n        record->subtype != 10U ||\n        record->alternateAttack != expectedBefore) {\n        return 0;\n    }\n    record->alternateAttack ^= 1U;\n    monsterView.stateFNV1a = recordsFNV();\n    return monsterView.stateFNV1a != 0U;\n}\n\n",
         ""),
    ),
}

def _git_blob(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()

def restore_pre_s1_subtype10(name, data):
    if name not in EXPECTED:
        return data
    as_bytes = isinstance(data, bytes)
    current = data if as_bytes else data.encode("utf-8")
    head_hash, base_hash = EXPECTED[name]
    assert _git_blob(current) == head_hash, ("GAME-S1 current source drift", name)
    source = current.decode("utf-8")
    for newer, older in REVERSE_EDITS[name]:
        assert source.count(newer) == 1, (
            "GAME-S1 frozen inverse hunk drift", name, newer[:80]
        )
        source = source.replace(newer, older, 1)
    original = source.encode("utf-8")
    assert _git_blob(original) == base_hash, ("GAME-S1 original blob drift", name)
    return original if as_bytes else source
