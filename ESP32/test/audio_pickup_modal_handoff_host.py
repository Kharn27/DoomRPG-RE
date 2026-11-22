#!/usr/bin/env python3
"""AUDIO-PICKUP #419: execute the REAL exported C++ handoff body on host.

The fake clock/PAK/SD/mixer are bounded controlled witnesses; the actual
AudioIntent FIFO C owner, PCM16/WAV parsers and admission policy are compiled.
This proves ordering/fail-closed/ACK ownership, NOT real DAC timing or sound.
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
music = (SRC / "esp_audio_entrance_music.cpp").read_text()
pickup = (SRC / "esp_native_gameplay_player_resources.c").read_text()
api = (SRC / "esp_audio_entrance_music.h").read_text()
gate = (SRC / "esp_audio_pickup_modal_gate.h").read_text()

def extract(source: str, signature: str) -> str:
    assert source.count(signature) == 1, signature
    begin = source.index(signature)
    open_brace = source.index("{", begin)
    depth = 0
    for idx in range(open_brace, len(source)):
        if source[idx] == "{":
            depth += 1
        elif source[idx] == "}":
            depth -= 1
            if depth == 0:
                return source[begin:idx + 1]
    raise AssertionError("Unterminated real function: " + signature)

handoff = extract(music, "int EspEntranceMusic_handoffPickupBeforeModal(")
assert '#include "esp_audio_pickup_modal_gate.h"' in music
assert 'int EspEntranceMusic_handoffPickupBeforeModal(uint32_t pickupSequence);' in api
assert 'EspAudioPickupModal_canMix(&gate)' in handoff
assert 'mixPendingSfxOne();' in handoff
assert 'EspAssetPack_close();' in handoff
assert 'if (!refill())' in handoff
assert 'refills<kCatchUpRefillBlocks' in handoff
assert 'EspNativeAudioIntent_acknowledgeThrough(' not in handoff
assert 'heap_caps_malloc' not in handoff
assert 'SD.open' not in handoff and 'DoomRPG.zip' not in handoff
assert 'ESP_NATIVE_SFX_WAV_PCM' in handoff
assert 'sfxPlayedCount==playedBefore+1U' in handoff
assert music.count('ring=(uint8_t*)heap_caps_malloc(') == 1
assert music.count('static void mixPendingSfxOne()') == 1

# Real resource owner: semantic commit+render succeeds, then publish+handoff,
# then and ONLY then the modal obtains its long-lived font/PAK lease.
assert pickup.count('EspNativeAudioIntent_publish(5060U,0U,3U)') == 1
assert pickup.count('EspNativeGameplayDialog_beginStandalone(firstWeaponDialog)') == 1
assert pickup.count('EspEntranceMusic_handoffPickupBeforeModal(') == 1
anchor = pickup.index('if (!syncRedraw ||\n        rerender(')
assert anchor < pickup.index('[PLAYERRES] COMMIT')
assert (pickup.index('[PLAYERRES] COMMIT') <
        pickup.index('EspNativeAudioIntent_publish(5060U,0U,3U)') <
        pickup.index('EspEntranceMusic_handoffPickupBeforeModal(') <
        pickup.index('EspNativeGameplayDialog_beginStandalone(firstWeaponDialog)'))
assert 'if (firstWeaponDialog != NULL && pickupCuePublished)' in pickup
assert 'ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK' in pickup

prefix = r"""
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "esp_native_audio_intent.h"
#include "esp_native_sfx_wav.h"
#include "esp_native_sfx_pcm_mix.h"
#include "esp_audio_pickup_modal_gate.h"

enum { kSilent=0U, kPlay=3U, ESP_AUDIO_LOADING_TRACK_DEFERRED=0U };
static constexpr uint32_t kRate=22050U;
static constexpr uint32_t kRingCapacity=16384U;
static constexpr uint32_t kSfxLeadFrames=1024U;
static constexpr uint32_t kMaxBlockSamples=2041U;
static constexpr uint32_t kCatchUpRefillBlocks=4U;
static constexpr uint8_t kGenericSfxVoiceCount=3U;
static constexpr uint32_t ESP_ASSET_PACK_FLAG_DIRECTORY=1U;
#define ESP_ASSET_PACK_DEFAULT_PATH "/DoomRPG-ESP32.pak"

