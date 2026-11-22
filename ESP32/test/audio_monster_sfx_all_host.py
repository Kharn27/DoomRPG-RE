#!/usr/bin/env python3
"""All 14 Doom RPG monster sound families: exact legacy corpus, bounded voice."""
import re
import subprocess
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
legacy = (root / "src/EntityMonster.c").read_text()
native = (root / "ESP32/src/esp_native_monster_sfx_table.h").read_text()
music = (root / "ESP32/src/esp_audio_entrance_music.cpp").read_text()
visual = (root / "ESP32/src/esp_native_gameplay_monster_attack_visual.c").read_text()
combat = (root / "ESP32/src/esp_native_gameplay_monster_combat.c").read_text()
pack = (root / "ESP32/src/esp_asset_pack.cpp").read_text()
intent = (root / "ESP32/src/esp_native_audio_intent.c").read_text()

def parse(block):
    clean = "\n".join(line.split("//", 1)[0] for line in block.splitlines())
    return [int(x) for x in re.findall(r"\b[0-9]+\b", clean)]

lhs = re.search(r"static unsigned short monsterSounds\[168\]\s*=\s*\{(.*?)\};", legacy, re.S)
rhs = re.search(r"static const uint16_t sounds\[14\]\[12\]\s*=\s*\{(.*?)\};", native, re.S)
assert lhs and rhs, "Missing exact source sound matrices"
before, after = parse(lhs.group(1)), parse(rhs.group(1))
assert len(before) == len(after) == 168, (len(before), len(after))
assert before == after, [(i, a, b) for i, (a, b) in enumerate(zip(before, after)) if a != b]
for family in range(14):
    row=after[family*12:(family+1)*12]
    assert row[4] and row[5] and row[6] and row[8], (family, row)
    assert all(v==0 or 5042<=v<=5138 for v in row), (family, row)
assert after[6*12+4] == 5072 and after[6*12+5] == 5096
assert "monster->mType" in visual and "EspNativeMonsterSfx_lookup(" in visual
assert "pendingAttackSoundAtMs=DoomRPG_GetUpTimeMS()+ATTACK_VISUAL_FRAME_MS;" in visual
assert "EspNativeAudioIntent_publish(pendingAttackSoundId,0U,3U)" in visual
assert "EspNativeAudioIntent_publish(shotSound,0U,5U)" in combat
assert "EspNativeAudioIntent_publish(consequenceSound,0U,lethal?5U:2U)" in combat
assert "consumePainSound(" in combat and "consumeDeathSound(" in combat
assert "constexpr uint8_t kGenericSfxVoiceCount=3U;" in music
assert "EspNativeAudioIntent_nextAfter(sfxSeenSequence,&next)" in music
assert "EspNativeSfxPcm_outputFrames(meta.sampleFrames,kRate)" in music
assert "EspNativeSfxPcm_publishedSpan(" in music
assert "if (n>8192U) n=8192U;" not in music
assert "if (!EspAssetPack_musicSdSafe()) return;" in music
assert "return !openReady || (physicalUsesMapFlash && !sourceProbeReady);" in pack
assert "retainIntent();" in intent
# AUDIO #377: real producer/consumer ownership is distinct from PCM mixing.
assert music.count("EspNativeAudioIntent_acknowledgeThrough(sfxSeenSequence);") == 3
assert music.count("EspNativeAudioIntent_discardThrough(sfxSeenSequence);") == 2
assert music.count("EspNativeAudioIntent_noteConsumerGap(sfxSeenSequence,intent->sequence);") == 3
assert "EspNativeAudioIntent_notePlaybackDrop();" in music
assert "[AUDIOINTENT] AUDIT reason=%s queueOverflow=%u" in music
assert "EspNativeAudioIntent_noteConsumerGap(uint32_t before," in intent
assert "static EspNativeAudioIntentStats intentStats;" in intent
assert "static void mixPendingSfxOne()" in music
assert "static void mixPendingSfx()" in music
assert "for (uint8_t i=0U;i<4U;++i)" in music
assert "[SFXP1] STREAM-FALLBACK id=%u" in music
# Music must be refilled BEFORE flash-backed SFX decoding in the post-frame
# pump. Previous order starved the 22kHz DAC during heavy monster sequences.
pump=music.split("void EspEntranceMusic_pumpGameplay(void) {",1)[1]
assert pump.index("if (queued()<=kRingCapacity-kMaxBlockSamples)") < pump.index("if (phase==kPlay) mixPendingSfx();")
assert "const uint32_t burstLimit=catchUp?" in pump
assert "kCatchUpRefillBlocks:kNormalRefillBlocks;" in pump
assert "for (uint32_t burst=0U;burst<burstLimit;burst++)" in pump
assert "kCatchUpRefillBlocks=4U;" in music
assert "lastMusicPublishMs=0U; /* ignore 1s clock-calibration prestart gap */" in music
assert "maxPublishGapMs=%u gapsOver500Ms=%u" in music
assert "if (phase==kPlay) mixPendingSfx();" in pump # even with full ring
assert "EspNativeGameplayMonsterCombat_painSettling(" in visual
assert "reason=committed-pain6/250ms" in visual
assert "EspNativeMonsterAmbience_sightSettling(" in visual
assert "guardMs=350" in visual
assert "painSoundHoldUntilMs=DoomRPG_GetUpTimeMS()+MONSTER_PAIN_MS;" in combat
assert "if (phase==kPlay) mixPendingSfx();" in music
# Sight sound belongs after the first confirmed physical frame of a door
# animation, rather than after its final frame and the monster-turn batch.
pump=music.split("void EspEntranceMusic_pumpGameplay(void) {",1)[1]
assert pump.index("EspNativeMonsterAmbience_service(millis(),true,false);") < pump.index("if (phase==kPlay) mixPendingSfx();"), "Late sight after audio mix"
assert pump.index("if (!EspAssetPack_musicSdSafe()) return;") < pump.index("EspNativeMonsterAmbience_service(millis(),true,false);")
assert "EspNativeMonsterAmbience_service(millis()," in music # main-loop fallback remains

with tempfile.TemporaryDirectory(prefix="doomrpg-monster-sfx-") as d:
    target = Path(d) / "intent"
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root / "ESP32/src"),
                    str(root / "ESP32/test/audio_monster_sfx_queue_host.c"),
                    str(root / "ESP32/src/esp_native_audio_intent.c"),
                    "-o", str(target)], check=True)
    subprocess.run([str(target)], check=True)
print("[MONSTERSFX] PASS 14 exact families / 168 legacy sound IDs, every attack/pain/death, 3 bounded streaming PCM voices, FIFO and flash-backed dialog music")
