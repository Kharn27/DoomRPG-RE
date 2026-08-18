#include <SDL.h>

#include <Arduino.h>
#include <TFT_eSPI.h>

#include <algorithm>

#include "esp32_sdl_platform.h"
#include "platform_video.h"
#include "platform_video_config.h"

struct SDL_Window {};
struct SDL_Renderer {};
struct SDL_GameController {};
struct SDL_Joystick {};
struct SDL_Haptic {};

struct SDL_Texture {
    int width;
    int height;
    int pitch;
    Uint16* pixels;
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    bool hasColorKey;
    Uint16 colorKey;
};

struct SDL_RWops {
    enum Kind { MEMORY, FILE_STREAM } kind;
    Uint8* memory;
    size_t length;
    size_t position;
    FILE* file;
};

namespace {

const char* lastError = "";
constexpr int kLogicalWidth = DOOMRPG_LOGICAL_WIDTH;
constexpr int kLogicalHeight = DOOMRPG_LOGICAL_HEIGHT;

TFT_eSPI* platformDisplay = nullptr;
Uint16* rendererPixels = nullptr;
Uint8 drawRed = 255;
Uint8 drawGreen = 255;
Uint8 drawBlue = 255;
Uint8 drawAlpha = 255;
int rendererBlendMode = SDL_BLENDMODE_NONE;
bool clipEnabled = false;
SDL_Rect rendererClip{0, 0, kLogicalWidth, kLogicalHeight};

void setError(const char* message) {
    lastError = message;
}

bool ensureRendererPixels() {
    if (rendererPixels != nullptr) return true;

    rendererPixels = PlatformVideo_framebuffer();
    if (rendererPixels == nullptr) {
        setError("platform video framebuffer unavailable");
        Serial.println("[SDL] Platform framebuffer is not initialized");
        return false;
    }

    Serial.printf("[SDL] Sharing platform framebuffer: %u bytes\n",
                  static_cast<unsigned int>(PlatformVideo_framebufferSizeBytes()));
    return true;
}

Uint16 rgb565(Uint8 r, Uint8 g, Uint8 b) {
    return static_cast<Uint16>(((r & 0xf8) << 8) |
                               ((g & 0xfc) << 3) | (b >> 3));
}

bool insideClip(int x, int y) {
    if (x < 0 || y < 0 || x >= kLogicalWidth || y >= kLogicalHeight) return false;
    if (!clipEnabled) return true;
    return x >= rendererClip.x && y >= rendererClip.y &&
           x < rendererClip.x + rendererClip.w &&
           y < rendererClip.y + rendererClip.h;
}

Uint16 blend565(Uint16 destination, Uint16 source, Uint8 alpha) {
    if (alpha == 255) return source;
    if (alpha == 0) return destination;
    const int inverse = 255 - alpha;
    const int sr = ((source >> 11) & 0x1f) * 255 / 31;
    const int sg = ((source >> 5) & 0x3f) * 255 / 63;
    const int sb = (source & 0x1f) * 255 / 31;
    const int dr = ((destination >> 11) & 0x1f) * 255 / 31;
    const int dg = ((destination >> 5) & 0x3f) * 255 / 63;
    const int db = (destination & 0x1f) * 255 / 31;
    return rgb565((sr * alpha + dr * inverse) / 255,
                  (sg * alpha + dg * inverse) / 255,
                  (sb * alpha + db * inverse) / 255);
}

void putPixel(int x, int y, Uint16 color) {
    if (!insideClip(x, y) || !ensureRendererPixels()) return;
    Uint16& destination = rendererPixels[y * kLogicalWidth + x];
    destination = rendererBlendMode == SDL_BLENDMODE_BLEND
                      ? blend565(destination, color, drawAlpha)
                      : color;
}

Uint16 modulate565(Uint16 color, const SDL_Texture* texture) {
    const int red = ((color >> 11) & 0x1f) * texture->red / 255;
    const int green = ((color >> 5) & 0x3f) * texture->green / 255;
    const int blue = (color & 0x1f) * texture->blue / 255;
    return static_cast<Uint16>((red << 11) | (green << 5) | blue);
}

bool rwSeek(SDL_RWops* source, size_t position) {
    if (source == nullptr) {
        return false;
    }
    if (source->kind == SDL_RWops::MEMORY) {
        if (position > source->length) {
            return false;
        }
        source->position = position;
        return true;
    }
    return fseek(source->file, static_cast<long>(position), SEEK_SET) == 0;
}

Uint16 readLe16(const Uint8* bytes) {
    return static_cast<Uint16>(bytes[0] | (bytes[1] << 8));
}

Uint32 readLe32(const Uint8* bytes) {
    return static_cast<Uint32>(bytes[0]) |
           (static_cast<Uint32>(bytes[1]) << 8) |
           (static_cast<Uint32>(bytes[2]) << 16) |
           (static_cast<Uint32>(bytes[3]) << 24);
}

void freeSurfaceParts(SDL_Surface* surface) {
    if (surface == nullptr) {
        return;
    }
    if (surface->format != nullptr) {
        if (surface->format->palette != nullptr) {
            free(surface->format->palette->colors);
            free(surface->format->palette);
        }
        free(surface->format);
    }
    free(surface->pixels);
}

}  // namespace

