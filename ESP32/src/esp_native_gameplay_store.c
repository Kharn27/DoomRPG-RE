#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_native_gameplay_store.h"
#include "esp_native_gameplay_hub_theme.h"
#include "esp_native_gameplay_hub_touch_ui.h"
#include "esp_native_gameplay_player_state.h"
#include "platform_video_c_bridge.h"
#include "platform_video_config.h"

#define STORE_COUNT 4U
#define STORE_TABLE_ROWS 17U
#define STORE_PAGE_ROWS 5U
#define STORE_MAX_STACK 99U

/* Exact Menu.c vendingMenuTable order and prices; non-positive entries are
 * Back/dividers/sentinels and are never sold. Name tags refer to the recovered
 * Menu_setStore() table below; all four stores use this same bounded catalog. */
static const uint8_t storeTags[STORE_COUNT][STORE_TABLE_ROWS] = {
    {0,1,2,3,6,7,8,9,11,10,12,13,14,15,16,0,0},
    {0,1,2,6,8,7,9,12,13,14,15,16,0,0,0,0,0},
    {0,1,2,3,4,6,7,8,11,9,10,12,13,14,15,16,0},
    {0,1,2,3,4,5,6,7,8,9,11,10,12,13,14,15,16}
};
static const int8_t storePrices[STORE_COUNT][STORE_TABLE_ROWS] = {
    {-1,-1,8,35,-1,5,5,10,10,15,-1,15,15,20,20,0,0},
    {-1,-1,6,-1,3,4,6,-1,15,15,20,20,0,0,0,0,0},
    {-1,-1,6,25,50,-1,3,4,6,8,12,-1,15,15,20,20,0},
    {-1,-1,4,15,30,40,-1,2,2,4,5,8,-1,8,8,10,10}
};
/* kind=1 inventory, 2 ammo, 3 character stat. Amounts match legacy. */
typedef struct StoreItem_s {
    const char* label;
    uint8_t kind;
    uint8_t index;
    uint8_t amount;
} StoreItem;
static const StoreItem storeItems[17] = {
    {"",0,0,0},{"",0,0,0},
    {"SM MEDKIT",1,0,1},{"LG MEDKIT",1,1,1},
    {"SOUL SPHERE",1,2,1},{"BERSERKER",1,3,1},
    {"",0,0,0},
    {"10X HAL CANS",2,0,10},{"10X BULLETS",2,1,10},
    {"10X SHELLS",2,2,10},{"10X CELLS",2,4,10},
    {"3X ROCKETS",2,3,3},{"",0,0,0},
    {"+1 ACCURACY",3,3,1},{"+1 AGILITY",3,2,1},
    {"+1 STRENGTH",3,1,1},{"+1 DEFENSE",3,0,1}
};

enum { STORE_CLOSED=0, STORE_PROMPT=1, STORE_LIST=2,
       STORE_CONFIRM=3, STORE_NOTICE=4 };
typedef struct StoreState_s {
    uint16_t eventIndex;
    uint32_t purchases;
    uint8_t storeId;
    uint8_t commandOffset;
    uint8_t stage;
    uint8_t selected;
    uint8_t page;
    uint8_t count;
    uint8_t closePending;
    uint8_t notice; /* 1 funds, 2 maximum, 3 invalid */
} StoreState;
static StoreState store;

static void rect(uint16_t* fb, int l, int t, int r, int b,
                 uint16_t color) {
    int x,y;
    if (fb == NULL) return;
    for (y=t; y<=b; ++y) {
        if (y<0 || y>=DOOMRPG_LOGICAL_HEIGHT) continue;
        for (x=l; x<=r; ++x) {
            if (x>=0 && x<DOOMRPG_LOGICAL_WIDTH)
                fb[y*DOOMRPG_LOGICAL_WIDTH+x]=color;
        }
    }
}
static void border(uint16_t* fb, int l, int t, int r, int b,
                   uint16_t color) {
    rect(fb,l,t,r,t,color);
    rect(fb,l,b,r,b,color);
    rect(fb,l,t,l,b,color);
    rect(fb,r,t,r,b,color);
}
static void title(uint16_t* fb, const char* text, int center, int top,
                  uint16_t color) {
    EspNativeGameplayHubTouchUi_drawCrispText(fb,text,center,top,color);
}
static void card(uint16_t* fb,int l,int t,int r,int b,
                 const char* text,int selected) {
    rect(fb,l,t,r,b,selected?ESP_HUB_COLOR_PANEL_ALT:ESP_HUB_COLOR_PANEL);
    border(fb,l,t,r,b,selected?ESP_HUB_COLOR_AMBER:ESP_HUB_COLOR_STEEL_DARK);
    title(fb,text,(l+r)/2,t+3,
          selected?ESP_HUB_COLOR_GREEN:ESP_HUB_COLOR_IVORY);
}

