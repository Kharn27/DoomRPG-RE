#include "platform_video_config.h"
#if DOOMRPG_RES60_P5_TITLE
#include <string.h>
#include "esp_asset_pack.h"
#include "esp_native_indexed_bmp.h"
#include "esp_res60_layout.h"
#include "esp_res60_title_preview.h"
int EspRes60TitlePreview_renderAt(uint16_t* fb, size_t bytes,
                                 int16_t logoX, int16_t logoY,
                                 EspRes60TitlePreviewReport* report) {
    EspNativeIndexedBmp bg, logo;
    EspNativeIndexedBmpStats io;
    int painted = 0;
    if (report) memset(report, 0, sizeof(*report));
    if (!fb || !report || bytes != ESP_RES60_FRAMEBUFFER_BYTES ||
        EspAssetPack_isOpen()) return 0;
    memset(&io, 0, sizeof(io));
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) return 0;
    /* Validate BOTH original BMPs before touching the framebuffer. */
    if (EspNativeIndexedBmp_open("c.bmp", &bg, &io) != ESP_NATIVE_INDEXED_BMP_OK ||
        EspNativeIndexedBmp_open("j.bmp", &logo, &io) != ESP_NATIVE_INDEXED_BMP_OK ||
        bg.width == 0 || bg.height == 0 || logo.width == 0 || logo.height == 0 ||
        logo.width > ESP_RES60_WIDTH || logo.height > ESP_RES60_HEIGHT)
        goto done;
    if (logoX == -1 && logoY == -1) {
        logoX = (int16_t)((ESP_RES60_WIDTH - logo.width) / 2U);
        logoY = (int16_t)((ESP_RES60_HEIGHT - logo.height) / 2U);
    }
    if (logoX < 0 || logoY < 0 ||
        (uint32_t)logoX + logo.width > ESP_RES60_WIDTH ||
        (uint32_t)logoY + logo.height > ESP_RES60_HEIGHT) goto done;
    /* P19: original c.bmp 192x128: two batched blits reuse the 1KiB
     * scratch instead of 144 separate SD row reads. The first 16 source
     * rows repeat at the bottom; fallback keeps generic tiling. */
    if (bg.width == ESP_RES60_WIDTH && bg.height == 128U) {
        if (EspNativeIndexedBmp_blit(&bg, fb, ESP_RES60_WIDTH,
                ESP_RES60_HEIGHT, 0, 0, ESP_RES60_WIDTH, 128U,
                0, 0, 0U, &io) != ESP_NATIVE_INDEXED_BMP_OK ||
            EspNativeIndexedBmp_blit(&bg, fb, ESP_RES60_WIDTH,
                ESP_RES60_HEIGHT, 0, 0, ESP_RES60_WIDTH, 16U,
                0, 128, 0U, &io) != ESP_NATIVE_INDEXED_BMP_OK)
            goto done;
    } else if (EspNativeIndexedBmp_tile(&bg, fb,
                   ESP_RES60_WIDTH, ESP_RES60_HEIGHT, 0, 0,
                   ESP_RES60_WIDTH, ESP_RES60_HEIGHT, 0U, &io)
                   != ESP_NATIVE_INDEXED_BMP_OK)
        goto done;
    if (EspNativeIndexedBmp_blit(&logo, fb, ESP_RES60_WIDTH,
            ESP_RES60_HEIGHT, 0, 0, logo.width, logo.height,
            logoX, logoY, 1U, &io) != ESP_NATIVE_INDEXED_BMP_OK)
        goto done;
    report->backgroundWidth = bg.width;
    report->backgroundHeight = bg.height;
    report->logoWidth = logo.width;
    report->logoHeight = logo.height;
    report->packReads = io.packReads;
    report->bytesRead = io.bytesRead;
    report->pixelsWritten = io.pixelsWritten;
    report->frameFNV = EspRes60Layout_fnv(fb, bytes);
    painted = report->frameFNV != 0U;
done:
    EspAssetPack_close();
    if (EspAssetPack_isOpen()) return 0;
    return painted;
}
/* P5 keeps the exact former centred original-title semantics. */
int EspRes60TitlePreview_render(uint16_t* fb, size_t bytes,
                               EspRes60TitlePreviewReport* report) {
    return EspRes60TitlePreview_renderAt(fb, bytes, -1, -1, report);
}
#endif