extern "C" {

Uint32 SDL_GetTicks(void) {
    return millis();
}

Uint16 SDL_SwapLE16(Uint16 value) {
    return value;
}

Uint32 SDL_SwapLE32(Uint32 value) {
    return value;
}

const char* SDL_GetError(void) {
    return lastError;
}

const char* SDL_GetScancodeName(int) {
    return "CYD input";
}

int SDL_SetMemoryFunctions(SDL_malloc_func, SDL_calloc_func,
                           SDL_realloc_func, SDL_free_func) {
    return 0;
}

void SDL_GetMemoryFunctions(SDL_malloc_func* mallocFn, SDL_calloc_func* callocFn,
                            SDL_realloc_func* reallocFn, SDL_free_func* freeFn) {
    if (mallocFn) *mallocFn = malloc;
    if (callocFn) *callocFn = calloc;
    if (reallocFn) *reallocFn = realloc;
    if (freeFn) *freeFn = free;
}

SDL_RWops* SDL_RWFromMem(void* memory, int size) {
    if (memory == nullptr || size < 0) {
        setError("invalid memory stream");
        return nullptr;
    }
    SDL_RWops* stream = static_cast<SDL_RWops*>(calloc(1, sizeof(SDL_RWops)));
    if (stream == nullptr) {
        setError("out of memory creating stream");
        return nullptr;
    }
    stream->kind = SDL_RWops::MEMORY;
    stream->memory = static_cast<Uint8*>(memory);
    stream->length = static_cast<size_t>(size);
    return stream;
}

SDL_RWops* SDL_RWFromFile(const char* path, const char* mode) {
    FILE* file = fopen(path, mode);
    if (file == nullptr) {
        setError("unable to open file");
        return nullptr;
    }
    SDL_RWops* stream = static_cast<SDL_RWops*>(calloc(1, sizeof(SDL_RWops)));
    if (stream == nullptr) {
        fclose(file);
        setError("out of memory creating file stream");
        return nullptr;
    }
    stream->kind = SDL_RWops::FILE_STREAM;
    stream->file = file;
    return stream;
}

Sint32 SDL_RWsize(SDL_RWops* context) {
    if (context == nullptr) {
        return -1;
    }
    if (context->kind == SDL_RWops::MEMORY) {
        return static_cast<Sint32>(context->length);
    }
    const long current = ftell(context->file);
    fseek(context->file, 0, SEEK_END);
    const long size = ftell(context->file);
    fseek(context->file, current, SEEK_SET);
    return static_cast<Sint32>(size);
}

Sint64 SDL_RWseek(SDL_RWops* context, Sint64 offset, int whence) {
    if (context == nullptr) return -1;
    if (context->kind == SDL_RWops::FILE_STREAM) {
        if (fseek(context->file, static_cast<long>(offset), whence) != 0) return -1;
        return static_cast<Sint64>(ftell(context->file));
    }

    Sint64 base = 0;
    if (whence == SEEK_CUR) base = static_cast<Sint64>(context->position);
    else if (whence == SEEK_END) base = static_cast<Sint64>(context->length);
    else if (whence != SEEK_SET) return -1;

    const Sint64 next = base + offset;
    if (next < 0 || static_cast<size_t>(next) > context->length) return -1;
    context->position = static_cast<size_t>(next);
    return next;
}

size_t SDL_RWread(SDL_RWops* context, void* ptr, size_t size, size_t count) {
    if (context == nullptr || ptr == nullptr || size == 0) {
        return 0;
    }
    if (context->kind == SDL_RWops::FILE_STREAM) {
        return fread(ptr, size, count, context->file);
    }
    const size_t requested = size * count;
    const size_t available = context->length - context->position;
    const size_t copied = std::min(requested, available);
    memcpy(ptr, context->memory + context->position, copied);
    context->position += copied;
    return copied / size;
}

size_t SDL_RWwrite(SDL_RWops* context, const void* ptr, size_t size, size_t count) {
    if (context == nullptr || ptr == nullptr || size == 0) {
        return 0;
    }
    if (context->kind == SDL_RWops::FILE_STREAM) {
        return fwrite(ptr, size, count, context->file);
    }
    const size_t requested = size * count;
    const size_t available = context->length - context->position;
    const size_t copied = std::min(requested, available);
    memcpy(context->memory + context->position, ptr, copied);
    context->position += copied;
    return copied / size;
}

int SDL_RWclose(SDL_RWops* context) {
    if (context == nullptr) {
        return 0;
    }
    int result = 0;
    if (context->kind == SDL_RWops::FILE_STREAM && context->file != nullptr) {
        result = fclose(context->file);
    }
    free(context);
    return result;
}

SDL_Surface* SDL_LoadBMP(const char* path) {
    SDL_RWops* stream = SDL_RWFromFile(path, "rb");
    return stream == nullptr ? nullptr : SDL_LoadBMP_RW(stream, SDL_TRUE);
}

SDL_Surface* SDL_LoadBMP_RW(SDL_RWops* source, int freeSource) {
    Uint8 header[54];
    if (source == nullptr || SDL_RWread(source, header, sizeof(header), 1) != 1 ||
        header[0] != 'B' || header[1] != 'M') {
        if (freeSource) SDL_RWclose(source);
        setError("unsupported BMP header");
        return nullptr;
    }

    const Uint32 pixelOffset = readLe32(header + 10);
    const Uint32 dibSize = readLe32(header + 14);
    const Sint32 width = static_cast<Sint32>(readLe32(header + 18));
    const Sint32 signedHeight = static_cast<Sint32>(readLe32(header + 22));
    const Uint16 bitsPerPixel = readLe16(header + 28);
    const Uint32 compression = readLe32(header + 30);
    Uint32 colorCount = readLe32(header + 46);

    if (dibSize < 40 || width <= 0 || signedHeight == 0 ||
        bitsPerPixel != 8 || compression != 0) {
        if (freeSource) SDL_RWclose(source);
        setError("only uncompressed 8-bit BMP is supported");
        return nullptr;
    }
    if (colorCount == 0) colorCount = 256;

    SDL_Surface* surface = static_cast<SDL_Surface*>(calloc(1, sizeof(SDL_Surface)));
    if (surface == nullptr) {
        if (freeSource) SDL_RWclose(source);
        setError("out of memory creating BMP surface");
        return nullptr;
    }
    surface->format = static_cast<SDL_PixelFormat*>(calloc(1, sizeof(SDL_PixelFormat)));
    surface->format->palette = static_cast<SDL_Palette*>(calloc(1, sizeof(SDL_Palette)));
    surface->format->palette->colors = static_cast<SDL_Color*>(calloc(colorCount, sizeof(SDL_Color)));
    surface->pixels = calloc(static_cast<size_t>(width), static_cast<size_t>(abs(signedHeight)));
    if (surface->format == nullptr || surface->format->palette == nullptr ||
        surface->format->palette->colors == nullptr || surface->pixels == nullptr) {
        freeSurfaceParts(surface);
        free(surface);
        if (freeSource) SDL_RWclose(source);
        setError("out of memory loading BMP");
        return nullptr;
    }

    surface->w = width;
    surface->h = abs(signedHeight);
    surface->pitch = width;
    surface->format->BitsPerPixel = 8;
    surface->format->BytesPerPixel = 1;
    surface->format->palette->ncolors = static_cast<int>(colorCount);

    if (!rwSeek(source, 14 + dibSize)) {
        SDL_FreeSurface(surface);
        if (freeSource) SDL_RWclose(source);
        setError("invalid BMP palette offset");
        return nullptr;
    }
    for (Uint32 index = 0; index < colorCount; ++index) {
        Uint8 bgra[4];
        if (SDL_RWread(source, bgra, sizeof(bgra), 1) != 1) {
            SDL_FreeSurface(surface);
            if (freeSource) SDL_RWclose(source);
            setError("truncated BMP palette");
            return nullptr;
        }
        SDL_Color& color = surface->format->palette->colors[index];
        color.r = bgra[2];
        color.g = bgra[1];
        color.b = bgra[0];
        color.a = 255;
    }

    const size_t filePitch = (static_cast<size_t>(width) + 3U) & ~3U;
    Uint8* row = static_cast<Uint8*>(malloc(filePitch));
    if (row == nullptr || !rwSeek(source, pixelOffset)) {
        free(row);
        SDL_FreeSurface(surface);
        if (freeSource) SDL_RWclose(source);
        setError("invalid BMP pixel data");
        return nullptr;
    }
    for (int fileY = 0; fileY < surface->h; ++fileY) {
        if (SDL_RWread(source, row, filePitch, 1) != 1) {
            free(row);
            SDL_FreeSurface(surface);
            if (freeSource) SDL_RWclose(source);
            setError("truncated BMP pixel data");
            return nullptr;
        }
        const int destinationY = signedHeight > 0 ? surface->h - 1 - fileY : fileY;
        memcpy(static_cast<Uint8*>(surface->pixels) + destinationY * surface->pitch,
               row, static_cast<size_t>(width));
    }
    free(row);
    if (freeSource) SDL_RWclose(source);
    return surface;
}

void SDL_FreeSurface(SDL_Surface* surface) {
    if (surface == nullptr) return;
    freeSurfaceParts(surface);
    free(surface);
}

Uint32 SDL_MapRGB(const SDL_PixelFormat*, Uint8 r, Uint8 g, Uint8 b) {
    return (static_cast<Uint32>(r) << 16) |
           (static_cast<Uint32>(g) << 8) | b;
}

int SDL_SetColorKey(SDL_Surface* surface, int flag, Uint32 key) {
    if (surface == nullptr) return -1;
    surface->hasColorKey = flag;
    surface->colorKey = key;
    return 0;
}

SDL_Texture* SDL_CreateTexture(SDL_Renderer*, Uint32 format, int, int width, int height) {
    if (format != SDL_PIXELFORMAT_RGB565 || width <= 0 || height <= 0) {
        setError("unsupported texture format");
        return nullptr;
    }
    SDL_Texture* texture = static_cast<SDL_Texture*>(calloc(1, sizeof(SDL_Texture)));
    if (texture == nullptr) return nullptr;
    texture->pixels = static_cast<Uint16*>(calloc(static_cast<size_t>(width) * height, sizeof(Uint16)));
    if (texture->pixels == nullptr) {
        free(texture);
        return nullptr;
    }
    texture->width = width;
    texture->height = height;
    texture->pitch = width * 2;
    texture->red = texture->green = texture->blue = 255;
    return texture;
}

SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer* renderer, SDL_Surface* surface) {
    if (surface == nullptr || surface->format == nullptr || surface->format->palette == nullptr) {
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             surface->w, surface->h);
    if (texture == nullptr) return nullptr;
    texture->hasColorKey = surface->hasColorKey;
    const Uint8* source = static_cast<const Uint8*>(surface->pixels);
    for (int y = 0; y < surface->h; ++y) {
        for (int x = 0; x < surface->w; ++x) {
            const SDL_Color& color = surface->format->palette->colors[source[y * surface->pitch + x]];
            texture->pixels[y * surface->w + x] = rgb565(color.r, color.g, color.b);
        }
    }
    if (texture->hasColorKey) {
        texture->colorKey = rgb565((surface->colorKey >> 16) & 0xff,
                                   (surface->colorKey >> 8) & 0xff,
                                   surface->colorKey & 0xff);
    }
    return texture;
}

