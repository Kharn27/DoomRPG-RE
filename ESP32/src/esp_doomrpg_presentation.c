#include <SDL.h> /* transitional DoomRPG.h type ABI; not a runtime SDL call */
#include "esp_native_texture.h"
#include "DoomRPG.h"
#include "esp_native_root_state.h"
#include "DoomCanvas.h"
#include "platform_video_c_bridge.h"

/*
 * Permanent classic-CYD presentation compatibility boundary.
 * Native EspNativeTexture owns RGB565 pixels, clip and draw color.
 * This module routes DoomRPG drawing ABI calls
 * to native pixel primitives (or the compact canvas font-color field).
 *
 * Source-equivalent to the generated ESP32 behavior: displayRect offsets
 * were already removed because CYD coordinates are logical 160x120.
 * No new frame, textures, world mutation, present cadence, or SDL globals.
 * Compare both implementations in ESP32/test/doomrpg_presentation_host.py.
 */

static boolean nativeGraphClipSet;

void EspNativePresentation_resetClipState(void) { nativeGraphClipSet = false; }
boolean EspNativePresentation_clipEnabled(void) { return nativeGraphClipSet; }

void DoomRPG_setClipFalse(DoomRPG_t* doomrpg)
{
	EspNativeTexture_setClipRect(NULL);
	(void)doomrpg;
	nativeGraphClipSet = false;
}

void DoomRPG_setClipTrue(DoomRPG_t* doomrpg, int x, int y, int w, int h)
{
	EspNativeRect clip;

	clip.x = x;
	clip.y = y;
	clip.w = w;
	clip.h = h;

	EspNativeTexture_setClipRect(&clip);
	(void)doomrpg;
	nativeGraphClipSet = true;
}

void DoomRPG_setColor(DoomRPG_t* doomrpg, int color)
{
	byte a, r, g, b;

	a = (color & 0xFF000000) >> 24;
	r = (color & 0x00FF0000) >> 16;
	g = (color & 0x0000FF00) >> 8;
	b = (color & 0x000000FF);

	EspNativeTexture_setDrawColor(r, g, b, a);
}

void DoomRPG_setFontColor(DoomRPG_t* doomrpg, int color)
{
	EspNativeRootOwners_live()->doomCanvas->fontColor = color;
}

void DoomRPG_fillRect(DoomRPG_t* doomrpg, int x, int y, int w, int h)
{
	EspNativeRect rect;

	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;
	EspNativeTexture_fillRect(&rect);
}

void DoomRPG_drawLine(DoomRPG_t* doomrpg, int x1, int y1, int x2, int y2)
{
	int sx = x1;
	int sy = y1;
	int ex = x2;
	int ey = y2;

	EspNativeTexture_drawLine(sx, sy, ex, ey);
}

void DoomRPG_flushGraphics(DoomRPG_t* doomrpg)
{
	(void)Esp32PlatformVideo_present();
}
