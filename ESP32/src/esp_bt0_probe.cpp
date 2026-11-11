#include "esp_bt0_probe.h"
#include "esp_bt0_probe_policy.h"

#include <Arduino.h>
#include <string.h>
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_heap_caps.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef DOOMRPG_ESP32_BT0_PROBE
#define DOOMRPG_ESP32_BT0_PROBE 0
#endif
#ifndef DOOMRPG_ESP32_BRINGUP_PROBES
#define DOOMRPG_ESP32_BRINGUP_PROBES 0
#endif

/* Leave bringup and trace unchanged; BT-0 is measured only in the normal
 * esp32-cyd profile. This compile-time gate avoids extra controller/host
 * linking into the two diagnostic firmwares. */
#if DOOMRPG_ESP32_BT0_PROBE && !DOOMRPG_ESP32_BRINGUP_PROBES && !defined(DOOMRPG_ESP32_TRACE_SPRITE)

/* Arduino-ESP32 2.0.17, esp32-hal-misc.c:
 *   weak btInUse() returns false unless esp32-hal-bt.o is linked.
 *   initArduino() then releases ESP_BT_MODE_BTDM irreversibly before setup.
 * Our IDF A2DP source does not link the Arduino btStart() helper, so the
 * weak false result made ESP-IDF Classic initialization impossible.
 * The strong normal-profile symbol reserves Classic at boot. */
extern "C" bool btInUse(void) { return true; }

struct EspBt0Boot {
    uint32_t heap8Before;
    uint32_t heap8After;
    uint32_t largest8Before;
    uint32_t largest8After;
    int controllerStatus;
    esp_err_t bleRelease;
    bool called;
};
static EspBt0Boot bt0Boot = {};

/* initVariant() is Arduino's WEAK hook: runs after NVS and before Doom RPG
 * setup and its allocations. Reclaim ONLY BLE here, not all of BTDM.
 * Zero Serial, SD or other game operations here: observe after gameplay.
 * Remains inert in bringup/trace because of the outer build guard. */
extern "C" void initVariant(void) {
    bt0Boot.controllerStatus = (int)esp_bt_controller_get_status();
    bt0Boot.heap8Before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    bt0Boot.largest8Before = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    if (bt0Boot.controllerStatus == (int)ESP_BT_CONTROLLER_STATUS_IDLE) {
        bt0Boot.bleRelease = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    } else {
        bt0Boot.bleRelease = ESP_ERR_INVALID_STATE;
    }
    bt0Boot.heap8After = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    bt0Boot.largest8After = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    bt0Boot.called = true;
    /* ROM UART is available before Serial.begin(). Without this witness, a
     * low-RAM crash during gameplay boot would hide the only BT reservation
     * measurements made before Doom RPG allocates its framebuffer/maps. */
    esp_rom_printf("[BT0] BOOT_EARLY ctrl=%d BLE=%ld heap8=%lu->%lu largest8=%lu->%lu\n",
                   bt0Boot.controllerStatus, (long)bt0Boot.bleRelease,
                   (unsigned long)bt0Boot.heap8Before,
                   (unsigned long)bt0Boot.heap8After,
                   (unsigned long)bt0Boot.largest8Before,
                   (unsigned long)bt0Boot.largest8After);
}

