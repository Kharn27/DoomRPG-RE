"""BT-0 Arduino-ESP32 2.0.17 boot guard and fail-closed Classic contract."""
from pathlib import Path

src = Path("ESP32/src/esp_bt0_probe.cpp").read_text()
pio = Path("ESP32/platformio.ini").read_text()
start = src[src.index("static void start() {"):src.index("} // namespace")]
boot = src[src.index('extern "C" bool btInUse'):src.index("namespace {")]

assert 'extern "C" bool btInUse(void) { return true; }' in boot
assert 'extern "C" void initVariant(void)' in boot
assert 'esp_bt_controller_mem_release(ESP_BT_MODE_BLE)' in boot
assert 'esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)' not in src
assert boot.index("esp_bt_controller_get_status()") < boot.index("esp_bt_controller_mem_release(ESP_BT_MODE_BLE)")
assert boot.index("esp_bt_controller_mem_release(ESP_BT_MODE_BLE)") < boot.index("bt0Boot.called = true;")
assert '[BT0] BOOT_HOOK btInUse=1' in start
assert 'FAIL-CLOSED boot-BT-memory-not-reserved' in start
assert 'bt0Boot.bleRelease != ESP_OK' in start
assert 'controllerBefore != ESP_BT_CONTROLLER_STATUS_IDLE' in start
assert start.index("EspBt0_preflight(snapshot())") < start.index("esp_bt_controller_init(&cfg)")
assert start.index("cfg.mode = ESP_BT_MODE_CLASSIC_BT") < start.index("esp_bt_controller_init(&cfg)")
assert start.index("esp_bt_controller_init(&cfg)") < start.index("esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)")
assert "esp_bt_controller_mem_release(" not in start  # before game allocation ONLY
assert "DOOMRPG_ESP32_BT0_PROBE=1" in pio
print("BT0 Arduino weak btInUse guard PASS: Classic retained, BLE early reclaim, fail closed")
