#!/usr/bin/env python3
"""Inspect deterministic 4bpp replacement candidates for all 94 large Doom glyphs.

Source is the verified user-approved 9x12 a.bmp sheet; preserve the two
individually restored 13x17 lowercase a/o exactly. All options retain the
existing 208x102 grid/ASCII c-33 index, BMP palette and magenta key.
An engineering preview is emitted without modifying the deployable BMP.
"""
from __future__ import annotations
import argparse, base64, io, hashlib, struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[2]
SMALL=ROOT/"sdroot/doomrpg/img/a.bmp"
LARGE=ROOT/"sdroot/doomrpg/img/larger_font.bmp"
WORK=ROOT/"artwork/fonts/previews"
SMALL_TO_BIG={0:2,1:1,2:0,3:12,4:10,5:8,6:7,7:5,8:14,9:13,10:9,11:6,12:4,13:3,14:15,15:11}
assert len(SMALL_TO_BIG)==16 and len(set(SMALL_TO_BIG.values()))==16
KEEP={ord("a")-33,ord("o")-33}
WIDTH,HEIGHT=208,102
PW,PH=13,17

def rgba_tile(src:Image.Image,left:int,top:int,w:int,h:int,palette:list[int],transparent:int)->Image.Image:
    pixels=[]
    for yy in range(top,top+h):
        for xx in range(left,left+w):
            ix=src.getpixel((xx,yy))
            k=ix*3
            pixels.append((*palette[k:k+3],0 if ix==transparent else 255))
    tile=Image.new("RGBA",(w,h))
    tile.putdata(pixels)
    return tile

def nearest_index(rgb, palette):
    a,b,c=rgb
    scores=[(5*(a-palette[k*3])**2+9*(b-palette[k*3+1])**2+4*(c-palette[k*3+2])**2,k)
           for k in range(1,16)]
    return min(scores)[1]

def make_option(mode:str,small:Image.Image,big:Image.Image)->Image.Image:
    result=big.copy()
    smallp=small.getpalette()
    for ci in range(96):
        if ci in KEEP: continue
        sx=(ci%16)*9; sy=(ci//16)*12
        dx=(ci%16)*PW; dy=(ci//16)*PH
        if mode=="nearest":
            cell=small.crop((sx,sy,sx+9,sy+12)).resize((PW,PH),Image.Resampling.NEAREST)
            result.paste(cell.point(lambda k:SMALL_TO_BIG.get(k,2)),(dx,dy))
            continue
        tile=rgba_tile(small,sx,sy,9,12,smallp,0)
        scaled=tile.resize((PW,PH),Image.Resampling.BICUBIC if mode=="smooth" else Image.Resampling.BILINEAR)
        hard=small.crop((sx,sy,sx+9,sy+12)).resize((PW,PH),Image.Resampling.NEAREST)
        for y in range(PH):
            for x in range(PW):
                r,g,b,alpha=scaled.getpixel((x,y))
                if mode=="smooth":
                    if alpha<124: k=2
                    else: k=SMALL_TO_BIG[nearest_index((r,g,b),smallp)]
                elif mode=="mask":
                    if alpha<119:k=2
                    else:
                        old=big.getpixel((dx+x,dy+y))
                        k=old if old!=2 else SMALL_TO_BIG[hard.getpixel((x,y))]
                elif mode=="crisp":
                    if alpha<110:k=2
                    else:k=SMALL_TO_BIG[hard.getpixel((x,y))]
                    if k==2 and alpha>=110:k=SMALL_TO_BIG[nearest_index((r,g,b),smallp)]
                else:raise ValueError(mode)
                result.putpixel((dx+x,dy+y),k)
    return result

def colored(source:Image.Image,scale:int=1)->Image.Image:
    rgba=Image.new("RGBA",source.size,(244,0,230,255))
    pal=source.getpalette()
    data=[]
    for idx in source.getdata():
        rgb=pal[idx*3:idx*3+3]
        data.append((*rgb,255))
    rgba.putdata(data)
    if scale>1:rgba=rgba.resize((rgba.width*scale,rgba.height*scale),Image.Resampling.NEAREST)
    return rgba

def render_report()->bytes:
    small=Image.open(SMALL)
    big=Image.open(LARGE)
    assert small.mode==big.mode=="P"
    assert small.size==(144,72) and big.size==(208,102)
    modes=("original","nearest","crisp","smooth","mask")
    # 5 columns, each a 208x102 atlas doubled in nearest-pixel scale.
    sheet=Image.new("RGB",(len(modes)*416+len(modes)*16+16,260),(26,27,32))
    draw=ImageDraw.Draw(sheet)
    for i,mode in enumerate(modes):
        img=big if mode=="original" else make_option(mode,small,big)
        draw.text((24+i*(416+16),7),mode,fill=(244,244,244))
        sheet.paste(colored(img,2).convert("RGB"),(16+i*(416+16),32))
        for char in "AaOoGgRr123?":
            ci=ord(char)-33
            x0=(ci%16)*PW;y0=(ci//16)*PH
            # comparison samples in row under the whole atlas
        draw.text((24+i*(416+16),243),"13x17 | 16 colors",fill=(230,230,230))
    buf=io.BytesIO()
    sheet.save(buf,"PNG",optimize=True)
    return buf.getvalue()

def main():
    pa=argparse.ArgumentParser()
    pa.add_argument("--report",action="store_true")
    pa.add_argument("--emit-base64",action="store_true")
    opt=pa.parse_args()
    assert opt.report
    small=Image.open(SMALL)
    big=Image.open(LARGE)
    assert (small.mode,big.mode)==("P","P")
    pal0=small.getpalette()
    pal1=big.getpalette()
    for s,b in SMALL_TO_BIG.items():
        assert pal0[s*3:s*3+3]==pal1[b*3:b*3+3],(s,b)
    for mode in ("nearest","crisp","smooth","mask"):
        o=make_option(mode,small,big)
        nonzero=0
        for i in range(96):
            if i in KEEP:
                a=big.crop(((i%16)*PW,(i//16)*PH,(i%16+1)*PW,(i//16+1)*PH))
                b=o.crop(((i%16)*PW,(i//16)*PH,(i%16+1)*PW,(i//16+1)*PH))
                assert a.tobytes()==b.tobytes(),("preserved a/o",mode)
            else:
                pass
        nonzero=sum(a!=b for a,b in zip(big.tobytes(),o.tobytes()))
        print(f"[FONT-CANDIDATE] {mode} changedPixels={nonzero} indexed=4 palette=canonical")
    b=render_report()
    target=WORK/"larger_font_full_repair_candidates.png"
    WORK.mkdir(exist_ok=True,parents=True)
    target.write_bytes(b)
    print(f"[FONT-CANDIDATE] report={target.relative_to(ROOT)} bytes={len(b)} sha256={hashlib.sha256(b).hexdigest()}")
    if opt.emit_base64:print(f"[FONT-PREVIEW-BLOB] {target.relative_to(ROOT)} {base64.b64encode(b).decode('ascii')}")
if __name__=="__main__":
    main()