struct EspAssetPackEntry { uint32_t flags,size; };
struct FakeVoice { bool active; };
static uint8_t phase=kPlay;
static uint8_t fakeRing=0;
static uint8_t* ring=&fakeRing;
static bool fileOpen=true, activeIntroTrack=false, activeMenuTrack=false;
static uint32_t activeGameplayTrack=1U;
static volatile uint32_t rd=0U, wr=12000U;
static uint32_t sfxSeenSequence=0U,sfxBusyUntil=0U;
static uint32_t sfxPlayedCount=0U,sfxRejectedCount=0U;
static FakeVoice fireSfx={false},genericSfx[kGenericSfxVoiceCount]={};
static bool packOpen=false, musicSafe=true, mapFlash=true, resident=true;
static bool pakPresent=true, validWav=true, readSucceeds=true;
static bool refillSucceeds=true, simPartial=false;
static uint32_t pakOpens=0U, pakCloses=0U, pakReads=0U;
static uint32_t refills=0U, mixerCalls=0U, closeMusicCalls=0U;
static const char* failure="simulated";
static uint32_t queued() { return wr-rd; }
static bool EspAssetPack_isOpen() { return packOpen; }
static bool EspAssetPack_musicSdSafe() { return musicSafe; }
static bool EspAssetPack_isMapFlashActive() { return mapFlash; }
static bool EspAssetPack_isResident() { return resident; }
static int EspAssetPack_open(const char*) {
    if (packOpen || !pakPresent) return 0;
    packOpen=true; ++pakOpens; return 1;
}
static void EspAssetPack_close() {
    assert(packOpen);
    packOpen=false; ++pakCloses;
}
static int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* e) {
    if (!packOpen || std::strcmp(name,"5060.wav")!=0) return 0;
    e->flags=0U;e->size=44U+7040U;return 1;
}
static void le16(uint8_t* p,uint16_t n) {
    p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);
}
static void le32(uint8_t* p,uint32_t n) {
    for (int i=0;i<4;i++) p[i]=(uint8_t)(n>>(8*i));
}
static int EspAssetPack_readRange(
    const EspAssetPackEntry* e,uint32_t offset,uint8_t* p,uint32_t n) {
    assert(e && packOpen && offset==0U && n>=44U);
    if (!readSucceeds) return 0;
    std::memset(p,0,n);
    std::memcpy(p,"RIFF",4);le32(p+4,e->size-8U);
    std::memcpy(p+8,"WAVEfmt ",8);
    le32(p+16,16U);
    le16(p+20,1U);le16(p+22,1U);
    le32(p+24,8000U);le32(p+28,16000U);
    le16(p+32,2U);le16(p+34,16U);
    std::memcpy(p+36,"data",4);le32(p+40,7040U);
    if (!validWav) p[0]='!';
    ++pakReads;return 1;
}
static bool refill() {
    ++refills;
    if (!refillSucceeds) return false;
    uint32_t space=kRingCapacity-queued();
    uint32_t added=space<kMaxBlockSamples?space:kMaxBlockSamples;
    wr+=added;
    return true;
}
static void closeMusic(const char*) {
    ++closeMusicCalls;
    phase=kSilent;fileOpen=false;ring=nullptr;
}
static void mixPendingSfxOne() {
    /* Deterministic mock of the EXISTING mixer's ACK-after-mix contract.
       The real mixer body remains untouched and CI runs its normal tests. */
    assert(phase==kPlay && !packOpen);
    EspNativeAudioIntent next={};
    assert(EspNativeAudioIntent_nextAfter(sfxSeenSequence,&next));
    assert(next.resourceId==5060U && next.sequence==sfxSeenSequence+1U);
    ++mixerCalls;
    sfxSeenSequence=next.sequence;
    EspNativeAudioIntent_acknowledgeThrough(sfxSeenSequence);
    if (simPartial) ++sfxRejectedCount;
    else ++sfxPlayedCount;
}
"""
suffix = r"""
static void resetFake(uint32_t q=12000U) {
    EspNativeAudioIntent_reset();
    assert(EspNativeAudioIntent_setSoundEnabled(1));
    phase=kPlay;ring=&fakeRing;fileOpen=true;
    activeIntroTrack=false;activeMenuTrack=false;activeGameplayTrack=1U;
    rd=0U;wr=q;sfxSeenSequence=0U;sfxBusyUntil=0U;
    sfxPlayedCount=0U;sfxRejectedCount=0U;
    fireSfx.active=false;
    for (auto &v:genericSfx) v.active=false;
    packOpen=false;musicSafe=true;mapFlash=true;resident=true;
    pakPresent=true;validWav=true;readSucceeds=true;
    refillSucceeds=true;simPartial=false;
    pakOpens=pakCloses=pakReads=refills=mixerCalls=closeMusicCalls=0U;
}
static uint32_t newPickup() {
    assert(EspNativeAudioIntent_publish(5060U,0U,3U));
    return EspNativeAudioIntent_count();
}
static void pending(uint32_t n) {
    EspNativeAudioIntentStats stats={};
    EspNativeAudioIntent_getStats(&stats);
    assert(stats.pending==n);
    assert(sfxSeenSequence==0U);
    assert(mixerCalls==0U);
    assert(!packOpen);
    assert(pakOpens==pakCloses);
}
int main() {
    assert(EspNativeSfxPcm_outputFrames(3520U,kRate)==9702U);
    assert(9702U+kSfxLeadFrames<kRingCapacity);

    resetFake();
    uint32_t seq=newPickup();
    assert(EspEntranceMusic_handoffPickupBeforeModal(seq)==1);
    assert(sfxSeenSequence==seq && sfxPlayedCount==1U);
    assert(mixerCalls==1U);
    EspNativeAudioIntentStats s={};
    EspNativeAudioIntent_getStats(&s);
    assert(s.pending==0U && s.consumed==1U && s.audibleSfxDropped==0U);
    assert(pakOpens==pakCloses && pakReads==1U);
    /* A second handoff cannot replay an ACKed pickup. */
    assert(EspEntranceMusic_handoffPickupBeforeModal(seq)==0);
    assert(mixerCalls==1U);

    resetFake(8000U);
    seq=newPickup();
    assert(EspEntranceMusic_handoffPickupBeforeModal(seq)==1);
    assert(refills>=1U && refills<=4U);
    assert(pakOpens==pakCloses);

    resetFake(1U);
    seq=newPickup();
    assert(EspEntranceMusic_handoffPickupBeforeModal(seq)==0);
    assert(refills<=4U);
    pending(1U);

    resetFake(10725U);
    seq=newPickup();
    assert(EspEntranceMusic_handoffPickupBeforeModal(seq)==1);
    assert(refills>=1U);
    resetFake(10726U);
    seq=newPickup();
    assert(EspEntranceMusic_handoffPickupBeforeModal(seq)==1);
    assert(refills==0U);

    resetFake();
    assert(EspNativeAudioIntent_publish(5046U,0U,3U));
    seq=newPickup();
    assert(EspEntranceMusic_handoffPickupBeforeModal(seq)==0);
    pending(2U); /* never skip a preceding gunshot */

    resetFake();
    seq=newPickup();fireSfx.active=true;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();
    seq=newPickup();genericSfx[2].active=true;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();
    seq=newPickup();sfxBusyUntil=10000U;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();
    seq=newPickup();packOpen=true;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));
    assert(sfxSeenSequence==0U && mixerCalls==0U);
    /* A separate modal owner still holds this lease; no read allowed. */
    assert(pakReads==0U);
    packOpen=false;pending(1U);

    resetFake();seq=newPickup();musicSafe=false;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();seq=newPickup();mapFlash=false;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();seq=newPickup();resident=false;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();seq=newPickup();phase=kSilent;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();seq=newPickup();activeMenuTrack=true;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();seq=newPickup();activeIntroTrack=true;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();seq=newPickup();activeGameplayTrack=ESP_AUDIO_LOADING_TRACK_DEFERRED;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();seq=newPickup();EspNativeAudioIntent_setSoundEnabled(0);
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);

    resetFake();seq=newPickup();validWav=false;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();seq=newPickup();readSucceeds=false;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();seq=newPickup();pakPresent=false;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake(8000U);seq=newPickup();refillSucceeds=false;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));
    assert(closeMusicCalls==1U && mixerCalls==0U);

    resetFake();seq=newPickup();simPartial=true;
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));
    EspNativeAudioIntent_getStats(&s);
    assert(mixerCalls==1U && s.pending==0U); /* never replay a partial */

    resetFake();
    assert(EspNativeAudioIntent_publish(5060U,0x80U,3U));
    seq=EspNativeAudioIntent_count();
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq));pending(1U);
    resetFake();seq=newPickup();
    assert(!EspEntranceMusic_handoffPickupBeforeModal(seq+1U));pending(1U);

    std::puts("PASS AUDIO-PICKUP #419: real C++ owner + real C FIFO/WAV/PCM gate, 24 bounded scenarios");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-audio-pickup-419-") as temp:
    d = Path(temp)
    code = d / "pickup_handoff.cpp"
    code.write_text(prefix + "\n" + handoff + "\n" + suffix, encoding="utf-8")
    obj = d / "audio_intent.o"
    exe = d / "pickup_handoff"
    subprocess.run(
        ["cc", "-std=c11", "-O0", "-Wall", "-Wextra", "-Werror",
         "-I", str(SRC), "-c", str(SRC / "esp_native_audio_intent.c"),
         "-o", str(obj)], check=True)
    subprocess.run(
        ["c++", "-std=c++17", "-O0", "-Wall", "-Wextra", "-Werror",
         "-I", str(SRC), str(code), str(obj), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=10)
print("PASS AUDIO-PICKUP #419: source order + native owner boundary + single ACK")
