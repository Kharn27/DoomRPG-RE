/* Real dialog paint/classification/cancel. Seed its private bounded owner to
 * avoid mocking event construction; PAK/font/present boundaries are mocked.
 * BACK is original cancellation, not a new paid-script YES/NO branch. */
#include <assert.h>
#include <stdio.h>
#include "../src/esp_native_gameplay_dialog.c"

static uint16_t pixels[160 * 120 + 2];
static int pack, presents;
int64_t esp_timer_get_time(void) { return 1000000; }
void* Esp32PlatformVideo_framebuffer(void) { return pixels + 1; }
size_t Esp32PlatformVideo_framebufferSizeBytes(void) { return 160U * 120U * 2U; }
int Esp32PlatformVideo_present(void) { ++presents; return 1; }
int EspAssetPack_isOpen(void) { return pack; }
void EspAssetPack_close(void) { pack = 0; }
EspNativeIndexedBmpStatus EspNativeIndexedBmp_blit(
    const EspNativeIndexedBmp* source, uint16_t* framebuffer,
    uint16_t fbWidth, uint16_t fbHeight, uint16_t sx, uint16_t sy,
    uint16_t width, uint16_t height, int16_t dx, int16_t dy,
    uint8_t transparent, EspNativeIndexedBmpStats* stats) {
    (void)source; (void)sx; (void)sy; (void)transparent; (void)stats;
    assert(fbWidth == 160U && fbHeight == 120U && dx >= 0 && dy >= 20);
    assert(dx + width <= 160 && dy + height <= 100);
    fillRect(framebuffer, dx, dy, width, height, 0xffffU);
    return ESP_NATIVE_INDEXED_BMP_OK;
}
static void seed(uint8_t back) {
    EspNativeGameplayDialog_reset();
    for (unsigned i = 0; i < 160 * 120 + 2; ++i) pixels[i] = 0xbeef;
    pack = 1; presents = 0;
    dialog.active = dialog.packOwned = 1;
    dialog.backAllowed = back;
    dialog.dialogCodeId = back ? 8U : 26U;
    dialog.owner.sourceEventIndex = 67U;
    dialog.owner.sourceCommandOffset = back ? 7U : 13U;
    dialog.resumeCommandOffset = back ? 8U : 14U;
    strcpy(dialog.text, "Payment offer");
    dialog.textLength = (uint16_t)strlen(dialog.text);
    dialog.lineCount = 1U;
    dialog.lineStartMs = 1000U;
}
int main(void) {
    EspNativeGameplayDialogClose close;
    seed(1);
    assert(paintDialog(1000U) && presents == 1);
    for (int y = 0; y < 120; ++y) for (int x = 0; x < 160; ++x) {
        EspNativeGameplayAction action = EspNativeGameplayDialog_tapAction(x, y);
        int isBack = x >= 16 && x < 60 && y >= 22 && y < 43;
        assert(action == (isBack ? ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN :
                                  ESP_NATIVE_GAMEPLAY_ACTION_SELECT));
    }
    assert(pixels[1 + 22 * 160 + 16] == ESP_HUB_COLOR_STEEL_DARK);
    assert(pixels[1 + 21 * 160 + 16] == 0xbeef);
    /* Test touch pixels and routing into the REAL recovered cancel handler. */
    assert(EspNativeGameplayDialog_handleAction(
        EspNativeGameplayDialog_tapAction(38, 32), &close) ==
            ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_CLOSE_CANCEL);
    assert(!pack && !EspNativeGameplayDialog_isActive());
    assert(close.sourceEventIndex == 67 && close.sourceCommandOffset == 7);
    assert(close.resumeCommandOffset == 8 && close.resumeRequested == 0 && close.backAllowed);
    assert(EspNativeGameplayDialog_tapAction(38, 32) == ESP_NATIVE_GAMEPLAY_ACTION_NONE);
    seed(0);
    assert(paintDialog(1000U));
    assert(pixels[1 + 22 * 160 + 16] == 0xbeef);
    assert(EspNativeGameplayDialog_tapAction(38, 32) == ESP_NATIVE_GAMEPLAY_ACTION_SELECT);
    assert(EspNativeGameplayDialog_handleAction(ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN, &close) ==
        ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_IGNORED);
    assert(pack && EspNativeGameplayDialog_isActive());
    assert(EspNativeGameplayDialog_handleAction(
        EspNativeGameplayDialog_tapAction(38, 32), &close) ==
            ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_REDRAWN);
    assert(EspNativeGameplayDialog_handleAction(
        EspNativeGameplayDialog_tapAction(38, 32), &close) ==
            ESP_NATIVE_GAMEPLAY_DIALOG_INPUT_CLOSE_RESUME);
    assert(close.resumeRequested == 1 && !close.backAllowed && !pack);
    seed(1);
    assert(EspNativeGameplayDialog_tapAction(-1, 32) == ESP_NATIVE_GAMEPLAY_ACTION_NONE);
    assert(EspNativeGameplayDialog_tapAction(160, 32) == ESP_NATIVE_GAMEPLAY_ACTION_NONE);
    assert(EspNativeGameplayDialog_tapAction(38, -1) == ESP_NATIVE_GAMEPLAY_ACTION_NONE);
    assert(EspNativeGameplayDialog_tapAction(38, 120) == ESP_NATIVE_GAMEPLAY_ACTION_NONE);
    assert(paintDialog(1000U));
    for (unsigned y = 0; y < 120; ++y) for (unsigned x = 0; x < 160; ++x)
        if (y < 20 || y >= 100) assert(pixels[1 + y * 160 + x] == 0xbeef);
    assert(pixels[0] == 0xbeef && pixels[160 * 120 + 1] == 0xbeef);
    EspNativeGameplayDialog_reset();
    puts("dialog_touch_host: PASS BACK paint/bounds/cancel, NOBACK SELECT-only, no HUD overwrite");
    return 0;
}
