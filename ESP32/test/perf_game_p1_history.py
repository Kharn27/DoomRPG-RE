"""PERF-GAME-P1 exact removal for Git SHA-pinned historical resident oracles.

This strips *only* the temporary measurement observer and restores the
byte-identical pre-P1 resident source. Do not update historical Git hashes.
"""
def restore_pre_perf_game_p1_resident(src: str) -> str:
    def undo(added: str, original: str) -> None:
        nonlocal src
        assert src.count(added) == 1, ("P1 historical reverse drift", added[:110], src.count(added))
        src = src.replace(added, original, 1)

    undo('#include "esp_native_gameplay_input.h"\n#include "esp_perf_game_p1.h"\n',
         '#include "esp_native_gameplay_input.h"\n')
    undo('        EspPerfGameP1_inputBlockedCombat();\n', '')
    undo(
        '#if DOOMRPG_PERF_GAME_P1\n'
        '            EspPerfGameP1_inputAccepted(EspNativeGameplayInput_peek()->sequence,\n'
        '                                         (uint64_t)esp_timer_get_time());\n'
        '#endif\n', '')
    undo('            EspPerfGameP1_inputBusy();\n', '')
    undo(
        '#if DOOMRPG_PERF_GAME_P1\n'
        '        EspPerfGameP1_inputAccepted(EspNativeGameplayInput_peek()->sequence,\n'
        '                                     (uint64_t)esp_timer_get_time());\n'
        '#endif\n', '')
    undo('        EspPerfGameP1_inputBusy();\n', '')
    undo(
        '#if DOOMRPG_PERF_GAME_P1\n'
        '    EspPerfGameP1_inputCommit(intent->sequence,\n'
        '                                   (uint64_t)esp_timer_get_time());\n'
        '#endif\n'
        '    printf("[RESIDENTGAMEPLAY] TURN n=',
        '    printf("[RESIDENTGAMEPLAY] TURN n=')
    undo(
        '#if DOOMRPG_PERF_GAME_P1\n'
        '        EspPerfGameP1_inputCommit(intent->sequence,\n'
        '                                   (uint64_t)esp_timer_get_time());\n'
        '#endif\n'
        '        printf("[RESIDENTGAMEPLAY] MOVE-DIALOG n=',
        '        printf("[RESIDENTGAMEPLAY] MOVE-DIALOG n=')
    undo(
        '#if DOOMRPG_PERF_GAME_P1\n'
        '    EspPerfGameP1_inputCommit(intent->sequence,\n'
        '                                   (uint64_t)esp_timer_get_time());\n'
        '#endif\n'
        '    printf("[RESIDENTGAMEPLAY] MOVE n=',
        '    printf("[RESIDENTGAMEPLAY] MOVE n=')
    undo('    EspPerfGameP1_reset();\n', '')
    undo(
        'static void EspNativeResidentGameplay_serviceBody(struct DoomRPG_s* doomRpgBase);\n'
        '\n'
        '/* Keep the public entrypoint before the real body for structural gate checks. */\n'
        'void EspNativeResidentGameplay_service(struct DoomRPG_s* doomRpgBase) {\n'
        '    EspNativeResidentGameplay_serviceBody(doomRpgBase);\n'
        '#if DOOMRPG_PERF_GAME_P1\n'
        '    EspPerfGameP1_inputDispatchDone((uint64_t)esp_timer_get_time());\n'
        '#endif\n'
        '}\n'
        '\n'
        'static void EspNativeResidentGameplay_serviceBody(struct DoomRPG_s* doomRpgBase) {',
        'void EspNativeResidentGameplay_service(struct DoomRPG_s* doomRpgBase) {'
    )
    assert 'EspPerfGameP1_' not in src and 'esp_perf_game_p1.h' not in src
    return src


