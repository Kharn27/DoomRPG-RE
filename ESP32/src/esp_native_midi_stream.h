#ifndef ESP_NATIVE_MIDI_STREAM_H
#define ESP_NATIVE_MIDI_STREAM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Streaming SMF-0/1 reader. One 64-byte cache per track, no heap or whole
 * MIDI allocation. The caller owns its PAK lease during begin/next. */
#define ESP_NATIVE_MIDI_MAX_TRACKS 24U
#define ESP_NATIVE_MIDI_CACHE_BYTES 64U

typedef int (*EspNativeMidiRead)(void* context, uint32_t offset,
                                 void* destination, size_t bytes);

enum {
    ESP_NATIVE_MIDI_NOTE_OFF = 1,
    ESP_NATIVE_MIDI_NOTE_ON = 2,
    ESP_NATIVE_MIDI_PROGRAM = 3,
    ESP_NATIVE_MIDI_CONTROL = 4,
    ESP_NATIVE_MIDI_TEMPO = 5
};

typedef struct EspNativeMidiEvent_s {
    uint64_t atUs;
    uint8_t type;
    uint8_t channel;
    uint8_t a;
    uint8_t b;
} EspNativeMidiEvent;

typedef struct EspNativeMidiTrack_s {
    uint32_t position;
    uint32_t end;
    uint32_t tick;
    uint32_t cacheStart;
    uint32_t tempo;
    uint8_t cache[ESP_NATIVE_MIDI_CACHE_BYTES];
    uint8_t cacheLength;
    uint8_t running;
    uint8_t ready;
    uint8_t finished;
    uint8_t type;
    uint8_t channel;
    uint8_t a;
    uint8_t b;
} EspNativeMidiTrack;

typedef struct EspNativeMidiSong_s {
    EspNativeMidiRead read;
    void* context;
    uint64_t elapsedUs;
    uint32_t fileBytes;
    uint32_t lastTick;
    uint32_t tempo;
    uint16_t ppqn;
    uint8_t trackCount;
    uint8_t finished;
    EspNativeMidiTrack tracks[ESP_NATIVE_MIDI_MAX_TRACKS];
} EspNativeMidiSong;

/* No raw ZIP, SD seek, synth, sample allocation or MIDI file-copy here.
 * Reject malformed headers/track lengths/running status/sysex/overflow. */
int EspNativeMidi_begin(EspNativeMidiSong* song, EspNativeMidiRead read,
                         void* context, uint32_t fileBytes);
/* 1: next note/program/control/tempo event, 0: all tracks finished,
 * -1: corrupt or unsupported stream. atUs is absolute from song start. */
int EspNativeMidi_next(EspNativeMidiSong* song, EspNativeMidiEvent* result);
/* Includes end-of-track silence; call only after next returns 0. */
uint64_t EspNativeMidi_durationUs(const EspNativeMidiSong* song);

#ifdef __cplusplus
}
#endif
#endif
