#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "SDL_Video.h"
#include "esp32_bmp.h"
#include "esp_legacy_asset_source.h"

/*
 * Permanent ESP32 image/resource compatibility owner. The bodies below
 * preserve the already hardware-validated generated DoomRPG.c behavior:
 * bounded per-asset PAK lease -> allocated bytes -> packed indexed BMP ->
 * palette quantization -> zero-copy SDL texture, with the same teardown.
 *
 * No ZIP is accessed by this runtime owner. The source-reference/patch-body
 * equality guards are in build_engine.py and the host regression.
 * Future refactors of the imageMemory metric, failure semantics, and pixel
 * ownership need their own hardware-tested milestone.
 */

void DoomRPG_createImage(DoomRPG_t* doomrpg, const char* resourceName, boolean isTransparentMask, Image_t* img)
{
	SDL_RWops* rw;
	SDL_Texture* newTexture = NULL;
	SDL_Surface* loadedSurface = NULL;
	char fileName[64];
	int mem;
	int r, g, b;

	//sprintf_s(fileName, 64, "BarData/%s", resourceName);

	mem = DoomRPG_freeMemory();

	// Load image file
	//loadedSurface = SDL_LoadBMP(fileName);

	snprintf(fileName, sizeof(fileName), "%s", resourceName);

	byte* fdata;
	int fSize;
	fdata = EspLegacyAssetSource_readAlloc(fileName, &fSize);

	rw = SDL_RWFromMem(fdata, fSize);
	if (!rw) {
		DoomRPG_Error("Error with SDL_RWFromMem: %s\n", SDL_GetError());
	}

	loadedSurface = Esp32Bmp_LoadRW(rw, SDL_TRUE);

	if (loadedSurface == NULL) {
		DoomRPG_Error("Unable to load image %s! SDL Error: %s\n", fileName, SDL_GetError());
	}

	img->isTransparentMask = isTransparentMask;
	if (isTransparentMask) {
		//Color key image
		Uint32 colorKey = SDL_MapRGB(loadedSurface->format, 255, 0, 255);
		SDL_SetColorKey(loadedSurface, SDL_TRUE, colorKey);
	}

	SDL_PixelFormat* fmt = loadedSurface->format;

	// Convierte a RGB565 y de nuevo a RGB888
	for (int i = 0; i < fmt->palette->ncolors; i++) {

		int color = ((fmt->palette->colors[i].r >> 3) << 11) | ((fmt->palette->colors[i].g >> 2) << 5) | (fmt->palette->colors[i].b >> 3);

		r = (color >> 11) & 0x1f;
		g = (color >> 5) & 0x3f;
		b = (color & 0x1f);

		fmt->palette->colors[i].r = (r << 3) | (r >> 2);
		fmt->palette->colors[i].g = (g << 2) | (g >> 4);
		fmt->palette->colors[i].b = (b << 3) | (b >> 2);
	}

	//Create texture from surface pixels
	newTexture = SDL_CreateTextureFromSurface(sdlVideo.renderer, loadedSurface);
	if (newTexture == NULL) {
		DoomRPG_Error("Unable to create texture from %s! SDL Error: %s\n", fileName, SDL_GetError());
	}

	// Get image dimensions
	img->width = loadedSurface->w;
	img->height = loadedSurface->h;
	img->imgBitmap = newTexture;

	//Get rid of old loaded surface
	SDL_FreeSurface(loadedSurface);

	SDL_free(fdata);

	doomrpg->imageMemory += (DoomRPG_freeMemory() - mem);
}

void DoomRPG_freeImage(DoomRPG_t* doomrpg, Image_t* image)
{
	int mem = DoomRPG_freeMemory();
	if (image->imgBitmap) {
		SDL_DestroyTexture(image->imgBitmap);
		image->imgBitmap = NULL;
	}
	doomrpg->imageMemory -= (mem - DoomRPG_freeMemory());
}

byte *DoomRPG_fileOpenRead(DoomRPG_t* doomrpg, const char* resourceName)
{
	//SDL_RWops* rw;
	byte *fdata;
	int fSize;
	char fileName[64];

	/*sprintf_s(fileName, sizeof(fileName), "BarData%s", resourceName);

	rw = SDL_RWFromFile(fileName, "r");
	if (rw == NULL) {
		DoomRPG_Error("DoomRPG_fileOpenRead: cannot open file %s", fileName);
	}

	fSize = (int)SDL_RWsize(rw);

	fdata = (byte*)SDL_malloc(fSize);
	if (fdata == NULL) {
		DoomRPG_Error("DoomRPG_fileOpenRead: Insufficient memory for allocation");
	}

	SDL_RWread(rw, fdata, fSize, 1);

	SDL_RWclose(rw);*/

	snprintf(fileName, sizeof(fileName), "%s", resourceName+1);

	fdata = EspLegacyAssetSource_readAlloc(fileName, &fSize);

	return fdata;
}
