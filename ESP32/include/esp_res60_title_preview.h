#ifndef DOOMRPG_ESP32_RES60_TITLE_PREVIEW_H
#define DOOMRPG_ESP32_RES60_TITLE_PREVIEW_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct EspRes60TitlePreviewReport_s {
    uint16_t backgroundWidth, backgroundHeight, logoWidth, logoHeight;
    uint32_t packReads, bytesRead, pixelsWritten, frameFNV;
} EspRes60TitlePreviewReport;
int EspRes60TitlePreview_render(uint16_t* fb, size_t bytes,
                               EspRes60TitlePreviewReport* report);
/* P6: same PAK lease/decoder, explicit logo location; (-1,-1) centres. */
int EspRes60TitlePreview_renderAt(uint16_t* fb, size_t bytes,
                                 int16_t logoX, int16_t logoY,
                                 EspRes60TitlePreviewReport* report);
#ifdef __cplusplus
}
#endif
#endif