void SDL_DestroyTexture(SDL_Texture* texture) {
    if (texture == nullptr) return;
    free(texture->pixels);
    free(texture);
}

int SDL_UpdateTexture(SDL_Texture* texture, const SDL_Rect* rect,
                      const void* pixels, int pitch) {
    if (texture == nullptr || pixels == nullptr) return -1;
    SDL_Rect area = rect == nullptr ? SDL_Rect{0, 0, texture->width, texture->height} : *rect;
    for (int y = 0; y < area.h; ++y) {
        memcpy(texture->pixels + (area.y + y) * texture->width + area.x,
               static_cast<const Uint8*>(pixels) + y * pitch,
               static_cast<size_t>(area.w) * sizeof(Uint16));
    }
    return 0;
}

int SDL_SetTextureColorMod(SDL_Texture* texture, Uint8 r, Uint8 g, Uint8 b) {
    if (texture == nullptr) return -1;
    texture->red = r;
    texture->green = g;
    texture->blue = b;
    return 0;
}

int SDL_RenderCopy(SDL_Renderer*, SDL_Texture* texture, const SDL_Rect* source,
                   const SDL_Rect* destination) {
    if (texture == nullptr || !ensureRendererPixels()) return -1;
    const SDL_Rect src = source == nullptr
        ? SDL_Rect{0, 0, texture->width, texture->height} : *source;
    const SDL_Rect dst = destination == nullptr
        ? SDL_Rect{0, 0, src.w, src.h} : *destination;
    if (src.w <= 0 || src.h <= 0 || dst.w <= 0 || dst.h <= 0) return -1;

    const int startX = std::max(0, dst.x);
    const int startY = std::max(0, dst.y);
    const int endX = std::min(kLogicalWidth, dst.x + dst.w);
    const int endY = std::min(kLogicalHeight, dst.y + dst.h);
    for (int y = startY; y < endY; ++y) {
        if (clipEnabled && (y < rendererClip.y || y >= rendererClip.y + rendererClip.h)) continue;
        const int sourceY = src.y + ((y - dst.y) * src.h) / dst.h;
        if (sourceY < 0 || sourceY >= texture->height) continue;
        for (int x = startX; x < endX; ++x) {
            if (!insideClip(x, y)) continue;
            const int sourceX = src.x + ((x - dst.x) * src.w) / dst.w;
            if (sourceX < 0 || sourceX >= texture->width) continue;
            const Uint16 color = texture->pixels[sourceY * texture->width + sourceX];
            if (texture->hasColorKey && color == texture->colorKey) continue;
            rendererPixels[y * kLogicalWidth + x] = modulate565(color, texture);
        }
    }
    return 0;
}

