/* Production HUB tab classifier + input router. Exhaustive 160x20 pixels.
 * The world AUTOMAP/PASS zones remain alive only AFTER the HUB is closed. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_gameplay_hub.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_hub_nonweapon.h"
#include "esp_native_gameplay_hub_weapon_grid.h"
#include "esp_native_gameplay_hub_theme.h"
#include "esp_native_gameplay_input.h"

#define WIDTH 160
#define HEIGHT 120
#define SENTINEL 0x5aa5U
static EspNativeGameplayHubView fake;
static uint16_t frame[WIDTH*HEIGHT+2];

const EspNativeGameplayHubView* EspNativeGameplayHub_view(void) {
    return &fake;
}
int EspNativeGameplayHub_notebookOpen(void) { return 0; }
int EspNativeGameplayPlayerState_snapshot(EspNativeGameplayPlayerState* out) {
    (void)out; return 0;
}
uint8_t EspNativeGameplayHubNonWeapon_entryCount(
    const EspNativeGameplayPlayerState* player) {
    (void)player; return 0;
}
int EspNativeGameplayHubWeaponGrid_hitTest(
    int x, int y, uint8_t* outWeapon) {
    (void)x; (void)y; (void)outWeapon; return 0;
}
void EspNativeGameplayHubWeaponGrid_cellBounds(
    uint8_t id, uint8_t* l, uint8_t* t, uint8_t* r, uint8_t* b) {
    (void)id; (void)l; (void)t; (void)r; (void)b;
}

static int tabFor(int x, int y) {
    if (y < ESP_NATIVE_HUB_TOP_TAB_TOP ||
        y > ESP_NATIVE_HUB_TOP_TAB_BOTTOM) return -1;
    for (int i=0;i<ESP_NATIVE_HUB_TOP_TAB_COUNT;++i) {
        int l,r;
        assert(EspNativeGameplayHubTouchUi_headerTabBounds(i,&l,&r));
        if(x>=l&&x<=r) return i;
    }
    return -1;
}
static uint8_t pageFor(int tab) {
    return tab==0?ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY:
           tab==1?ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS:
                  ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM;
}
static void seedFrame(void) {
    for (unsigned i=0;i<sizeof(frame)/sizeof(frame[0]);++i)
        frame[i]=SENTINEL;
}
int main(void) {
    EspNativeGameplayTouchHit hit;
    EspNativeGameplayInputState intent;
    const uint8_t pages[]={
        ESP_NATIVE_GAMEPLAY_HUB_PAGE_INVENTORY,
        ESP_NATIVE_GAMEPLAY_HUB_PAGE_STATUS,
        ESP_NATIVE_GAMEPLAY_HUB_PAGE_SYSTEM
    };
    fake.active=1;
    for(int p=0;p<3;++p) {
        fake.page=pages[p];
        seedFrame();
        assert(EspNativeGameplayHubTouchUi_paintHeaderTabs(frame+1,fake.page));
        assert(frame[0]==SENTINEL && frame[WIDTH*HEIGHT+1]==SENTINEL);
        for(int y=0;y<HEIGHT;++y)for(int x=0;x<WIDTH;++x) {
            if(x<32 || y>=20) assert(frame[1+y*WIDTH+x]==SENTINEL);
        }
        for(int tab=0;tab<3;++tab) {
            int l,r;
            assert(EspNativeGameplayHubTouchUi_headerTabBounds(tab,&l,&r));
            assert(frame[1+ESP_NATIVE_HUB_TOP_TAB_TOP*WIDTH+l] ==
                   (p==tab ? ESP_HUB_COLOR_AMBER : ESP_HUB_COLOR_STEEL));
        }
        for(int y=0;y<20;++y)for(int x=0;x<160;++x) {
            const int tab=tabFor(x,y);
            const EspNativeGameplayInputStatus status =
                EspNativeGameplayInput_classify(x,y,&hit);
            if(x<=31) {
                assert(status==ESP_NATIVE_GAMEPLAY_INPUT_OK);
                assert(hit.action==ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN);
            } else if(tab>=0 && pageFor(tab)!=fake.page) {
                assert(status==ESP_NATIVE_GAMEPLAY_INPUT_OK);
                assert(hit.action==ESP_NATIVE_GAMEPLAY_ACTION_TURN_LEFT ||
                       hit.action==ESP_NATIVE_GAMEPLAY_ACTION_TURN_RIGHT);
                assert(hit.left <= x && x <= hit.right &&
                       hit.top <= y && y <= hit.bottom);
                EspNativeGameplayInput_reset();
                assert(EspNativeGameplayInput_route(&hit,x,y)==
                       ESP_NATIVE_GAMEPLAY_INPUT_OK);
                assert(EspNativeGameplayInput_consume(&intent)==
                       ESP_NATIVE_GAMEPLAY_INPUT_OK);
                uint8_t target=255;
                assert(EspNativeGameplayHubTouchUi_consumedPageTarget(&target));
                assert(target==pageFor(tab));
            } else {
                assert(status==ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT);
            }
            if(status==ESP_NATIVE_GAMEPLAY_INPUT_OK) {
                assert(hit.action!=ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP);
                assert(hit.action!=ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN);
            }
        }
    }
    /* Standalone weapon page protects upper band but preserves MENU hand. */
    fake.page=ESP_NATIVE_GAMEPLAY_HUB_PAGE_WEAPONS;
    assert(!EspNativeGameplayHubTouchUi_paintHeaderTabs(frame+1,fake.page));
    assert(EspNativeGameplayInput_classify(150,10,&hit)==
           ESP_NATIVE_GAMEPLAY_INPUT_NO_HIT);
    assert(EspNativeGameplayInput_classify(15,10,&hit)==
           ESP_NATIVE_GAMEPLAY_INPUT_OK);
    assert(hit.action==ESP_NATIVE_GAMEPLAY_ACTION_MENU_OPEN);
    /* After HUB close, world topbar controls keep exact former semantics. */
    fake.active=0;
    assert(EspNativeGameplayInput_classify(150,10,&hit)==
           ESP_NATIVE_GAMEPLAY_INPUT_OK);
    assert(hit.action==ESP_NATIVE_GAMEPLAY_ACTION_AUTOMAP);
    assert(EspNativeGameplayInput_classify(80,10,&hit)==
           ESP_NATIVE_GAMEPLAY_INPUT_OK);
    assert(hit.action==ESP_NATIVE_GAMEPLAY_ACTION_PASS_TURN);
    puts("HUB-TOPBAR-LIVE PASS real paint/classify/consumed target; AUTOMAP isolated");
    return 0;
}
