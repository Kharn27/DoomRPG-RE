/* Real dialog paint/classification/cancel. Seed its private bounded owner to
 * avoid mocking event construction; PAK/font/present boundaries are mocked.
 * BACK is original cancellation, not a new paid-script YES/NO branch. */
#include <assert.h>
#include <stdio.h>
#include "../src/esp_native_gameplay_dialog.c"

static uint16_t pixels[160 * 120 + 2];
static int pack, presents, glyphBlits;
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
    (void)sx; (void)sy; (void)stats;
    /* Full original 9x12 PAK glyph goes directly into the shared 160x120
     * framebuffer again, not a miniaturized intermediate tile. Verify
     * transparency and the number of font reads by simulating opaque and
     * transparent texels without real PAK access. */
    assert(source != NULL && source->paletteCount >= 2U);
    assert(fbWidth == 160U && fbHeight == 120U);
    assert(width == DIALOG_FONT_WIDTH && height == DIALOG_FONT_HEIGHT);
    assert(dx >= DIALOG_TEXT_X && dy >= DIALOG_TEXT_Y);
    assert((int)dx + (int)width <= 160 && (int)dy + (int)height <= 100);
    assert(transparent == DIALOG_TRANSPARENT);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x)
            if (!(x == 1U && y == 0U))
                framebuffer[((unsigned)dy + y) * fbWidth +
                            (unsigned)dx + x] = source->paletteRgb565[1];
    ++glyphBlits;
    return ESP_NATIVE_INDEXED_BMP_OK;
}
static void seed(uint8_t back) {
    EspNativeGameplayDialog_reset();
    for (unsigned i = 0; i < 160 * 120 + 2; ++i) pixels[i] = 0xbeef;
    pack = 1; presents = 0; glyphBlits = 0;
    dialog.font.paletteCount = 2U;
    dialog.font.paletteRgb565[0] = 0xf81fU;
    dialog.font.paletteRgb565[1] = EspMainMenuSkin_uiInk();
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
    uint16_t lastAccent = 0U;
    uint16_t lastInk = 0U;
    for (uint8_t theme = ESP_MAIN_MENU_THEME_CLASSIC;
         theme <= ESP_MAIN_MENU_THEME_PHOSPHORE; ++theme) {
        assert(EspMainMenuSkin_setTheme(theme));
        seed(1);
        dialog.font.paletteCount = 4U;
        dialog.font.paletteRgb565[0] = 0xf81fU; /* transparent magenta */
        dialog.font.paletteRgb565[1] = 0xffffU; /* full white glyph */
        dialog.font.paletteRgb565[2] = 0U;      /* dark outline */
        dialog.font.paletteRgb565[3] = 0x7befU; /* half-bright glyph */
        tintDialogFontInk();
        assert(dialog.font.paletteRgb565[0] == 0xf81fU);
        assert(dialog.font.paletteRgb565[1] == EspMainMenuSkin_uiInk());
        assert(dialog.font.paletteRgb565[2] == 0U);
        assert(dialog.font.paletteRgb565[3] != dialog.font.paletteRgb565[1]);
        assert(paintDialog(1000U) && presents == 1);
        /* Readable original 9x12 source and four 12px rows are back.
         * The industrial box expands enough to protect bottom row 84..95
         * and never touches the permanent bottom HUD at y=100. */
        assert(DIALOG_BORDER_LEFT == 15 && DIALOG_BORDER_RIGHT == 144);
        assert(DIALOG_BORDER_TOP == 45 && DIALOG_BORDER_BOTTOM == 99);
        assert(DIALOG_FONT_WIDTH == 9U && DIALOG_FONT_HEIGHT == 12U);
        assert(DIALOG_FONT_ADVANCE == 7);
        assert(pixels[1 + 47 * 160 + 16] == ESP_HUB_COLOR_BG);
        assert(pixels[1 + 46 * 160 + 30] == EspMainMenuSkin_uiAccentDim());
        assert(pixels[1 + 46 * 160 + 80] == EspMainMenuSkin_uiAccent());
        assert(pixels[1 + 98 * 160 + 80] == EspMainMenuSkin_uiAccent());
        /* Smaller 3x5 BACK lettering, NOT the former 9-row plate font.
         * The chassis and actual touch box remain the same 38x18. */
        assert(DIALOG_BACK_W == 38 && DIALOG_BACK_H == 18);
        assert(pixels[1 + 30 * 160 + 30] ==
               EspMainMenuSkin_accentTone(ESP_MAIN_MENU_SKIN_IDLE));
        assert(pixels[1 + 33 * 160 + 32] == EspMainMenuSkin_uiInk());
        /* COMMS was a decorative dead button. Nothing paints that region. */
        assert(pixels[1 + 30 * 160 + 100] == 0xbeef);
        assert(pixels[1 + 42 * 160 + 140] == 0xbeef);
        assert(glyphBlits == 0);
        assert(paintDialog(1100U) && presents == 2);
        assert(glyphBlits == 4); /* 25ms/character: first 4 of Payment. */
        assert(pixels[1 + DIALOG_TEXT_Y * 160 + DIALOG_TEXT_X] ==
               EspMainMenuSkin_uiInk());
        assert(pixels[1 + DIALOG_TEXT_Y * 160 + DIALOG_TEXT_X + 1] ==
               ESP_HUB_COLOR_BG); /* source alpha still honored */
        if (theme != ESP_MAIN_MENU_THEME_CLASSIC) {
            assert(lastAccent != EspMainMenuSkin_uiAccent());
            assert(lastInk != EspMainMenuSkin_uiInk());
        }
        lastAccent = EspMainMenuSkin_uiAccent();
        lastInk = EspMainMenuSkin_uiInk();
        for (unsigned y = 0; y < 120; ++y)
            for (unsigned x = 0; x < 160; ++x) {
                const int inBox = x >= DIALOG_BORDER_LEFT &&
                    x <= DIALOG_BORDER_RIGHT && y >= DIALOG_BORDER_TOP &&
                    y <= DIALOG_BORDER_BOTTOM;
                const int inBack = x >= DIALOG_BACK_X &&
                    x < DIALOG_BACK_X + DIALOG_BACK_W &&
                    y >= DIALOG_BACK_Y && y < DIALOG_BACK_Y + DIALOG_BACK_H;
                if (!inBox && !inBack)
                    assert(pixels[1 + y * 160 + x] == 0xbeef);
            }
    }
    assert(EspMainMenuSkin_setTheme(ESP_MAIN_MENU_THEME_CLASSIC));
    seed(1);
    assert(paintDialog(1000U) && presents == 1);
    for (int y = 0; y < 120; ++y) for (int x = 0; x < 160; ++x) {
        EspNativeGameplayAction action = EspNativeGameplayDialog_tapAction(x, y);
        int isBack = x >= DIALOG_BACK_X &&
            x < DIALOG_BACK_X + DIALOG_BACK_W &&
            y >= DIALOG_BACK_Y && y < DIALOG_BACK_Y + DIALOG_BACK_H;
        assert(action == (isBack ? ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN :
                                  ESP_NATIVE_GAMEPLAY_ACTION_SELECT));
    }
    assert(pixels[1 + 30 * 160 + 30] ==
           EspMainMenuSkin_accentTone(ESP_MAIN_MENU_SKIN_IDLE));
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
    assert(pixels[1 + 28 * 160 + 20] == 0xbeef);
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
    /* Four full-size original PAK rows use y=48..95, not the HUD. */
    seed(1);
    strcpy(dialog.text, "A|B|C|D");
    dialog.textLength = (uint16_t)strlen(dialog.text);
    dialog.lineCount = ESP_NATIVE_GAMEPLAY_DIALOG_PAGE_LINES;
    dialog.dialogTypeLineIdx = ESP_NATIVE_GAMEPLAY_DIALOG_PAGE_LINES;
    assert(paintDialog(1000U));
    assert(glyphBlits == 4);
    for (unsigned row = 0; row < ESP_NATIVE_GAMEPLAY_DIALOG_PAGE_LINES; ++row) {
        assert(pixels[1 + (DIALOG_TEXT_Y + row * DIALOG_FONT_HEIGHT) * 160 +
                      DIALOG_TEXT_X] == EspMainMenuSkin_uiInk());
        assert(pixels[1 + (DIALOG_TEXT_Y + row * DIALOG_FONT_HEIGHT +
                           DIALOG_FONT_HEIGHT - 1U) * 160 + DIALOG_TEXT_X] ==
               EspMainMenuSkin_uiInk());
    }
    assert(pixels[1 + 99 * 160 + DIALOG_TEXT_X] != 0xbeef);
    assert(pixels[1 + 100 * 160 + DIALOG_TEXT_X] == 0xbeef);

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
    puts("dialog_touch_host: PASS original full-size 9x12 PAK dialog text, 130x55 themed industrial frame, 3x5 BACK lettering in 38x18 hitbox, no COMMS, 3 themes, 4 lines, alpha, touch/cancel, HUD guards");
    return 0;
}
