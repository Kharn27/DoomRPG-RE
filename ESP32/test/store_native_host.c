/* Host-only logic test for native EV_OPENSTORE. Compile with the exact
 * production store .c; replace only framebuffer/text and PlayerState leaves.
 * No ESP32, PAK, RNG, map or hardware runtime is linked. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "esp_native_gameplay_store.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_player_state.h"
#include "platform_video_config.h"

static uint16_t pixels[DOOMRPG_LOGICAL_WIDTH * DOOMRPG_LOGICAL_HEIGHT];
static EspNativeGameplayPlayerState player;
static unsigned presents;
static unsigned textDraws;

void* Esp32PlatformVideo_framebuffer(void) { return pixels; }
size_t Esp32PlatformVideo_framebufferSizeBytes(void) {
    return sizeof(pixels);
}
int Esp32PlatformVideo_present(void) { ++presents;return 1; }
void EspNativeGameplayHubTouchUi_drawCrispText(
    uint16_t* framebuffer,const char* str,int x,int y,uint16_t color) {
    assert(framebuffer == pixels);
    assert(str!=NULL && x>=0 && x<160 && y>=0 && y<120);
    (void)color;
    ++textDraws;
}
const EspNativeGameplayPlayerState* EspNativeGameplayPlayerState_view(void) {
    return player.active ? &player : NULL;
}
int EspNativeGameplayPlayerState_snapshot(EspNativeGameplayPlayerState* out) {
    if (out==NULL || player.active!=1U) return 0;
    *out=player;return 1;
}
int EspNativeGameplayPlayerState_restore(
    const EspNativeGameplayPlayerState* in) {
    if (in==NULL || in->active!=1U) return 0;
    player=*in;return 1;
}
uint32_t EspNativeGameplayPlayerState_fingerprint(void) {
    return (uint32_t)(player.credits ^ player.param2 ^
                      ((uint32_t)player.inventory[0]<<16));
}
static void seed(unsigned credits) {
    memset(&player,0,sizeof(player));
    player.active=1U;player.credits=credits;
    player.param2=0x100e0c10U;
}
static void enterList(uint8_t storeId) {
    assert(EspNativeGameplayStore_begin(storeId,0U,0U));
    assert(EspNativeGameplayStore_isActive());
    assert(EspNativeGameplayStore_handleTap(80,60)==1);
}
static void pickFirst(void) {
    assert(EspNativeGameplayStore_handleTap(60,43)==1);
}
static void confirmBuy(void) {
    assert(EspNativeGameplayStore_handleTap(80,76)==1);
}
static void closeFromList(void) {
    assert(EspNativeGameplayStore_handleTap(60,110)==2);
    assert(!EspNativeGameplayStore_isActive());
    assert(EspNativeGameplayStore_closePending());
    EspNativeGameplayStore_finishClose();
    assert(!EspNativeGameplayStore_closePending());
}

int main(void) {
    /* Store 0: first offer = small medkit at 8 credits. */
    seed(100);
    assert(!EspNativeGameplayStore_begin(4U,0U,0U));
    enterList(0U);pickFirst();confirmBuy();
    assert(player.credits==92U && player.inventory[0]==1U);
    closeFromList();

    /* Insufficient funds must neither debit credits nor grant items. */
    EspNativeGameplayStore_reset();seed(7);
    enterList(0U);pickFirst();confirmBuy();
    assert(player.credits==7U && player.inventory[0]==0U);
    assert(EspNativeGameplayStore_handleTap(80,82)==1); /* notice Back */
    closeFromList();

    /* Capped stack refuses the purchase atomically. */
    EspNativeGameplayStore_reset();seed(100);
    player.inventory[0]=99U;
    enterList(0U);pickFirst();confirmBuy();
    assert(player.credits==100U && player.inventory[0]==99U);
    assert(EspNativeGameplayStore_handleTap(80,82)==1);
    closeFromList();

    /* A vendor can be refused at the opening prompt; no credits change. */
    EspNativeGameplayStore_reset();seed(50);
    assert(EspNativeGameplayStore_begin(2U,0U,0U));
    assert(EspNativeGameplayStore_handleTap(80,85)==2);
    assert(player.credits==50U);
    EspNativeGameplayStore_finishClose();

    /* Store 3 first offer costs 4, and the second page is reachable. */
    EspNativeGameplayStore_reset();seed(100);
    enterList(3U);pickFirst();confirmBuy();
    assert(player.credits==96U && player.inventory[0]==1U);
    assert(EspNativeGameplayStore_handleTap(140,80)==1); /* Next */
    assert(EspNativeGameplayStore_handleTap(140,50)==1); /* Prev */
    closeFromList();

    /* Store 1 is a different valid catalog: first medkit costs 6. */
    EspNativeGameplayStore_reset();seed(50);
    enterList(1U);pickFirst();confirmBuy();
    assert(player.credits==44U && player.inventory[0]==1U);
    closeFromList();

    /* A page-2 stat upgrade mutates only the corresponding byte of param2. */
    EspNativeGameplayStore_reset();seed(100);
    enterList(0U);
    assert(EspNativeGameplayStore_handleTap(140,80)==1);
    assert(EspNativeGameplayStore_handleTap(60,70)==1); /* idx 7, ACC */
    confirmBuy();
    assert(player.credits==85U && player.param2==0x110e0c10U);
    closeFromList();

    assert(presents>20U && textDraws>20U);
    puts("[STORE] HOST-SELF-TEST PASS vendor-prices/player-credit/stack-cap/page/stat/no-rng");
    return 0;
}
