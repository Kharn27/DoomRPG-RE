#!/usr/bin/env python3
"""Deterministic pixel-exact OPTIONS native-120 proof (host only).
Same SD frame/button, 13x17 Doom font indices, 4-rail (or 5-choice) geometry.
The game framebuffer is never duplicated on the ESP32.
"""
from __future__ import annotations
import argparse, base64
from pathlib import Path
from PIL import Image
from render_corrected_120 import ROOT, SD, OUT, blend_on, glyph_color, png_bytes, DARK

PAGES=(
    ("OPTIONS", ("BACK","VIDEO","SOUND"),1),
    ("VIDEO", ("BACK","LUMINOSITE","ORIENTATION","THEME"),1),
    ("LUMINOSITE", ("BACK","25%","50%","75%","100%"),2),
    ("ORIENTATION", ("BACK","NORMAL","INVERSE"),2),
    ("THEME", ("BACK","CLASSIC","INFERNO","PHOSPHORE"),2),
    ("SOUND", ("BACK","ON","VOLUME"),1),
    ("VOLUME", ("25%","50%","75%","100%"),3),
    ("SOUND-OFF", ("BACK","OFF","VOLUME"),1),
)

def compose(labels:tuple[str,...],selected:int)->Image.Image:
    count=len(labels)
    assert count in (3,4,5)
    base=Image.new("RGBA",(118,240),DARK)
    frame=Image.open(SD/"menu_frame_120.bmp").convert("RGBA").resize((118,240),Image.Resampling.NEAREST)
    button=Image.open(SD/"menu_generic_120.bmp").convert("RGBA")
    atlas=Image.open(SD/"larger_font.bmp")
    assert button.size==(55,20) and atlas.size==(208,102) and atlas.mode=="P"
    # 3-choice pages still paint ALL FOUR physical rails; slot 4 stays blank.
    tops=[12+46*i for i in range(5)] if count==5 else [20,74,128,182]
    height=32 if count==5 else 40
    scaled=Image.new("RGBA",(110,height),(0,0,0,0))
    for y in range(height):
        for x in range(110):
            scaled.putpixel((x,y),button.getpixel((x//2,y*20//height)))
    for top in tops:
        blend_on(base,scaled,(4,top))
    blend_on(base,frame,(0,0))
    palette=atlas.getpalette()
    for i,label in enumerate(labels):
        assert label and all(ch in "%01257ABCDEFHIKLMNOPRSTUV" for ch in label),label
        advance=min(14,98//len(label))
        text_h=19 if count==5 else 21 if advance<14 else 23
        x0=4+(110-len(label)*advance)//2
        y0=tops[i]+(height-text_h)//2+(0 if count==5 else 2)
        assert x0>=10 and x0+len(label)*advance<=108
        assert y0>=tops[i] and y0+text_h<=tops[i]+height
        for j,ch in enumerate(label):
            tile=ord(ch)-33
            sx=(tile%16)*13;sy=(tile//16)*17
            for ty in range(text_h):
                for tx in range(advance):
                    ink=atlas.getpixel((sx+tx*13//advance,sy+ty*17//text_h))
                    if ink==2:continue
                    rgb=tuple(palette[ink*3:ink*3+3])
                    base.putpixel((x0+j*advance+tx,y0+ty),
                                  glyph_color(rgb,i==selected))
    return base.convert("RGB")

def main()->None:
    p=argparse.ArgumentParser()
    m=p.add_mutually_exclusive_group(required=True)
    m.add_argument("--write",action="store_true")
    m.add_argument("--check",action="store_true")
    p.add_argument("--emit-base64",action="store_true")
    args=p.parse_args()
    ims=[compose(labels,selected) for _,labels,selected in PAGES]
    sheet=Image.new("RGB",(len(ims)*(118+10)+10,260),(13,20,27))
    for i,im in enumerate(ims):sheet.paste(im,(10+i*128,10))
    targets={"options_120_root.png":ims[0],"proof_options_120_all_pages.png":sheet}
    mismatches=[]
    for filename,im in targets.items():
        blob=png_bytes(im)
        path=OUT/filename
        if args.write:
            OUT.mkdir(parents=True,exist_ok=True)
            path.write_bytes(blob)
        elif path.read_bytes()!=blob:
            # Exact regeneration payload for CI failure diagnosis/recovery.
            print(f"[UAC-OPTIONS-REGEN] artwork/menu-uac/generic/previews/{filename} {base64.b64encode(blob).decode()}",flush=True)
            mismatches.append(filename)
        print(f"[UAC-OPTIONS-PROOF] {filename} {im.size} {len(blob)} bytes")
        if args.emit_base64:
            print(f"[UAC-OPTIONS-BLOB] artwork/menu-uac/generic/previews/{filename} {base64.b64encode(blob).decode()}")
    assert not mismatches,"stale OPTIONS visual proofs: "+", ".join(mismatches)
    print(f"[UAC-OPTIONS-PROOF] {'WRITE' if args.write else 'CHECK'} PASS")
if __name__=="__main__":main()
