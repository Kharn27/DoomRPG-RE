#include "esp_native_midi_stream.h"

#include <limits.h>
#include <string.h>

static uint16_t be16(const uint8_t* p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int getByte(EspNativeMidiSong* song, EspNativeMidiTrack* t, uint8_t* out) {
    uint32_t available;
    if (t->position >= t->end || t->end > song->fileBytes) return 0;
    if (t->cacheLength == 0U ||
        t->position < t->cacheStart ||
        t->position - t->cacheStart >= t->cacheLength) {
        available = t->end - t->position;
        if (available > ESP_NATIVE_MIDI_CACHE_BYTES)
            available = ESP_NATIVE_MIDI_CACHE_BYTES;
        if (available == 0U ||
            !song->read(song->context, t->position, t->cache, available))
            return 0;
        t->cacheStart = t->position;
        t->cacheLength = (uint8_t)available;
    }
    *out = t->cache[t->position - t->cacheStart];
    ++t->position;
    return 1;
}

static int vlq(EspNativeMidiSong* song, EspNativeMidiTrack* t,
               uint32_t* out) {
    uint32_t v = 0U;
    unsigned i;
    uint8_t b;
    for (i = 0U; i < 4U; ++i) {
        if (!getByte(song, t, &b)) return 0;
        v = (v << 7) | (b & 0x7fU);
        if (!(b & 0x80U)) {
            *out = v;
            return 1;
        }
    }
    return 0;  /* MIDI VLQ cannot have five bytes. */
}

static int skip(EspNativeMidiTrack* t, uint32_t bytes) {
    if (bytes > t->end - t->position) return 0;
    t->position += bytes;
    return 1;
}

static int loadTrack(EspNativeMidiSong* song, EspNativeMidiTrack* t) {
    /* Read until one observable event or EOT. The caller caps work per loop
     * via EspNativeMidi_next(); unsupported meta/channel messages stay bounded
     * by their declared lengths. */
    while (t->position < t->end) {
        uint32_t delta, length;
        uint8_t first, status, a = 0U, b = 0U, type = 0U;
        if (!vlq(song, t, &delta) || UINT32_MAX - t->tick < delta)
            return -1;
        t->tick += delta;
        if (!getByte(song, t, &first)) return -1;
        if (first < 0x80U) {
            if (t->running < 0x80U || t->running >= 0xf0U)
                return -1;
            status = t->running;
            a = first;
        } else {
            status = first;
            if (status < 0xf0U) t->running = status;
            else t->running = 0U;
        }

        if (status == 0xffU) {
            uint8_t meta;
            if (!getByte(song, t, &meta) || !vlq(song, t, &length) ||
                length > t->end - t->position) return -1;
            if (meta == 0x2fU) {
                if (length != 0U) return -1;
                t->finished = 1U;
                return 0;
            }
            if (meta == 0x51U && length == 3U) {
                uint8_t bytes[3];
                if (!getByte(song, t, &bytes[0]) ||
                    !getByte(song, t, &bytes[1]) ||
                    !getByte(song, t, &bytes[2])) return -1;
                t->tempo = ((uint32_t)bytes[0] << 16) |
                           ((uint32_t)bytes[1] << 8) | bytes[2];
                if (t->tempo == 0U) return -1;
                type = ESP_NATIVE_MIDI_TEMPO;
            } else if (!skip(t, length)) {
                return -1;
            }
        } else if (status == 0xf0U || status == 0xf7U) {
            if (!vlq(song, t, &length) || !skip(t, length)) return -1;
        } else if (status >= 0x80U && status <= 0xefU) {
            const uint8_t op = status & 0xf0U;
            if (first >= 0x80U && !getByte(song, t, &a)) return -1;
            if (a >= 0x80U) return -1;
            if (op != 0xc0U && op != 0xd0U) {
                if (!getByte(song, t, &b) || b >= 0x80U) return -1;
            }
            if (op == 0x80U) type = ESP_NATIVE_MIDI_NOTE_OFF;
            else if (op == 0x90U) type = ESP_NATIVE_MIDI_NOTE_ON;
            else if (op == 0xc0U) type = ESP_NATIVE_MIDI_PROGRAM;
            else if (op == 0xb0U) type = ESP_NATIVE_MIDI_CONTROL;
        } else {
            return -1; /* Unsupported system-common stream: fail closed. */
        }
        if (type != 0U) {
            t->type = type;
            t->channel = status & 0x0fU;
            t->a = a;
            t->b = b;
            t->ready = 1U;
            return 1;
        }
    }
    t->finished = 1U;
    return 0;
}

int EspNativeMidi_begin(EspNativeMidiSong* song, EspNativeMidiRead read,
                         void* context, uint32_t fileBytes) {
    uint8_t header[14], chunk[8];
    uint32_t pos;
    uint16_t format, count, division;
    unsigned i;
    if (song == NULL || read == NULL || fileBytes < sizeof(header))
        return 0;
    memset(song, 0, sizeof(*song));
    song->read = read;
    song->context = context;
    song->fileBytes = fileBytes;
    if (!read(context, 0U, header, sizeof(header)) ||
        memcmp(header, "MThd", 4U) != 0 ||
        be32(header + 4U) < 6U) return 0;
    format = be16(header + 8U);
    count = be16(header + 10U);
    division = be16(header + 12U);
    if (format > 1U || count == 0U ||
        count > ESP_NATIVE_MIDI_MAX_TRACKS ||
        (format == 0U && count != 1U) ||
        division == 0U || (division & 0x8000U))
        return 0;
    pos = 8U + be32(header + 4U);
    if (pos > fileBytes) return 0;
    for (i = 0U; i < count; ++i) {
        uint32_t bytes;
        EspNativeMidiTrack* t = &song->tracks[i];
        if (pos > fileBytes || fileBytes - pos < sizeof(chunk) ||
            !read(context, pos, chunk, sizeof(chunk)) ||
            memcmp(chunk, "MTrk", 4U) != 0)
            return 0;
        bytes = be32(chunk + 4U);
        pos += (uint32_t)sizeof(chunk);
        if (bytes > fileBytes - pos) return 0;
        t->position = pos;
        t->end = pos + bytes;
        pos += bytes;
    }
    song->ppqn = division;
    song->trackCount = (uint8_t)count;
    song->tempo = 500000U;
    return 1;
}

int EspNativeMidi_next(EspNativeMidiSong* song, EspNativeMidiEvent* result) {
    int chosen = -1;
    unsigned i;
    uint32_t nextTick = UINT32_MAX, maxTick = 0U;
    if (song == NULL || result == NULL || song->read == NULL)
        return -1;
    if (song->finished) return 0;
    for (i = 0U; i < song->trackCount; ++i) {
        EspNativeMidiTrack* t = &song->tracks[i];
        if (!t->finished && !t->ready) {
            if (loadTrack(song, t) < 0) return -1;
        }
        if (t->ready &&
            (t->tick < nextTick ||
             (t->tick == nextTick &&
              t->type == ESP_NATIVE_MIDI_TEMPO &&
              chosen >= 0 &&
              song->tracks[chosen].type != ESP_NATIVE_MIDI_TEMPO))) {
            nextTick = t->tick;
            chosen = (int)i;
        }
        if (t->tick > maxTick) maxTick = t->tick;
    }
    if (chosen < 0) {
        if (maxTick >= song->lastTick)
            song->elapsedUs += ((uint64_t)(maxTick - song->lastTick) *
                                song->tempo) / song->ppqn;
        song->lastTick = maxTick;
        song->finished = 1U;
        return 0;
    }
    if (nextTick < song->lastTick) return -1;
    song->elapsedUs += ((uint64_t)(nextTick - song->lastTick) *
                        song->tempo) / song->ppqn;
    song->lastTick = nextTick;
    {
        EspNativeMidiTrack* t = &song->tracks[chosen];
        result->atUs = song->elapsedUs;
        result->type = t->type;
        result->channel = t->channel;
        result->a = t->a;
        result->b = t->b;
        if (t->type == ESP_NATIVE_MIDI_TEMPO)
            song->tempo = t->tempo;
        t->ready = 0U;
    }
    return 1;
}

uint64_t EspNativeMidi_durationUs(const EspNativeMidiSong* song) {
    return song != NULL && song->finished ? song->elapsedUs : 0U;
}
