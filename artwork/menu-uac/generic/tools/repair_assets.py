#!/usr/bin/env python3
"""Rebuild pixel-exact UAC generic edges, never modify active Doom fonts.

The START V4 alpha/metal rim is the geometry oracle. Only its perimeter
is borrowed; the original generic metal center stays blank. All 96 large-font
glyphs, including lowercase a/o, are rebuilt identically from the small
Doom atlas and its canonical silver/copper gradient. No font header or palette bytes
are modified, and only the 4bpp pixel indexes of the 96 cells change.

--write rewrites sources, final SD BMPs and the generic manifest;
--check is deterministic and does not write;
--emit-base64 is an optional GitHub bridge for binary payloads, for tooling
that cannot directly upload/write repository binary assets.
"""
from __future__ import annotations
from pathlib import Path
import argparse, base64, hashlib, io, struct
from PIL import Image
from export_generic import make_bmp_v4

ROOT=Path(__file__).resolve().parents[4]
ART=ROOT/"artwork/menu-uac/generic"
SD=ROOT/"sdroot/doomrpg/img"
# Destination palette index 0..15 -> canonical a.bmp index 0..15.
# Source index 2 is magenta; mapping[2] MUST stay magenta.
PALETTE_MAP=(2,1,0,13,12,7,11,6,5,10,4,15,3,9,8,14)
assert sorted(PALETTE_MAP)==list(range(16)) and PALETTE_MAP[2]==0

def v4_rgba(path:Path, expected:tuple[int,int])->Image.Image:
    data=path.read_bytes()
    assert data[:2]==b"BM" and struct.unpack_from("<I",data,14)[0]==108
    assert struct.unpack_from("<ii",data,18)==expected
    assert struct.unpack_from("<H",data,28)[0]==32 and struct.unpack_from("<I",data,30)[0]==3
    return Image.open(io.BytesIO(data)).convert("RGBA")

def png_bytes(im:Image.Image)->bytes:
    stream=io.BytesIO()
    im.save(stream,format="PNG",optimize=False,compress_level=9)
    return stream.getvalue()

def repair_generic(tier:int,width:int,height:int)->tuple[bytes,bytes,int]:
    path=ART/f"sources/menu_generic_{tier}.png"
    old=Image.open(path).convert("RGBA")
    golden=v4_rgba(SD/f"menu_start_active_{tier}.bmp",(width,height))
    assert old.size==golden.size==(width,height)
    # Original START alpha is the optical silhouette, not rectangular generic alpha.
    alpha=golden.getchannel("A")
    old.putalpha(alpha)
    p=old.load()
    g=golden.load()
    # Borrow the exact original beveled/antialiased metal rim, not its label.
    # Conservative border: lettering stays entirely in the generic metal core.
    band=3 if tier==120 else 5
    changed=0
    for y in range(height):
        for x in range(width):
            if y<band or y>=height-band or x<band or x>=width-band:
                oldpixel=p[x,y]
                if oldpixel!=g[x,y]: changed+=1
                p[x,y]=g[x,y]
    # Check every pixel's alpha against the target source, including partial edges.
    assert old.getchannel("A").tobytes()==alpha.tobytes()
    src=png_bytes(old)
    # Encode BMP using the same tool as the original Adobe art; no rescaling.
    # Conversion accepts only RGBA PNG with exactly one source geometry.
    tmp=io.BytesIO(src)
    with Image.open(tmp) as t:
        rgba=t.tobytes()
    raw=bytearray(width*height*4)
    for y in range(height):
        for x in range(width):
            pos=(y*width+x)*4
            dest=((height-y-1)*width+x)*4
            r,gc,b,a=rgba[pos:pos+4]
            raw[dest:dest+4]=bytes((b,gc,r,a))
    size=122+len(raw)
    dib=struct.pack("<IiiHHIIiiII",108,width,height,1,32,3,len(raw),2835,2835,0,0)
    dib+=struct.pack("<IIIII",0x00FF0000,0x0000FF00,0x000000FF,0xFF000000,0x73524742)+bytes(48)
    assert len(dib)==108
    bmp=b"BM"+struct.pack("<IHHI",size,0,0,122)+dib+raw
    assert len(bmp)==122+width*height*4
    return src,bytes(bmp),changed

def repair_large_font()->tuple[bytes,int]:
    # Permanent source is artwork/fonts/sources/larger_font_pre_full_repair.bmp,
    # not the already-shipped output, so --write and --check are idempotent.
    import sys
    sys.path.insert(0,str(ROOT/"artwork/fonts"))
    from preview_full_repair import build_refined_data
    font=build_refined_data()
    reference=(SD/"a.bmp").read_bytes()
    assert font[:2]==b"BM" and reference[:2]==b"BM"
    assert struct.unpack_from("<I",font,10)[0]==118
    assert struct.unpack_from("<ii",font,18)==(208,102)
    assert struct.unpack_from("<H",font,28)[0]==4
    assert struct.unpack_from("<ii",reference,18)==(144,72)
    # Big atlas source-index -> canonical small-atlas palette index.
    for src_idx,ref_idx in enumerate(PALETTE_MAP):
        assert font[54+src_idx*4:58+src_idx*4]==reference[54+ref_idx*4:58+ref_idx*4]
    old=(SD/"larger_font.bmp").read_bytes()
    return font,sum(x!=y for x,y in zip(old,font))


def sha(x:bytes)->str:
    return hashlib.sha256(x).hexdigest()

def all_outputs()->dict[str,bytes]:
    outputs:dict[str,bytes]={}
    manifest=[]
    for tier,w,h in ((120,55,20),(240,110,40)):
        src,bmp,changed=repair_generic(tier,w,h)
        source=f"artwork/menu-uac/generic/sources/menu_generic_{tier}.png"
        target=f"sdroot/doomrpg/img/menu_generic_{tier}.bmp"
        outputs[source]=src;outputs[target]=bmp
        manifest.extend(((sha(src),f"sources/menu_generic_{tier}.png"),
                         (sha(bmp),f"../../../sdroot/doomrpg/img/menu_generic_{tier}.bmp")))
        print(f"[UAC-REPAIR] tier={tier} alpha=START-exact bevel=START-exact perimeterChanged={changed} bmp={len(bmp)} sha256={sha(bmp)}")
    outputs["artwork/menu-uac/generic/SHA256SUMS.txt"]="".join(
        f"{digest}  {name}\n" for digest,name in manifest
    ).encode("utf-8")
    # Font assets are now independent FONT-NEUTRAL inputs supplied by the user.
    # This UAC bezel repair script MUST NOT regenerate/overwrite their active BMPs.
    return outputs

def main()->None:
    parser=argparse.ArgumentParser()
    grp=parser.add_mutually_exclusive_group(required=True)
    grp.add_argument("--write",action="store_true")
    grp.add_argument("--check",action="store_true")
    parser.add_argument("--emit-base64",action="store_true")
    args=parser.parse_args()
    out=all_outputs()
    for name,data in out.items():
        path=ROOT/name
        if args.write:
            path.write_bytes(data)
        else:
            assert path.read_bytes()==data,f"{name}: regenerated bytes differ"
        if args.emit_base64:
            print(f"[UAC-BLOB] {name} {base64.b64encode(data).decode('ascii')}")
    print(f"[UAC-REPAIR] {'WRITE' if args.write else 'CHECK'} PASS {len(out)} artifacts")

if __name__=="__main__":
    main()
