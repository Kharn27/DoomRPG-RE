#include "esp_bt_test_midi.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "esp_asset_pack.h"

#if defined(DOOMRPG_ESP32_BT_TEST_PROFILE) && DOOMRPG_ESP32_BT_TEST_PROFILE
namespace {
constexpr uint32_t RATE = 44100U;
constexpr uint32_t WINDOW = 256U;
constexpr uint8_t MAX_TRACKS = 24U;
constexpr uint16_t QSIZE = 128U; /* ring leaves one empty */
constexpr uint8_t VOICES = 12U;
struct MidiCmd {
    uint32_t sample;
    uint8_t type; /* 1=off, 2=on */
    uint8_t channel;
    uint8_t note;
    uint8_t vel;
};
struct MidiTrack {
    uint32_t pos, end, tick;
    uint8_t running, kind, channel, note, velocity;
    uint32_t extra;
    bool valid, ended;
};
struct Voice {
    uint32_t phase, increment;
    uint8_t channel, note, velocity;
    bool active;
};
static EspAssetPackEntry entry = {};
static MidiTrack tracks[MAX_TRACKS] = {};
static uint8_t trackCount = 0;
static uint16_t division = 0;
static uint32_t tempoUs = 500000U, tickAt = 0;
static uint64_t samplesAt = 0;
static uint8_t window[WINDOW] = {};
static uint32_t windowStart = UINT32_MAX, windowSize = 0;
static MidiCmd queue[QSIZE] = {};
static volatile uint16_t rd = 0, wr = 0;
static volatile uint32_t playbackSamples = 0;
static Voice voices[VOICES] = {};
static uint32_t eventCount = 0, callbackCount = 0, underruns = 0, pcmBytes = 0;
static uint8_t nextVoice = 0;
static bool ready = false, finished = false, ioFailed = false;

static bool at(uint32_t offset, uint8_t *value) {
    if (offset >= entry.size || !value || ioFailed) return false;
    if (windowStart == UINT32_MAX || offset < windowStart ||
        offset - windowStart >= windowSize) {
        windowStart = offset & ~(WINDOW - 1U);
        windowSize = entry.size - windowStart;
        if (windowSize > WINDOW) windowSize = WINDOW;
        if (!EspAssetPack_readRange(&entry, windowStart, window, windowSize)) {
            ioFailed = true;
            return false;
        }
    }
    *value = window[offset - windowStart];
    return true;
}
static bool bytes(uint32_t pos, uint8_t *dest, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        if (!at(pos + i, dest + i)) return false;
    return true;
}
static uint16_t be16(const uint8_t *b) {
    return (uint16_t)((uint16_t)b[0] << 8 | b[1]);
}
static uint32_t be32(const uint8_t *b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}
static bool take(MidiTrack &t, uint8_t *v) {
    if (t.pos >= t.end) return false;
    return at(t.pos++, v);
}
static bool vlq(MidiTrack &t, uint32_t *out) {
    uint32_t x = 0;
    for (int n = 0; n < 4; ++n) {
        uint8_t b;
        if (!take(t, &b)) return false;
        x = (x << 7) | (b & 127U);
        if (!(b & 128U)) { *out = x; return true; }
    }
    return false;
}
static bool prime(MidiTrack &t) {
    if (t.valid || t.ended) return !t.ended;
    /* Skip unrendered controller/program/pitch events without storing them. */
    for (unsigned limit = 0; limit < 16384U; ++limit) {
        if (t.pos >= t.end) { t.ended = true; return false; }
        uint32_t delta = 0;
        if (!vlq(t, &delta) || delta > UINT32_MAX - t.tick) break;
        t.tick += delta;
        uint8_t first;
        if (!take(t, &first)) break;
        uint8_t status = first;
        bool running = first < 0x80U;
        if (running) {
            status = t.running;
            if (status < 0x80U || status >= 0xF0U) break;
        } else if (status < 0xF0U) {
            t.running = status;
        } else {
            t.running = 0;
        }
        if (status == 0xFFU) {
            uint8_t type;
            uint32_t size = 0;
            if (!take(t, &type) || !vlq(t, &size) || size > t.end - t.pos) break;
            if (type == 0x2FU) { t.ended = true; return false; }
            if (type == 0x51U && size == 3U) {
                uint8_t b[3];
                if (!take(t, &b[0]) || !take(t, &b[1]) || !take(t, &b[2])) break;
                t.extra = ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2];
                t.kind = 3;
                t.valid = true;
                return true;
            }
            t.pos += size;
            continue;
        }
        if (status == 0xF0U || status == 0xF7U) {
            uint32_t size = 0;
            if (!vlq(t, &size) || size > t.end - t.pos) break;
            t.pos += size;
            continue;
        }
        if (status < 0x80U || status >= 0xF0U) break;
        const uint8_t operation = status & 0xF0U;
        uint8_t d1 = first, d2 = 0;
        if (!running && !take(t, &d1)) break;
        if (operation != 0xC0U && operation != 0xD0U && !take(t, &d2)) break;
        if (operation == 0x90U || operation == 0x80U) {
            t.kind = (operation == 0x90U && d2 != 0U) ? 2U : 1U;
            t.channel = status & 0x0FU;
            t.note = d1;
            t.velocity = d2;
            t.valid = true;
            return true;
        }
    }
    t.ended = true;
    ioFailed = true;
    return false;
}
static void note(const MidiCmd &c) {
    if (c.type == 1U) {
        for (unsigned i = 0; i < VOICES; ++i)
            if (voices[i].active && voices[i].channel == c.channel &&
                voices[i].note == c.note) voices[i].active = false;
        return;
    }
    if (c.type != 2U) return;
    unsigned slot = VOICES;
    for (unsigned i = 0; i < VOICES; ++i)
        if (!voices[i].active) { slot = i; break; }
    if (slot == VOICES) { slot = nextVoice; nextVoice = (nextVoice + 1U) % VOICES; }
    Voice &v = voices[slot];
    v.active = true;
    v.phase = 0;
    v.channel = c.channel;
    v.note = c.note;
    v.velocity = c.vel;
    const double hz = 440.0 * pow(2.0, ((int)c.note - 69) / 12.0);
    const double step = hz * 4294967296.0 / RATE;
    v.increment = (uint32_t)(step < 1.0 ? 1.0 : step > 4294967295.0 ? 4294967295.0 : step);
}
} // namespace