namespace {

enum State : uint8_t { WAIT_GAME, RUNNING, DRAIN, WAIT_DEINIT, OFF };
static State state = WAIT_GAME;
static bool attempted = false;
static bool controllerInit = false;
static bool controllerEnabled = false;
static bool hostInit = false;
static bool hostEnabled = false;
static bool sourceInit = false;
static bool scanStarted = false;
static bool deinitRequested = false;
static bool disconnectRequested = false;
static volatile bool connected = false;
static volatile bool connecting = false;
static volatile bool profileDeinited = false;
static uint32_t startedMs = 0;
static uint32_t stoppedMs = 0;
static uint32_t lastReportMs = 0;
static uint32_t silenceCalls = 0;
static uint32_t silenceBytes = 0;

/* Discover once. Connecting automatically is permitted only if exactly ONE
 * named BR/EDR device advertises the Rendering service class. Never connect
 * to an arbitrary first discovery result or repeatedly reconnect. */
static esp_bd_addr_t peerBda = {};
static char peerName[33] = {};
static uint8_t renderCandidates = 0;
/* GAP may report the same device several times. Bounded unique addresses
 * avoid a false ambiguous-device verdict; >8 unique renderers always refuse
 * automatic pairing. Scan logs are capped to protect gameplay latency. */
static esp_bd_addr_t rendererAddresses[8] = {};
static uint8_t discoveryLogs = 0;

static EspBt0Mem snapshot() {
    EspBt0Mem m = {};
    m.heap8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    m.largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    m.dma = heap_caps_get_free_size(MALLOC_CAP_DMA);
    m.largestDma = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    return m;
}

static void logMem(const char* label) {
    const EspBt0Mem m = snapshot();
    Serial.printf("[BT0] RAM %s heap8=%lu largest8=%lu dma=%lu largestDMA=%lu minHeap=%lu tasks=%lu\n",
                  label, (unsigned long)m.heap8, (unsigned long)m.largest8,
                  (unsigned long)m.dma, (unsigned long)m.largestDma,
                  (unsigned long)ESP.getMinFreeHeap(),
                  (unsigned long)uxTaskGetNumberOfTasks());
}

static void logErr(const char* step, esp_err_t status) {
    Serial.printf("[BT0] %s result=%s code=%ld\n", step,
                  esp_err_to_name(status), (long)status);
}

static bool equalBda(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, ESP_BD_ADDR_LEN) == 0;
}

static void printBda(const uint8_t* address, char* out) {
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             address[0], address[1], address[2],
             address[3], address[4], address[5]);
}

static int32_t silenceCallback(uint8_t* buffer, int32_t len) {
    if (len <= 0 || buffer == nullptr) return 0;
    /* A2DP media START is NEVER issued in BT-0. A defensive zero PCM
     * callback makes an unexpected read bounded and allocation-free. */
    memset(buffer, 0, (size_t)len);
    ++silenceCalls;
    silenceBytes += (uint32_t)len;
    return len;
}

static void a2dpCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* p) {
    if (!p) return;
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
            connected = p->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED;
            connecting = p->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTING ||
                         p->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTING;
            Serial.printf("[BT0] A2DP connection=%d reason=%d\n",
                          (int)p->conn_stat.state, (int)p->conn_stat.disc_rsn);
            break;
        case ESP_A2D_AUDIO_STATE_EVT:
            Serial.printf("[BT0] A2DP audioState=%d (BT-0 no media start)\n",
                          (int)p->audio_stat.state);
            break;
        case ESP_A2D_PROF_STATE_EVT:
            /* Source deinit is asynchronous; allow host/controller teardown
             * only after the profile confirms it (or a bounded timeout). */
            if (deinitRequested) profileDeinited = true;
            Serial.printf("[BT0] A2DP profile state=%d\n",
                          (int)p->a2d_prof_stat.init_state);
            break;
        default:
            break;
    }
}

