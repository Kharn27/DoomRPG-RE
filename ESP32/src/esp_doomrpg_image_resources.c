#include <SDL.h>
#include <stdio.h>

#include "DoomRPG.h"
#include "SDL_Video.h"
#include "esp_native_indexed_image.h"
#include "esp_native_texture_bridge.h"
#include "esp_legacy_asset_source.h"

/*
 * SDL-C1: production image creation reads indexed BMP ranges directly from
 * the native PAK into its final packed pixels and quantized palette. The
 * short-lived adapter transfers both allocations to the existing texture
 * renderer without copying. Legacy DoomRPG Image_t ABI and imageMemory
 * accounting remain unchanged; ZIP is not a runtime dependency.
 */

void DoomRPG_createImage(DoomRPG_t* doomrpg, const char* resourceName, boolean isTransparentMask, Image_t* img)
{
    EspNativeIndexedImage decoded = {0};
    EspNativeImageStatus status;
    SDL_Texture* texture;
    char fileName[64];
    int mem;

    if (doomrpg == NULL || resourceName == NULL || img == NULL) {
        DoomRPG_Error("Invalid native image arguments");
        return;
    }
    mem = DoomRPG_freeMemory();
    snprintf(fileName, sizeof(fileName), "%s", resourceName);

    status = EspNativeImage_loadPack(fileName, isTransparentMask, &decoded);
    if (status != ESP_NATIVE_IMAGE_OK) {
        DoomRPG_Error("Unable to load native indexed image %s (status %d)\n",
                      fileName, (int)status);
        return;
    }

    texture = EspNativeTexture_adoptIndexed(&decoded);
    if (texture == NULL) {
        /* Adoption failed: both allocations still belong to decoded. */
        EspNativeImage_dispose(&decoded);
        DoomRPG_Error("Unable to adopt native indexed image %s! SDL Error: %s\n",
                      fileName, SDL_GetError());
        return;
    }

    img->isTransparentMask = isTransparentMask;
    img->width = decoded.width;
    img->height = decoded.height;
    img->imgBitmap = texture;
    /* The adapter nulled owned pointers; this is an empty, safe disposal. */
    EspNativeImage_dispose(&decoded);
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
