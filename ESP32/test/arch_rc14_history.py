"""ARCH-RC14 strict inverse for frozen historical source checks ONLY.

Do not apply to firmware or change historical Git SHAs. The old Session
configuration raw entry had public spelling before SAVE reprime interposition
was made explicit; reconstruct precisely in SDL-F27's original C oracle.
"""

def restore_pre_rc14(name, source):
    if name != "esp_native_gameplay_session.c":
        return source
    old = "int EspNativeGameplaySession_configure(\n"
    new = "/* Raw session configuration leaf: SAVE reprime owner calls this once. */\nint EspNativeGameplaySession_configureBase(\n"
    was_bytes = isinstance(source, bytes)
    text = source.decode("utf-8") if was_bytes else source
    assert text.count(new) == 1, "ARCH-RC14 frozen session source drift"
    text = text.replace(new, old, 1)
    return text.encode("utf-8") if was_bytes else text
