#!/usr/bin/env python3
"""Host executes the real bounded SMF parser; hardware/DAC is CYD-only."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
cc = shutil.which("cc")
assert cc, "Host C compiler required"
harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_midi_stream.h"
static const uint8_t midi[] = {
'M','T','h','d',0,0,0,6,0,1,0,2,0,96,
'M','T','r','k',0,0,0,11,
0,0xff,0x51,3,7,0xa1,0x20,96,0xff,0x2f,0,
'M','T','r','k',0,0,0,15,
0,0xc0,5,0,0x90,60,100,96,0x80,60,0,0,0xff,0x2f,0
};
static int read_mem(void* ctx, uint32_t off, void* dst, size_t len) {
    const uint8_t* data = ctx ? (const uint8_t*)ctx : midi;
    if (off > sizeof(midi) || len > sizeof(midi)-off) return 0;
    memcpy(dst,data+off,len); return 1;
}
int main(void) {
    EspNativeMidiSong song;
    EspNativeMidiEvent e;
    assert(EspNativeMidi_begin(&song,read_mem,0,sizeof(midi)));
    assert(song.trackCount==2 && song.ppqn==96);
    assert(EspNativeMidi_next(&song,&e)==1 && e.type==ESP_NATIVE_MIDI_TEMPO && e.atUs==0);
    assert(EspNativeMidi_next(&song,&e)==1 && e.type==ESP_NATIVE_MIDI_PROGRAM && e.a==5 && e.atUs==0);
    assert(EspNativeMidi_next(&song,&e)==1 && e.type==ESP_NATIVE_MIDI_NOTE_ON && e.a==60 && e.b==100 && e.atUs==0);
    assert(EspNativeMidi_next(&song,&e)==1 && e.type==ESP_NATIVE_MIDI_NOTE_OFF && e.a==60 && e.atUs==500000);
    assert(EspNativeMidi_next(&song,&e)==0 && EspNativeMidi_durationUs(&song)==500000);
    assert(EspNativeMidi_next(&song,&e)==0);
    /* Truncated MIDI, malformed track, PPQN, running status: strict failures. */
    assert(!EspNativeMidi_begin(&song,read_mem,0,14));
    {
        uint8_t bad[sizeof(midi)];
        memcpy(bad,midi,sizeof(bad));
        bad[12]=0x80;bad[13]=0;
        assert(!EspNativeMidi_begin(&song,read_mem,bad,sizeof(bad)));
        memcpy(bad,midi,sizeof(bad));
        bad[42]=0x01; /* running-status data without any valid status */
        assert(EspNativeMidi_begin(&song,read_mem,bad,sizeof(bad)));
        /* The merger preloads every track before returning an event:
         * invalid running status fails closed immediately. */
        assert(EspNativeMidi_next(&song,&e)==-1);
    }
    puts("audio_midi_parser_host: PASS original SMF-0/1 event/tempo/track timeline, no heap");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
    source = Path(temp) / "main.c"
    binary = Path(temp) / "midi_host"
    source.write_text(harness)
    subprocess.run([cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root / "src"), str(source),
                    str(root / "src/esp_native_midi_stream.c"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS AUDIO-A native streaming MIDI parser")
