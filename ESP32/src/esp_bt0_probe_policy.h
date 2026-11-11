#ifndef ESP_BT0_PROBE_POLICY_H
#define ESP_BT0_PROBE_POLICY_H

#include <stdint.h>

/* BT-0 deliberately reserves the same 56-KiB heap8 gameplay floor used by
 * the native RAMBUDGET gate. These are experimental thresholds, not a claimed
 * measured cost of Bluedroid, the controller or A2DP. */
#define ESP_BT0_MIN_PRE_HEAP8       (64U * 1024U)
#define ESP_BT0_MIN_PRE_LARGEST8    (32U * 1024U)
#define ESP_BT0_MIN_PRE_DMA         (48U * 1024U)
#define ESP_BT0_MIN_PRE_LARGEST_DMA (24U * 1024U)
#define ESP_BT0_MIN_POST_HEAP8      (56U * 1024U)
#define ESP_BT0_MIN_POST_LARGEST8   (24U * 1024U)
#define ESP_BT0_MIN_POST_DMA        (40U * 1024U)
#define ESP_BT0_MIN_POST_LARGEST_DMA (16U * 1024U)

typedef struct {
    uint32_t heap8;
    uint32_t largest8;
    uint32_t dma;
    uint32_t largestDma;
} EspBt0Mem;

static inline int EspBt0_preflight(EspBt0Mem m) {
    return m.heap8 >= ESP_BT0_MIN_PRE_HEAP8 &&
           m.largest8 >= ESP_BT0_MIN_PRE_LARGEST8 &&
           m.dma >= ESP_BT0_MIN_PRE_DMA &&
           m.largestDma >= ESP_BT0_MIN_PRE_LARGEST_DMA;
}

static inline int EspBt0_postflight(EspBt0Mem m) {
    return m.heap8 >= ESP_BT0_MIN_POST_HEAP8 &&
           m.largest8 >= ESP_BT0_MIN_POST_LARGEST8 &&
           m.dma >= ESP_BT0_MIN_POST_DMA &&
           m.largestDma >= ESP_BT0_MIN_POST_LARGEST_DMA;
}

#endif /* ESP_BT0_PROBE_POLICY_H */
