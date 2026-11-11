#ifndef ESP_BT_TEST_POLICY_H
#define ESP_BT_TEST_POLICY_H
#include <stdint.h>
/* Isolated MAIN_MENU-only test profile. No gameplay/cache/map is allocated.
 * Below floors are strictly scoped to THIS TEST; never copy to production
 * BT-0 gameplay (64 KiB pre, 56 KiB post) or main firmware. */
#define ESP_BTTEST_MIN_PRE_HEAP8       (48U * 1024U)
#define ESP_BTTEST_MIN_PRE_LARGEST8    (24U * 1024U)
#define ESP_BTTEST_MIN_PRE_DMA         (40U * 1024U)
#define ESP_BTTEST_MIN_PRE_LARGEST_DMA (20U * 1024U)
#define ESP_BTTEST_MIN_POST_HEAP8      (12U * 1024U)
#define ESP_BTTEST_MIN_POST_LARGEST8   (8U * 1024U)
#define ESP_BTTEST_MIN_POST_DMA        (10U * 1024U)
#define ESP_BTTEST_MIN_POST_LARGEST_DMA (4U * 1024U)
typedef struct { uint32_t heap8, largest8, dma, largestDma; } EspBtTestMem;
static inline int EspBtTest_preflight(EspBtTestMem m) {
    return m.heap8 >= ESP_BTTEST_MIN_PRE_HEAP8 &&
           m.largest8 >= ESP_BTTEST_MIN_PRE_LARGEST8 &&
           m.dma >= ESP_BTTEST_MIN_PRE_DMA &&
           m.largestDma >= ESP_BTTEST_MIN_PRE_LARGEST_DMA;
}
static inline int EspBtTest_postflight(EspBtTestMem m) {
    return m.heap8 >= ESP_BTTEST_MIN_POST_HEAP8 &&
           m.largest8 >= ESP_BTTEST_MIN_POST_LARGEST8 &&
           m.dma >= ESP_BTTEST_MIN_POST_DMA &&
           m.largestDma >= ESP_BTTEST_MIN_POST_LARGEST_DMA;
}
#endif
