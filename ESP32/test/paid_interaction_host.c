/* Real event/filter/opcode/continuation + capture painter; only platform,
 * PlayerState, topology, dialog, notebook and PAK I/O boundaries are mocked.
 * Raw sequence is the supplied event67 Serial witness, not a firmware special case. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "SDL.h"
#include "DoomRPG.h"
#include "esp_map_automap_state.h"
#include "esp_map_catalog.h"
#include "esp_map_line_texture_state.h"
#include "esp_map_sprite_topology.h"
#include "esp_map_ui_intent.h"
#include "esp_native_gameplay_event_chain.h"
#include "esp_native_gameplay_frame.h"
#include "esp_native_gameplay_player_state.h"
#include "esp_player_view_state.h"

static EspMapRuntimeView runtime;
static EspPlayerViewState view;
static EspNativeGameplayPlayerState player;
static EspMapByteCode commands[200];
static uint32_t events[70], clockMs;
static uint8_t states[70], removedBits[200], topology[16];
static EspMapSpriteTopologyView topologyView;
static uint16_t pixels[160 * 120 + 2];
static int dialogActive, dialogOk = 1, standalone, frameOk = 1, presentOk = 1;
static int gate, pack, noteOk = 1, noteStaged, notes, sounds, captures;
static int showOk = 1, hides, shows, renderCalls, presents;

EspNativeGameplayDialogResumeStatus __wrap_EspNativeGameplayDialog_resume(
    const EspNativeGameplayDialogClose*, EspNativeGameplayDialogResumeResult*);
int __wrap_EspNativeGameplayDialog_rollbackResume(const EspNativeGameplayDialogResumeResult*);

size_t heap_caps_get_free_size(unsigned int caps) { (void)caps; return 100000U; }
unsigned int DoomRPG_GetUpTimeMS(void) { return clockMs; }
const EspMapRuntimeView* EspMapRuntime_view(void) { return &runtime; }
int EspMapRuntime_getEvent(uint32_t index, uint32_t* value) {
    if (index >= 70U || value == NULL) return 0;
    *value = events[index]; return 1;
}
int EspMapRuntime_getByteCode(uint32_t index, EspMapByteCode* out) {
    if (index >= 200U || out == NULL) return 0;
    *out = commands[index]; return 1;
}
int EspMapScriptState_isReady(void) { return 1; }
int EspMapScriptState_getEventState(uint32_t index, uint8_t* out) {
    if (index >= 70U) return 0;
    *out = states[index]; return 1;
}
int EspMapScriptState_setEventState(uint32_t index, uint8_t value) {
    if (index >= 70U) return 0;
    states[index] = value; return 1;
}
int EspMapScriptState_isCommandRemoved(uint32_t index, uint8_t* out) {
    if (index >= 200U) return 0;
    *out = removedBits[index]; return 1;
}
int EspMapScriptState_setCommandRemoved(uint32_t index, uint8_t value) {
    if (index >= 200U) return 0;
    removedBits[index] = value; return 1;
}
const EspNativeGameplayPlayerState* EspNativeGameplayPlayerState_view(void) { return &player; }
int EspNativeGameplayPlayerState_snapshot(EspNativeGameplayPlayerState* out) { *out = player; return 1; }
int EspNativeGameplayPlayerState_restore(const EspNativeGameplayPlayerState* in) { player = *in; return 1; }
uint16_t EspNativeGameplayPlayerState_completedLevels(void) { return 0; }
const EspPlayerViewState* EspPlayerView_view(void) { return &view; }
int EspMapSpriteTopology_isReady(void) { return 1; }
const EspMapSpriteTopologyView* EspMapSpriteTopology_view(void) { return &topologyView; }
EspMapSpriteTopologyStatus EspMapSpriteTopology_previewShow(
    const EspMapEventDescriptor* d, uint32_t offset, EspMapShowResult* out) {
    (void)d; (void)offset; memset(out, 0, sizeof(*out));
    return showOk ? ESP_MAP_SPRITE_TOPOLOGY_OK : ESP_MAP_SPRITE_TOPOLOGY_RANDOM_BLOCKER;
}
EspMapSpriteTopologyStatus EspMapSpriteTopology_applyShow(
    const EspMapEventDescriptor* d, uint32_t offset, EspMapShowResult* out) {
    ++shows; topology[0]++;
    (void)d; (void)offset; memset(out, 0, sizeof(*out)); out->visualAfter = 1U;
    return showOk ? ESP_MAP_SPRITE_TOPOLOGY_OK : ESP_MAP_SPRITE_TOPOLOGY_INVALID;
}
EspMapSpriteTopologyStatus EspMapSpriteTopology_applyHide(
    const EspMapEventDescriptor* d, uint32_t offset, EspMapHideResult* out) {
    ++hides; topology[0]++;
    (void)d; (void)offset; memset(out, 0, sizeof(*out)); out->effectFlags = 1U;
    return ESP_MAP_SPRITE_TOPOLOGY_OK;
}
int EspMapLineState_isReady(void) { return 1; }
int EspMapLineTextureState_isReady(void) { return 1; }
int EspMapLineState_setLocked(uint32_t line, uint8_t locked) { (void)line; (void)locked; return 0; }
int EspMapLineTextureState_setDoorTexture(uint32_t line, uint16_t texture) { (void)line; (void)texture; return 0; }
EspMapLineUnlockStatus EspMapLineTextureState_applyUnlockCommand(
    const EspMapEventDescriptor* d, uint32_t offset, EspMapLineUnlockResult* out) {
    (void)d; (void)offset; (void)out; return ESP_MAP_LINE_UNLOCK_INVALID;
}
int EspMapAutomapState_isReady(void) { return 1; }
int EspMapState_isReady(void) { return 1; }
int EspMapAutomapState_snapshot(EspMapAutomapSnapshot* out) { (void)out; return 0; }
int EspMapAutomapState_restore(const EspMapAutomapSnapshot* out) { (void)out; return 0; }
EspMapGiveMapStatus EspMapAutomapState_applyGiveMapCommand(
    const EspMapEventDescriptor* d, uint32_t offset, EspMapGiveMapResult* out) {
    (void)d; (void)offset; (void)out; return ESP_MAP_GIVEMAP_INVALID;
}
EspMapUiIntentStatus EspMapUiIntent_build(const EspMapEventDescriptor* d, uint32_t offset, EspMapUiIntent* out) {
    memset(out, 0, sizeof(*out)); out->kind = ESP_MAP_UI_INTENT_DIALOG;
    out->codeId = commands[d->firstCommandIndex + offset].id;
    return ESP_MAP_UI_INTENT_OK;
}
int EspMapStrings_getRef(uint32_t index, EspMapStringRef* out) {
    if (index > 31U) return 0;
    memset(out, 0, sizeof(*out)); out->index = (uint16_t)index; out->length = 20;
    return 1;
}
EspMapStringReadStatus EspMapStrings_read(const EspAssetPackEntry* e, const EspMapStringRef* ref,
    char* dst, size_t cap, size_t* length) {
    (void)e; (void)length; assert(ref->index == 29U && cap >= 21U);
    strcpy(dst, "Not enough credits."); return ESP_MAP_STRING_READ_OK;
}
int EspAssetPack_isOpen(void) { return pack; }
int EspAssetPack_open(const char* path) { (void)path; assert(!pack); pack = 1; return 1; }
void EspAssetPack_close(void) { pack = 0; }
int EspAssetPack_findEntry(const char* name, EspAssetPackEntry* entry) {
    (void)name; memset(entry, 0, sizeof(*entry)); return 1;
}
EspNativeGameplayDialogBeginStatus __real_EspNativeGameplayDialog_begin(
    uint16_t event, uint8_t offset, uint32_t flags) {
    assert(event == 67U && offset == 13U && flags == 0x500U);
    for (unsigned i = 14U; i < 23U; ++i) assert(removedBits[148U + i] == 1U);
    if (!dialogOk) return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_IO_FAILED;
    dialogActive = 1; return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK;
}
int EspNativeGameplayDialog_isActive(void) { return dialogActive; }
void EspNativeGameplayDialog_reset(void) { dialogActive = 0; }
EspNativeGameplayDialogBeginStatus EspNativeGameplayDialog_beginStandalone(const char* text) {
    assert(strcmp(text, "Not enough credits.") == 0); ++standalone;
    dialogActive = 1; return ESP_NATIVE_GAMEPLAY_DIALOG_BEGIN_OK;
}
int EspNativeGameplayNotePrefix_prepare(uint16_t event, uint8_t offset) {
    assert(event == 67U && offset == 20U && !noteStaged);
    if (!noteOk) return 0;
    noteStaged = 1; return 1;
}
void EspNativeGameplayNotePrefix_commit(void) { assert(noteStaged); ++notes; noteStaged = 0; }
void EspNativeGameplayNotePrefix_cancel(void) { noteStaged = 0; }
int EspNativeAudioIntent_publish(uint16_t sound, uint8_t flags, uint8_t priority) {
    assert(sound == 5091U && flags == 0 && priority == 3); ++sounds; return 1;
}
int EspNativeGameplayFacingLabel_refresh(const char* reason) { (void)reason; return 1; }
int EspNativeGameplayPresentGate_armOne(void) { assert(!gate); gate = 1; return 1; }
void EspNativeGameplayPresentGate_cancel(void) { gate = 0; }
void* Esp32PlatformVideo_framebuffer(void) { return pixels + 1; }
size_t Esp32PlatformVideo_framebufferSizeBytes(void) { return 160U * 120U * 2U; }
int __real_Esp32PlatformVideo_present(void) {
    ++presents; return presentOk;
}
int Esp32PlatformVideo_present(void) {
    if (gate) { gate = 0; return 1; }
    return __real_Esp32PlatformVideo_present();
}
int EspNativeGameplayFrame_renderTurn(struct Render_s* render, uint8_t angle,
    EspNativeGameplayFrameStats* out) {
    (void)render; assert(angle == 0); ++renderCalls;
    memset(out, 0, sizeof(*out));
    if (!frameOk) return 0;
    for (unsigned y = 20; y < 100; ++y) for (unsigned x = 0; x < 160; ++x)
        pixels[1 + y * 160 + x] = 0;
    ++captures;
    return Esp32PlatformVideo_present();
}
static void seed(unsigned credits) {
    EspNativeGameplayEventChain_reset();
    memset(&runtime, 0, sizeof(runtime));
    runtime.eventCount = 70; runtime.byteCodeCount = 200; runtime.mapSpriteCount = 44;
    runtime.eventBytes = 70U * ESP_MAP_EVENT_RECORD_BYTES;
    runtime.byteCodeBytes = 200U * ESP_MAP_BYTECODE_RECORD_BYTES;
    runtime.events = (const uint8_t*)events;
    runtime.byteCodes = (const uint8_t*)commands;
    memset(&view, 0, sizeof(view)); view.active = 1; view.targetMapId = 1;
    memset(&player, 0, sizeof(player)); player.active = 1; player.credits = credits;
    memset(removedBits, 0, sizeof(removedBits)); memset(topology, 0, sizeof(topology));
    memset(&topologyView, 0, sizeof(topologyView)); topologyView.storage = topology;
    topologyView.storageBytes = sizeof(topology);
    for (unsigned i = 0; i < 70; ++i) { states[i] = 1; events[i] = i * 8; }
    events[17] = 132; events[66] = 609; events[67] = 675U | (148U << 10) | (23U << 19) | (4U << 25);
    events[68] = 700; events[69] = 720; states[67] = 4;
    static const EspMapByteCode witness[] = {
        {8,0x18,0x100},{11,0x21301,0x100},{19,0x1503,0x100},
        {40,0x19,0x10300},{8,0x1a,0x10100},{19,0x1503,0x10100},
        {8,0x1b,0x20100},{8,0x1c,0x40100},{19,0x1503,0x40100},
        {23,0x1d0a02,0x80100},{22,0xa02,0x80100},{18,0x1301,0x80300},
        {11,0x41301,0x80100},{26,0x1e,0x80100},{35,0x230080ff,0x80100},
        {42,0x13e3,0x80100},{37,0x15e,0x80100},{18,0x1503,0x80300},
        {7,9,0x80300},{36,0,0x80100},{40,0x1f,0x80300},
        {11,0x10404,0x80100},{19,0x1503,0x80100}
    };
    memset(commands, 0, sizeof(commands)); memcpy(commands + 148, witness, sizeof(witness));
    dialogActive = standalone = noteStaged = notes = sounds = hides = shows = gate = pack = 0;
    dialogOk = noteOk = frameOk = presentOk = showOk = 1;
    captures = presents = renderCalls = 0; clockMs = 1000;
    for (unsigned i = 0; i < 160 * 120 + 2; ++i) pixels[i] = 0xbeef;
}
static void openPaid(void) {
    EspNativeGameplayDialogResumeResult result;
    unsigned after = player.credits - 10U;
    assert(EspNativeGameplayEventChain_preflightInteraction(67,9,0x500) ==
        ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_OK);
    assert(EspNativeGameplayEventChain_execute(67,9,0x500,&result) == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_PAUSED);
    assert(player.credits == after && states[66] == 4 && removedBits[159] == 1);
    assert(dialogActive && !EspNativeGameplayEventChain_isWaiting());
    for (unsigned i = 14; i < 23; ++i) assert(!removedBits[148+i]);
}
static void closePaid(void) {
    EspNativeGameplayDialogClose close = {0}; EspNativeGameplayDialogResumeResult result;
    close.sourceEventIndex = 67; close.resumeCommandOffset = 14;
    close.runFlags = 0x500; close.resumeRequested = 1;
    dialogActive = 0;
    assert(__wrap_EspNativeGameplayDialog_resume(&close,&result) == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_PAUSED);
    assert(EspNativeGameplayEventChain_isWaiting() && notes == 0 && shows == 0);
}
int main(void) {
    EspNativeGameplayDialogResumeResult result;
    struct Render_s* render = (void*)1;
    seed(9);
    assert(EspNativeGameplayEventChain_execute(67,9,0x500,&result) == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_PAUSED);
    assert(standalone == 1 && player.credits == 9 && hides == 0 && !pack && notes == 0);
    seed(10); dialogOk = 0;
    assert(EspNativeGameplayEventChain_execute(67,9,0x500,&result) == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_EXEC_FAILED);
    assert(player.credits == 10 && states[66] == 1 && topology[0] == 0 && removedBits[159] == 0);
    dialogOk = 1; openPaid(); closePaid();
    frameOk = 0; clockMs = 50000;
    assert(EspNativeGameplayEventChain_serviceWait(render) && sounds == 0 && gate == 0 && notes == 0);
    frameOk = 1;
    assert(EspNativeGameplayEventChain_serviceWait(render) && sounds == 1);
    unsigned colored = 0;
    for (unsigned i = 0; i < 160U * 120U; ++i) colored += pixels[1+i] == 0xfc00U;
    assert(colored > 45U); /* Canonical RGB565 orange; no red/blue swap. */
    clockMs += 349; assert(EspNativeGameplayEventChain_serviceWait(render) && notes == 0);
    frameOk = 0; clockMs++;
    assert(EspNativeGameplayEventChain_serviceWait(render));
    assert(EspNativeGameplayEventChain_isWaiting() && noteStaged == 0 && notes == 0);
    assert(states[67] == 4 && removedBits[168] == 0 && topology[0] == 1);
    frameOk = 1;
    clockMs += 100;
    assert(EspNativeGameplayEventChain_serviceWait(render));
    assert(!EspNativeGameplayEventChain_isWaiting() && player.credits == 0 && sounds == 1);
    assert(notes == 1 && states[67] == 5 && states[17] == 1 && removedBits[168] == 1);
    for (unsigned x = 0; x < 160; ++x) {
        assert(pixels[1+x] == 0xbeef && pixels[1+119*160+x] == 0xbeef);
    }
    assert(pixels[0] == 0xbeef && pixels[160*120+1] == 0xbeef);
    seed(10); commands[163].id = 38;
    assert(EspNativeGameplayEventChain_execute(67,9,0x500,&result) == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID);
    assert(player.credits == 10 && !dialogActive && hides == 0);
    seed(10); showOk = 0;
    assert(EspNativeGameplayEventChain_preflightInteraction(67,9,0x500) == ESP_NATIVE_GAMEPLAY_EVENT_CHAIN_PREFLIGHT_UNSUPPORTED);
    assert(player.credits == 10 && topology[0] == 0);
    seed(10); openPaid(); closePaid(); clockMs = UINT32_MAX - 99U;
    assert(EspNativeGameplayEventChain_serviceWait(render));
    clockMs = 249; assert(EspNativeGameplayEventChain_serviceWait(render) && notes == 0);
    clockMs = 250; assert(EspNativeGameplayEventChain_serviceWait(render) && notes == 1);
    assert(sounds == 1);
    seed(30); openPaid();
    assert(EspNativeGameplayEventChain_execute(67,9,0x500,&result) == ESP_NATIVE_GAMEPLAY_DIALOG_RESUME_INVALID);
    assert(player.credits == 20); /* Repeated SELECT cannot charge a second time. */
    closePaid(); assert(EspNativeGameplayEventChain_serviceWait(render));
    clockMs += 350; noteOk = 0;
    assert(EspNativeGameplayEventChain_serviceWait(render));
    assert(EspNativeGameplayEventChain_isWaiting() && notes == 0 && states[67] == 4);
    noteOk = 1; clockMs += 100;
    assert(EspNativeGameplayEventChain_serviceWait(render) && notes == 1);
    assert(player.credits == 20 && sounds == 1);
    seed(10); openPaid(); closePaid(); presentOk = 0;
    assert(EspNativeGameplayEventChain_serviceWait(render));
    clockMs += 10000;
    assert(EspNativeGameplayEventChain_serviceWait(render));
    assert(sounds == 0 && notes == 0 && EspNativeGameplayEventChain_isWaiting());
    presentOk = 1;
    assert(EspNativeGameplayEventChain_serviceWait(render) && sounds == 1);
    clockMs += 350;
    assert(EspNativeGameplayEventChain_serviceWait(render) && notes == 1);
    EspNativeGameplayEventChain_reset();
    assert(!EspNativeGameplayEventChain_isWaiting());
    puts("paid_interaction_host: PASS credit gate/debit rollback, dialog/WAIT pauses, retry, NOTE commit, wrap, guards");
    return 0;
}
