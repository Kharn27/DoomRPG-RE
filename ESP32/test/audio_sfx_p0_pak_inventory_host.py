#!/usr/bin/env python3
"""AUDIO-SFX-P0: standalone strict WAV metadata and PAK lease contract."""
from __future__ import annotations

import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
SOURCE = ROOT / "ESP32/src/esp_native_sfx_probe.cpp"
HEADER = ROOT / "ESP32/src/esp_native_sfx_wav.h"
MAIN = ROOT / "ESP32/src/main.cpp"
SOUND = ROOT / "src/Sound.c"
PACK = ROOT / "ESP32/tools/build_asset_pack.py"

probe = SOURCE.read_text()
main = MAIN.read_text()
sound = SOUND.read_text()
pack = PACK.read_text()
assert "static soundTable[MAX_AUDIOFILES]" in sound
assert '"%03d.wav", soundTable[i]' in sound
assert "Mix_LoadWAV_RW" in sound and "readZipFileEntry" in sound
assert "Pack v2 stores every ZIP entry uncompressed" in pack
for resource in (5042, 5044, 5045, 5046, 5060, 5061, 5062, 5065, 5134):
    assert f"{resource}" in sound
    assert f"{resource}U" in probe
assert "EspAssetPack_isMapFlashActive()" in probe
assert "EspAssetPack_isResident()" in probe
assert "EspAssetPack_isOpen()" in probe
assert "EspAssetPack_readRange(&entry,0U,header,length)" in probe
assert "EspAssetPack_close();" in probe
assert "readZipFileEntry" not in probe and "SD.open" not in probe
assert "Mix_" not in probe and "malloc(" not in probe.lower()
assert "EspNativeSfxProbe_service(EspNativeGameplaySession_isActive());" in main
assert main.index("EspNativeGameplaySession_service(doomRpg);") < main.index(
    "EspNativeSfxProbe_service(EspNativeGameplaySession_isActive());"
) < main.index(
    "EspEntranceMusic_service(EspNativeGameplaySession_isActive(), sdReady);"
)

C = r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "esp_native_sfx_wav.h"
static void le16(uint8_t* p,uint16_t n) { p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8); }
static void le32(uint8_t* p,uint32_t n) {
    p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);
    p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);
}
static void pcm(uint8_t* p,uint16_t ch,uint16_t bits,uint32_t hz) {
    memset(p,0,128);
    memcpy(p,"RIFF",4);le32(p+4,40);
    memcpy(p+8,"WAVEfmt ",8);le32(p+16,16);
    le16(p+20,1);le16(p+22,ch);le32(p+24,hz);
    le16(p+32,(uint16_t)(ch*bits/8));
    le32(p+28,hz*(uint32_t)(ch*bits/8));
    le16(p+34,bits);
    memcpy(p+36,"data",4);le32(p+40,4);
}
static void ima(uint8_t* p) {
    memset(p,0,128);
    memcpy(p,"RIFF",4);le32(p+4,1076);
    memcpy(p+8,"WAVEfmt ",8);le32(p+16,20);
    le16(p+20,0x11);le16(p+22,1);le32(p+24,22050);
    le32(p+28,11025);le16(p+32,1024);le16(p+34,4);
    le16(p+36,2);le16(p+38,2041);
    memcpy(p+40,"fact",4);le32(p+44,4);le32(p+48,2041);
    memcpy(p+52,"data",4);le32(p+56,1024);
}
int main(void) {
    uint8_t wav[128]={0};
    EspNativeSfxWavInfo meta;
    assert(EspNativeSfxWav_probe(NULL,64,64,&meta)==ESP_NATIVE_SFX_WAV_BAD);
    assert(EspNativeSfxWav_probe(wav,128,128,NULL)==ESP_NATIVE_SFX_WAV_BAD);

    pcm(wav,1,8,11025);
    assert(EspNativeSfxWav_probe(wav,48,48,&meta)==ESP_NATIVE_SFX_WAV_PCM);
    assert(meta.channels==1 && meta.bitsPerSample==8);
    assert(meta.sampleRate==11025 && meta.dataBytes==4);
    assert(meta.dataOffset==44 && meta.sampleFrames==4);
    pcm(wav,2,16,22050);
    assert(EspNativeSfxWav_probe(wav,48,48,&meta)==ESP_NATIVE_SFX_WAV_PCM);
    assert(meta.channels==2 && meta.bitsPerSample==16 && meta.sampleFrames==1);

    ima(wav);
    assert(EspNativeSfxWav_probe(wav,60,1084,&meta)==ESP_NATIVE_SFX_WAV_IMA_MONO);
    assert(meta.formatTag==17 && meta.hasFact && meta.sampleFrames==2041);
    assert(meta.samplesPerBlock==2041 && meta.dataOffset==60);
    le16(wav+38,2000);
    assert(EspNativeSfxWav_probe(wav,60,1084,&meta)==ESP_NATIVE_SFX_WAV_BAD);

    pcm(wav,1,8,11025);
    le16(wav+20,2);
    assert(EspNativeSfxWav_probe(wav,48,48,&meta)==ESP_NATIVE_SFX_WAV_OTHER);
    assert(meta.formatTag==2 && meta.sampleRate==11025);
    pcm(wav,1,8,11025);
    le32(wav+4,100000U);
    assert(EspNativeSfxWav_probe(wav,48,48,&meta)==ESP_NATIVE_SFX_WAV_BAD);
    pcm(wav,1,8,11025);
    le32(wav+40,1000);
    assert(EspNativeSfxWav_probe(wav,48,48,&meta)==ESP_NATIVE_SFX_WAV_BAD);
    pcm(wav,1,8,11025);
    le32(wav+16,700);
    le32(wav+4,740);
    assert(EspNativeSfxWav_probe(wav,48,748,&meta)==ESP_NATIVE_SFX_WAV_HEADER_TOO_FAR);
    pcm(wav,1,16,11025);
    le16(wav+32,1);
    assert(EspNativeSfxWav_probe(wav,48,48,&meta)==ESP_NATIVE_SFX_WAV_BAD);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doomrpg-sfx-p0-") as td:
    source = pathlib.Path(td) / "probe.c"
    binary = pathlib.Path(td) / "probe"
    source.write_text(C)
    subprocess.run(
        ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
         "-I", str(HEADER.parent), str(source), "-o", str(binary)],
        check=True, cwd=ROOT
    )
    subprocess.run([str(binary)], check=True, cwd=ROOT)
print("PASS AUDIO-SFX-P0: 512B PAK-only WAV inventory, C11 parser and strict fail-closed bounds")
