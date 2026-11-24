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
