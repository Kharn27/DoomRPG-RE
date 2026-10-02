#ifndef MENUSYSTEM_H__
#define MENUSYSTEM_H__

#include "MenuItem.h"

struct DoomRPG_s;
struct Image_s;

#ifdef DOOMRPG_ESP32
#define MAX_MENUITEMS 8
#else
#define MAX_MENUITEMS 96
#endif

#ifdef DOOMRPG_ESP32
typedef struct EspNativeMenuState_s
{
	struct Image_s imgLogo;
	struct Image_s* imgBG;
	struct MenuItem_s items[MAX_MENUITEMS];
	int numItems;
	int menu;
	int oldMenu;
	int selectedIndex;
	int scrollIndex;
	int type;
	int maxItems;
} EspNativeMenuState_t;

typedef EspNativeMenuState_t MenuSystem_t;
#else
typedef struct MenuSystem_s
{
#ifndef DOOMRPG_ESP32
	struct DoomRPG_s* doomRpg;
#endif
#ifndef DOOMRPG_ESP32
	int memory;
	struct Image_s imgHand;
	struct Image_s imgArrowUpDown;
#endif
	struct Image_s imgLogo;
	struct Image_s* imgBG;
	struct MenuItem_s items[MAX_MENUITEMS];
	int numItems;
#ifndef DOOMRPG_ESP32
	int field_0xc58;
#endif
	int menu;
	int oldMenu;
	int selectedIndex;
	int scrollIndex;
	int type;
	int maxItems;
#ifndef DOOMRPG_ESP32
	int f749g;
#endif
#ifndef DOOMRPG_ESP32
	int cheatCombo;
	int digitCount;
#endif
#ifndef DOOMRPG_ESP32
	boolean paintMenu;
#endif

#ifndef DOOMRPG_ESP32
	boolean setBind;// new
#endif
#ifndef DOOMRPG_ESP32
	char stringBuffer[32];
	int bindIndx;// new
	int nextMsgTime; // New
	int nextMsg;// new
#endif

} MenuSystem_t;
#endif


MenuSystem_t* MenuSystem_init(MenuSystem_t* menuSystem, DoomRPG_t* doomRpg);
void MenuSystem_free(MenuSystem_t* menuSystem, boolean freePtr);
void MenuSystem_back(MenuSystem_t* menuSystem);
char* MenuSystem_buildDivider(MenuSystem_t* menuSystem, char* str);
void MenuSystem_select(MenuSystem_t* menuSystem);
boolean MenuSystem_checkMenu(MenuSystem_t* menuSystem);
boolean MenuSystem_enterDigit(MenuSystem_t* menuSystem, int n);
void MenuSystem_moveDir(MenuSystem_t* menuSystem, int i);
void MenuSystem_paint(MenuSystem_t* menuSystem);
void MenuSystem_scrollDown(MenuSystem_t* menuSystem);
void MenuSystem_scrollPageDown(MenuSystem_t* menuSystem);
void MenuSystem_scrollPageUp(MenuSystem_t* menuSystem);
void MenuSystem_scrollUp(MenuSystem_t* menuSystem);
void MenuSystem_setMenu(MenuSystem_t* menuSystem, int menu);
void MenuSystem_playSound(MenuSystem_t* menuSystem);

void MenuSystem_startup(MenuSystem_t* menuSystem);

#endif
