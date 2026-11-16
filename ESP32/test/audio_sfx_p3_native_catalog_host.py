#!/usr/bin/env python3
"""AUDIO-SFX-P3: exact 95-ID desktop table, 3 MIDI + 92 native WAV catalog."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
src=root/"ESP32/src"
sound=(root/"src/Sound.c").read_text()
probe=(src/"esp_native_sfx_probe.cpp").read_text()
combat=(src/"esp_native_gameplay_monster_combat.c").read_text()
music=(src/"esp_audio_entrance_music.cpp").read_text()
workflow=(root/".github/workflows/esp32-cyd.yml").read_text()
table=re.search(r"static soundTable\[MAX_AUDIOFILES\] = \{([\s\S]*?)\};",sound)
assert table
original=list(map(int,re.findall(r"\b\d{4}\b",table.group(1))))
assert len(original)==95 and len(set(original))==95
midi={5039,5040,5043}
p0={5042,5044,5045,5046,5060,5061,5062,5065,5134}
cat=re.search(r"kP3Ids\[\]=\{([\s\S]*?)\};",probe)
assert cat
p3=list(map(int,re.findall(r"(\d+)U",cat.group(1))))
assert len(p3)==83 and len(set(p3))==83
assert not(set(p3)&(p0|midi))
assert set(p3)|p0==set(original)-midi
assert 5081 in p3
enemy=(5082,5084,5085,5089,5090,5091,5095,5099,5101,
       5102,5107,5110,5111,5113,5117,5118,5122,5123,
       5126,5129,5137,5138)
for ident in enemy:
    assert ident in p3 and f"{ident}U" in combat
for marker in ("painSounds","deathSounds","consumePainSound","consumeDeathSound"):
    assert marker in combat
assert "[SFXP3] BEGIN remaining=83 p0=9 totalOriginalWav=92" in probe
assert "[SFXP3] END scanned=83 p0=9 totalOriginalWav=92" in probe
assert "[SFXP3] ENTRY id=" in probe
assert probe.index("[SFXP0] END scanned=")<probe.index("[SFXP3] BEGIN")
for token in ("EspNativeSfxWav_probe(header,len,entry.size,&meta)",
              "EspAssetPack_isMapFlashActive()","EspAssetPack_isResident()",
              "EspAssetPack_isOpen()","EspAssetPack_close();",
              "EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)"):
    assert token in probe
assert "SD.open" not in probe and "readZipFileEntry" not in probe
assert "malloc(" not in probe.lower() and "playback=no" in probe
assert "id!=5045U && id!=5046U && id!=5060U" in music
assert "Test AUDIO-SFX-P3 complete original WAV catalog" in workflow
print("PASS AUDIO-SFX-P3: 95 soundTable = 3 MIDI + 9 P0 WAV + 83 P3 WAV; combat references audited, no playback")
