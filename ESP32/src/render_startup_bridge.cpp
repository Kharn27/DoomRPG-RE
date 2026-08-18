extern "C" {
#include "DoomRPG.h"
#include "Render.h"
#include "SDL_Video.h"
#include "Z_Zip.h"

extern DoomRPG_t* doomRpg;
void __real_Render_free(Render_t* render, boolean freePtr);
}

#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

#include "platform_video.h"
#include "render_startup_probe.h"

namespace {

bool renderStartupAttempted = false;
bool renderStartupReady = false;

uint32_t heap8Free() {
    return static_cast<uint32_t>(heap_caps_get_free_size(MALLOC_CAP_8BIT));
}

uint32_t largest8Block() {
    return static_cast<uint32_t>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

const zip_entry_t* findZipEntry(const char* name) {
    if (name == nullptr || zipFile.entry == nullptr) {
        return nullptr;
    }

    for (int i = 0; i < zipFile.entry_count; ++i) {
        const zip_entry_t* entry = &zipFile.entry[i];
        if (entry->name != nullptr && SDL_strcasecmp(entry->name, name) == 0) {
            return entry;
        }
    }

    return nullptr;
}

bool preflightRenderResources() {
    static const char* const required[] = {
        "sintable.bin",
        "palettes.bin",
    };

    bool allPresent = true;
    printf("[RENDERSTART] Resource preflight (%u files)\n",
           static_cast<unsigned int>(sizeof(required) / sizeof(required[0])));

    for (const char* name : required) {
        const zip_entry_t* entry = findZipEntry(name);
        if (entry == nullptr) {
            printf("[RENDERSTART] MISSING %s\n", name);
            allPresent = false;
            continue;
        }

        printf("[RENDERSTART] %-14s c=%d u=%d\n",
               name, entry->csize, entry->usize);
    }

    if (!allPresent) {
        printf("[RENDERSTART] Resource preflight FAILED; startup skipped safely\n");
        return false;
    }

    printf("[RENDERSTART] Resource preflight OK\n");
    return true;
}

}  // namespace

extern "C" int __wrap_Render_startup(Render_t* render) {
    if (render == nullptr || render->doomRpg == nullptr ||
        render->doomRpg->doomCanvas == nullptr) {
        printf("[RENDER] ERROR invalid Render_startup object graph\n");
        return 0;
    }

    byte* fData = DoomRPG_fileOpenRead(render->doomRpg, "/sintable.bin");
    if (fData == nullptr) {
        printf("[RENDER] ERROR unable to load sintable.bin\n");
        return 0;
    }

    SDL_memmove(render->sinTable, fData, sizeof(render->sinTable));
    SDL_free(fData);
    for (int i = 0; i < 256; ++i) {
        render->sinTable[i] = SDL_SwapLE32(render->sinTable[i]);
    }
    printf("[RENDER] sintable loaded: %u bytes\n",
           static_cast<unsigned int>(sizeof(render->sinTable)));

    render->clipRect.x = render->doomRpg->doomCanvas->displayRect.x;
    render->clipRect.y = render->doomRpg->doomCanvas->displayRect.y;
    render->clipRect.w = render->doomRpg->doomCanvas->displayRect.w;
    render->clipRect.h = render->doomRpg->doomCanvas->displayRect.h;

    const int width = sdlVideo.rendererW;
    const int height = sdlVideo.rendererH;
    render->pitch = ((width * static_cast<int>(sizeof(uint16_t))) + 3) & ~3;
    const size_t framebufferBytes =
        static_cast<size_t>(render->pitch) * static_cast<size_t>(height);
    byte* const sharedFramebuffer =
        reinterpret_cast<byte*>(PlatformVideo_framebuffer());

    if (sharedFramebuffer == nullptr ||
        PlatformVideo_framebufferSizeBytes() < framebufferBytes) {
        printf("[RENDER] ERROR platform framebuffer unavailable: need=%u have=%u\n",
               static_cast<unsigned int>(framebufferBytes),
               static_cast<unsigned int>(PlatformVideo_framebufferSizeBytes()));
        return 0;
    }

    /*
     * Desktop Render_startup() owns both a streaming SDL texture and a second
     * RGB565 framebuffer. On the CYD the SDL renderer already draws directly
     * into PlatformVideo's 160x120 RGB565 framebuffer, so Render uses that same
     * storage and piDIB deliberately stays NULL. SDL_UpdateTexture/RenderCopy
     * in the legacy copy paths already reject a NULL texture safely; the next
     * SDL_RenderPresent() presents this shared buffer directly.
     */
    render->piDIB = nullptr;
    render->framebuffer = sharedFramebuffer;
    memset(render->framebuffer, 0xff, framebufferBytes);

    printf("[RENDER] Shared framebuffer %dx%d pitch=%d bytes=%u ptr=%p\n",
           width, height, render->pitch,
           static_cast<unsigned int>(framebufferBytes),
           static_cast<void*>(render->framebuffer));

    Render_loadPalettes(render);
    if (render->mediaPalettes == nullptr || render->mediaPalettesLength <= 0) {
        printf("[RENDER] ERROR palettes not initialized\n");
        return 0;
    }

    printf("[RENDER] palettes loaded: entries=%d bytes=%u\n",
           render->mediaPalettesLength,
           static_cast<unsigned int>(render->mediaPalettesLength * sizeof(short)));

    return 1;
}

extern "C" void __wrap_Render_free(Render_t* render, boolean freePtr) {
    if (render != nullptr &&
        render->framebuffer == reinterpret_cast<byte*>(PlatformVideo_framebuffer())) {
        /* Render does not own PlatformVideo's shared framebuffer. */
        render->framebuffer = nullptr;
    }

    __real_Render_free(render, freePtr);
}

extern "C" int DoomRPG_probeRenderStartup(int preRenderReady) {
    if (renderStartupAttempted) {
        return renderStartupReady ? 1 : 0;
    }
    renderStartupAttempted = true;

    printf("\n=== Doom RPG shared Render_startup probe ===\n");

    if (!preRenderReady) {
        printf("[RENDERSTART] Pre-render startup is not ready; probe skipped safely\n");
        return 0;
    }

    if (doomRpg == nullptr || doomRpg->render == nullptr ||
        doomRpg->doomCanvas == nullptr) {
        printf("[RENDERSTART] Core object graph incomplete; probe refused\n");
        return 0;
    }

    if (!preflightRenderResources()) {
        return 0;
    }

    Render_t* const render = doomRpg->render;
    byte* const expectedFramebuffer =
        reinterpret_cast<byte*>(PlatformVideo_framebuffer());
    const uint32_t heapBefore = heap8Free();
    const uint32_t largestBefore = largest8Block();

    printf("[RENDERSTART] Begin: heap8=%u largest8=%u platformFB=%p bytes=%u\n",
           static_cast<unsigned int>(heapBefore),
           static_cast<unsigned int>(largestBefore),
           static_cast<void*>(expectedFramebuffer),
           static_cast<unsigned int>(PlatformVideo_framebufferSizeBytes()));

    const int result = __wrap_Render_startup(render);
    const uint32_t heapAfter = heap8Free();
    const uint32_t largestAfter = largest8Block();
    const uint32_t used = heapBefore >= heapAfter ? heapBefore - heapAfter : 0;

    printf("[RENDERSTART] Render_startup result=%d used=%u heap8=%u largest8=%u\n",
           result,
           static_cast<unsigned int>(used),
           static_cast<unsigned int>(heapAfter),
           static_cast<unsigned int>(largestAfter));

    const int expectedPitch =
        ((sdlVideo.rendererW * static_cast<int>(sizeof(uint16_t))) + 3) & ~3;

    if (!result || render->framebuffer != expectedFramebuffer ||
        render->piDIB != nullptr || render->pitch != expectedPitch ||
        render->clipRect.w != doomRpg->doomCanvas->displayRect.w ||
        render->clipRect.h != doomRpg->doomCanvas->displayRect.h ||
        render->mediaPalettes == nullptr || render->mediaPalettesLength <= 0) {
        printf("[RENDERSTART] FAILED fb=%p expected=%p piDIB=%p pitch=%d/%d clip=%dx%d palettes=%p len=%d\n",
               static_cast<void*>(render->framebuffer),
               static_cast<void*>(expectedFramebuffer),
               static_cast<void*>(render->piDIB),
               render->pitch, expectedPitch,
               render->clipRect.w, render->clipRect.h,
               static_cast<void*>(render->mediaPalettes),
               render->mediaPalettesLength);
        return 0;
    }

    renderStartupReady = true;
    printf("[RENDERSTART] READY shared framebuffer, sintable and palettes initialized\n");
    printf("[RENDERSTART] fb=%p pitch=%d paletteEntries=%d paletteBytes=%u\n",
           static_cast<void*>(render->framebuffer),
           render->pitch,
           render->mediaPalettesLength,
           static_cast<unsigned int>(render->mediaPalettesLength * sizeof(short)));
    printf("[RENDERSTART] Game_loadConfig / mappings / BSP still NOT executed\n");

    return 1;
}
