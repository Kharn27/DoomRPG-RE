#!/usr/bin/env python3
"""AUDIO-R2a: real 22.05kHz mono WAV IMA ADPCM header/decoder and ownership.

Synthetic fixtures only; NEVER bundle the copyrighted M_STALKS recording.
"""
from pathlib import Path
import subprocess
import tempfile
import shutil

root = Path(__file__).resolve().parents[1]
src = root / "src"
main = (src / "main.cpp").read_text(encoding="utf-8")
runtime = (src / "esp_audio_entrance_music.cpp").read_text(encoding="utf-8")
header = (src / "esp_audio_entrance_adpcm.h").read_text(encoding="utf-8")
frame = (src / "esp_native_gameplay_frame.c").read_text(encoding="utf-8")
api = (src / "esp_audio_entrance_music.h").read_text(encoding="utf-8")
ci = (root.parent / ".github/workflows/esp32-cyd.yml").read_text(encoding="utf-8")

assert "EspEntranceMusic_service(EspNativeGameplaySession_isActive(), sdReady);" in main
assert "EspAudioR1_timerTryStartInGameplay();" not in main
assert "EspAudioR1_runColdBootProbe();" not in main
assert "/doomrpg/music/opl/M_STALKS.wav" in runtime
assert "/M_STALKS.wav" in runtime
assert "22050U" in runtime and "1814U" in runtime
assert "IMA-ADPCM" in runtime
assert "kRingCapacity=16384U" in runtime
assert "kPrimeFrames=8192U" in runtime and "kResumeFrames=4096U" in runtime
assert "kMaxBlockBytes=1024U" in runtime
assert "kMaxBlockSamples=2041U" in runtime
assert "heap_caps_malloc(" in runtime and "MALLOC_CAP_INTERNAL" in runtime
assert "ESP_INTR_FLAG_IRAM" in runtime
assert "IRAM_ATTR onAudioTick" in runtime
assert "RTC_IO_PAD_DAC2_REG,RTC_IO_PDAC2_DAC" in runtime
assert "DAC_CHANNEL_2" in runtime and "DAC_CHANNEL_1" not in runtime
assert "ESP_INTR_FLAG_LEVEL1" in runtime
assert "timer_isr_callback_remove" in runtime and "timer_deinit" in runtime
assert "musicFile.close();" in runtime and "heap_caps_free(ring)" in runtime
assert "SESSION_EXIT" in runtime and "FAIL-UNDERRUN-TIMEOUT" in runtime
assert 'closeMusic("FAIL-UNDERRUN")' not in runtime
assert "if (recovering && w-r>=kResumeFrames) recovering=0U" in runtime
assert "maxSilentStreak" in runtime and "recoveryStarts" in runtime
assert "for (uint32_t burst=0U;burst<2U;burst++)" in runtime
assert "FAIL-CLOCK" in runtime and "FAIL-HEAP" in runtime
assert "gapsOver90us" in runtime and "maxSdReadUs" in runtime
assert "SD.open(" in runtime and "musicFile.read(raw,wav.blockAlign)" in runtime
assert "if (kRingCapacity-q<wav.samplesPerBlock)" in runtime
assert "EspEntranceAdpcm_decodeBlockObserved(" in runtime
assert runtime.index("if (measured<20900U||measured>23200U)") < runtime.index("if (!startPlaying())")
assert "i2s_" not in runtime and "SPI." not in runtime
assert "xTaskCreate" not in runtime and "DoomRPG.zip" not in runtime
assert "WAVE_FORMAT_IMA_ADPCM" not in runtime   # pure numeric codec owner
assert "test/audio_r2a_entrance_host.py" in ci
assert "sampleFrames" in header and "blockAlign" in header
# AUDIO-R2d: actual SD music begins on the first DAC tick, no 440Hz beep.
assert '#include "esp_audio_r1_pcm.h"' not in runtime
assert "kWitnessFrames" not in runtime
assert "witnessWave" not in runtime
assert "EspAudioR1Pcm_dacCode" not in runtime
assert "witness=none source=SD-ring" in runtime
assert "EspEntranceMusic_pumpGameplay(void)" in api
assert "if (ok && publish) EspEntranceMusic_pumpGameplay();" in frame
assert "if (EspAssetPack_isOpen()) EspAssetPack_close();" in frame
assert frame.index("if (EspAssetPack_isOpen()) EspAssetPack_close();") < frame.index("if (ok && publish) EspEntranceMusic_pumpGameplay();")
assert "if (phase!=kPlay || !fileOpen || ring==nullptr) return;" in runtime
assert "pumpCalls=%u pumpReads=%u" in runtime
assert "ringConsumed=count+1U" in runtime
assert 'signalReport("3s")' in runtime
assert 'signalReport("STREAM")' in runtime
assert 'signalReport("FINAL")' in runtime
assert "publishedPrefixFnv" in runtime and "consumedPrefixFnv" in runtime
assert "EspEntranceAdpcm_decodeBlockObserved(" in header
assert "pcm16MinMax" in runtime and "publishedDacMinMax" in runtime
assert "gainDiv=14" in runtime and "softKneeFrames" in runtime
assert "EspNativeAudioIntent_musicVolumePercent()" in runtime
assert "volume)/100" in runtime
assert "masterVolume=" in runtime
assert "pcm/14" in header and "magnitude=96+(excess*16)/(excess+16)" in header
assert "scaled < -96 || scaled > 96" in header
assert "MALLOC_CAP_INTERNAL" in runtime and "DAC_CHANNEL_1" not in runtime

