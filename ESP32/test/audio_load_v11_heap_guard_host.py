#!/usr/bin/env python3
"""AUDIO-LOAD-V11: real heap policy C vs actual classic-CYD NEW/LOAD evidence.

Reject post-ring memory policy drift, unsafe ring admission and lost fail-closed
semantics. This is a host oracle, NOT a substitute for flashing esp32-cyd.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/esp_audio_entrance_heap_guard.h"
MUSIC = (ROOT / "src/esp_audio_entrance_music.cpp").read_text()

def require(condition, reason):
    if not condition:
        raise AssertionError(reason)

# Exercise the very policy header compiled by the firmware. Keep C++ runtime
# ownership unchanged: no second ring, no timer/ISR allocation and no ZIP.
for text in (
    '#include "esp_audio_entrance_heap_guard.h"',
    "if (!EspAudioMusic_heapBeforeRingOk(heapBefore,largest))",
    "EspAudioMusic_heapAfterRingOk(ring!=nullptr,postRingHeap8,",
    "heap_caps_malloc(",
    "kRingCapacity,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT",
    'closeMusic("FAIL-HEAP");',
    "[ENTRANCEMUSIC] HEAP-POST-REJECT",
    "kRingCapacity=16384U",
):
    require(text in MUSIC, "production post-ring gate changed: " + text)
require("heap_caps_get_free_size(MALLOC_CAP_8BIT)<49152U" not in MUSIC,
        "original over-strict LOAD guard resurrected")
require(MUSIC.count("ring=(uint8_t*)heap_caps_malloc(") == 1,
        "music acquired a second ring")
require("heap_caps_free(ring)" in MUSIC, "failed ring must be freed")

PROGRAM = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_audio_entrance_heap_guard.h"

int main(void) {
    /* Real CYD: NEW GAME after ring=49388; LOAD before=69344 and FAIL-HEAP.
       Projected LOAD after ring=69344-(70164-49388)=48568; that post
       value was NOT serialized in the original failing firmware.
       Both the measured NEW case and modeled V11 case must be admitted. */
    assert(EspAudioMusic_heapBeforeRingOk(70164U, 47092U));
    assert(EspAudioMusic_heapAfterRingOk(1, 49388U, 31732U));
    assert(EspAudioMusic_heapBeforeRingOk(69344U, 47092U));
    assert(EspAudioMusic_heapAfterRingOk(1, 48568U, 31732U));
    assert(48568U < 49152U); /* The original bad post-ring floor. */

    /* Conservatively reserve 46KiB free + a further 16KiB contiguous.
       The 32KiB general gameplay requirement still fits in 46KiB. */
    assert(ESP_AUDIO_MUSIC_POST_MIN_HEAP8 == 47104U);
    assert(ESP_AUDIO_MUSIC_POST_MIN_LARGEST8 == 16384U);
    assert(ESP_AUDIO_MUSIC_PRE_MIN_HEAP8 == 61440U);
    assert(ESP_AUDIO_MUSIC_PRE_MIN_LARGEST8 == 24576U);
    assert(ESP_AUDIO_MUSIC_POST_MIN_HEAP8 >= 32768U + 14336U);

    assert(EspAudioMusic_heapBeforeRingOk(61440U, 24576U));
    assert(!EspAudioMusic_heapBeforeRingOk(61439U, 24576U));
    assert(!EspAudioMusic_heapBeforeRingOk(61440U, 24575U));
    assert(!EspAudioMusic_heapBeforeRingOk(52000U, 49000U));
    assert(EspAudioMusic_heapAfterRingOk(1, 47104U, 16384U));
    assert(!EspAudioMusic_heapAfterRingOk(1, 47103U, 16384U));
    assert(!EspAudioMusic_heapAfterRingOk(1, 47104U, 16383U));
    assert(!EspAudioMusic_heapAfterRingOk(0, 70000U, 50000U));
    assert(!EspAudioMusic_heapAfterRingOk(0, 48568U, 31732U));
    assert(!EspAudioMusic_heapAfterRingOk(1, 0U, 50000U));
    assert(EspAudioMusic_heapAfterRingOk(1, 70000U, 50000U));

    /* Deterministic boundary/capability probes: never accept a missing ring
       or either non-conforming free-8bit/contiguous-block predicate. */
    uint32_t state=0x12345U;
    for (int i=0; i<8192; ++i) {
        state=1664525U*state+1013904223U;
        uint32_t free8=state%90000U;
        state=1664525U*state+1013904223U;
        uint32_t largest=state%65000U;
        int allocated=(state>>17)&1U;
        int before=(free8>=61440U && largest>=24576U);
        int after=(allocated && free8>=47104U && largest>=16384U);
        assert(EspAudioMusic_heapBeforeRingOk(free8,largest)==before);
        assert(EspAudioMusic_heapAfterRingOk(allocated,free8,largest)==after);
    }
    puts("PASS AUDIO-LOAD-V11: real C policy NEW/LOAD, limits, 8192 adverse cases");
    return 0;
}
"""