static void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* p) {
    if (!p) return;
    if (event == ESP_BT_GAP_DISC_RES_EVT) {
        if (state != RUNNING) return;
        uint32_t cod = 0;
        char name[33] = {};
        for (int i = 0; i < p->disc_res.num_prop; ++i) {
            const esp_bt_gap_dev_prop_t& prop = p->disc_res.prop[i];
            if (!prop.val) continue;
            if (prop.type == ESP_BT_GAP_DEV_PROP_COD && prop.len >= (int)sizeof(cod)) {
                memcpy(&cod, prop.val, sizeof(cod));
            } else if (prop.type == ESP_BT_GAP_DEV_PROP_BDNAME) {
                size_t n = (size_t)prop.len;
                if (n > sizeof(name) - 1U) n = sizeof(name) - 1U;
                memcpy(name, prop.val, n);
                name[n] = '\0';
            } else if (prop.type == ESP_BT_GAP_DEV_PROP_EIR && name[0] == '\0') {
                uint8_t n = 0;
                uint8_t* value = esp_bt_gap_resolve_eir_data(
                    (uint8_t*)prop.val, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &n);
                if (!value) value = esp_bt_gap_resolve_eir_data(
                    (uint8_t*)prop.val, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &n);
                if (value && n) {
                    if (n > sizeof(name) - 1U) n = sizeof(name) - 1U;
                    memcpy(name, value, n);
                    name[n] = '\0';
                }
            }
        }
        const bool validCod = esp_bt_gap_is_valid_cod(cod);
        const bool renderer = validCod &&
            (esp_bt_gap_get_cod_srvc(cod) & ESP_BT_COD_SRVC_RENDERING);
        char address[18];
        printBda(p->disc_res.bda, address);
        if (discoveryLogs < 16U) {
            ++discoveryLogs;
            Serial.printf("[BT0] DISCOVERY SEEN count=%u addr=%s cod=%08lx renderer=%u named=%u name=%s\n",
                          (unsigned)discoveryLogs, address, (unsigned long)cod,
                          (unsigned)renderer, (unsigned)(name[0] != '\0'),
                          name[0] ? name : "(no-name)");
        }
        /* No name, or no A2DP Rendering service class = no pairing attempt.
         * This is a diagnostic probe, not an auto-join of arbitrary devices. */
        if (!name[0] || !renderer) return;
        const uint8_t stored = renderCandidates < 8U ? renderCandidates : 8U;
        for (uint8_t i = 0; i < stored; ++i) {
            if (equalBda(rendererAddresses[i], p->disc_res.bda)) return;
        }
        if (renderCandidates < 255U) ++renderCandidates;
        if (renderCandidates <= 8U) {
            memcpy(rendererAddresses[renderCandidates - 1U],
                   p->disc_res.bda, ESP_BD_ADDR_LEN);
        }
        Serial.printf("[BT0] DISCOVERY renderer=%u addr=%s cod=%08lx name=%s\n",
                      (unsigned)renderCandidates, address, (unsigned long)cod, name);
        if (renderCandidates == 1U) {
            memcpy(peerBda, p->disc_res.bda, ESP_BD_ADDR_LEN);
            memcpy(peerName, name, sizeof(peerName));
        }
    } else if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT &&
               p->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
        Serial.printf("[BT0] DISCOVERY done candidates=%u\n", (unsigned)renderCandidates);
        if (state == RUNNING && renderCandidates == 1 && EspBt0_postflight(snapshot())) {
            esp_err_t e = esp_a2d_source_connect(peerBda);
            logErr("A2DP_CONNECT_REQUEST", e);
            connecting = (e == ESP_OK);
        } else {
            Serial.printf("[BT0] DISCOVERY no-connect cause=%s\n",
                          renderCandidates == 0 ? "no-named-renderer" :
                          renderCandidates > 1 ? "ambiguous-multiple-renderers" :
                          "insufficient-ram");
        }
    } else if (event == ESP_BT_GAP_AUTH_CMPL_EVT) {
        Serial.printf("[BT0] GAP auth=%s\n",
                      p->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS ? "SUCCESS" : "FAIL");
    }
#if defined(CONFIG_BT_SSP_ENABLED) && CONFIG_BT_SSP_ENABLED
    else if (event == ESP_BT_GAP_CFM_REQ_EVT) {
        /* Only confirm the UNIQUE renderer we already requested. */
        bool approved = renderCandidates == 1 &&
                        equalBda(peerBda, p->cfm_req.bda) &&
                        (connecting || connected) && state == RUNNING;
        Serial.printf("[BT0] GAP SSP confirmation restricted=%s\n",
                      approved ? "yes" : "no");
        esp_bt_gap_ssp_confirm_reply(p->cfm_req.bda, approved);
    }
#endif
}

static void teardownCore() {
    if (hostEnabled) {
        logErr("BLUEDROID_DISABLE", esp_bluedroid_disable());
        hostEnabled = false;
    }
    if (hostInit) {
        logErr("BLUEDROID_DEINIT", esp_bluedroid_deinit());
        hostInit = false;
    }
    if (controllerEnabled) {
        logErr("CTRL_DISABLE", esp_bt_controller_disable());
        controllerEnabled = false;
    }
    if (controllerInit) {
        logErr("CTRL_DEINIT", esp_bt_controller_deinit());
        controllerInit = false;
    }
    state = OFF;
    logMem("STOPPED");
    Serial.printf("[BT0] OFF silenceCalls=%lu silenceBytes=%lu noMediaStart=1\n",
                  (unsigned long)silenceCalls, (unsigned long)silenceBytes);
}