int SDL_RenderSetClipRect(SDL_Renderer*, const SDL_Rect* rect) {
    clipEnabled = rect != nullptr;
    rendererClip = rect == nullptr ? SDL_Rect{0, 0, kLogicalWidth, kLogicalHeight} : *rect;
    return 0;
}

int SDL_SetRenderDrawColor(SDL_Renderer*, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    drawRed = r;
    drawGreen = g;
    drawBlue = b;
    drawAlpha = a;
    return 0;
}

int SDL_SetRenderDrawBlendMode(SDL_Renderer*, int blendMode) {
    rendererBlendMode = blendMode;
    return 0;
}

int SDL_RenderClear(SDL_Renderer*) {
    if (!ensureRendererPixels()) return -1;
    std::fill(rendererPixels, rendererPixels + kLogicalWidth * kLogicalHeight,
              rgb565(drawRed, drawGreen, drawBlue));
    return 0;
}

void SDL_RenderPresent(SDL_Renderer*) {
    if (platformDisplay == nullptr || !ensureRendererPixels()) return;
    if (!PlatformVideo_present()) {
        setError("platform video present failed");
        Serial.println("[SDL] Platform video present failed");
    }
}

int SDL_RenderDrawRect(SDL_Renderer* renderer, const SDL_Rect* rect) {
    if (rect == nullptr) return -1;
    SDL_RenderDrawLine(renderer, rect->x, rect->y, rect->x + rect->w - 1, rect->y);
    SDL_RenderDrawLine(renderer, rect->x, rect->y + rect->h - 1,
                       rect->x + rect->w - 1, rect->y + rect->h - 1);
    SDL_RenderDrawLine(renderer, rect->x, rect->y, rect->x, rect->y + rect->h - 1);
    SDL_RenderDrawLine(renderer, rect->x + rect->w - 1, rect->y,
                       rect->x + rect->w - 1, rect->y + rect->h - 1);
    return 0;
}