static int catalogEntry(uint8_t vendor, uint8_t ordinal,
                        const StoreItem** item, uint8_t* price) {
    uint8_t count=0U, row;
    if (item!=NULL) *item=NULL;
    if (price!=NULL) *price=0U;
    if (vendor>=STORE_COUNT || item==NULL || price==NULL) return 0;
    for (row=0U;row<STORE_TABLE_ROWS;++row) {
        const uint8_t tag=storeTags[vendor][row];
        const int8_t cost=storePrices[vendor][row];
        if (cost<=0 || tag>=17U || storeItems[tag].kind==0U) continue;
        if (count==ordinal) {
            *item=&storeItems[tag];
            *price=(uint8_t)cost;
            return 1;
        }
        ++count;
    }
    return 0;
}
static uint8_t catalogCount(uint8_t vendor) {
    uint8_t count=0U;
    const StoreItem* item;
    uint8_t price;
    while (count<STORE_TABLE_ROWS &&
           catalogEntry(vendor,count,&item,&price)) ++count;
    return count;
}
static int paint(void) {
    uint16_t* fb=(uint16_t*)Esp32PlatformVideo_framebuffer();
    const EspNativeGameplayPlayerState* player=
        EspNativeGameplayPlayerState_view();
    char line[36];
    if (fb==NULL || Esp32PlatformVideo_framebufferSizeBytes()!=
            (size_t)(DOOMRPG_LOGICAL_WIDTH*DOOMRPG_LOGICAL_HEIGHT*2) ||
        player==NULL || player->active!=1U) return 0;
    rect(fb,0,0,159,119,ESP_HUB_COLOR_BG);
    border(fb,1,1,158,118,ESP_HUB_COLOR_STEEL);
    title(fb,"ITEM VENDOR",80,6,ESP_HUB_COLOR_AMBER);
    if (store.stage==STORE_PROMPT) {
        title(fb,"PURCHASE ITEMS?",80,31,ESP_HUB_COLOR_IVORY);
        card(fb,12,52,147,71,"YES",1);
        card(fb,12,78,147,97,"NO",0);
    } else if (store.stage==STORE_LIST) {
        snprintf(line,sizeof(line),"CREDITS: %lu",(unsigned long)player->credits);
        title(fb,line,80,17,ESP_HUB_COLOR_IVORY);
        snprintf(line,sizeof(line),"PAGE %u/%u",(unsigned)(store.page+1U),
                 (unsigned)((store.count+STORE_PAGE_ROWS-1U)/STORE_PAGE_ROWS));
        title(fb,line,80,28,ESP_HUB_COLOR_STEEL);
        for (uint8_t row=0U;row<STORE_PAGE_ROWS;++row) {
            const uint8_t entry=(uint8_t)(store.page*STORE_PAGE_ROWS+row);
            const StoreItem* item;
            uint8_t price;
            if (entry>=store.count ||
                !catalogEntry(store.storeId,entry,&item,&price)) break;
            snprintf(line,sizeof(line),"%s %u",item->label,(unsigned)price);
            card(fb,7,38+13*row,113,49+13*row,line,entry==store.selected);
        }
        card(fb,118,39,155,64,"PREV",store.page>0U);
        card(fb,118,71,155,96,"NEXT",
             (uint8_t)(store.page+1U)*STORE_PAGE_ROWS<store.count);
        card(fb,7,105,113,117,"BACK",0);
    } else if (store.stage==STORE_CONFIRM) {
        const StoreItem* item;
        uint8_t price;
        if (!catalogEntry(store.storeId,store.selected,&item,&price)) return 0;
        title(fb,"CONFIRM PURCHASE",80,25,ESP_HUB_COLOR_IVORY);
        title(fb,item->label,80,40,ESP_HUB_COLOR_AMBER);
        snprintf(line,sizeof(line),"COST %u CR",(unsigned)price);
        title(fb,line,80,52,ESP_HUB_COLOR_IVORY);
        card(fb,12,68,147,86,"YES - BUY",1);
        card(fb,12,92,147,111,"NO - BACK",0);
    } else if (store.stage==STORE_NOTICE) {
        title(fb,store.notice==1U?"NOT ENOUGH CREDITS":
                 store.notice==2U?"MAXIMUM REACHED":"PURCHASE BLOCKED",
              80,37,ESP_HUB_COLOR_IVORY);
        card(fb,12,76,147,98,"BACK",1);
    } else return 0;
    return Esp32PlatformVideo_present()!=0;
}