def restore_pre_perf_game_p1_active_sequence(src: str) -> str:
    """Remove only PERF-GAME-P1 additions before older SDL-F25 history checks."""
    def undo(added: str, original: str) -> None:
        nonlocal src
        assert src.count(added) == 1, (
            "P1 active-sequence reverse drift", added[:110], src.count(added))
        src = src.replace(added, original, 1)

    undo(
        '#include "esp_player_view_state.h"\n'
        '#include "esp_perf_game_p1.h"\n'
        '#if DOOMRPG_PERF_GAME_P1\n'
        '#include <esp_timer.h>\n'
        '#endif\n',
        '#include "esp_player_view_state.h"\n')
    undo("#if DOOMRPG_PERF_GAME_P1\n        EspPerfGameP1_phaseBegin(0, (uint64_t)esp_timer_get_time());\n#endif\n        memberStatus = EspNativeGameplayMonsterTurn_probeActiveMember(\n            doomRpg, spriteIndex);\n#if DOOMRPG_PERF_GAME_P1\n        EspPerfGameP1_phaseEnd(0, (uint64_t)esp_timer_get_time());\n#endif", "        memberStatus = EspNativeGameplayMonsterTurn_probeActiveMember(\n            doomRpg, spriteIndex);")
    undo("#if DOOMRPG_PERF_GAME_P1\n        if (memberStatus == ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_RANGED_MOVE ||\n            memberStatus == ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_NO_IMMEDIATE_ATTACK)\n            EspPerfGameP1_phaseBegin(1, (uint64_t)esp_timer_get_time());\n#endif\n        if (memberStatus == ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_RANGED_MOVE) {", "        if (memberStatus == ESP_NATIVE_GAMEPLAY_MONSTER_MEMBER_RANGED_MOVE) {")
    undo("#if DOOMRPG_PERF_GAME_P1\n        EspPerfGameP1_phaseEnd(1, (uint64_t)esp_timer_get_time());\n#endif\n#if DOOMRPG_PERF_GAME_P1\n        EspPerfGameP1_member(perfMemberStart,", "#if DOOMRPG_PERF_GAME_P1\n        EspPerfGameP1_member(perfMemberStart,")
    undo('static void EspNativeGameplayMonsterActiveSequence_serviceBody(',
         'void EspNativeGameplayMonsterActiveSequence_service(')
    undo(
        '            EspPerfGameP1_abort();\n'
        '            printf("[MONSTERACTIVESEQ] TERMINAL',
        '            printf("[MONSTERACTIVESEQ] TERMINAL')
    undo(
        '        EspPerfGameP1_abort();\n'
        '        printf("[MONSTERACTIVESEQ] DEFER turn=%u cause=activation-count-regressed',
        '        printf("[MONSTERACTIVESEQ] DEFER turn=%u cause=activation-count-regressed')
    undo(
        '            EspPerfGameP1_abort();\n'
        '            printf("[MONSTERACTIVESEQ] DEFER turn=%u ordinal=%u activeCount=%u',
        '            printf("[MONSTERACTIVESEQ] DEFER turn=%u ordinal=%u activeCount=%u')
    undo(
        '            EspPerfGameP1_abort();\n'
        '            printf("[MONSTERACTIVESEQ] DEFER turn=%u ordinal=%u/%u sprite=%u subtype=%u cause=member-probe-failed',
        '            printf("[MONSTERACTIVESEQ] DEFER turn=%u ordinal=%u/%u sprite=%u subtype=%u cause=member-probe-failed')
    undo(
        '#if DOOMRPG_PERF_GAME_P1\n'
        '        const uint64_t perfMemberStart = (uint64_t)esp_timer_get_time();\n'
        '#endif\n'
        '        beforeTurn =',
        '        beforeTurn =')
    undo(
        '#if DOOMRPG_PERF_GAME_P1\n'
        '        EspPerfGameP1_member(perfMemberStart, (uint64_t)esp_timer_get_time(),\n'
        '                              activeSeq.turnActivationCount);\n'
        '#endif\n'
        '        ++activeSeq.turnDelivered;',
        '        ++activeSeq.turnDelivered;')
    undo(
        '\n\n/* Exactly the same one-member cooperative service; P1 only surrounds it\n'
        ' * with monotonic wall-clock samples. Macro OFF compiles out the probes. */\n'
        'void EspNativeGameplayMonsterActiveSequence_service(struct DoomRPG_s* doomRpg) {\n'
        '#if DOOMRPG_PERF_GAME_P1\n'
        '    const uint64_t startUs = (uint64_t)esp_timer_get_time();\n'
        '    const uint32_t beforeTurn = activeSeq.expandedTurns;\n'
        '    const uint8_t beforeBusy = activeSeq.turnInProgress;\n'
        '    EspPerfGameP1_serviceEnter();\n'
        '#endif\n'
        '    EspNativeGameplayMonsterActiveSequence_serviceBody(doomRpg);\n'
        '#if DOOMRPG_PERF_GAME_P1\n'
        '    EspPerfGameP1_sequence(startUs, (uint64_t)esp_timer_get_time(),\n'
        '        beforeTurn, activeSeq.expandedTurns, beforeBusy,\n'
        '        activeSeq.turnInProgress, activeSeq.turnActivationCount,\n'
        '        activeSeq.turnDelivered, activeSeq.pauseProbe);\n'
        '#endif\n'
        '}\n',
        '\n')
    assert 'EspPerfGameP1_' not in src and 'esp_perf_game_p1.h' not in src
    return src


def restore_pre_perf_game_p1_frame(src: str) -> str:
    """Restore the complete pre-P1 frame compositor before pinned SDL-F27 checks."""
    include = '#include <esp_timer.h>\n#include "esp_perf_game_p1.h"\n'
    former = '#include <esp_timer.h>\n'
    assert src.count(include) == 1, "P1 frame import drift"
    src = src.replace(include, former, 1)
    hook = (
        '    *outStats = *stats;\n'
        '    EspPerfGameP1_frame(stats->totalMicros, stats->presentMicros,\n'
        '                         (uint8_t)preview, (uint8_t)publish,\n'
        '                         stats->finalPresented);\n'
        '    frameScratch.busy = 0U;'
    )
    assert src.count(hook) == 1, "P1 frame probe drift"
    src = src.replace(hook,
                      '    *outStats = *stats;\n    frameScratch.busy = 0U;', 1)
    assert 'EspPerfGameP1_' not in src and 'esp_perf_game_p1.h' not in src
    return src