int SDL_RenderFillRect(SDL_Renderer*, const SDL_Rect* rect) {
    const SDL_Rect area = rect == nullptr
        ? SDL_Rect{0, 0, kLogicalWidth, kLogicalHeight} : *rect;
    const Uint16 color = rgb565(drawRed, drawGreen, drawBlue);
    for (int y = area.y; y < area.y + area.h; ++y) {
        for (int x = area.x; x < area.x + area.w; ++x) putPixel(x, y, color);
    }
    return 0;
}

int SDL_RenderDrawLine(SDL_Renderer*, int x1, int y1, int x2, int y2) {
    const Uint16 color = rgb565(drawRed, drawGreen, drawBlue);
    const int dx = abs(x2 - x1);
    const int sx = x1 < x2 ? 1 : -1;
    const int dy = -abs(y2 - y1);
    const int sy = y1 < y2 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        putPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        const int doubled = error * 2;
        if (doubled >= dy) { error += dy; x1 += sx; }
        if (doubled <= dx) { error += dx; y1 += sy; }
    }
    return 0;
}

void SDL_RenderDrawCircle(SDL_Renderer*, int cx, int cy, int radius) {
    const Uint16 color = rgb565(drawRed, drawGreen, drawBlue);
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    while (x >= y) {
        putPixel(cx + x, cy + y, color); putPixel(cx + y, cy + x, color);
        putPixel(cx - y, cy + x, color); putPixel(cx - x, cy + y, color);
        putPixel(cx - x, cy - y, color); putPixel(cx - y, cy - x, color);
        putPixel(cx + y, cy - x, color); putPixel(cx + x, cy - y, color);
        ++y;
        if (error < 0) error += 2 * y + 1;
        else { --x; error += 2 * (y - x + 1); }
    }
}

