"""Strict inverse of RC19 source-owner relocation for immutable history SHA tests.

Only historical test code calls this. The real firmware still has no GNU alias
for getLine/view and the frozen Git blobs are never edited or relaxed.
"""


def restore_pre_rc19_dynamic(source: bytes) -> bytes:
    steps = (
        (b"int EspMapRuntime_getLineBase(",
         b"int __real_EspMapRuntime_getLine(", 1),
        (b"const EspMapLineStateView* EspMapLineState_viewBase(void);",
         b"const EspMapLineStateView* __real_EspMapLineState_view(void);", 1),
        (b"int EspMapRuntime_getLine(",
         b"int __wrap_EspMapRuntime_getLine(", 1),
        (b"EspMapRuntime_getLineBase(index, outLine)",
         b"__real_EspMapRuntime_getLine(index, outLine)", 1),
        (b"const EspMapLineStateView* EspMapLineState_view(void)",
         b"const EspMapLineStateView* __wrap_EspMapLineState_view(void)", 1),
        (b"EspMapLineState_viewBase()",
         b"__real_EspMapLineState_view()", 2),
    )
    for newer, older, count in steps:
        assert source.count(newer) == count, ("RC19 historical inverse drift", newer)
        source = source.replace(newer, older)
    return source
