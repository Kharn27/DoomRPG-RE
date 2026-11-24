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


# BARREL-5061 P1: actual committed C branch extracted into a host harness.
# Original Game_gsprite_allocAnim(index=1) plays 5061 with priority=4.
legacy=(root/"src/Game.c").read_text()
assert "Sound_playSound(game->doomRpg->sound, 5061, 0, 4);" in legacy
barrel=gameplay[gameplay.index('if (isBarrel) {\n            printf("[BARREL] COMMIT'):]
barrel=barrel[:barrel.index("\n        if (isFireMiss) {")]
cue="if (barrelRemoved != 0U && barrelExplosionArmed != 0U)"
assert barrel.count(cue)==1
assert barrel.count("EspNativeAudioIntent_publish(5061U, 0U, 4U)")==1
assert barrel.index("[BARREL] COMMIT") < barrel.index(cue)
assert gameplay.count("EspNativeAudioIntent_publish(5061U, 0U, 4U)")==1
assert "barrelExplosionSound=5061-on-commit" in barrel
statement=barrel[barrel.index(cue):]
statement=statement[:statement.index(";")+1]
barrel_test=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint8_t barrelRemoved, barrelExplosionArmed;
static unsigned published, lastId, lastFlags, lastPriority;
static int EspNativeAudioIntent_publish(uint16_t id, uint8_t flags,
                                        uint8_t priority) {
    ++published;lastId=id;lastFlags=flags;lastPriority=priority;
    return 1;
}
static void commit(void) {
""" + statement + r"""
}
int main(void) {
    barrelRemoved=0U;barrelExplosionArmed=1U;commit();assert(!published);
    barrelRemoved=1U;barrelExplosionArmed=0U;commit();assert(!published);
    barrelRemoved=1U;barrelExplosionArmed=1U;commit();
    assert(published==1U && lastId==5061U &&
           lastFlags==0U && lastPriority==4U);
    puts("BARREL-5061: real commit branch exact cue + no early/rollback cue PASS");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-barrel-5061-") as td:
    code=Path(td)/"barrel.c"
    exe=Path(td)/"barrel"
    code.write_text(barrel_test)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                    "-pedantic",str(code),"-o",str(exe)],check=True,cwd=root)
    subprocess.run([str(exe)],check=True,cwd=root)


