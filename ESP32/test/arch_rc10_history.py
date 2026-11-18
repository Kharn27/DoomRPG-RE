"""ARCH-RC10: exact inverse of two newly explicit preview callsites.

Used only by frozen SDL Git-blob oracles. Never rewrites firmware sources;
only removes the additive RC10 import and restores the two original leaf
names before checking historical SHA byte-for-byte.
"""


def restore_pre_rc10_resident(source):
    binary = isinstance(source, bytes)
    if binary:
        source = source.decode("utf-8")
    changes = (
        ('#include "esp_native_dynamic_line_render.h"\n', ''),
        ('if (!EspNativeDynamicLineRender_renderVisualPose(',
         'if (!EspNativeGameplayFrame_renderVisualPose('),
        ('if (!EspNativeDynamicLineRender_renderVisualMove(',
         'if (!EspNativeGameplayFrame_renderVisualMove('),
    )
    for newer, older in changes:
        assert source.count(newer) == 1, f"RC10 historic resident anchor: {newer}"
        source = source.replace(newer, older, 1)
    return source.encode("utf-8") if binary else source