MUTANTS = [
    ("reject real V11 load", "ESP_AUDIO_MUSIC_POST_MIN_HEAP8 = 47104U",
     "ESP_AUDIO_MUSIC_POST_MIN_HEAP8 = 49152U"),
    ("overly strict post-ring", "ESP_AUDIO_MUSIC_POST_MIN_HEAP8 = 47104U",
     "ESP_AUDIO_MUSIC_POST_MIN_HEAP8 = 49153U"),
    ("accept zero post reserve", "ESP_AUDIO_MUSIC_POST_MIN_HEAP8 = 47104U",
     "ESP_AUDIO_MUSIC_POST_MIN_HEAP8 = 4096U"),
    ("unbounded post contiguity", "ESP_AUDIO_MUSIC_POST_MIN_LARGEST8 = 16384U",
     "ESP_AUDIO_MUSIC_POST_MIN_LARGEST8 = 4096U"),
    ("overly strict post contiguity", "ESP_AUDIO_MUSIC_POST_MIN_LARGEST8 = 16384U",
     "ESP_AUDIO_MUSIC_POST_MIN_LARGEST8 = 24576U"),
    ("unbounded preflight memory", "ESP_AUDIO_MUSIC_PRE_MIN_HEAP8 = 61440U",
     "ESP_AUDIO_MUSIC_PRE_MIN_HEAP8 = 4096U"),
    ("overly strict preflight", "ESP_AUDIO_MUSIC_PRE_MIN_HEAP8 = 61440U",
     "ESP_AUDIO_MUSIC_PRE_MIN_HEAP8 = 65535U"),
    ("overly strict initial contiguity",
     "ESP_AUDIO_MUSIC_PRE_MIN_LARGEST8 = 24576U",
     "ESP_AUDIO_MUSIC_PRE_MIN_LARGEST8 = 32768U"),
    ("accept NULL ring", "return ringAllocated &&",
     "return (ringAllocated | 1) &&"),
    ("reverse post free capacity",
     "free8 >= ESP_AUDIO_MUSIC_POST_MIN_HEAP8 &&",
     "free8 <= ESP_AUDIO_MUSIC_POST_MIN_HEAP8 &&"),
    ("reverse post contiguous capacity",
     "largest8 >= ESP_AUDIO_MUSIC_POST_MIN_LARGEST8;",
     "largest8 <= ESP_AUDIO_MUSIC_POST_MIN_LARGEST8;"),
    ("reverse pre free capacity",
     "free8 >= ESP_AUDIO_MUSIC_PRE_MIN_HEAP8 &&",
     "free8 <= ESP_AUDIO_MUSIC_PRE_MIN_HEAP8 &&"),
    ("reverse pre contiguous capacity",
     "largest8 >= ESP_AUDIO_MUSIC_PRE_MIN_LARGEST8;",
     "largest8 <= ESP_AUDIO_MUSIC_PRE_MIN_LARGEST8;"),
]

def trial(text: str):
    with tempfile.TemporaryDirectory(prefix="audio-load-v11-") as tmp:
        d=Path(tmp)
        (d / HEADER.name).write_text(text)
        (d / "witness.c").write_text(PROGRAM)
        compile=subprocess.run(
            ["cc", "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
             "-I", str(d), str(d/"witness.c"), "-o", str(d/"witness")],
            capture_output=True, text=True)
        require(compile.returncode == 0,
                "real C policy failed to compile: " + compile.stderr[:1500])
        run=subprocess.run([str(d/"witness")],capture_output=True,text=True,
                           timeout=10)
        return run.returncode == 0 and "PASS AUDIO-LOAD-V11" in run.stdout

def main():
    original=HEADER.read_text()
    require(trial(original), "unmodified real C policy rejects witnessed LOAD")
    for description,old,new in MUTANTS:
        require(original.count(old)==1,"mutation anchor drift: "+description)
        mutant=original.replace(old,new,1)
        require(not trial(mutant), "adversarial mutant survived: "+description)
    print("PASS AUDIO-LOAD-V11: 2 real CYD scenarios, 8192 policy probes, "
          f"{len(MUTANTS)} rejected C mutants, one music ring")

if __name__ == "__main__":
    main()
