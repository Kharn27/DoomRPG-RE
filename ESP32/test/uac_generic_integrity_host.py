#!/usr/bin/env python3
"""Pin corrected UAC blank button shape and fail-closed SHA; stdlib only."""
from pathlib import Path
import hashlib
import struct

ROOT=Path(__file__).resolve().parents[2]
ASSETS=ROOT/"sdroot/doomrpg/img"
GENERIC=ASSETS/"menu_generic_120.bmp"
START=ASSETS/"menu_start_active_120.bmp"
EXPECTED="1aa91c6aa6a1ac515be55c9552ee84f22bea550a1c6e0951b3eef23a525b2274"
OLD_OPAQUE="6a188ff11105257f15ab50c1b0671675f6377cbe3fedbb1ea3efa6942268edda"

def read_bmp(path):
    data=path.read_bytes()
    assert data[:2]==b"BM" and len(data)==4522
    w,h=struct.unpack_from("<II",data,18)
    assert (w,h)==(55,20), path
    assert struct.unpack_from("<I",data,10)[0]==122
    assert struct.unpack_from("<I",data,14)[0]==108
    assert struct.unpack_from("<H",data,28)[0]==32
    return data

def at(b,x,y):
    offset=122+((19-y)*55+x)*4
    return tuple(b[offset:offset+4])

gen=read_bmp(GENERIC)
start=read_bmp(START)
assert hashlib.sha256(gen).hexdigest()==EXPECTED
assert EXPECTED!=OLD_OPAQUE
for y in range(20):
    for x in range(55):
        a=at(gen,x,y)
        b=at(start,x,y)
        assert a[3]==b[3],("alpha drift",x,y)
        if y<3 or y>=17 or x<3 or x>=52:
            assert a==b,("START bezel drift",x,y)
for x,y in ((0,0),(54,0),(0,19),(54,19)):
    assert at(gen,x,y)[3]==0,(x,y)
assert at(gen,27,10)[3]==255
src=(ROOT/"ESP32/src/esp_uac_menu_preview.cpp").read_text()
assert EXPECTED in src and 'verifyCachedBmpSha(' in src
assert '"1aa91c6aa6a1ac515be55c9552ee84f22bea550a1c6e0951b3eef23a525b2274",true)' in src
assert '[UAC] GENERIC120 integrity=%s' in src
print("UAC generic integrity PASS: START-exact bezel/alpha, transparent corners, SHA fail-closed")