static void requestStop(const char* reason) {
    if (state != RUNNING) return;
    Serial.printf("[BT0] FAIL-CLOSED shutdown=%s connected=%u\n",
                  reason, (unsigned)connected);
    state = DRAIN;
    stoppedMs = millis();
    if (scanStarted) esp_bt_gap_cancel_discovery();
}

static bool check(esp_err_t e, const char* step) {
    logErr(step, e);
    if (e != ESP_OK) {
        requestStop(step);
        return false;
    }
    logMem(step);
    return true;
}

static void start() {
    attempted = true;
    logMem("BEFORE");
    Serial.printf("[BT0] START profile=normal gameplay=active noPSRAM=%u sample=Classic-A2DP-SOURCE-no-media\n",
                  (unsigned)ESP.getPsramSize());
#if !defined(CONFIG_BT_ENABLED) || !CONFIG_BT_ENABLED || !defined(CONFIG_BT_CLASSIC_ENABLED) || !CONFIG_BT_CLASSIC_ENABLED || !defined(CONFIG_BT_A2DP_ENABLE) || !CONFIG_BT_A2DP_ENABLE
    Serial.println("[BT0] FAIL-CLOSED sdkconfig missing Classic/A2DP");
    state = OFF;
    return;
#endif
    if (ESP.getPsramSize() != 0) {
        Serial.println("[BT0] FAIL-CLOSED unexpected PSRAM board");
        state = OFF;
        return;
    }
    Serial.printf("[BT0] BOOT_HOOK btInUse=1 called=%u controller=%d BLE=%s code=%ld heap8=%lu->%lu largest8=%lu->%lu\n",
                  (unsigned)bt0Boot.called, bt0Boot.controllerStatus,
                  esp_err_to_name(bt0Boot.bleRelease), (long)bt0Boot.bleRelease,
                  (unsigned long)bt0Boot.heap8Before, (unsigned long)bt0Boot.heap8After,
                  (unsigned long)bt0Boot.largest8Before, (unsigned long)bt0Boot.largest8After);
    /* No attempt to recover already released Classic controller memory at
     * gameplay time. That would be irreversible, and only a reboot with the
     * correct strong Arduino btInUse guard can restore it. */
    if (!bt0Boot.called ||
        bt0Boot.controllerStatus != (int)ESP_BT_CONTROLLER_STATUS_IDLE ||
        bt0Boot.bleRelease != ESP_OK) {
        Serial.println("[BT0] FAIL-CLOSED boot-BT-memory-not-reserved");
        state = OFF;
        return;
    }
    const esp_bt_controller_status_t controllerBefore = esp_bt_controller_get_status();
    Serial.printf("[BT0] CTRL_STATUS before=%d idle=%d\n",
                  (int)controllerBefore, (int)ESP_BT_CONTROLLER_STATUS_IDLE);
    if (controllerBefore != ESP_BT_CONTROLLER_STATUS_IDLE) {
        Serial.println("[BT0] FAIL-CLOSED controller-not-IDLE; no takeover");
        state = OFF;
        return;
    }
    state = RUNNING;
    startedMs = millis();
    logMem("BLE_RELEASE");
    if (!EspBt0_preflight(snapshot())) {
        requestStop("preflight-after-early-BLE-release");
        return;
    }

    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    /* IDF 4.4 requires controller enable mode == the mode selected at init.
     * The Arduino default may be BTDM; we need BR/EDR exclusively. */
    cfg.mode = ESP_BT_MODE_CLASSIC_BT;
    Serial.printf("[BT0] STACK controllerMode=%d controllerConfigBytes=%u tasksBefore=%lu\n",
                  (int)cfg.mode, (unsigned)sizeof(cfg),
                  (unsigned long)uxTaskGetNumberOfTasks());
    esp_err_t e = esp_bt_controller_init(&cfg);
    if (!check(e, "CTRL_INIT")) return;
    controllerInit = true;
    e = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (!check(e, "CTRL_ENABLE")) return;
    controllerEnabled = true;
    e = esp_bluedroid_init();
    if (!check(e, "BLUEDROID_INIT")) return;
    hostInit = true;
    e = esp_bluedroid_enable();
    if (!check(e, "BLUEDROID_ENABLE")) return;
    hostEnabled = true;

    if (!check(esp_bt_gap_register_callback(gapCallback), "GAP_CALLBACK")) return;
    if (!check(esp_bt_dev_set_device_name("DoomRPG-CYD-BT0"), "LOCAL_NAME")) return;
    if (!check(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE),
               "SCAN_MODE")) return;
    if (!check(esp_a2d_register_callback(a2dpCallback), "A2DP_CALLBACK")) return;
    e = esp_a2d_source_init();
    if (!check(e, "A2DP_SOURCE_INIT")) return;
    sourceInit = true;
    if (!check(esp_a2d_source_register_data_callback(silenceCallback),
               "A2DP_SILENCE_CALLBACK")) return;

    Serial.printf("[BT0] INIT_COMPLETE elapsedMs=%lu noMediaStart=1\n",
                  (unsigned long)(millis() - startedMs));
    if (!EspBt0_postflight(snapshot())) requestStop("post-init-RAM-budget");
}

} // namespace

