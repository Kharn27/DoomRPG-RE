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
    # R1g temporarily inserts a cold-boot test before SD/TFT initialization.
    # Historical root parity must strip this ENTIRELY, just like the already
    # isolated AUDIO-R1 gameplay hook. Preserve exact pre-audio Git blobs.
    cold_boot_hook = (
        '    /* AUDIO-R1g TEMPORARY diagnostic: the 8s 440Hz R1f tone plays before\\n'
        '     * the display, SD and gameplay initialize. Expect a ~19s delay before\\n'
        '     * MENU_MAIN; after this one-shot, the normal game boots silently. */\\n'
        '    EspAudioR1_runColdBootProbe();\\n'
        '\\n'
    )
    assert text.count(include) == 1, "AUDIO-R1 include integration drift"
    assert text.count(hook) == 1, "AUDIO-R1 gameplay integration drift"
    assert text.count(cold_boot_hook) == 1, "AUDIO-R1g cold-boot integration drift"
    return (text.replace(include, original, 1)
                .replace(hook, "", 1)
                .replace(cold_boot_hook, "", 1))
