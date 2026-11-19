"""Strict inverse of ARCH-RC18's two public symbol-owner handoffs.

Only for immutable historical Git SHA oracles. This never runs on firmware,
and does not relax any expected historical blob SHA.
"""

def restore_pre_rc18_menu_touch(source: str) -> str:
    new = (
        "/* ARCH-RC18: explicit native facade for menu-gated registration.\n"
        " * Generic sampling remains in PlatformInput_setTapCallbackBase(). */\n"
        "void PlatformInput_setTapCallback(PlatformTapCallback callback) {"
    )
    old = "void __wrap_PlatformInput_setTapCallback(PlatformTapCallback callback) {"
    assert source.count(new) == 1, "RC18 public callback facade drift"
    source = source.replace(new, old, 1)
    leaf = "PlatformInput_setTapCallbackBase("
    assert source.count(leaf) == 4, "RC18 PlatformInput raw C ABI drift"
    return source.replace(leaf, "__real_PlatformInput_setTapCallback(")
