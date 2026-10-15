#include <SDL.h>
#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "SDL_Video.h"

/*
 * Permanent classic-CYD presentation compatibility boundary.
 * The existing ESP32 SDL software renderer owns RGB565 pixels and clip/color.
 * This module only routes the seven still-linked DoomRPG drawing ABI entries
 * into that renderer (or the compact canvas font-color field).
 *
 * Source-equivalent to the generated ESP32 behavior: displayRect offsets
 * were already removed because CYD coordinates are logical 160x120.
 * No new frame, textures, world mutation, present cadence, or SDL globals.
 * Compare both implementations in ESP32/test/doomrpg_presentation_host.py.
 */

void DoomRPG_setClipFalse(DoomRPG_t* doomrpg)
{
	SDL_RenderSetClipRect(sdlVideo.renderer, NULL);
	doomrpg->graphSetCliping = false;
}

void DoomRPG_setClipTrue(DoomRPG_t* doomrpg, int x, int y, int w, int h)
{
	SDL_Rect clip;

	clip.x = x;
	clip.y = y;
	clip.w = w;
	clip.h = h;

	SDL_RenderSetClipRect(sdlVideo.renderer, &clip);
	doomrpg->graphSetCliping = true;
}

void DoomRPG_setColor(DoomRPG_t* doomrpg, int color)
{
	byte a, r, g, b;

	a = (color & 0xFF000000) >> 24;
	r = (color & 0x00FF0000) >> 16;
	g = (color & 0x0000FF00) >> 8;
	b = (color & 0x000000FF);

	SDL_SetRenderDrawColor(sdlVideo.renderer, r, g, b, a);
}

void DoomRPG_setFontColor(DoomRPG_t* doomrpg, int color)
{
	doomrpg->doomCanvas->fontColor = color;
}

void DoomRPG_fillRect(DoomRPG_t* doomrpg, int x, int y, int w, int h)
{
	SDL_Rect rect;

	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;
	SDL_RenderFillRect(sdlVideo.renderer, &rect);
}

void DoomRPG_drawLine(DoomRPG_t* doomrpg, int x1, int y1, int x2, int y2)
{
	int sx = x1;
	int sy = y1;
	int ex = x2;
	int ey = y2;

	SDL_RenderDrawLine(sdlVideo.renderer, sx, sy, ex, ey);
}

void DoomRPG_flushGraphics(DoomRPG_t* doomrpg)
{
	SDL_RenderPresent(sdlVideo.renderer);
}
