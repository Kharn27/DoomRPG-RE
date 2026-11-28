"""Exact P3m inverse for the frozen SDL-F28 original body."""
def restore_pre_p3m_sprite(value: bytes) -> bytes:
    for new, old in (
        ("#include <stddef.h>\n#include <stdint.h>\n#include <stdio.h>".encode("utf-8"), "#include <stdint.h>\n#include <stdio.h>".encode("utf-8")),
        ("    /* P3m: bounds-only admission needs the scalar header, not payload.\n     * A successful payload read overwrites every live mask/texel byte,\n     * prefix[0..width] and all 16 palette entries before any consumer.\n     * Failure is never drawable; unused array tails are not observable. */\n    memset(frame, 0, offsetof(Frame, palette));".encode("utf-8"), "    memset(frame, 0, sizeof(*frame));".encode("utf-8")),
    ):
        assert value.count(new) == 1, "P3m historical inverse anchor"
        value = value.replace(new, old, 1)
    return value