# AUDIO-BARREL-GUN: restore legacy simultaneous shot + explosion, not just
# barrel destruction SFX. Compile the two REAL production C statement bodies.
combat=(root/"src/Combat.c").read_text()
assert "snd = (combat->attackerWeaponId + 5044);" in combat
assert "Sound_playSound(combat->doomRpg->sound, snd, 0, 5);" in combat
gun_gate="if (crateWeapon != NULL && crateWeapon->resourceId != 0U)"
assert barrel.count(gun_gate)==1
assert barrel.index("[BARREL] COMMIT") < barrel.index(gun_gate) < barrel.index(cue)
gun_expr=barrel[barrel.index(gun_gate):]
gun_expr=gun_expr[:gun_expr.index(";")+1]
barrel_sound_host=r"""
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
typedef struct {
    uint16_t resourceId;
} Weapon;
static const Weapon pistol={5046U};
static const Weapon shotgun={5047U};
static const Weapon muted={0U};
static const Weapon* crateWeapon;
static uint8_t barrelRemoved, barrelExplosionArmed;
static unsigned count;
static uint16_t ids[4];
static uint8_t priorities[4];
static int EspNativeAudioIntent_publish(uint16_t id, uint8_t flags,
                                        uint8_t priority) {
    assert(flags==0U && count<4U);
    ids[count]=id;priorities[count]=priority;++count;
    return 1;
}
static void committed_barrel(void) {
""" + gun_expr + "\n" + statement + r"""
}
static void clear(void) { count=0U;barrelRemoved=0U;barrelExplosionArmed=0U; }
int main(void) {
    /* Failed/no-shot routes must never invoke post-commit code at all.
     * Within a committed shot a hit with no explosion still sounds the gun. */
    clear();crateWeapon=&pistol;committed_barrel();
    assert(count==1U && ids[0]==5046U && priorities[0]==5U);
    clear();crateWeapon=&pistol;barrelRemoved=1U;
    barrelExplosionArmed=1U;committed_barrel();
    assert(count==2U && ids[0]==5046U && priorities[0]==5U &&
           ids[1]==5061U && priorities[1]==4U);
    clear();crateWeapon=&shotgun;barrelRemoved=1U;
    barrelExplosionArmed=1U;committed_barrel();
    assert(count==2U && ids[0]==5047U && ids[1]==5061U);
    clear();crateWeapon=&muted;barrelRemoved=0U;committed_barrel();
    assert(count==0U);
    clear();crateWeapon=NULL;committed_barrel();
    assert(count==0U);
    puts("BARREL-GUN PASS: actual post-commit C plays 5046 prio5 before 5061 prio4");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-barrel-gun-") as td:
    source=Path(td)/"barrel_gun.c"
    exe=Path(td)/"barrel_gun"
    source.write_text(barrel_sound_host)
    subprocess.run(["cc","-std=c11","-Wall","-Wextra","-Werror",
                    "-pedantic",str(source),"-o",str(exe)],check=True,cwd=root)
    subprocess.run([str(exe)],check=True,cwd=root)


# BARREL-SFX-AUDIBLE: the previous "publish both" fix started 5061 on
# exactly the same output-frame window as pistol 5046, masking/cutting the
# gun on the physical CYD. Check source plumbing AND compile real C policy.
music=(root/"ESP32/src/esp_audio_entrance_music.cpp").read_text()
pair_header=(root/"ESP32/src/esp_native_sfx_barrel_pair.h").read_text()
assert '#include "esp_native_sfx_barrel_pair.h"' in music
assert "EspNativeSfxBarrelPair_explosionStart(" in music
assert "noteBarrelGun(intent,audibleEnd);" in music
assert "noteBarrelGun(intent,start+mixed);" in music
assert "startGenericSfx(id,intent->sequence,&audibleEnd)" in music
assert "v->start=rdNow+kSfxLeadFrames;" in music
assert "if (id==5061U)" in music
assert "sfxBarrelShotSequence=0U;" in music
assert "sfxBarrelShotEnd=0U;" in music
assert "sfxBarrelShotSequence=intent->sequence;" in music
assert "sfxBarrelShotEnd=audibleEnd;" in music
assert "explosionSequence != gunSequence + 1U" in pair_header
# Reset on music stop/change and new session, not only one test map.
assert music.count("    sfxBarrelShotSequence=0U;") >= 3
harness=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_native_sfx_barrel_pair.h"
#include "esp_native_sfx_window.h"
enum { RING=16384U, LEAD=1024U, SHOT_FRAMES=11466U };
int main(void) {
    const uint32_t reader=210000U;
    const uint32_t gunStart=reader+LEAD;
    const uint32_t gunEnd=gunStart+SHOT_FRAMES;
    const uint32_t nextStart=EspNativeSfxBarrelPair_explosionStart(
        reader+LEAD,gunEnd,25U,26U);
    assert(nextStart==gunEnd);
    assert(nextStart-gunStart==SHOT_FRAMES); /* pistol .520s complete */
    /* No SFX in additional storage: the delayed voice can start AFTER
     * current music publication and waits for the ring to advance. */
    assert(nextStart > reader+11000U);
    assert(EspNativeSfxPcm_publishedSpan(
        nextStart,reader+11000U,37485U,RING)==0U);
    assert(EspNativeSfxPcm_publishedSpan(
        nextStart,nextStart+2000U,37485U,RING)==2000U);
    /* No artificial 8192 cap: original 5061 can continue for 37485
     * DAC frames in the existing 16384-byte ring progressively. */
    assert(EspNativeSfxPcm_publishedSpan(
        nextStart,nextStart+10000U,37485U,RING)==10000U);
    assert(EspNativeSfxBarrelPair_explosionStart(
        reader+LEAD,gunEnd,25U,27U)==reader+LEAD); /* unrelated */
    assert(EspNativeSfxBarrelPair_explosionStart(
        reader+LEAD,gunEnd,0U,1U)==reader+LEAD);
    assert(EspNativeSfxBarrelPair_explosionStart(
        reader+LEAD,gunEnd,UINT32_MAX,0U)==reader+LEAD);
    assert(EspNativeSfxBarrelPair_explosionStart(
        reader+LEAD,reader+500U,25U,26U)==reader+LEAD);
    printf("BARREL-SFX-AUDIBLE PASS: gun 11466 frames before 5061, "
           "unrelated immediate, future ring publish, no heap\n");
    return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="doom-barrel-sequence-") as td:
    source=Path(td)/"sequence.c"
    exe=Path(td)/"sequence"
    source.write_text(harness)
    subprocess.run(["cc","-std=c11","-O2","-Wall","-Wextra","-Werror",
                    "-pedantic","-I",str(root/"ESP32/src"),str(source),
                    "-o",str(exe)],check=True,cwd=root)
    subprocess.run([str(exe)],check=True,cwd=root)
