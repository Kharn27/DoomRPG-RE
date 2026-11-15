"""Strip only the temporary AUDIO-R1l gameplay hook to preserve exact
historical Root-J2..J5 Git blob references. Fail closed if source drifts.
The production game still invokes the hook; AUDIO-R1l tests inspect it.
"""

def restore_pre_audio_r1(text: str) -> str:
    include = '#include "esp_asset_pack.h"\n#include "esp_audio_r1_timer.h"\n'
    original = '#include "esp_asset_pack.h"\n'
    hook = (
        '    /* AUDIO-R1l TEMPORARY: bounded 440 Hz PCM timer at real gameplay load.\n'
        '     * No I2S or cold-boot audio; service is non-blocking on each main loop. */\n'
        '    const bool gameAudioActive = EspNativeGameplaySession_isActive();\n'
        '    if (gameAudioActive) EspAudioR1_timerTryStartInGameplay();\n'
        '    EspAudioR1_timerService(gameAudioActive);\n'
    )
    assert text.count(include) == 1, "AUDIO-R1l include integration drift"
    assert text.count(hook) == 1, "AUDIO-R1l gameplay integration drift"
    return text.replace(include, original, 1).replace(hook, "", 1)