/* A purchase is a single bounded 52-byte player snapshot/restore.
 * Reject before mutation if funds insufficient or the destination is capped.
 * There is no RNG consumption, world mutation or monster turn. */
static int buySelected(void) {
    EspNativeGameplayPlayerState before,after;
    const StoreItem* item;
    uint8_t price;
    uint8_t current;
    uint32_t beforeFNV,afterFNV;
    if (!catalogEntry(store.storeId,store.selected,&item,&price) ||
        !EspNativeGameplayPlayerState_snapshot(&before)) return 0;
    if (before.credits<price) {
        store.notice=1U;store.stage=STORE_NOTICE;return 1;
    }
    after=before;
    if (item->kind==1U &&
        item->index<ESP_NATIVE_GAMEPLAY_PLAYER_INVENTORY_SLOTS) {
        current=after.inventory[item->index];
        if (current>=STORE_MAX_STACK) {
            store.notice=2U;store.stage=STORE_NOTICE;return 1;
        }
        after.inventory[item->index]=(uint8_t)(
            current+item->amount>STORE_MAX_STACK
                ? STORE_MAX_STACK:current+item->amount);
    } else if (item->kind==2U &&
               item->index<ESP_NATIVE_GAMEPLAY_PLAYER_AMMO_TYPES) {
        current=after.ammo[item->index];
        if (current>=STORE_MAX_STACK) {
            store.notice=2U;store.stage=STORE_NOTICE;return 1;
        }
        after.ammo[item->index]=(uint8_t)(
            current+item->amount>STORE_MAX_STACK
                ? STORE_MAX_STACK:current+item->amount);
    } else if (item->kind==3U && item->index<4U) {
        const uint8_t shift=(uint8_t)(item->index*8U);
        current=(uint8_t)((after.param2>>shift)&0xffU);
        if (current>=STORE_MAX_STACK) {
            store.notice=2U;store.stage=STORE_NOTICE;return 1;
        }
        after.param2=(after.param2&~((uint32_t)0xffU<<shift))|
                     ((uint32_t)(current+1U)<<shift);
    } else {
        store.notice=3U;store.stage=STORE_NOTICE;return 1;
    }
    after.credits-=price;
    beforeFNV=EspNativeGameplayPlayerState_fingerprint();
    if (!EspNativeGameplayPlayerState_restore(&after)) return 0;
    afterFNV=EspNativeGameplayPlayerState_fingerprint();
    ++store.purchases;
    printf("[STORE] PURCHASE vendor=%u entry=%u item=%s kind=%u index=%u amount=%u price=%u credits=%lu->%lu playerFNV=%08lx->%08lx mutation=player-only rollback=closed turn=no rng=untouched\n",
           (unsigned)store.storeId,(unsigned)store.selected,item->label,
           (unsigned)item->kind,(unsigned)item->index,(unsigned)item->amount,
           (unsigned)price,(unsigned long)before.credits,
           (unsigned long)after.credits,(unsigned long)beforeFNV,
           (unsigned long)afterFNV);
    store.stage=STORE_LIST;
    return 1;
}

void EspNativeGameplayStore_reset(void) { memset(&store,0,sizeof(store)); }
int EspNativeGameplayStore_isActive(void) {return store.stage!=STORE_CLOSED;}
int EspNativeGameplayStore_closePending(void) {return store.closePending!=0U;}
void EspNativeGameplayStore_finishClose(void) {store.closePending=0U;}