void EspBt0Probe_service(int gameplayActive) {
    if (state == WAIT_GAME) {
        if (gameplayActive && !attempted) start();
        return;
    }
    if (state == OFF) return;

    const uint32_t now = millis();
    if (state == RUNNING) {
        if (!gameplayActive) {
            requestStop("gameplay-exit");
            return;
        }
        if (!EspBt0_postflight(snapshot())) {
            logMem("LOW_MEMORY");
            requestStop("gameplay-RAM-budget");
            return;
        }
        if (!scanStarted && now - startedMs >= 250U) {
            scanStarted = true;
            logErr("DISCOVERY_START", esp_bt_gap_start_discovery(
                ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0));
        }
        if (now - lastReportMs >= 5000U) {
            lastReportMs = now;
            logMem("RUNNING");
            Serial.printf("[BT0] STATE connected=%u connecting=%u candidates=%u silenceCalls=%lu\n",
                          (unsigned)connected, (unsigned)connecting,
                          (unsigned)renderCandidates, (unsigned long)silenceCalls);
        }
    } else if (state == DRAIN) {
        /* The callback may still be sending connection events. Never tear
         * Bluedroid down while a link is known to be active. */
        if ((connected || connecting) && !disconnectRequested) {
            disconnectRequested = true;
            logErr("A2DP_DISCONNECT_REQUEST", esp_a2d_source_disconnect(peerBda));
        }
        if (connected || connecting) {
            if (now - stoppedMs < 3000U) return;
            Serial.println("[BT0] DISCONNECT_TIMEOUT; no further BT calls (safe stop)");
            state = OFF; /* Fail closed; do not deinit an active link. */
            return;
        }
        if (sourceInit && !deinitRequested) {
            deinitRequested = true;
            stoppedMs = now;
            logErr("A2DP_SOURCE_DEINIT_REQUEST", esp_a2d_source_deinit());
            state = WAIT_DEINIT;
            return;
        }
        teardownCore();
    } else if (state == WAIT_DEINIT) {
        if (!profileDeinited && now - stoppedMs < 2000U) return;
        if (!profileDeinited) {
            Serial.println("[BT0] PROFILE_DEINIT_TIMEOUT; no unsafe host teardown");
            state = OFF;
            return;
        }
        teardownCore();
    }
}

#else
void EspBt0Probe_service(int /*gameplayActive*/) {}
#endif

/* Linker wrapper avoids touching the byte-identical main.cpp guarded by
 * Root-J2/J3/J4/J5/J6 source-parity host tests. Every loop already calls the
 * native resident menu-exit service. Preserve its exact call ordering and
 * only observe native gameplay state AFTER the real function returns. */
#include "esp_native_gameplay_session.h"
#include "esp_native_resident_gameplay.h"
extern "C" void __real_EspNativeResidentGameplay_serviceMenuExit(struct DoomRPG_s*);
extern "C" void __wrap_EspNativeResidentGameplay_serviceMenuExit(struct DoomRPG_s* root) {
    __real_EspNativeResidentGameplay_serviceMenuExit(root);
    EspBt0Probe_service(EspNativeGameplaySession_isActive());
}
