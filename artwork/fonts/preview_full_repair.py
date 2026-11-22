#!/usr/bin/env python3
"""Rebuild all 96 Doom large-font glyphs from the same canonical small atlas.

No exception for lowercase a/o: previous special-casing made two characters
look visibly different from the other 94. Retain the exact 208x102 layout,
96 13x17 cells, ASCII c-33, a.bmp color palette and magenta transparency.
"""
from __future__ import annotations
import argparse, base64, io, hashlib, struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[2]
SMALL=ROOT/"sdroot/doomrpg/img/a.bmp"
LARGE=ROOT/"artwork/fonts/sources/larger_font_pre_full_repair.bmp"
FINAL=ROOT/"sdroot/doomrpg/img/larger_font.bmp"
WORK=ROOT/"artwork/fonts/previews"
SMALL_TO_BIG={0:2,1:1,2:0,3:12,4:10,5:8,6:7,7:5,8:14,9:13,10:9,11:6,12:4,13:3,14:15,15:11}
assert len(SMALL_TO_BIG)==16 and len(set(SMALL_TO_BIG.values()))==16
AO_SLOTS={ord("a")-33,ord("o")-33}  # regression oracle only; never skipped
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

def build_refined_data()->bytes:
    """Generate a 4bpp indexed BMP for ALL 96 glyphs with one identical method.

    Start from the canonical a.bmp palette and the archived large-font header.
    Every glyph, including lowercase a/o, is rebuilt from the clean 9x12
    source using bicubic RGBA sampling, an identical transparency threshold
    and nearest of the 15 opaque canonical palette colors. The previous
    exception for lowercase a/o is deliberately removed.
    """
    base=LARGE.read_bytes()
    assert len(base)==10726 and base[:2]==b"BM"
    assert struct.unpack_from("<I",base,10)[0]==118
    assert struct.unpack_from("<ii",base,18)==(WIDTH,HEIGHT)
    assert struct.unpack_from("<H",base,28)[0]==4
    small=Image.open(SMALL)
    big=Image.open(LARGE)
    result=make_option("smooth",small,big)
    output=bytearray(base)
    pitch=((WIDTH*4+31)//32)*4
    changed=0
    changed_cells=0
    ao_changed={}
    for ci in range(96):
        changed_this=0
        for cy in range(PH):
            y=(ci//16)*PH+cy
            for cx in range(PW):
                x=(ci%16)*PW+cx
                old=big.getpixel((x,y))
                value=result.getpixel((x,y))
                if value==old:continue
                changed+=1
                changed_this+=1
                loc=118+(HEIGHT-1-y)*pitch+x//2
                if x&1:output[loc]=(output[loc]&240)|value
                else:output[loc]=(output[loc]&15)|(value<<4)
        if changed_this:changed_cells+=1
        if ci in AO_SLOTS:
            assert changed_this>0,("special-case glyph was not unified",chr(ci+33))
            ao_changed[chr(ci+33)]=changed_this
    assert output[:118]==base[:118]
    assert changed>10000 and changed_cells==96,(changed,changed_cells)
    assert bytes(output)!=base
    img=Image.open(io.BytesIO(output))
    assert img.mode=="P" and img.size==(WIDTH,HEIGHT)
    assert img.tobytes()==result.tobytes()
    other=bytearray()
    for ci in range(96):
        if ci in AO_SLOTS:continue
        x=(ci%16)*PW;y=(ci//16)*PH
        for row in range(PH):
            for col in range(PW):other.append(img.getpixel((x+col,y+row)))
    print(f"[FONT-REBUILD] changedPixels={changed} changedGlyphCells={changed_cells}/96 "
          f"lowercaseChanged={ao_changed} other94SHA256={hashlib.sha256(other).hexdigest()} "
          f"indexed=4 palette=a.bmp")
    return bytes(output)

def main():
    pa=argparse.ArgumentParser()
    pa.add_argument("--report",action="store_true")
    pa.add_argument("--proof",action="store_true")
    pa.add_argument("--build",action="store_true")
    pa.add_argument("--check",action="store_true")
    pa.add_argument("--emit-base64",action="store_true")
    opt=pa.parse_args()
    assert opt.report or opt.build or opt.check or opt.proof
    if opt.build or opt.check:
        font=build_refined_data()
        if opt.build:FINAL.write_bytes(font)
        if opt.check:assert FINAL.read_bytes()==font,"SD large font is stale"
        print(f"[FONT-REBUILD] {'WRITE' if opt.build else 'CHECK'} PASS size={len(font)} sha256={hashlib.sha256(font).hexdigest()}")
        if opt.emit_base64:print(f"[FONT-FINAL-BLOB] sdroot/doomrpg/img/larger_font.bmp {base64.b64encode(font).decode('ascii')}")
    if opt.proof:
        final=Image.open(FINAL)
        data=colored(final,4)
        output=WORK/"larger_font_clean_zoom.png"
        WORK.mkdir(parents=True,exist_ok=True)
        buf=io.BytesIO()
        data.save(buf,"PNG",optimize=True)
        content=buf.getvalue()
        output.write_bytes(content)
        print(f"[FONT-PROOF] {output.relative_to(ROOT)} {data.size} {len(content)} bytes")
        if opt.emit_base64:
            print(f"[FONT-ZOOM-BLOB] {output.relative_to(ROOT)} {base64.b64encode(content).decode()}")
    if not opt.report:return
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
            if i in AO_SLOTS:
                a=big.crop(((i%16)*PW,(i//16)*PH,(i%16+1)*PW,(i//16+1)*PH))
                b=o.crop(((i%16)*PW,(i//16)*PH,(i%16+1)*PW,(i//16+1)*PH))
                assert a.tobytes()!=b.tobytes(),("a/o must use the SAME reconstruction as all others",mode)
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
