#ifndef ESP_NATIVE_MENU_STATE_H
#define ESP_NATIVE_MENU_STATE_H

#include "DoomRPG.h"
#include "MenuItem.h"

#define ESP_NATIVE_MENU_MAX_ITEMS 8

typedef struct EspNativeMenuState_s
{
    Image_t imgLogo;
    Image_t* imgBG;
    MenuItem_t items[ESP_NATIVE_MENU_MAX_ITEMS];
    int numItems;
    int menu;
    int oldMenu;
    int selectedIndex;
    int scrollIndex;
    int type;
    int maxItems;
} EspNativeMenuState_t;

#endif