compiler = shutil.which("cc")
assert compiler, "C compiler required"
harness = r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "esp_audio_entrance_adpcm.h"

static void u16(uint8_t* p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void u32(uint8_t* p,uint32_t v) {
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
}
static void fixture(uint8_t* wav) {
    memset(wav,0,68);
    memcpy(wav,"RIFF",4);u32(wav+4,60);memcpy(wav+8,"WAVE",4);
    memcpy(wav+12,"fmt ",4);u32(wav+16,20);
    u16(wav+20,0x11);u16(wav+22,1);u32(wav+24,22050);
    u32(wav+28,11025);u16(wav+32,8);u16(wav+34,4);
    u16(wav+36,2);u16(wav+38,9);
    memcpy(wav+40,"fact",4);u32(wav+44,4);u32(wav+48,9);
    memcpy(wav+52,"data",4);u32(wav+56,8);
    u16(wav+60,16384);wav[62]=0;wav[63]=0;
    /* Four zero delta bytes => 9 samples including predictor. */
}
int main(void) {
    uint8_t wav[68];EspEntranceAdpcmWav view={0};
    fixture(wav);
    assert(EspEntranceAdpcm_parse(wav,68,68,&view));
    assert(view.dataOffset==60 && view.dataBytes==8);
    assert(view.sampleFrames==9 && view.blockAlign==8);
    assert(view.samplesPerBlock==9);
    assert(EspEntranceAdpcm_parse(wav,60,68,&view));
    assert(!EspEntranceAdpcm_parse(wav,59,68,&view));
    uint8_t samples[9]={0};
    assert(EspEntranceAdpcm_decodeBlock(wav+60,8,9,samples,9,4410));
    assert(samples[0]==239);
    for(int i=1;i<9;i++) assert(samples[i]==239);
    assert(EspEntranceAdpcm_decodeBlock(wav+60,8,9,samples,9,0));
    assert(samples[0]==128);
    assert(!EspEntranceAdpcm_decodeBlock(wav+60,8,9,samples,8,0));
    wav[62]=89;
    assert(!EspEntranceAdpcm_decodeBlock(wav+60,8,9,samples,9,4410));
    wav[62]=0;wav[63]=1;
    assert(!EspEntranceAdpcm_decodeBlock(wav+60,8,9,samples,9,4410));
    fixture(wav);
    u16(wav+20,1);assert(!EspEntranceAdpcm_parse(wav,68,68,&view));
    fixture(wav);
    u16(wav+22,2);assert(!EspEntranceAdpcm_parse(wav,68,68,&view));
    fixture(wav);
    u32(wav+24,16000);assert(!EspEntranceAdpcm_parse(wav,68,68,&view));
    fixture(wav);
    u16(wav+34,16);assert(!EspEntranceAdpcm_parse(wav,68,68,&view));
    fixture(wav);
    u16(wav+38,8);assert(!EspEntranceAdpcm_parse(wav,68,68,&view));
    fixture(wav);
    u32(wav+48,10);assert(!EspEntranceAdpcm_parse(wav,68,68,&view));
    fixture(wav);
    u32(wav+56,7);assert(!EspEntranceAdpcm_parse(wav,68,68,&view));
    fixture(wav);
    memcpy(wav+40,"JUNK",4);assert(!EspEntranceAdpcm_parse(wav,68,68,&view));
    fixture(wav);
    memcpy(wav,"RIFX",4);assert(!EspEntranceAdpcm_parse(wav,68,68,&view));
    fixture(wav);
    u32(wav+4,99999);assert(!EspEntranceAdpcm_parse(wav,68,68,&view));
    /* Typical 1024-byte mono block = 2041 samples. */
    uint8_t block[1024]={0};
    uint8_t decoded[2041]={0};
    assert(EspEntranceAdpcm_decodeBlock(block,1024,2041,
                                         decoded,2041,5000));
    for(int i=0;i<2041;i++) assert(decoded[i]==128);
    assert(!EspEntranceAdpcm_decodeBlock(block,1024,2040,
                                          decoded,2041,5000));
    /* PCM16 must be observed BEFORE the DAC8 quantizer, including header
     * predictor. Verify positive and negative deltas and LOW nibble first. */
    EspEntranceAdpcmDecodeStats stats={0};
    assert(EspEntranceAdpcm_decodeBlockObserved(wav+60,8,9,
           samples,9,4410,&stats));
    assert(stats.minPcm==16384 && stats.maxPcm==16384);
    assert(stats.nonZeroPcm==9 && stats.gainLimited==9);
    uint8_t nibbleBlock[8]={0};
    nibbleBlock[2]=70;  /* step=5894 */
    nibbleBlock[4]=0x78;  /* LOW 8 => -736, then HIGH 7 => +9309 */
    assert(EspEntranceAdpcm_decodeBlockObserved(nibbleBlock,8,9,
           samples,9,4410,&stats));
    assert(samples[0]==128 && samples[1]==76 && samples[2]==239);
    assert(stats.minPcm==-736 && stats.maxPcm>=9309);
    assert(stats.nonZeroPcm==8 && stats.gainLimited>0);
    /* Host hard limits and fade: no 8-bit overflow or speaker overdrive. */
    assert(EspEntranceAdpcm_toDac(2252,4410)==236);
    assert(EspEntranceAdpcm_toDac(-2252,4410)==20);
    assert(EspEntranceAdpcm_toDac(32767,4410)==239);
    assert(EspEntranceAdpcm_toDac(-32768,4410)==17);
    assert(EspEntranceAdpcm_toDac(2252,0)==128);
    /* Compared with R2d /16, level increases at moderate PCM16 while
     * headroom remains: no hard rail or peak expansion into 0/255. */
    assert(EspEntranceAdpcm_toDac(700,4410)==178);
    assert(EspEntranceAdpcm_toDac(-700,4410)==78);
    assert(EspEntranceAdpcm_toDac(1477,4410)==229);
    assert(EspEntranceAdpcm_toDac(-1497,4410)==26);
    assert(EspEntranceAdpcm_toDac(0,4410)==128);
    assert(EspEntranceAdpcm_toDac(2252,2205)==182);
    return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / "adpcm_test.c"
    program = Path(tmp) / "adpcm_test"
    source.write_text(harness, encoding="utf-8")
    subprocess.run([compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I", str(src), str(source), "-o", str(program)], check=True)
    subprocess.run([str(program)], check=True)

print("PASS AUDIO-R2e: gentle +1.16dB gain trim, exact soft knee and unchanged R2d transport")