void SDL_RenderDrawFillCircle(SDL_Renderer* renderer, int cx, int cy, int radius) {
    for (int y = -radius; y <= radius; ++y) {
        const int halfWidth = static_cast<int>(sqrtf(radius * radius - y * y));
        SDL_RenderDrawLine(renderer, cx - halfWidth, cy + y, cx + halfWidth, cy + y);
    }
}
int SDL_RenderSetIntegerScale(SDL_Renderer*, SDL_bool) { return 0; }

int SDL_ShowMessageBox(const SDL_MessageBoxData* data, int*) {
    if (data != nullptr) Serial.printf("[DOOM ERROR] %s: %s\n", data->title, data->message);
    return 0;
}

int SDL_ShowSimpleMessageBox(Uint32, const char* title, const char* message, SDL_Window*) {
    Serial.printf("[DOOM ERROR] %s: %s\n", title, message);
    return 0;
}

int SDL_SetHint(const char*, const char*) { return SDL_TRUE; }
int SDL_ShowCursor(int toggle) { return toggle; }
int SDL_SetWindowFullscreen(SDL_Window*, Uint32) { return 0; }
int SDL_NumJoysticks(void) { return 0; }
SDL_bool SDL_IsGameController(int) { return SDL_FALSE; }
int SDL_GameControllerRumble(SDL_GameController*, Uint16, Uint16, Uint32) { return 0; }
int SDL_HapticRumblePlay(SDL_Haptic*, float, Uint32) { return 0; }
int SDL_HapticRumbleStop(SDL_Haptic*) { return 0; }

}  // extern "C"

