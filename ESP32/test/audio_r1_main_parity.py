"""Strip the AUDIO-R2a main hook for historical root/main SHA parity.

This function is deliberately fail-closed when the exact instrumented
main.cpp changes; it does NOT remove native gameplay or any other features.
"""

def restore_pre_audio_r1(text: str) -> str:
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
