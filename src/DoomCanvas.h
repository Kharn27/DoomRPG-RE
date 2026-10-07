#ifndef DOOMCANVAS_H__
#define DOOMCANVAS_H__

#include "Render.h"

struct Image_s;
struct Entity_s;
struct Line_s;

struct DoomRPG_s;
struct Render_s;
struct Player_s;
struct Game_s;
struct EntityDefManager_s;
struct Combat_s;
struct Hud_s;
#ifdef DOOMRPG_ESP32
struct EspNativeMenuState_s;
#else
struct MenuSystem_s;
#endif
struct ParticleSystem_s;

typedef struct DoomCanvas_s
{
#ifndef DOOMRPG_ESP32
	int memory;
#endif
	struct Image_s imgFont;
	struct Image_s imgLargerFont;
#ifndef DOOMRPG_ESP32
	struct Image_s imgMapCursor;
#endif
	int SCR_CX;
	int SCR_CY;
	int viewX;
	int viewY;
	int viewZ;
	int viewAngle;
#ifndef DOOMRPG_ESP32
	int destX;
	int destY;
	int destAngle;
	int viewSin;
	int viewCos;
	int viewStepX;
	int viewStepY;
#endif
	int animFrames;
	int animPos;
	int animAngle;
	boolean skipCheckState;
	boolean isUpdateView;
	boolean staleView;
#ifndef DOOMRPG_ESP32
	boolean f438d;
	boolean automapDrawn;
	boolean speeds;
	boolean s_debug;
#endif
	boolean skipShakeX;
#ifndef DOOMRPG_ESP32
	boolean slowBlit;
#endif
	boolean sndFXOnly;
#ifndef DOOMRPG_ESP32
	boolean combatDone;
	boolean unloadMedia;
#endif
	boolean largeStatus;
	int time;
	int state;
	int oldState;
#ifndef DOOMRPG_ESP32
	short loadMapID;
#endif
	short startupMap;
#ifndef DOOMRPG_ESP32
	int loadType;
	int saveType;
	boolean renderOnly;
#endif
#ifndef DOOMRPG_ESP32
	boolean vibrateEnabled;
#endif
	boolean skipIntro;
#ifndef DOOMRPG_ESP32
	boolean legalsSkip;
	boolean abortMove;
	int shaketime;
	int shakeVal;
#endif
	int shakeX;
	int shakeY;
#ifdef DOOMRPG_ESP32
	/* Dialog text/index payloads are owned by the native dialog runtime. Keep
	 * these desktop map-wide stores out of the permanent ESP32 compatibility
	 * object so any future direct reuse fails at compile time. */
#else
	short dialogIndexes[1024];
	char dialogBuffer[512];
	int dialogLineStartTime;
	int dialogTypeLineIdx;
	int numDialogLines;
	int currentDialogLine;
	boolean dialogBackSoftKey;
#endif
#ifndef DOOMRPG_ESP32
	int passwordTime;
	char passInput;
	char passCode[8];
	char strPassCode[8];
#endif
#ifndef DOOMRPG_ESP32
	int beforeRender;
	int afterRender;
	int loopStart;
	int loopEnd;
	int lastRenderTime;
	int lastLoopTime;
#endif
#ifndef DOOMRPG_ESP32
	int lastFrameTime;
	int idleTime;
	boolean benchmarkString;
	int st_count;
	int renderAvgMs;
	int horizAvgMs;
	int loopAvgMs;
	int events[8]; // old short type
	byte numEvents;
#endif
#ifndef DOOMRPG_ESP32
	struct Image_s imgSpaceBG;
	struct Image_s imgLinesLayer;
	struct Image_s imgPlanetLayer;
	struct Image_s imgSpaceship;
	int storyTextTime;
	int storyAnimTime;
	char *storyText1[2];
	char *storyText2;
	int storyPage;
	int storyTextPage;
	boolean showTextDone;
#else
	/* ST_INTRO assets/text/page state are transient ESP32-native ownership. */
#endif
#ifndef DOOMRPG_ESP32
	struct Image_s imgLegals;
#endif
#ifndef DOOMRPG_ESP32
	char epilogueText[2][150];
	int epilogueTextPage;
	int epilogueTextTime;
	int automapBlinkTime;
	int automapBlinkState;
	char printMsg[128];
	int deathTime;
	int creditsTextTime;
	char *creditsText;
	int captureState;
	int captureTime;
#endif
#ifndef DOOMRPG_ESP32
	struct Entity_s *castEntity;
	int castSeq;
	int castTime;
	int castEntityLoopFrames;
	int castEntityBegAttackTime;
	int castEntityEndAttackTime;
	int field_0xdcc;
	int castEntityX;
	int castEntityY;
#endif
#ifndef DOOMRPG_ESP32
	int field_0xdd8;
	int field_0xddc;
	int legalsTime;
	int legalsNextImage;
#endif
#ifndef DOOMRPG_ESP32
	boolean insufficientSpace;
#endif
	boolean displaySoftKeys;
#ifndef DOOMRPG_ESP32
	struct Line_s* openDoors[8];
	int animFrameCount;
	int openDoorsCount;
	int fillRectIndex;
	int lastPacifierUpdate;
#endif
	SDL_Rect clipRect;
	SDL_Rect displayRect;
	SDL_Rect screenRect;
	short softKeyY;
	char softKeyLeft[12];
	char softKeyRight[12];

#ifndef DOOMRPG_ESP32
	boolean restoreSoftKeys; // New
#endif
	int fontColor; // New
#ifndef DOOMRPG_ESP32
	int mouseSensitivity; // New
	boolean mouseYMove; // New
	boolean sndPriority; // New
#endif
	boolean renderFloorCeilingTextures; // New

	struct DoomRPG_s* doomRpg;
	struct Render_s* render;
#ifndef DOOMRPG_ESP32
	struct Player_s* player;
	struct Game_s* game;
	struct EntityDefManager_s* entityDef;
	struct Combat_s* combat;
	struct Hud_s* hud;
	struct MenuSystem_s* menuSystem;
	struct ParticleSystem_s* particleSystem;
#endif
} DoomCanvas_t;


