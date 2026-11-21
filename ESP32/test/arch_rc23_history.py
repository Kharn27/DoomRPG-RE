"""Strict ARCH-RC23 source-only inverse for frozen SDL-F27 and SDL-F28 blobs.

No production use: restore exactly the two source symbol renames and the
historical GNU prototype solely inside byte-exact Git-blob parity oracles.
Original Git blob SHAs are never updated and all unexpected drift fails.
"""


def replace_once(content: bytes, newer: bytes, older: bytes) -> bytes:
    count = content.count(newer)
    assert count == 1, ("ARCH-RC23 frozen source drift", newer, count)
    return content.replace(newer, older, 1)


def restore_pre_rc23(filename: str, current: bytes) -> bytes:
    if filename == "esp_native_gameplay_frame.c":
        return replace_once(
            current,
            b"int EspNativeGameplayFrame_renderTurnBase(\n",
            b"int EspNativeGameplayFrame_renderTurn(\n",
        )
    if filename == "esp_native_dynamic_line_render.c":
        current = replace_once(
            current,
            b"int EspNativeGameplayFrame_renderTurn(\n",
            b"int __wrap_EspNativeGameplayFrame_renderTurn(\n",
        )
        current = replace_once(
            current,
            b"ok = EspNativeGameplayFrame_renderTurnBase(render, angle, outStats);",
            b"ok = __real_EspNativeGameplayFrame_renderTurn(render, angle, outStats);",
        )
        current = replace_once(
            current,
            b"void EspNativeDynamicLineRender_resumeFromAutomap(int wasScoped) {\n"
            b"    if (wasScoped) dynamicFrameActive = 1U;\n"
            b"}\n\n"
            b"int EspMapRuntime_getLineBase(uint32_t index, EspMapLine* outLine);",
            b"void EspNativeDynamicLineRender_resumeFromAutomap(int wasScoped) {\n"
            b"    if (wasScoped) dynamicFrameActive = 1U;\n"
            b"}\n\n"
            b"int __real_EspNativeGameplayFrame_renderTurn(\n"
            b"    struct Render_s* render,\n"
            b"    uint8_t angle,\n"
            b"    EspNativeGameplayFrameStats* outStats);\n"
            b"int EspMapRuntime_getLineBase(uint32_t index, EspMapLine* outLine);",
        )
        return current
    return current
