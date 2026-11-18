"""ARCH-RC12: strict inverses only for frozen pre-RC12 Git-blob oracles.

No production source is rewritten by these functions. Each changed token and
its multiplicity are verified before reconstructing historical SHA fingerprints.
"""

def _inverse(source, changes):
    binary = isinstance(source, bytes)
    if binary:
        source = source.decode("utf-8")
    for current, former, expected in changes:
        assert source.count(current) == expected, (
            "ARCH-RC12 immutable oracle drift", current,
            source.count(current), expected)
        source = source.replace(current, former)
    return source.encode("utf-8") if binary else source


def restore_pre_rc12_resident(source):
    return _inverse(source, (
        ('#include "esp_native_gameplay_note_prefix.h"\n', '', 1),
        ('EspNativeGameplayNotePrefix_beginDialog(', 'EspNativeGameplayDialog_begin(', 2),
        ('EspNativeGameplayEventChain_resumeDialog(', 'EspNativeGameplayDialog_resume(', 2),
        ('EspNativeGameplayEventChain_rollbackDialogResume(',
         'EspNativeGameplayDialog_rollbackResume(', 4),
    ))


def restore_pre_rc12_event_chain(source):
    return _inverse(source, (
        ('EspNativeGameplayEventChain_resumeDialog(',
         '__wrap_EspNativeGameplayDialog_resume(', 1),
        ('EspNativeGameplayEventChain_rollbackDialogResume(',
         '__wrap_EspNativeGameplayDialog_rollbackResume(', 3),
        ('EspNativeGameplayDialog_begin(', '__real_EspNativeGameplayDialog_begin(', 2),
    ))


def restore_pre_rc12_note_prefix(source):
    restored = _inverse(source, (
        ('EspNativeGameplayNotePrefix_beginDialog(',
         '__wrap_EspNativeGameplayDialog_begin(', 1),
    ))
    binary = isinstance(restored, bytes)
    if binary:
        restored = restored.decode("utf-8")
    anchor = 'static int eventDescriptorForIndex(uint16_t eventIndex,\n'
    assert restored.count(anchor) == 1, "ARCH-RC12 NOTE extern insertion anchor"
    extern = ('extern EspNativeGameplayDialogBeginStatus\n'
              '__real_EspNativeGameplayDialog_begin(uint16_t eventIndex,\n'
              '                                     uint8_t commandOffset,\n'
              '                                     uint32_t runFlags);\n\n')
    restored = restored.replace(anchor, extern + anchor, 1)
    return restored.encode("utf-8") if binary else restored
