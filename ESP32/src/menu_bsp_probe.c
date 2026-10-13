#include <stdio.h>

#include "menu_bsp_probe.h"

/* ZIP is retired for every ESP32 profile. Keep the bringup diagnostic
 * callable and fail closed; native PAK readers own BSP validation. */
int DoomRPG_probeMenuBspHeader(int configMappingsReady) {
    (void)configMappingsReady;
    printf("[MENUBSP] RETIRED: legacy ZIP BSP header probe unavailable; native PAK BSP reader owns validation\n");
    return 0;
}
