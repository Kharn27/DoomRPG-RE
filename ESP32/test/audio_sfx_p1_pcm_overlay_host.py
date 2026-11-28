#!/usr/bin/env python3
"""AUDIO-SFX-P1: first actual native 5046/5060 PCM SFX inside buffered music."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
music=(src/"esp_audio_entrance_music.cpp").read_text()
header=src/"esp_native_sfx_pcm_mix.h"
gameplay=(src/"esp_native_gameplay_action_engine.c").read_text()
pickup=(src/"esp_native_gameplay_player_resources.c").read_text()
p0=(src/"esp_native_sfx_wav.h").read_text()
assert '#include "esp_native_sfx_pcm_mix.h"' in music
assert '#include "esp_native_sfx_wav.h"' in music
assert "EspAssetPack_isMapFlashActive()" in music
assert "EspAssetPack_isResident()" in music
assert "EspAssetPack_isOpen()" in music
assert 'EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)' in music
assert 'EspAssetPack_findEntry(filename,&entry)' in music
assert 'EspAssetPack_readRange(&entry,' in music
assert 'EspAssetPack_close();' in music
assert 'source=verified-flash-pak' in music
assert 'format=PCM16-mono-8000' in music
assert 'id!=5046U && id!=5060U' in music
assert 'output+kSfxLeadFrames>kRingCapacity' in music
assert 'writer-reader<output+kSfxLeadFrames' in music
assert 'kSfxLeadFrames=1024U' in music
assert 'ring[slot]=EspNativeSfxPcm_mixGain(ring[slot],pcm,id);' in music
assert 'if (!activeIntroTrack && !activeMenuTrack)\n            mixPendingSfx();' in music
# The refill producer now also observes phase==kPlay for bounded gap
# telemetry. Scope the phase ordering assertion to the actual service function.
service=music.split("void EspEntranceMusic_service(",1)[1]
assert service.index('if (phase==kPlay) {') > service.index('if (phase==kClock)')
assert service.index('mixPendingSfx(); /* SFX only over real gameplay music. */') > service.index('if (phase==kPlay) {')
assert 'if (phase!=kPlay || !fileOpen || ring==nullptr) return;' in music
assert 'phase=kPlay;' in music
assert 'RTC_IO_PAD_DAC2_REG,RTC_IO_PDAC2_DAC,code' in music
isr=music[music.index('static bool IRAM_ATTR onAudioTick'):music.index('static uint32_t queued()')]
assert 'sfx' not in isr.lower() and 'EspAssetPack' not in isr
assert 'EspEntranceMusic_pumpGameplay' in music
assert "DoomRPG.zip" not in music and "Mix_" not in music
assert 'EspNativeAudioIntent_publish(5046U,0U,3U)' in gameplay
assert gameplay.index('[CRATE] COMMIT') < gameplay.index(
    'EspNativeAudioIntent_publish(5046U,0U,3U)')
assert 'crateOutcome != ESP_NATIVE_GAMEPLAY_CRATE_OUTCOME_INVALID' in gameplay
assert '#include "esp_audio_pickup_cue.h"' in pickup
assert 'EspAudioPickupCue_weapon(applied[i].subtype)' in pickup
assert 'EspNativeAudioIntent_publish(pickupSoundId,0U,3U)' in pickup
assert pickup.index('[PLAYERRES] COMMIT') < pickup.index(
    'EspNativeAudioIntent_publish(pickupSoundId,0U,3U)')
assert 'PCM' in p0 and 'ESP_NATIVE_SFX_WAV_PCM' in music
assert 'sampleMode=uncapped-interval' in music # R2f stays intact
code=r"""
#include <assert.h>
#include <stdint.h>
#include "esp_native_sfx_pcm_mix.h"
static uint32_t countSfxIndex(uint32_t outFrames) {
    uint32_t phase=0,idx=0;
    for (uint32_t i=0;i<outFrames;i++) {
        phase+=8000;
        if (phase>=22050) {phase-=22050;idx++;}
    }
    return idx;
}
int main(void) {
    assert(EspNativeSfxPcm_outputFrames(4160,22050)==11466);
    assert(EspNativeSfxPcm_outputFrames(3520,22050)==9702);
    assert(EspNativeSfxPcm_outputFrames(13600,22050)==37485);
    assert(EspNativeSfxPcm_outputFrames(0,22050)==0);
    assert(EspNativeSfxPcm_outputFrames(4160,0)==0);
    assert(EspNativeSfxPcm_outputFrames(UINT32_MAX,22050)==0);
    assert(11466U+1024U<16384U);
    assert(9702U+1024U<16384U);
    assert(37485U+1024U>16384U);
    assert(countSfxIndex(11466)==4160);
    assert(countSfxIndex(9702)==3520);
    const uint8_t a[]={0,0,0,128,255,127,255,255};
    assert(EspNativeSfxPcm_le16(a)==0);
    assert(EspNativeSfxPcm_le16(a+2)==-32768);
    assert(EspNativeSfxPcm_le16(a+4)==32767);
    assert(EspNativeSfxPcm_le16(a+6)==-1);
    assert(EspNativeSfxPcm_mix(128,0)==128);
    assert(EspNativeSfxPcm_mix(128,512)==129);
    assert(EspNativeSfxPcm_mix(128,-512)==127);
    assert(EspNativeSfxPcm_mix(230,32767)==240);
    assert(EspNativeSfxPcm_mix(25,-32768)==16);
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-sfx-p1-") as td:
    s=Path(td)/"mix.c"
    binary=Path(td)/"mix"
    s.write_text(code)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror","-pedantic",
                    "-I",str(src),str(s),"-o",str(binary)],check=True,cwd=root)
    subprocess.run([str(binary)],check=True,cwd=root)
