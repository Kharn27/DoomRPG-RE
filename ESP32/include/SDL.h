#ifndef DOOMRPG_ESP32_SDL_H
#define DOOMRPG_ESP32_SDL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SDLCALL
#define SDL_TRUE 1
#define SDL_FALSE 0
#define SDL_ENABLE 1
#define SDL_DISABLE 0

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;
typedef int64_t Sint64;
typedef int SDL_bool;

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_GameController SDL_GameController;
typedef struct SDL_Joystick SDL_Joystick;
typedef struct SDL_Haptic SDL_Haptic;
typedef struct SDL_RWops SDL_RWops;

typedef struct SDL_Rect {
    int x, y, w, h;
} SDL_Rect;

typedef struct SDL_Point {
    int x, y;
} SDL_Point;

typedef struct SDL_Color {
    Uint8 r, g, b, a;
} SDL_Color;

typedef struct SDL_Palette {
    int ncolors;
    SDL_Color* colors;
} SDL_Palette;

typedef struct SDL_PixelFormat {
    Uint32 format;
    SDL_Palette* palette;
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
    Uint8 padding[2];
    Uint32 Rmask, Gmask, Bmask, Amask;
} SDL_PixelFormat;

typedef struct SDL_Surface {
    Uint32 flags;
    SDL_PixelFormat* format;
    int w, h;
    int pitch;
    void* pixels;
    Uint32 colorKey;
    SDL_bool hasColorKey;
} SDL_Surface;

typedef void* (SDLCALL *SDL_malloc_func)(size_t size);
typedef void* (SDLCALL *SDL_calloc_func)(size_t nmemb, size_t size);
typedef void* (SDLCALL *SDL_realloc_func)(void* mem, size_t size);
typedef void (SDLCALL *SDL_free_func)(void* mem);

enum {
    SDL_SCANCODE_A = 4,
    SDL_SCANCODE_C = 6,
    SDL_SCANCODE_D = 7,
    SDL_SCANCODE_X = 27,
    SDL_SCANCODE_Z = 29,
    SDL_SCANCODE_1 = 30,
    SDL_SCANCODE_0 = 39,
    SDL_SCANCODE_RETURN = 40,
    SDL_SCANCODE_ESCAPE = 41,
    SDL_SCANCODE_TAB = 43,
    SDL_SCANCODE_RIGHT = 79,
    SDL_SCANCODE_LEFT = 80,
    SDL_SCANCODE_DOWN = 81,
    SDL_SCANCODE_UP = 82,
    SDL_SCANCODE_KP_1 = 89,
    SDL_SCANCODE_KP_0 = 98,
    SDL_NUM_SCANCODES = 512,
};

enum {
    SDL_PIXELFORMAT_RGB565 = 1,
    SDL_TEXTUREACCESS_STREAMING = 1,
    SDL_BLENDMODE_NONE = 0,
    SDL_BLENDMODE_BLEND = 1,
    SDL_WINDOW_FULLSCREEN = 1,
    SDL_MESSAGEBOX_ERROR = 0x10,
};

#define SDL_HINT_RENDER_VSYNC "SDL_RENDER_VSYNC"
#define SDL_BYTESPERPIXEL(format) ((format) == SDL_PIXELFORMAT_RGB565 ? 2 : 0)
#define SDL_arraysize(array) (sizeof(array) / sizeof((array)[0]))

#define SDL_malloc malloc
#define SDL_calloc calloc
#define SDL_realloc realloc
#define SDL_free free
#define SDL_memset memset
#define SDL_memcpy memcpy
#define SDL_memmove memmove
#define SDL_memcmp memcmp
#define SDL_strlen strlen
#define SDL_strcmp strcmp
#define SDL_strcasecmp strcasecmp
#define SDL_strchr strchr
#define SDL_atoi atoi
#define SDL_snprintf snprintf

typedef struct SDL_MessageBoxButtonData {
    Uint32 flags;
    int buttonid;
    const char* text;
} SDL_MessageBoxButtonData;

enum {
    SDL_MESSAGEBOX_COLOR_BACKGROUND,
    SDL_MESSAGEBOX_COLOR_TEXT,
    SDL_MESSAGEBOX_COLOR_BUTTON_BORDER,
    SDL_MESSAGEBOX_COLOR_BUTTON_BACKGROUND,
    SDL_MESSAGEBOX_COLOR_BUTTON_SELECTED,
    SDL_MESSAGEBOX_COLOR_MAX,
};