int EspBtTestMidi_begin(void) {
    static const char* songs[] = {"5039.mid", "5040.mid", "5043.mid"};
    if (!EspAssetPack_open(ESP_ASSET_PACK_DEFAULT_PATH)) {
        Serial.println("[BTTEST MIDI] FAIL cannot-open-native-PAK");
        return 0;
    }
    const char *song = nullptr;
    for (const char* candidate : songs)
        if (EspAssetPack_findEntry(candidate, &entry)) { song = candidate; break; }
    if (!song) {
        Serial.println("[BTTEST MIDI] FAIL no-5039-5040-5043.mid-in-PAK");
        EspAssetPack_close();
        return 0;
    }
    uint8_t h[14];
    if (entry.size < 14U || !bytes(0U, h, sizeof h) ||
        memcmp(h, "MThd", 4) != 0 || be32(h + 4) < 6U ||
        be32(h + 4) > entry.size - 8U || be16(h + 8) > 1U ||
        be16(h + 10) == 0U || be16(h + 10) > MAX_TRACKS ||
        be16(h + 12) == 0U || (h[12] & 0x80U)) {
        Serial.printf("[BTTEST MIDI] FAIL bad-header song=%s bytes=%lu\n",
                      song, (unsigned long)entry.size);
        EspAssetPack_close();
        return 0;
    }
    trackCount = (uint8_t)be16(h + 10);
    division = be16(h + 12);
    uint32_t offset = 8U + be32(h + 4);
    for (uint8_t i = 0; i < trackCount; ++i) {
        uint8_t th[8];
        if (offset > entry.size || entry.size - offset < sizeof th ||
            !bytes(offset, th, sizeof th) || memcmp(th, "MTrk", 4) != 0 ||
            be32(th + 4) > entry.size - offset - 8U) {
            Serial.printf("[BTTEST MIDI] FAIL track=%u song=%s\n", i, song);
            EspAssetPack_close();
            return 0;
        }
        tracks[i].pos = offset + 8U;
        tracks[i].end = tracks[i].pos + be32(th + 4);
        offset = tracks[i].end;
    }
    ready = true;
    Serial.printf("[BTTEST MIDI] READY song=%s size=%lu tracks=%u ppq=%u source=native-PAK renderer=12voice-triangle-PCM44100-stereo16 noFluidSynth\n",
                  song, (unsigned long)entry.size, trackCount, division);
    EspBtTestMidi_pump();
    return 1;
}

