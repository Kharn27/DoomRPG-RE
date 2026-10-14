/* Retired one-off firmware probes, using real divider/story/free code.
 * Image I/O and intro cleanup are host seams: no PAK/ESP32 heap proof here. */
#include <assert.h>
#include <string.h>
#include <SDL.h>
#include "DoomRPG.h"
#include "DoomCanvas.h"
#include "esp_native_text_format.h"
#include "native_story_fit.h"

/* Host pointers are 64-bit. Production's 32-bit layout assertion stays intact
 * in the real ESP32 build; disable ONLY this TU's assertion in this harness. */
#define _Static_assert(condition, message)
#include "../src/esp_legacy_doomcanvas_bridge.c"
#undef _Static_assert

static unsigned imageLive, imageCreated, imageFreed, introReleases;
static int failImage;
static DoomRPG_t* lastFreedOwner;

Uint32 SDL_GetTicks(void) { return 0U; }
void EspNativeIntroState_release(struct DoomRPG_s* owner) {
    assert(owner != NULL);
    ++introReleases;
}
void DoomRPG_createImage(DoomRPG_t* owner, const char* name,
                        boolean transparent, Image_t* image) {
    assert(owner != NULL && strcmp(name, "p.bmp") == 0 && transparent);
    if (failImage) { image->imgBitmap = NULL; return; }
    image->imgBitmap = calloc(1, 1); /* Opaque mock texture, never dereferenced. */
    assert(image->imgBitmap != NULL);
    image->width = image->height = 16;
    ++imageCreated;
    ++imageLive;
}
void DoomRPG_freeImage(DoomRPG_t* owner, Image_t* image) {
    if (image->imgBitmap == NULL) return;
    assert(owner != NULL && imageLive > 0U);
    lastFreedOwner = owner;
    free(image->imgBitmap);
    image->imgBitmap = NULL;
    ++imageFreed;
    --imageLive;
}

int main(void) {
    static const struct { const char* text; const char* expected; } cases[] = {
        {"Level up!", "\x80\x80 Level up! \x80\x80"},
        {"Near Death!", "\x80 Near Death! \x80"},
        {"Low Health!", "\x80 Low Health! \x80"},
        {"Armor Gone!", "\x80 Armor Gone! \x80"},
    };
    unsigned char guarded[48];
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        memset(guarded, 0x5a, sizeof(guarded));
        char* out = (char*)&guarded[8];
        assert(EspNativeText_buildDivider(out, cases[i].text) == out);
        assert(memcmp(out, cases[i].expected, 16U) == 0);
        assert(out[15] == '\0');
        for (unsigned g = 0; g < 8U; ++g) assert(guarded[g] == 0x5a);
        for (unsigned g = 40; g < sizeof(guarded); ++g) assert(guarded[g] == 0x5a);
    }
    DoomRPG_t ownerA = {0}, ownerB = {0};
    DoomCanvas_t canvasA = {0}, canvasB = {0};
    canvasA.doomRpg = &ownerA;
    canvasB.doomRpg = &ownerB;
    assert(!Esp32StoryFit_hasHand());
    assert(!Esp32StoryFit_prepare(NULL));
    failImage = 1;
    assert(!Esp32StoryFit_prepare(&canvasA) && !Esp32StoryFit_hasHand());
    failImage = 0;
    assert(Esp32StoryFit_prepare(&canvasA) && Esp32StoryFit_hasHand());
    assert(Esp32StoryFit_prepare(&canvasA) && imageCreated == 1U);
    assert(Esp32StoryFit_prepare(&canvasB) && imageCreated == 2U);
    assert(imageLive == 1U && imageFreed == 1U && lastFreedOwner == &ownerA);
    DoomCanvas_free(&canvasB, false);
    assert(!Esp32StoryFit_hasHand() && imageLive == 0U);
    assert(imageFreed == 2U && lastFreedOwner == &ownerB && introReleases == 1U);
    DoomCanvas_free(&canvasB, false);
    DoomCanvas_free(NULL, false);
    assert(imageFreed == 2U && introReleases == 2U);
    puts("startup_regressions_host: PASS divider bytes/guards, story reuse/owner-change/free/idempotence; image I/O mocked");
    return 0;
}
