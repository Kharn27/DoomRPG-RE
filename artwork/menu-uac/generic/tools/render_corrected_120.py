#!/usr/bin/env python3
"""Pixel-art proof of the actual UAC native-120 font composition (host only).

No procedural font. The 120 BMPs are nearest-scaled x2; cached Doom 13x17
indexed glyphs are sampled nearest to physical 14x27 on a 110x40 button,
matching esp_uac_menu_preview.cpp exactly, with its original palette.
"""
from __future__ import annotations
from pathlib import Path
import argparse, base64, io
from PIL import Image
from repair_assets import repair_generic, PALETTE_MAP

ROOT=Path(__file__).resolve().parents[4]
SD=ROOT/"sdroot/doomrpg/img"
ART=ROOT/"artwork/menu-uac/generic"
OUT=ART/"previews"
LABELS=("START","LOAD","OPTIONS","HELP")
TOPS=(20,74,128,182)
PAINT_W,PAINT_H,ADVANCE=14,27,14
assert len("OPTIONS")*ADVANCE==98 and PAINT_H<=28
DARK=(16,21,29,255)

def blend_on(dest:Image.Image, src:Image.Image, pos:tuple[int,int])->None:
    dest.alpha_composite(src,pos)

def rgb565(x:tuple[int,int,int])->tuple[int,int,int]:
    r,g,b=x
    r5=r>>3; g6=g>>2; b5=b>>3
    return (r5<<3)|(r5>>2),(g6<<2)|(g6>>4),(b5<<3)|(b5>>2)

def glyph_color(rgb:tuple[int,int,int],selected:bool)->tuple[int,int,int, int]:
    c=rgb565(rgb)
    r5,g6,b5=(c[0]>>3,c[1]>>2,c[2]>>3)
    if not selected:
        r5=r5*3//4;g6=g6*3//4;b5=b5*3//4
    return ((r5<<3)|(r5>>2),(g6<<2)|(g6>>4),(b5<<3)|(b5>>2),255)

def compose(index:int)->Image.Image:
    f=Image.open(SD/"menu_frame_120.bmp").convert("RGBA")
    b=Image.open(SD/"menu_generic_120.bmp").convert("RGBA")
    original=Image.open(SD/"menu_start_active_120.bmp").convert("RGBA")
    assert b.size==original.size==(55,20)
    assert b.getchannel("A").tobytes()==original.getchannel("A").tobytes(), "shape is NOT exactly START"
    for y in range(20):
        for x in range(55):
            if y<3 or y>=17 or x<3 or x>=52:
                assert b.getpixel((x,y))==original.getpixel((x,y)), (x,y)
    frame=f.resize((118,240),Image.Resampling.NEAREST)
    button=b.resize((110,40),Image.Resampling.NEAREST)
    bg=Image.new("RGBA",(118,240),DARK)
    for y in TOPS:
        blend_on(bg,button,(4,y))
    blend_on(bg,frame,(0,0))
    atlas=Image.open(SD/"larger_font.bmp")
    assert atlas.mode=="P" and atlas.size==(208,102)
    for i,label in enumerate(LABELS):
        x0=4+(110-len(label)*ADVANCE)//2
        y0=TOPS[i]+(40-PAINT_H)//2
        assert x0>=4+6 and x0+len(label)*ADVANCE<=4+110-6
        assert y0>=TOPS[i]+6 and y0+PAINT_H<=TOPS[i]+40-6
        for j,char in enumerate(label):
            tile=ord(char)-33
            sx0=(tile%16)*13
            sy0=(tile//16)*17
            for cy in range(PAINT_H):
                for cx in range(PAINT_W):
                    source_y=cy*17//PAINT_H
                    source_x=cx*13//PAINT_W
                    palette_index=atlas.getpixel((sx0+source_x,sy0+source_y))
                    if palette_index==2:continue
                    palette=atlas.getpalette()
                    rgb=tuple(palette[palette_index*3:palette_index*3+3])
                    bg.putpixel((x0+j*ADVANCE+cx,y0+cy),
                                glyph_color(rgb,i==index))
    return bg.convert("RGB")

def png_bytes(img:Image.Image)->bytes:
    o=io.BytesIO();img.save(o,format="PNG",compress_level=9);return o.getvalue()

def main()->None:
    parser=argparse.ArgumentParser()
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write",action="store_true")
    mode.add_argument("--check",action="store_true")
    parser.add_argument("--emit-base64",action="store_true")
    args=parser.parse_args()
    images=[compose(i) for i in range(4)]
    large=Image.new("RGB",(4*118+5*10,240+20),(13,20,27))
    for i,im in enumerate(images):
        large.paste(im,(10+i*(118+10),10))
    files={"preview_120_start.png":images[0],"proof_120_four_selections.png":large}
    for name,img in files.items():
        data=png_bytes(img)
        target=OUT/name
        if args.write:
            OUT.mkdir(parents=True,exist_ok=True)
            target.write_bytes(data)
        else:
            assert target.read_bytes()==data, name+" differs"
        print(f"[UAC-PROOF] {name} {img.size} {len(data)} bytes")
        if args.emit_base64:
            print(f"[UAC-PREVIEW-BLOB] artwork/menu-uac/generic/previews/{name} {base64.b64encode(data).decode()}")
    print(f"[UAC-PROOF] {'WRITE' if args.write else 'CHECK'} PASS")

if __name__=="__main__":
    main()