void EspBtTestMidi_pump(void) {
    if (!ready || finished || ioFailed) return;
    /* Producer on Arduino loop only. Callback only consumes this tiny SPSC
     * FIFO. Never access SD/PAK from the Bluedroid PCM worker thread. */
    for (unsigned budget = 0; budget < 64U; ++budget) {
        const uint16_t next = (uint16_t)((wr + 1U) % QSIZE);
        if (next == rd) break;
        if (samplesAt > (uint64_t)playbackSamples + RATE * 2U) break;
        uint8_t best = MAX_TRACKS;
        for (uint8_t i = 0; i < trackCount; ++i) {
            prime(tracks[i]);
            if (!tracks[i].valid) continue;
            if (best == MAX_TRACKS || tracks[i].tick < tracks[best].tick)
                best = i;
        }
        if (ioFailed || best == MAX_TRACKS) {
            if (ioFailed) Serial.println("[BTTEST MIDI] FAIL malformed-track-read");
            else { finished = true; Serial.printf("[BTTEST MIDI] END events=%lu\n", (unsigned long)eventCount); }
            break;
        }
        MidiTrack &t = tracks[best];
        if (t.tick < tickAt) { ioFailed = true; break; }
        const uint64_t dt = (uint64_t)(t.tick - tickAt) * tempoUs * RATE;
        samplesAt += dt / ((uint64_t)division * 1000000ULL);
        tickAt = t.tick;
        if (t.kind == 3U) {
            if (t.extra > 0U) tempoUs = t.extra;
        } else {
            MidiCmd c = {(uint32_t)samplesAt, t.kind, t.channel, t.note, t.velocity};
            queue[wr] = c;
            __sync_synchronize();
            wr = next;
            ++eventCount;
        }
        t.valid = false;
    }
}

int32_t EspBtTestMidi_fill(uint8_t *pcm, int32_t len) {
    if (len <= 0 || !pcm) return 0;
    memset(pcm, 0, (size_t)len);
    if (!ready) return len;
    ++callbackCount;
    for (int32_t i = 0; i + 3 < len; i += 4) {
        while (rd != wr) {
            __sync_synchronize();
            const MidiCmd c = queue[rd];
            if (c.sample > playbackSamples) break;
            note(c);
            rd = (uint16_t)((rd + 1U) % QSIZE);
        }
        int32_t value = 0;
        for (unsigned v = 0; v < VOICES; ++v) {
            Voice &voice = voices[v];
            if (!voice.active) continue;
            voice.phase += voice.increment;
            uint32_t wave = voice.phase >> 16;
            if (wave > 32767U) wave = 65535U - wave;
            value += ((int32_t)wave - 16384) * (int32_t)voice.velocity / 2048;
        }
        if (value < -28000) value = -28000;
        if (value > 28000) value = 28000;
        const int16_t sample = (int16_t)value;
        pcm[i] = (uint8_t)sample;
        pcm[i + 1] = (uint8_t)((uint16_t)sample >> 8);
        pcm[i + 2] = pcm[i];
        pcm[i + 3] = pcm[i + 1];
        ++playbackSamples;
        if (rd == wr && !finished) ++underruns;
    }
    pcmBytes += (uint32_t)len;
    return len;
}
int EspBtTestMidi_isReady(void) { return ready && !ioFailed; }
void EspBtTestMidi_report(void) {
    Serial.printf("[BTTEST MIDI] PCM callbacks=%lu bytes=%lu elapsedMs=%lu events=%lu pending=%u underrunFrames=%lu done=%u error=%u\n",
                  (unsigned long)callbackCount, (unsigned long)pcmBytes,
                  (unsigned long)((uint64_t)playbackSamples * 1000ULL / RATE),
                  (unsigned long)eventCount, (unsigned)((wr + QSIZE - rd) % QSIZE),
                  (unsigned long)underruns, (unsigned)finished, (unsigned)ioFailed);
}
#else
int EspBtTestMidi_begin(void) { return 0; }
void EspBtTestMidi_pump(void) {}
int32_t EspBtTestMidi_fill(uint8_t *, int32_t) { return 0; }
int EspBtTestMidi_isReady(void) { return 0; }
void EspBtTestMidi_report(void) {}
#endif
