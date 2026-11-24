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