void Esp32Sdl_attachDisplay(TFT_eSPI* display) {
    platformDisplay = display;
    Serial.println("[SDL] CYD display attached; renderer will share platform video");
}

void Esp32Sdl_showTestPattern() {
    SDL_SetRenderDrawBlendMode(nullptr, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(nullptr, 0, 0, 0, 255);
    SDL_RenderClear(nullptr);

    const Uint8 colors[4][3] = {
        {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255},
    };
    for (int index = 0; index < 4; ++index) {
        SDL_SetRenderDrawColor(nullptr, colors[index][0], colors[index][1], colors[index][2], 255);
        SDL_Rect bar{4 + index * 39, 4, 35, 18};
        SDL_RenderFillRect(nullptr, &bar);
    }

    SDL_SetRenderDrawColor(nullptr, 255, 255, 255, 255);
    SDL_RenderDrawLine(nullptr, 0, 0, kLogicalWidth - 1, kLogicalHeight - 1);
    SDL_RenderDrawLine(nullptr, kLogicalWidth - 1, 0, 0, kLogicalHeight - 1);
    SDL_RenderDrawCircle(nullptr, 40, 72, 22);
    SDL_SetRenderDrawColor(nullptr, 255, 0, 255, 255);
    SDL_RenderDrawFillCircle(nullptr, 80, 72, 18);

    Uint16 checker[16 * 16];
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            checker[y * 16 + x] = ((x / 4 + y / 4) & 1)
                ? rgb565(255, 255, 255) : rgb565(24, 24, 24);
        }
    }
    SDL_Texture* texture = SDL_CreateTexture(nullptr, SDL_PIXELFORMAT_RGB565,
                                             SDL_TEXTUREACCESS_STREAMING, 16, 16);
    SDL_UpdateTexture(texture, nullptr, checker, 16 * sizeof(Uint16));
    SDL_Rect target{108, 48, 44, 44};
    SDL_RenderCopy(nullptr, texture, nullptr, &target);
    SDL_DestroyTexture(texture);

    SDL_SetRenderDrawBlendMode(nullptr, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(nullptr, 255, 255, 255, 96);
    SDL_Rect translucent{12, 104, 136, 16};
    SDL_RenderFillRect(nullptr, &translucent);
    SDL_SetRenderDrawBlendMode(nullptr, SDL_BLENDMODE_NONE);
    SDL_RenderPresent(nullptr);
}
