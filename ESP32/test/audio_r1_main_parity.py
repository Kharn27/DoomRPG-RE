"""Remove ONLY AUDIO-R1's dedicated injection for historical root Git parity.

The old Root-J2..J5 oracles reconstruct pre-AUDIO-R1 Git blobs. The current
firmware still executes the injected code; the AUDIO-R1 host test audits it.
Strict unique markers fail closed if either integration point drifts.
"""

def restore_pre_audio_r1(text: str) -> str:
    include = '#include "esp_asset_pack.h"\n#include "esp_audio_r1_analog.h"\n'
    original = '#include "esp_asset_pack.h"\n'
    hook = (
        '    /* AUDIO-R1: non-blocking, one-shot GPIO26 analog proof only after the\n'
        '     * permanent native gameplay session has become fully active. */\n'
        '    if (EspNativeGameplaySession_isActive()) {\n'
        '        EspAudioR1_tryStartInGameplay();\n'
        '    } else {\n'
        '        EspAudioR1_requestStop();\n'
        '    }\n'
        '    EspAudioR1_poll();\n'
    )
    assert text.count(include) == 1, "AUDIO-R1 include integration drift"
    assert text.count(hook) == 1, "AUDIO-R1 gameplay integration drift"
    return text.replace(include, original, 1).replace(hook, "", 1)
