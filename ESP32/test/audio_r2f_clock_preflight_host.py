#!/usr/bin/env python3
"""AUDIO-R2f: clock calibration remains valid when gameplay renders during 1s gate."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
music=(root/"ESP32/src/esp_audio_entrance_music.cpp").read_text()
hdr=root/"ESP32/src/esp_audio_clock_guard.h"
main=(root/"ESP32/src/main.cpp").read_text()
assert '#include "esp_audio_clock_guard.h"' in music
assert "if (clockTicks!=UINT32_MAX) ++clockTicks;" in music
assert "if (clockTicks<kRate) ++clockTicks;" not in music
assert "EspAudioClockGuard_rate(" in music
assert 'sampleMode=uncapped-interval' in music
assert 'timer_pause(kGroup,kTimer)!=ESP_OK' in music
assert "closeMusic(\"FAIL-CLOCK-PAUSE\")" in music
assert "closeMusic(\"FAIL-CLOCK-TIMEOUT\")" in music
assert 'measured<20900U||measured>23200U' in music
assert music.index("timer_pause(kGroup,kTimer)!=ESP_OK") < music.index(
    "const uint32_t measured=EspAudioClockGuard_rate("
)
assert "if (!startPlaying()) closeMusic(failure);" in music
assert 'EspEntranceMusic_service(EspNativeGameplaySession_isActive(), sdReady);' in main
assert "if (ok && publish) EspEntranceMusic_pumpGameplay();" in (
    root/"ESP32/src/esp_native_gameplay_frame.c").read_text()

fixture=r"""
#include <stdint.h>
#include <assert.h>
#include "esp_audio_clock_guard.h"
static int ok(uint32_t n,uint64_t us) {
    const uint32_t hz=EspAudioClockGuard_rate(n,us);
    return hz>=20900U&&hz<=23200U;
}
int main(void) {
    /* Two genuine hardware snapshots in user log: 1st timer started,
     * but the main task did a movement frame before it checked. */
    assert(EspAudioClockGuard_rate(22050,1202801ULL)==18332U);
    assert(!ok(22050,1202801ULL)); /* old counter capped: false fail */
    assert(EspAudioClockGuard_rate(26524,1202801ULL)==22051U);
    assert(ok(26524,1202801ULL)); /* new true uncapped ticks: pass */
    assert(ok(22050U,1000000ULL));
    assert(ok(26460U,1200000ULL)); /* 200ms extra gameplay render */
    assert(ok(33075U,1500000ULL)); /* max active gate window */
    assert(!ok(18000U,1000000ULL)); /* genuine slow hardware */
    assert(!ok(24000U,1000000ULL)); /* genuine fast hardware */
    assert(!ok(0U,1000000ULL));
    assert(!ok(22050U,0ULL));
    assert(EspAudioClockGuard_rate(UINT32_MAX,1ULL)==0U);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-audio-r2f-") as td:
    src=Path(td)/"clock.c"
    dst=Path(td)/"clock"
    src.write_text(fixture)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-pedantic",
                    "-I",str(hdr.parent),str(src),"-o",str(dst)],
                   check=True,cwd=root)
    subprocess.run([str(dst)],check=True,cwd=root)
print("PASS AUDIO-R2f: uncapped ISR ticks, delayed service still 22kHz and genuine faults fail closed")