def restore_pre_perf_game_p1_movement_probe(src: str) -> str:
    """Reverse exactly ten P1 timing hooks, retaining SDL-F20 historical SHA."""
    import re
    header = ('#include "esp_perf_game_p1.h"\n'
              '#if DOOMRPG_PERF_GAME_P1\n#include <esp_timer.h>\n#endif\n')
    assert src.count(header) == 1, "P1 movement-probe header drift"
    src = src.replace(header, "", 1)
    pattern = (r'(?m)^#if DOOMRPG_PERF_GAME_P1\n'
               r'[ ]*EspPerfGameP1_phase(?:Begin|End)\([23], \(uint64_t\)esp_timer_get_time\(\)\);\n'
               r'#endif\n')
    src, count = re.subn(pattern, "", src)
    assert count == 10, ("P1 movement-probe clock hooks drift", count)
    assert 'EspPerfGameP1_' not in src
    return src


def restore_pre_perf_game_p1_geometry(name: str, src: str) -> str:
    """Remove only P1/P1h additions before SDL-F26 exact original Git SHA."""
    import re
    assert name in ('esp_native_gameplay_monster_movement.c',
                    'esp_native_gameplay_monster_three_goal_turn.c'), name
    marker = '/* P1H-SPRITE-INDEX BEGIN: shadow authority stays with the original scan. */'
    start = src.index('#if DOOMRPG_PERF_GAME_P1\n' + marker + '\n')
    end = src.index('#endif\n', src.index('/* P1H-SPRITE-INDEX END */', start)) + len('#endif\n')
    assert src[start - 1] == '\n', 'P1h extra separator drift'
    src = src[:start - 1] + src[end:]
    assert src.count('static int blockingSpriteOnTileReference(') == 1
    src = src.replace('static int blockingSpriteOnTileReference(',
                      'static int blockingSpriteOnTile(', 1)
    header = ('#include "esp_perf_game_p1.h"\n'
              '#if DOOMRPG_PERF_GAME_P1\n'
              '#include "esp_perf_trace_cache.h"\n'
              '#include "esp_perf_spatial_sprite.h"\n'
              '#endif\n'
              '#if DOOMRPG_PERF_GAME_P1\n'
              '#include <esp_timer.h>\n#endif\n')
    assert src.count(header) == 1, "P1 geometry header drift"
    src = src.replace(header, '', 1)
    wrapper_start = src.index('#if DOOMRPG_PERF_GAME_P1\n#define traceCache ')
    target = ('static PathResult calcPath(' if name.endswith('monster_movement.c')
              else 'static ThreeGoalPathResult calcPath(')
    wrapper_end = src.index(target, wrapper_start)
    src = src[:wrapper_start] + src[wrapper_end:]
    assert src.count('static int cardinalTraceClearBody(') == 1
    src = src.replace('static int cardinalTraceClearBody(',
                      'static int cardinalTraceClear(', 1)
    phase = 6 if name.endswith('monster_movement.c') else 7
    b = ('#if DOOMRPG_PERF_GAME_P1\n'
         '    EspPerfTraceCache_begin(&traceCache);\n'
         f'    EspPerfGameP1_phaseBegin({phase}, (uint64_t)esp_timer_get_time());\n'
         '    (void)EspPerfSpatialSprite_begin(&EspPerfGameP1_spatialSpriteIndex);\n'
         '#endif\n')
    e = ('#if DOOMRPG_PERF_GAME_P1\n'
         f'    EspPerfGameP1_phaseEnd({phase}, (uint64_t)esp_timer_get_time());\n'
         '    EspPerfSpatialSprite_end(&EspPerfGameP1_spatialSpriteIndex);\n'
         '    EspPerfTraceCache_end(&traceCache);\n#endif\n')
    assert src.count(b) == 1 and src.count(e) == 1, "P1h scope drift"
    src = src.replace(b, '', 1).replace(e, '', 1)
    pattern = (r'(?m)^#if DOOMRPG_PERF_GAME_P1\n'
               r'[ ]*EspPerfGameP1_phase(?:Begin|End)\([45], \(uint64_t\)esp_timer_get_time\(\)\);\n'
               r'#endif\n')
    src, count = re.subn(pattern, '', src)
    assert count == (0 if phase == 6 else 8), ("P1 continuation clock drift", count)
    assert 'EspPerfGameP1_' not in src and 'EspPerfTraceCache' not in src
    return src
