"""Strip the AUDIO-R2a main hook for historical root/main SHA parity.

This function is deliberately fail-closed when the exact instrumented
main.cpp changes; it does NOT remove native gameplay or any other features.
"""

def restore_pre_audio_r1(text: str) -> str:
    # SFX-P0 added a strictly temporary PAK-format scan before AUDIO-R2a.
    # Remove precisely the added header and call, retaining the historical
    # fixed Root-J2 Git blob (never update the SHA witness).
    sfx_include = '#include "esp_native_sfx_probe.h"\n'
    sfx_hook = (
        '    /* SFX-P0: first validate the original WAV formats from verified PAK.\n'
        '     * Exactly once per gameplay session, before music producer activity. */\n'
        '    EspNativeSfxProbe_service(EspNativeGameplaySession_isActive());\n'
    )
    assert text.count(sfx_include) == 1, "SFX-P0 header integration drift"
    assert text.count(sfx_hook) == 1, "SFX-P0 one-shot service integration drift"
    text = text.replace(sfx_include, "", 1).replace(sfx_hook, "", 1)
    include = '#include "esp_asset_pack.h"\n#include "esp_audio_entrance_music.h"\n'
    hook = (
        '    /* AUDIO-R2a: selected Entrance soundtrack, SD IMA-ADPCM 22050Hz.\n'
        '     * Game-loop producer owns all SD reads/decoding; DAC timer ISR DRAM-only.\n'
        '     * Automatically stop and close on session or map exit, fail closed. */\n'
        '    EspEntranceMusic_service(EspNativeGameplaySession_isActive(), sdReady);\n'
    )
    assert text.count(include) == 1, "AUDIO-R2a include integration drift"
    assert text.count(hook) == 1, "AUDIO-R2a service integration drift"
    return text.replace(include, '#include "esp_asset_pack.h"\n', 1).replace(hook, "", 1)
