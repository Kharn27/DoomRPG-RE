#ifndef ESP_UAC_BMP32_H
#define ESP_UAC_BMP32_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pure bounded BMP V4 BGRA32 BI_BITFIELDS decoder. No filesystem or heap. */
typedef int (*EspUacBmpReadAt)(void*,uint32_t,void*,size_t);
typedef struct {
    uint32_t pixelOffset, fileBytes;
    uint16_t width,height;
} EspUacBmp32;
int EspUacBmp32_open(EspUacBmp32* out,EspUacBmpReadAt reader,void* context,
                     uint32_t fileBytes,uint16_t width,uint16_t height);
/* topRow from top, bottom-up BMP. scale=1 240 direct, scale=2 120 nearest.
 * One source scanline scratch width*4. Clip to [0,destWidth). */
int EspUacBmp32_blendRow(const EspUacBmp32* bmp,EspUacBmpReadAt reader,
                        void* context,uint16_t topRow,int32_t destX,uint8_t scale,
                        uint16_t* dest,uint16_t destWidth,
                        uint8_t* scratch,size_t scratchCapacity);
/* 240 BMP -> virtual 120 -> x2 output, choose texel (2x,2y).
 * Alpha, clip and backing-store bounds are identical to the native reader. */
int EspUacBmp32_blendRow240As120(const EspUacBmp32* bmp,
                         EspUacBmpReadAt reader,void* context,
                         uint16_t virtualTopRow,int32_t destX,uint8_t scale,
                         uint16_t* dest,uint16_t destWidth,
                         uint8_t* scratch,size_t scratchCapacity);
/* Borrowed bounded batch: one backing read for multiple bottom-up rows. */
typedef struct {
    const uint8_t* bytes;
    uint32_t fileOffset, size;
} EspUacBmpRowBatch;
int EspUacBmp32_loadBatch(const EspUacBmp32* bmp,EspUacBmpReadAt reader,
                          void* context,uint16_t firstTop,uint16_t rowCount,
                          uint8_t* storage,size_t capacity,
                          EspUacBmpRowBatch* out);
int EspUacBmp32_readBatch(void* context,uint32_t offset,void* dst,size_t n);
/* Same physical hitboxes at both resolutions. */
enum {
    ESP_UAC_PANEL_LEFT=202, ESP_UAC_PANEL_WIDTH=118,
    ESP_UAC_BUTTON_INSET=4, ESP_UAC_BUTTON_WIDTH=110
};
/* One geometry contract for touch, native compositor and tests. */
int EspUacMenu_hitPhysical(int x,int y);
#ifdef __cplusplus
}
#endif
#endif