print("PASS AUDIO-SFX-P1: two real native gameplay cues, PCM16/8k mix over R2e ring within 46ms")


# BARREL-VISIBLE-SFX: original Combat.c produces the gun at its firing
# frame, while Game_gsprite_allocAnim produces 5061 on FIRST blast frame.
# The prior candidate published both only after THREE explosion frames, then
# added a 520ms explosion serialization: hardware heard gun AFTER animation.
# Compile the actual source gates and test the first visible frame boundaries.
legacy_combat=(root/"src/Combat.c").read_text()
legacy_game=(root/"src/Game.c").read_text()
assert "snd = (combat->attackerWeaponId + 5044);" in legacy_combat
assert "Sound_playSound(combat->doomRpg->sound, snd, 0, 5);" in legacy_combat
assert "Sound_playSound(game->doomRpg->sound, 5061, 0, 4);" in legacy_game
assert "EspNativeSfxBarrelPair" not in music
assert "sfxBarrelShotEnd" not in music
assert not (src/"esp_native_sfx_barrel_pair.h").exists()
assert '#include "esp_audio_entrance_music.h"' in gameplay
assert 'sound=%u-attack-visible barrelExplosionSound=5061-first-wave-visible' in gameplay

def real_if_after(text, marker, prefix):
    pos = text.index(marker)
    start = text.index(prefix, pos)
    brace = text.index("{", start)
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise AssertionError("unclosed gate " + marker)

gun_tag="/* BARREL-GUN-VISUAL:"
blast_tag="/* BARREL-5061-VISUAL:"
gun_gate=real_if_after(
    gameplay,gun_tag,"if ((isBarrel || barrelExplosionArmed != 0U) && frame.finalPresented != 0U &&")
blast_gate=real_if_after(
    gameplay,blast_tag,"if (waveOrdinal == 1U &&")
assert gun_gate.count("EspNativeAudioIntent_publish(")==1
assert gun_gate.count("EspEntranceMusic_pumpGameplay();")==1
assert "crateWeapon->resourceId, 0U, 5U" in gun_gate
assert blast_gate.count("EspNativeAudioIntent_publish(5061U, 0U, 4U)")==1
assert blast_gate.count("EspEntranceMusic_pumpGameplay();")==1
assert "barrelExplosionFrame == 0U" in blast_gate
assert "frame.finalPresented != 0U" in blast_gate
first_success=gameplay.index(
    'logActionFrame(&pending, animateWeapon ? "attack" : "commit", &frame);')
first_wave=gameplay.index('printf("[BARREL] WAVE-FRAME')
barrel_commit=gameplay.index('printf("[BARREL] COMMIT')
assert first_success < gameplay.index(gun_tag) < first_wave
assert first_wave < gameplay.index(blast_tag) < barrel_commit
commit_region=gameplay[barrel_commit:gameplay.index("if (isFireMiss) {",barrel_commit)]
assert "EspNativeAudioIntent_publish(5061U" not in commit_region
assert "crateWeapon->resourceId, 0U, 5U" not in commit_region
# Two separate origins, each with one first-visible-frame cue. Preserve
# the exact original barrel gate above and independently prove subtype8.
death_start=gameplay.index("int EspNativeGameplayActionEngine_runDeathBlast(")
death_end=gameplay.index("static int traceAction(",death_start)
death_blast=gameplay[death_start:death_end]
assert death_blast.count("EspNativeAudioIntent_publish(5061U, 0U, 4U)")==1
assert death_blast.index("EspNativeDynamicLineRender_renderAmbient(") < death_blast.index(
    "if (phase == 0U && frame.finalPresented != 0U)") < death_blast.index(
    "EspNativeAudioIntent_publish(5061U, 0U, 4U)") < death_blast.index(
    "DoomRPG_randNextInt(")
