"""BT-1 isolation: the frozen home screen and native PAK MIDI must remain test-only."""
from pathlib import Path

root=Path("ESP32")
pio=(root/"platformio.ini").read_text()
normal=pio.split("[env:esp32-cyd]",1)[1].split("[env:esp32-cyd-bringup]",1)[0]
profile=pio.split("[env:esp32-cyd-bt-test]",1)[1]
assert "-<esp_bt_test_main.cpp>" in normal
assert "-<esp_bt_test_midi.cpp>" in normal
assert "-<esp_bt_test_a2dp.cpp>" in normal
assert "-<main.cpp>" in profile
assert "DOOMRPG_ESP32_BT_TEST_PROFILE=1" in profile
assert "board_build.partitions = partitions_cyd_bt0_raw_pak.csv" in profile
assert "1703936" in profile

source=(root/"src/esp_bt_test_main.cpp").read_text()
assert '#include "main.cpp"' in source
assert "#define setup DoomRPG_ProductionSetup_Inert" in source
assert "#define loop DoomRPG_ProductionLoop_Inert" in source
assert "initializeMainMenu();" in source
assert "EspBtTestMidi_begin();" in source
assert "EspBtTestA2dp_begin();" in source
assert "EspBtTestMidi_pump();" in source
assert "EspBtTestA2dp_service();" in source
assert "input.begin();" not in source.split("void setup() {",1)[1]
assert "EspNativeGameplaySession_service(doomRpg)" not in source.split("void loop() {",1)[1]
assert "EspNativeResidentGameplay_serviceMenuExit(doomRpg)" not in source.split("void loop() {",1)[1]

bt=(root/"src/esp_bt_test_a2dp.cpp").read_text()
assert "extern \"C\" bool btInUse(void) { return true; }" in bt
assert "esp_bt_controller_mem_release(ESP_BT_MODE_BLE)" in bt
assert "esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)" not in bt
assert "EspBtTest_preflight(snapshot())" in bt
assert "EspBtTest_postflight(snapshot())" in bt
assert "ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY" in bt
assert "ESP_A2D_MEDIA_CTRL_START" in bt
assert "esp_a2d_source_register_data_callback(pcmCallback)" in bt
assert "EspBtTestMidi_fill(buffer, len)" in bt
assert "DISCOVERY no-connect" in bt

midi=(root/"src/esp_bt_test_midi.cpp").read_text()
assert '5039.mid' in midi and '5040.mid' in midi and '5043.mid' in midi
assert 'EspAssetPack_readRange' in midi
assert 'memcmp(h, "MThd"' in midi
assert 'memcmp(th, "MTrk"' in midi
assert "static bool prime(MidiTrack &t)" in midi
assert "static void note(const MidiCmd &c)" in midi
assert "44100U" in midi
cb=midi.split("int32_t EspBtTestMidi_fill(",1)[1].split("int EspBtTestMidi_isReady",1)[0]
for forbidden in ("EspAssetPack_", "SD.", "malloc(", "Serial.", "new "):
    assert forbidden not in cb, f"callback forbidden {forbidden}"
policy=(root/"src/esp_bt_test_policy.h").read_text()
assert "ESP_BTTEST_MIN_POST_HEAP8" in policy
assert "12U * 1024U" in policy
print("BT-1 host PASS: isolated frozen native menu, actual MIDI from PAK, bounded audio callback, explicit A2DP media")