int EspNativeGameplayStore_begin(uint8_t storeId,
                                  uint16_t eventIndex,
                                  uint8_t commandOffset) {
    if (storeId>=STORE_COUNT || store.stage!=STORE_CLOSED ||
        store.closePending!=0U || EspNativeGameplayPlayerState_view()==NULL)
        return 0;
    memset(&store,0,sizeof(store));
    store.storeId=storeId;
    store.eventIndex=eventIndex;
    store.commandOffset=commandOffset;
    store.count=catalogCount(storeId);
    store.stage=STORE_PROMPT;
    if (store.count==0U || !paint()) {
        EspNativeGameplayStore_reset();
        return 0;
    }
    printf("[STORE] OPEN event=%u cmd=%u opcode=33 vendor=%u entries=%u stage=confirmation credits=%lu script=paused turn=no worldMutation=no legacyMenuSystem=retired\n",
           (unsigned)eventIndex,(unsigned)commandOffset,
           (unsigned)storeId,(unsigned)store.count,
           (unsigned long)EspNativeGameplayPlayerState_view()->credits);
    return 1;
}
int EspNativeGameplayStore_handleTap(int x,int y) {
    EspNativeGameplayPlayerState purchaseBefore;
    const uint8_t purchaseAttempt =
        store.stage==STORE_CONFIRM && y>=68 && y<=86;
    const uint32_t purchasesBefore=store.purchases;
    if (store.closePending!=0U) return 0;
    if (store.stage==STORE_CLOSED || x<0 || x>=160 || y<0 || y>=120)
        return 0;
    if (purchaseAttempt &&
        !EspNativeGameplayPlayerState_snapshot(&purchaseBefore)) return -1;
    if (store.stage==STORE_PROMPT) {
        if (y>=52 && y<=71) store.stage=STORE_LIST;
        else if (y>=78 && y<=97) {
            store.stage=STORE_CLOSED;store.closePending=1U;
        } else return 0;
    } else if (store.stage==STORE_LIST) {
        if (x>=118 && x<=155 && y>=39 && y<=64 && store.page>0U) {
            --store.page;
            store.selected=(uint8_t)(store.page*STORE_PAGE_ROWS);
        } else if (x>=118 && x<=155 && y>=71 && y<=96 &&
                   (uint8_t)(store.page+1U)*STORE_PAGE_ROWS<store.count) {
            ++store.page;
            store.selected=(uint8_t)(store.page*STORE_PAGE_ROWS);
        } else if (x>=7 && x<=113 && y>=105 && y<=117) {
            store.stage=STORE_CLOSED;store.closePending=1U;
        } else if (x>=7 && x<=113 && y>=38 && y<=101) {
            const int row=(y-38)/13;
            const uint8_t chosen=(uint8_t)(store.page*STORE_PAGE_ROWS+row);
            if (row<0 || row>=STORE_PAGE_ROWS || chosen>=store.count ||
                y>49+13*row) return 0;
            store.selected=chosen;
            store.stage=STORE_CONFIRM;
        } else return 0;
    } else if (store.stage==STORE_CONFIRM) {
        if (y>=68 && y<=86) {
            if (!buySelected()) return -1;
        } else if (y>=92 && y<=111) store.stage=STORE_LIST;
        else return 0;
    } else if (store.stage==STORE_NOTICE) {
        if (y<76 || y>98) return 0;
        store.stage=STORE_LIST;
    } else return -1;
    if (store.closePending!=0U) {
        printf("[STORE] CLOSE vendor=%u purchases=%lu worldRedraw=pending playerMutation=purchases-only turn=no\n",
               (unsigned)store.storeId,(unsigned long)store.purchases);
        return 2;
    }
    if (!paint()) {
        if (purchaseAttempt && store.purchases!=purchasesBefore) {
            const int restored=EspNativeGameplayPlayerState_restore(
                &purchaseBefore);
            store.purchases=purchasesBefore;
            printf("[STORE] ROLLBACK reason=purchase-paint-failed restored=%s turn=no rng=untouched\n",
                   restored?"yes":"NO");
        }
        return -1;
    }
    return 1;
}
