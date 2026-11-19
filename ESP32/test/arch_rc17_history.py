"""ARCH-RC17 strict inverse of the two RNG public-owner symbol renames.

Only historical pre-SDL-F20 SHA verification uses this inverse. The real
firmware is NEVER rewritten or linked to a resurrected GNU __wrap name.
"""
def restore_pre_rc17(name, source):
    if name != "esp_native_rng_replay_guard.c":
        return source
    was_bytes = isinstance(source, bytes)
    s = source.decode("utf-8") if was_bytes else source
    pairs = (
        ("byte DoomRPG_randNextByte(Random_t* rand) {",
         "byte __wrap_DoomRPG_randNextByte(Random_t* rand) {"),
        ("int DoomRPG_randNextInt(Random_t* rand) {",
         "int __wrap_DoomRPG_randNextInt(Random_t* rand) {"),
    )
    for public, old in pairs:
        assert s.count(public) == 1, "ARCH-RC17 public RNG symbol drift"
        assert s.count(old) == 0, "ARCH-RC17 old GNU wrapper in current source"
        s = s.replace(public, old, 1)
    return s.encode("utf-8") if was_bytes else s
