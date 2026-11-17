#!/usr/bin/env python3
"""Exact AUDIO-MONSTER-SFX-only deltas reversed for frozen SDL-F25 source hash.

Each added span must exist verbatim. The frozen historical four-C-file oracle
still checks the original Git blob; it does not permit changing old gameplay.
"""
ADDITIONS = {
    "esp_native_gameplay_monster_attack_visual.c": (
        "#include \"esp_native_monster_ambience.h\"\n",
        "    /* Let the first sight cry become audible before the first attack pose.\n     * This guard is only presentation time; the committed attack probe waits\n     * untouched, with world input blocked and gameplay RNG unchanged. */\n    if (EspNativeMonsterAmbience_sightSettling(\n            monster->spriteIndex,DoomRPG_GetUpTimeMS())) {\n        if (sightHoldLoggedProbe!=turn->attackProbes) {\n            sightHoldLoggedProbe=turn->attackProbes;\n            printf(\"[MONSTERSFX] SIGHT-HOLD probe=%u sprite=%u \"\n                   \"guardMs=350 turnMutation=no gameplayRng=untouched\\n\",\n                   (unsigned)turn->attackProbes,\n                   (unsigned)monster->spriteIndex);\n        }\n        return;\n    }\n\n",
        "#include \"esp_native_audio_intent.h\"\n#include \"esp_native_monster_sfx_table.h\"\n",
        "#include \"esp_native_gameplay_monster_combat.h\"\n",
        "static uint32_t painHoldLoggedProbe;\nstatic uint32_t sightHoldLoggedProbe;\nstatic uint16_t pendingAttackSoundId;\nstatic uint32_t pendingAttackSoundAtMs;\n",
        "    pendingAttackSoundId=0U;\n    pendingAttackSoundAtMs=0U;\n",
        "    /* Legacy attack sound at +150ms from the successfully presented pose.\n     * No sound on render rollback. No gameplay RNG or PAK I/O here. */\n    if (pendingAttackSoundAtMs!=0U &&\n        (int32_t)(now-pendingAttackSoundAtMs)>=0) {\n        if (pendingAttackSoundId!=0U) {\n            (void)EspNativeAudioIntent_publish(pendingAttackSoundId,0U,3U);\n            printf(\"[MONSTERSFX] ATTACK probe=%u sprite=%u shot=%u/%u id=%u timing=pose+150ms\\n\",\n                   (unsigned)attackVisual.activeProbe,\n                   (unsigned)attackVisual.activeSpriteIndex,\n                   (unsigned)activeCompletedLoops,(unsigned)activeTotalLoops,\n                   (unsigned)pendingAttackSoundId);\n        }\n        pendingAttackSoundAtMs=0U;\n    }\n",
        "    pendingAttackSoundAtMs=DoomRPG_GetUpTimeMS()+ATTACK_VISUAL_FRAME_MS;\n",
        "    pendingAttackSoundId=0U;\n    pendingAttackSoundAtMs=0U;\n    painHoldLoggedProbe=0U;\n    sightHoldLoggedProbe=0U;\n",
        "    /* A confirmed hit owns pain6 for 250ms. Previously the monster's reply\n     * attack pose was admitted immediately after the player's settle frame,\n     * often BEFORE the queued pain sound became audible. Keep the existing\n     * delivered probe pending (and world input blocked) until that exact\n     * presentation deadline; no artificial turn, RNG draw or redraw loop. */\n    if (turn->lastReason==ESP_NATIVE_GAMEPLAY_MONSTER_TURN_PLAYER_ATTACK &&\n        EspNativeGameplayMonsterCombat_painSettling(\n            monster->spriteIndex,DoomRPG_GetUpTimeMS())) {\n        if (painHoldLoggedProbe!=turn->attackProbes) {\n            painHoldLoggedProbe=turn->attackProbes;\n            printf(\"[MONSTERSFX] PAIN-HOLD probe=%u sprite=%u \"\n                   \"reason=committed-pain6/250ms next=existing-attack-probe \"\n                   \"turnMutation=no gameplayRng=untouched\\n\",\n                   (unsigned)turn->attackProbes,\n                   (unsigned)monster->spriteIndex);\n        }\n        return;\n    }\n\n",
        "    pendingAttackSoundId=EspNativeMonsterSfx_lookup(monster->mType,\n        (uint8_t)(monster->alternateAttack!=0U ? 5U : 4U));\n    pendingAttackSoundAtMs=DoomRPG_GetUpTimeMS()+ATTACK_VISUAL_FRAME_MS;\n\n",
    ),
    "esp_native_gameplay_monster_combat.c": (
        "    painSoundHoldUntilMs=0U;\n    painSoundHoldSprite=MONSTER_NO_SPRITE;\n",
        "/* Post-commit audio presentation only: not in rollback or checkpoint owner. */\nstatic uint32_t painSoundHoldUntilMs;\nstatic uint16_t painSoundHoldSprite;\n",
        "#include \"esp_native_audio_intent.h\"\n",
        "int EspNativeGameplayMonsterCombat_painSettling(\n    uint16_t spriteIndex, uint32_t nowMs) {\n    return combatOwner.view.active == 1U &&\n           ((combatOwner.view.painSpriteIndex == spriteIndex &&\n             combatOwner.painUntilMs != 0U &&\n             (int32_t)(nowMs-combatOwner.painUntilMs) < 0) ||\n            (painSoundHoldSprite == spriteIndex &&\n             painSoundHoldUntilMs != 0U &&\n             (int32_t)(nowMs-painSoundHoldUntilMs) < 0));\n}\n\n",
        "    /* Both sounds are committed AFTER attack-frame success, never during\n     * probe/rollback. The audio owner retains both ordered intents. */\n    {\n        const uint16_t shotSound=pending.weapon==0U ? 5136U : weapon->resourceId;\n        if (shotSound!=0U) (void)EspNativeAudioIntent_publish(shotSound,0U,5U);\n        if (consequenceSound!=0U) {\n            (void)EspNativeAudioIntent_publish(consequenceSound,0U,lethal?5U:2U);\n            /* Visual pain began before the attack frame was rendered. Anchor\n             * a separate short audio hold to this actual sound publication. */\n            if (!lethal && EspNativeAudioIntent_isSoundEnabled()) {\n                painSoundHoldSprite=pending.spriteIndex;\n                painSoundHoldUntilMs=DoomRPG_GetUpTimeMS()+MONSTER_PAIN_MS;\n            }\n        }\n        printf(\"[MONSTERSFX] HIT seq=%u mType=%u shot=%u result=%u lethal=%u rollback=closed\\n\",\n               (unsigned)pending.sequence,(unsigned)targetBefore.mType,\n               (unsigned)shotSound,(unsigned)consequenceSound,(unsigned)lethal);\n    }\n",
    ),
}


def restore_pre_monster_sfx(name: str, current: str) -> str:
    for added in ADDITIONS.get(name, ()):
        assert current.count(added) >= 1, (name, "SFX delta drift")
        current = current.replace(added, "", 1)
    return current
