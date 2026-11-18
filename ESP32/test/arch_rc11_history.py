"""ARCH-RC11: strict inverses for frozen SHA oracles only.

These transforms operate on in-memory text, never firmware files. Each inverse
must find its unique RC11 addition before the older RC10 oracle runs.
"""

def restore_pre_rc11_resident(source):
    binary = isinstance(source, bytes)
    if binary: source = source.decode("utf-8")
    from arch_rc12_history import restore_pre_rc12_resident
    source = restore_pre_rc12_resident(source)
    new = "int painted=EspNativeDynamicLineRender_renderAmbient("
    old = "int painted=EspNativeGameplayFrame_renderAmbient("
    assert source.count(new) == 1, "ARCH-RC11 resident ambient oracle anchor"
    source = source.replace(new, old, 1)
    return source.encode("utf-8") if binary else source

def restore_pre_rc11_event_chain(source):
    binary = isinstance(source, bytes)
    if binary: source = source.decode("utf-8")
    from arch_rc12_history import restore_pre_rc12_event_chain
    source = restore_pre_rc12_event_chain(source)
    imports = '#include "esp_native_dynamic_line_render.h"\n'
    new = "if (!EspNativeDynamicLineRender_composeTurn("
    old = "if (!EspNativeGameplayFrame_composeTurn("
    assert source.count(imports) == 1 and source.count(new) == 1, (
        "ARCH-RC11 event-chain oracle anchors")
    source = source.replace(imports, "", 1).replace(new, old, 1)
    return source.encode("utf-8") if binary else source