enum _CanvasState {
	ST_LEGALS = 0,
	ST_UNK1,
	ST_MENU,
	ST_PLAYING,
	ST_COMBAT,
	ST_AUTOMAP,
	ST_LOADING,
	ST_DIALOG,
	ST_DIALOGPASSWORD,
	ST_INTRO,
	ST_BENCHMARK,
	ST_BENCHMARKDONE,
	ST_CAPTUREDOG,
	ST_DYING,
	ST_PARTICLE,
	ST_EPILOGUE,
	ST_CAST,
	ST_CREDITS,
	ST_SAVING,
	ST_SORRY
};

DoomCanvas_t* DoomCanvas_init(DoomCanvas_t * doomCanvas, DoomRPG_t* doomRpg);
void DoomCanvas_free(DoomCanvas_t* doomCanvas, boolean freePtr);
void DoomCanvas_updatePlayerDoors(DoomCanvas_t* doomCanvas, Line_t* line);
void DoomCanvas_attemptMove(DoomCanvas_t* doomCanvas, int x, int y);
void DoomCanvas_automapState(DoomCanvas_t* doomCanvas);
void DoomCanvas_setupmenu(DoomCanvas_t* doomCanvas, boolean notdrawLoading);
#ifndef DOOMRPG_ESP32
int DoomCanvas_getOverall(DoomCanvas_t* doomCanvas);
#endif
void DoomCanvas_captureDogState(DoomCanvas_t* doomCanvas);
void DoomCanvas_closeDialog(DoomCanvas_t* doomCanvas);
void DoomCanvas_combatState(DoomCanvas_t* doomCanvas);
void DoomCanvas_dialogState(DoomCanvas_t* doomCanvas);
void DoomCanvas_disposeEpilogue(DoomCanvas_t* doomCanvas);
void DoomCanvas_disposeIntro(DoomCanvas_t* doomCanvas);
void DoomCanvas_drawAutomap(DoomCanvas_t* doomCanvas, boolean z);
void DoomCanvas_drawCredits(DoomCanvas_t* doomCanvas);
void DoomCanvas_castState(DoomCanvas_t* doomCanvas);
void DoomCanvas_drawEpilogue(DoomCanvas_t* doomCanvas);
void DoomCanvas_drawImage(DoomCanvas_t* doomcanvas, Image_t* img, int x, int y, int flags);
void DoomCanvas_drawStory(DoomCanvas_t* doomCanvas);
void DoomCanvas_drawRGB(DoomCanvas_t* doomCanvas);
void DoomCanvas_drawImageSpecial(DoomCanvas_t* doomCanvas, Image_t* img, int xSrc, int ySrc, int width, int height, int param_7, int xDst, int yDst, int flags);
void DoomCanvas_drawScrollBar(DoomCanvas_t* doomCanvas, int y, int totalHeight, int i3, int i4, int i5);
void DoomCanvas_drawSoftKeys(DoomCanvas_t* doomCanvas, char* softKeyLeft, char* softKeyRight);
void DoomCanvas_scrollSpaceBG(DoomCanvas_t* doomCanvas);
void DoomCanvas_drawString1(DoomCanvas_t* doomCanvas, char* text, int x, int y, int flags);
void DoomCanvas_drawString2(DoomCanvas_t* doomCanvas, char* text, int x, int y, int flags, int param_6);
void DoomCanvas_drawFont(DoomCanvas_t* doomCanvas, char* text, int x, int y, int flags, int strBeg, int strEnd, boolean isLargerFont);
void DoomCanvas_dyingState(DoomCanvas_t* doomCanvas);
void DoomCanvas_sorryState(DoomCanvas_t* doomCanvas);
void DoomCanvas_finishMovement(DoomCanvas_t* doomCanvas);
void DoomCanvas_finishRotation(DoomCanvas_t* doomCanvas);
int DoomCanvas_flagForFacingDir(DoomCanvas_t* doomCanvas);
int DoomCanvas_getKeyAction(DoomCanvas_t* doomCanvas, int i);
void DoomCanvas_handleDialogEvents(DoomCanvas_t* doomCanvas, int i);
void DoomCanvas_handleEvent(DoomCanvas_t* doomCanvas, int i);
void DoomCanvas_handlePasswordEvents(DoomCanvas_t* doomCanvas, int i);
void DoomCanvas_handleMenuEvents(DoomCanvas_t* doomCanvas, int i);
void DoomCanvas_handlePlayingEvents(DoomCanvas_t* doomCanvas, int i);
#ifndef DOOMRPG_ESP32
void DoomCanvas_initCredits(DoomCanvas_t* doomCanvas);
void DoomCanvas_loadEpilogueText(DoomCanvas_t* doomCanvas);
#endif
#ifndef DOOMRPG_ESP32
void DoomCanvas_loadPrologueText(DoomCanvas_t* doomCanvas);
#endif
void DoomCanvas_keyPressed(DoomCanvas_t* doomCanvas, int keyCode);
void DoomCanvas_loadMap(DoomCanvas_t* doomCanvas, int mapID);
boolean DoomCanvas_loadMedia(DoomCanvas_t* doomCanvas);
void DoomCanvas_LoadMenuMap(DoomCanvas_t* doomCanvas);
void DoomCanvas_loadState(DoomCanvas_t* doomCanvas, int i, char* text);
void DoomCanvas_legalsState(DoomCanvas_t* doomCanvas);
void DoomCanvas_menuState(DoomCanvas_t* doomCanvas);
void DoomCanvas_handleEpilogueInput(DoomCanvas_t* doomCanvas);
void DoomCanvas_handleStoryInput(DoomCanvas_t* doomCanvas);
void DoomCanvas_changeStoryPage(DoomCanvas_t* doomCanvas);
void DoomCanvas_playingState(DoomCanvas_t* doomCanvas);
void DoomCanvas_prepareDialog(DoomCanvas_t* doomCanvas, char* str, boolean dialogBackSoftKey);
void DoomCanvas_restoreSoftKeys(DoomCanvas_t* doomCanvas);
void DoomCanvas_renderOnlyState(DoomCanvas_t* doomCanvas);
#ifndef DOOMRPG_ESP32
void DoomCanvas_renderScene(DoomCanvas_t* doomCanvas, int x, int y, int angle);
#endif
void DoomCanvas_resume(DoomCanvas_t* doomCanvas);
void DoomCanvas_runInputEvents(DoomCanvas_t* doomCanvas);
void DoomCanvas_run(DoomCanvas_t* doomCanvas);
void DoomCanvas_saveState(DoomCanvas_t* doomCanvas, int i, char* text);
void DoomCanvas_setAnimFrames(DoomCanvas_t* doomCanvas, int i);
void DoomCanvas_setState(DoomCanvas_t* doomCanvas, int stateNum);
byte DoomCanvas_checkState(DoomCanvas_t* doomCanvas);
void DoomCanvas_invalidateRectAndUpdateView(DoomCanvas_t* doomCanvas);
void DoomCanvas_updateViewTrue(DoomCanvas_t* doomCanvas);
void DoomCanvas_startDialog(DoomCanvas_t* doomCanvas, char* text, boolean dialogBackSoftKey);
void DoomCanvas_startDialogPassword(DoomCanvas_t* doomCanvas, char* text);
void DoomCanvas_startShake(DoomCanvas_t* doomCanvas, int i, int i2, int i3);
void DoomCanvas_startSpeedTest(DoomCanvas_t* doomCanvas, boolean b);
void DoomCanvas_vibrate(DoomCanvas_t* doomCanvas, int i);
void DoomCanvas_startup(DoomCanvas_t* doomCanvas);
void DoomCanvas_uncoverAutomap(DoomCanvas_t* doomCanvas);
void DoomCanvas_unloadMedia(DoomCanvas_t* doomCanvas);
void DoomCanvas_checkFacingEntity(DoomCanvas_t* doomCanvas);
void DoomCanvas_updateLoadingBar(DoomCanvas_t* doomCanvas);
boolean DoomCanvas_updatePlayerAnimDoors(DoomCanvas_t* doomCanvas);
void DoomCanvas_updateView(DoomCanvas_t* doomCanvas);

#endif
