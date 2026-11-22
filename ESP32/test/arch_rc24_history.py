"""Strict inverse of ARCH-RC24 present spelling ONLY for immutable Git-blob oracles.

Never alter production, historical blob SHAs or non-present data. This helper
reconstructs the exact pre-RC24 tokens before older SDL-F18/F24/F27 checks.
"""
def restore_pre_rc24(filename: str, original: bytes) -> bytes:
    value = original
    leaf = b"Esp32PlatformVideo_presentBase"
    legacy = b"__real_Esp32PlatformVideo_present"
    if filename == "esp_native_gameplay_event_chain.c":
        assert value.count(leaf) == 2, "RC24 event chain raw leaf drift"
        return value.replace(leaf, legacy)
    if filename == "esp_native_gameplay_gib_fx.c":
        assert value.count(leaf) == 2, "RC24 GIB direct physical leaf drift"
        return value.replace(leaf, legacy)
    if filename == "esp_native_gameplay_action_engine.c":
        assert value.count(leaf) == 7, "RC24 action physical leaf drift"
        value = value.replace(leaf, legacy)
        public_leaf = b"EspNativeGameplayActionEngine_presentBase"
        assert value.count(public_leaf) == 7, "RC24 action raw-leaf drift"
        value = value.replace(public_leaf, b"__wrap_Esp32PlatformVideo_present")
        marker = b"                                          uint16_t* outLinkOrder);\n\nstatic uint32_t actionNowMs(void) {"
        assert value.count(marker) == 1, "RC24 original real-present prototype anchor drift"
        value = value.replace(marker, b"                                          uint16_t* outLinkOrder);\n" +
                              b"int __real_Esp32PlatformVideo_present(void);\n\nstatic uint32_t actionNowMs(void) {", 1)
        return value
    raise AssertionError("unrecognized RC24 historical owner: " + filename)