typedef struct SDL_MessageBoxColor {
    Uint8 r, g, b;
} SDL_MessageBoxColor;

typedef struct SDL_MessageBoxColorScheme {
    SDL_MessageBoxColor colors[SDL_MESSAGEBOX_COLOR_MAX];
} SDL_MessageBoxColorScheme;

typedef struct SDL_MessageBoxData {
    Uint32 flags;
    SDL_Window* window;
    const char* title;
    const char* message;
    int numbuttons;
    const SDL_MessageBoxButtonData* buttons;
    const SDL_MessageBoxColorScheme* colorScheme;
} SDL_MessageBoxData;

Uint32 SDL_GetTicks(void);
Uint16 SDL_SwapLE16(Uint16 value);
Uint32 SDL_SwapLE32(Uint32 value);
const char* SDL_GetError(void);
const char* SDL_GetScancodeName(int scancode);

int SDL_SetMemoryFunctions(SDL_malloc_func mallocFn, SDL_calloc_func callocFn,
                           SDL_realloc_func reallocFn, SDL_free_func freeFn);
void SDL_GetMemoryFunctions(SDL_malloc_func* mallocFn, SDL_calloc_func* callocFn,
                            SDL_realloc_func* reallocFn, SDL_free_func* freeFn);

SDL_RWops* SDL_RWFromMem(void* memory, int size);
SDL_RWops* SDL_RWFromFile(const char* path, const char* mode);
Sint32 SDL_RWsize(SDL_RWops* context);
Sint64 SDL_RWseek(SDL_RWops* context, Sint64 offset, int whence);
size_t SDL_RWread(SDL_RWops* context, void* ptr, size_t size, size_t count);
size_t SDL_RWwrite(SDL_RWops* context, const void* ptr, size_t size, size_t count);
int SDL_RWclose(SDL_RWops* context);

SDL_Surface* SDL_LoadBMP(const char* path);
SDL_Surface* SDL_LoadBMP_RW(SDL_RWops* source, int freeSource);
void SDL_FreeSurface(SDL_Surface* surface);
Uint32 SDL_MapRGB(const SDL_PixelFormat* format, Uint8 r, Uint8 g, Uint8 b);
int SDL_SetColorKey(SDL_Surface* surface, int flag, Uint32 key);

SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, Uint32 format,
                               int access, int width, int height);
SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface);
void SDL_DestroyTexture(SDL_Texture* texture);
int SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect,
                      const void* pixels, int pitch);
int SDL_SetTextureColorMod(SDL_Texture* texture, Uint8 r, Uint8 g, Uint8 b);
int SDL_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture,
                   const SDL_Rect* source, const SDL_Rect* destination);

int SDL_RenderSetClipRect(SDL_Renderer* renderer, const SDL_Rect* rect);
int SDL_SetRenderDrawColor(SDL_Renderer* renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int SDL_SetRenderDrawBlendMode(SDL_Renderer* renderer, int blendMode);
int SDL_RenderClear(SDL_Renderer* renderer);
void SDL_RenderPresent(SDL_Renderer* renderer);
int SDL_RenderDrawRect(SDL_Renderer* renderer, const SDL_Rect* rect);
int SDL_RenderFillRect(SDL_Renderer* renderer, const SDL_Rect* rect);
int SDL_RenderDrawLine(SDL_Renderer* renderer, int x1, int y1, int x2, int y2);
int SDL_RenderSetIntegerScale(SDL_Renderer* renderer, SDL_bool enabled);

int SDL_ShowMessageBox(const SDL_MessageBoxData* data, int* buttonId);
int SDL_ShowSimpleMessageBox(Uint32 flags, const char* title,
                             const char* message, SDL_Window* window);
int SDL_SetHint(const char* name, const char* value);
int SDL_ShowCursor(int toggle);
int SDL_SetWindowFullscreen(SDL_Window* window, Uint32 flags);
int SDL_NumJoysticks(void);
SDL_bool SDL_IsGameController(int index);
int SDL_GameControllerRumble(SDL_GameController* controller, Uint16 low,
                             Uint16 high, Uint32 durationMs);
int SDL_HapticRumblePlay(SDL_Haptic* haptic, float strength, Uint32 durationMs);
int SDL_HapticRumbleStop(SDL_Haptic* haptic);

#ifdef __cplusplus
}
#endif

#endif
