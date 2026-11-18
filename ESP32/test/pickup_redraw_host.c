/* Run the real bounded pickup redraw coalescer on the host. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_pickup_redraw.h"

int main(void) {
    EspPickupRedraw state;
    memset(&state, 0, sizeof(state));
    _Static_assert(sizeof(EspPickupRedraw) <= 8U,
                   "pickup visual coalescer must stay tiny");
    assert(!EspPickupRedraw_due(&state, UINT32_MAX));
    assert(!EspPickupRedraw_satisfy(&state));

    /* Repeated pickups postpone just the cosmetic redraw, not gameplay. */
    EspPickupRedraw_schedule(&state, 1000U);
    assert(state.pending && state.lastPickupMs == 1000U);
    assert(!EspPickupRedraw_due(&state, 1599U));
    EspPickupRedraw_schedule(&state, 1200U);
    assert(!EspPickupRedraw_due(&state, 1600U));
    assert(!EspPickupRedraw_due(&state, 1799U));
    assert(EspPickupRedraw_due(&state, 1800U));

    /* A later real settled world frame cancels the redundant idle repaint. */
    assert(EspPickupRedraw_satisfy(&state));
    assert(!EspPickupRedraw_due(&state, 2000U));
    assert(!EspPickupRedraw_satisfy(&state));

    /* One pickup and no further input eventually refreshes the HUD/world. */
    EspPickupRedraw_schedule(&state, 3000U);
    assert(!EspPickupRedraw_due(&state, 3599U));
    assert(EspPickupRedraw_due(&state, 3600U));
    assert(EspPickupRedraw_satisfy(&state));

    /* 32-bit millisecond timer wraps normally; no freeze or early repaint. */
    EspPickupRedraw_schedule(&state, UINT32_MAX - 99U);
    assert(!EspPickupRedraw_due(&state, 100U));
    assert(!EspPickupRedraw_due(&state, 499U));
    assert(EspPickupRedraw_due(&state, 500U));
    assert(EspPickupRedraw_satisfy(&state));

    /* A new pickup after a world frame starts a fresh single-slot lease. */
    EspPickupRedraw_schedule(&state, 42U);
    assert(state.pending && state.lastPickupMs == 42U);
    assert(EspPickupRedraw_due(&state, 642U));
    assert(EspPickupRedraw_satisfy(&state));
    puts("pickup_redraw_host: PASS bounded 8B, bursts coalesced, settled frame satisfies, idle fallback, wrap-safe");
    return 0;
}
