"""Strict inverse of exactly GAME-S1 subtype10 P1 for frozen Git-blob tests.

This historical oracle does not touch the firmware. The current full C source
must match the pinned feature-branch Git blob; the recovered earlier source
must match the pinned main Git blob. A future edit fails closed until reviewed.
"""
import hashlib
from pathlib import Path
import subprocess

BASE = "d9896fcf5f07ab8d41935565002792b608a41333"
EXPECTED = {
    "esp_native_gameplay_monster_active_sequence.c": (
        "2637539668f8a46c37e157772cd73a33101c4b0e", "369339178076eac89e8cc0f7b47d0fb5540619d9"
    ),
}

def _git_blob(source):
    return hashlib.sha1(
        b"blob " + str(len(source)).encode() + b"\0" + source
    ).hexdigest()

def restore_pre_s1_subtype10(name, data):
    if name not in EXPECTED:
        return data
    was_bytes = isinstance(data, bytes)
    current = data if was_bytes else data.encode("utf-8")
    head_hash, base_hash = EXPECTED[name]
    assert _git_blob(current) == head_hash, (
        "GAME-S1 subtype10 current source changed outside frozen inverse", name
    )
    path = "ESP32/src/" + name
    baseline = subprocess.check_output(["git", "show", f"{BASE}:{path}"])
    assert _git_blob(baseline) == base_hash, (
        "GAME-S1 subtype10 baseline historical source drifted", name
    )
    return baseline if was_bytes else baseline.decode("utf-8")