assert gameplay.count("EspNativeAudioIntent_publish(5061U, 0U, 4U)")==2

host = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef struct {uint8_t finalPresented;} Frame;
typedef struct {uint16_t resourceId;} Weapon;
typedef struct {uint32_t sequence;} Pending;
static Frame frame;
static const Weapon pistol={5046U}, shotgun={5047U}, mute={0U};
static const Weapon* crateWeapon;
static Pending pending={14U};
static int isBarrel;
static uint8_t waveOrdinal, barrelExplosionFrame, barrelRemoved;
static uint8_t barrelExplosionArmed;
static unsigned played, pumped, order;
static uint16_t ids[8];
static uint8_t priorities[8];
static unsigned orderAtPublish[8], orderAtPump[8];
static int EspNativeAudioIntent_publish(uint16_t id, uint8_t flags,
                                        uint8_t priority) {
    assert(flags==0U && played<8U);
    ids[played]=id;priorities[played]=priority;
    orderAtPublish[played]=++order;++played;return 1;
}
static void EspEntranceMusic_pumpGameplay(void) {
    assert(pumped<8U);
    orderAtPump[pumped]=++order;++pumped;
}
static void attack_presented(void) {
""" + gun_gate + r"""
}
static void blast_presented(void) {
""" + blast_gate + r"""
}
static void reset(void) {
    frame.finalPresented=0U;crateWeapon=&pistol;isBarrel=1;
    waveOrdinal=1U;barrelExplosionFrame=0U;
    barrelRemoved=1U;barrelExplosionArmed=1U;
    played=0U;pumped=0U;order=0U;
}
int main(void) {
    /* A failed attack frame cannot publish a speculative gunshot. */
    reset();attack_presented();assert(played==0U && pumped==0U);
    reset();isBarrel=0;barrelExplosionArmed=0U;frame.finalPresented=1U;
    attack_presented();assert(played==0U);
    /* Trapped crate uses exactly the same visible gun and blast gates. */
    reset();isBarrel=0;frame.finalPresented=1U;
    attack_presented();blast_presented();
    assert(played==2U && ids[0]==5046U && ids[1]==5061U && pumped==2U);
    reset();frame.finalPresented=1U;crateWeapon=&mute;
    attack_presented();assert(played==0U);
    reset();frame.finalPresented=1U;crateWeapon=&shotgun;
    attack_presented();assert(played==1U && ids[0]==5047U);
    /* FIRST successful firing frame, BEFORE a single explosion frame. */
    reset();frame.finalPresented=1U;
    attack_presented();assert(played==1U && pumped==1U);
    assert(ids[0]==5046U && priorities[0]==5U);
    assert(orderAtPublish[0]<orderAtPump[0]);
    frame.finalPresented=0U;
    blast_presented();assert(played==1U); /* failed blast frame */
    frame.finalPresented=1U;
    waveOrdinal=2U;blast_presented();assert(played==1U);
    waveOrdinal=1U;barrelExplosionFrame=1U;
    blast_presented();assert(played==1U);
    barrelExplosionFrame=0U;barrelRemoved=0U;
    blast_presented();assert(played==1U);
    barrelRemoved=1U;barrelExplosionArmed=0U;
    blast_presented();assert(played==1U);
    barrelExplosionArmed=1U;
    blast_presented();assert(played==2U && pumped==2U);
    assert(ids[1]==5061U && priorities[1]==4U);
    assert(orderAtPump[0]<orderAtPublish[1] &&
           orderAtPublish[1]<orderAtPump[1]);
    /* An actual no-effect barrel shot still plays 5046, not 5061. */
    reset();frame.finalPresented=1U;barrelRemoved=0U;
    barrelExplosionArmed=0U;
    attack_presented();blast_presented();
    assert(played==1U && ids[0]==5046U && pumped==1U);
    puts("BARREL-VISIBLE-SFX PASS: real C frames, 5046 before first blast, "
         "5061 only first visible wave, no 520ms serialization, no heap");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-barrel-visible-") as td:
    source=Path(td)/"barrel_visible.c"
    exe=Path(td)/"barrel_visible"
    source.write_text(host)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                    "-pedantic",str(source),"-o",str(exe)],
                   check=True,cwd=root)
    subprocess.run([str(exe)],check=True,cwd=root)
