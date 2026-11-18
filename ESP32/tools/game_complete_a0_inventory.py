#!/usr/bin/env python3
"""GAME-COMPLETE-A0: inspect original Doom RPG BSPs through the ESP32 v2 PAK.

Host-only, read-only, standard library. No firmware memory or production changes.
Source of structural rules: ESP32/src/esp_bsp_reader.c, esp_map_events.h,
build_asset_pack.py and ESP32/src/esp_map_catalog.c.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import zipfile
import zlib

HEADER = struct.Struct('<8sIIII')
ENTRY = struct.Struct('<IIIII')
COUNT = struct.Struct('<H')
EVENT = struct.Struct('<I')
BYTECODE = struct.Struct('<BII')
OPCODE_NAMES = {1:'GOTO',2:'CHANGEMAP',3:'TRIGGER',4:'MESSAGE',5:'PAIN',
 6:'MOVELINE',7:'SHOW',8:'DIALOG',9:'GIVEMAP',10:'PASSWORD',11:'CHANGESTATE',
 12:'LOCK',13:'UNLOCK',14:'TOGGLELOCK',15:'OPENLINE',16:'CLOSELINE',
 17:'MOVELINE2',18:'HIDE',19:'NEXTSTATE',20:'PREVSTATE',21:'INCSTAT',
 22:'DECSTAT',23:'REQSTAT',24:'FORCEMESSAGE',25:'ANIM',26:'DIALOGNOBACK',
 27:'SAVEGAME',28:'ABORTMOVE',29:'SCREENSHAKE',30:'CHANGEFLOORCOLOR',
 31:'CHANGECEILCOLOR',32:'ENABLEWEAPONS',33:'OPENSTORE',34:'CHANGESPRITE',
 35:'SPAWNPARTICLES',36:'REFRESHVIEW',37:'WAIT',38:'ACTIVE_PORTAL',
 39:'CHECK_COMPLETED_LEVEL',40:'NOTE',41:'CHECK_KEY',42:'PLAYSOUND'}
TEXT_OPS = {4,8,24,26,40}
TRANSITION_OPS = {2,27}


def norm(name: str) -> str:
    return name.replace('\\', '/').lstrip('/').lower()


def fnv_name(name: str) -> int:
    h = 0x811c9dc5
    for b in norm(name).encode('ascii'):
        h = ((h ^ b) * 0x01000193) & 0xffffffff
    return h


def load_catalog(path: Path) -> list[str]:
    source = path.read_text(encoding='utf-8')
    match = re.search(r'mapNames\[ESP_MAP_CATALOG_COUNT\]\s*=\s*\{(.*?)\};', source, re.S)
    if match is None:
        raise ValueError(f'cannot find the native map catalog in {path}')
    names = re.findall(r'"(/[^"\s]+\.bsp)"', match.group(1))
    if len(names) != len(set(names)) or not names:
        raise ValueError('invalid/duplicate map names in catalog')
    header = path.parent.parent / 'include' / 'esp_map_catalog.h'
    if header.exists():
        m = re.search(r'#define\s+ESP_MAP_CATALOG_COUNT\s+(\d+)U', header.read_text())
        if m and len(names) != int(m.group(1)):
            raise ValueError('catalog C count differs from header count')
    return names


def index_pack(path: Path) -> tuple[dict[int, dict], dict]:
    entries = {}
    with path.open('rb') as f:
        size = path.stat().st_size
        raw = f.read(HEADER.size)
        if len(raw) != HEADER.size:
            raise ValueError('truncated PAK header')
        magic, version, count, index_offset, data_offset = HEADER.unpack(raw)
        if (magic, version, index_offset, data_offset) != (
                b'DRPGESP2', 2, HEADER.size, HEADER.size + ENTRY.size * count):
            raise ValueError('invalid PAK v2 header/index layout')
        if data_offset > size:
            raise ValueError('PAK index exceeds file size')
        last = -1
        ranges = []
        for _ in range(count):
            raw = f.read(ENTRY.size)
            if len(raw) != ENTRY.size:
                raise ValueError('truncated PAK index')
            h, off, n, crc, flags = ENTRY.unpack(raw)
            if h <= last or flags & ~1 or off < data_offset or off > size or n > size - off:
                raise ValueError('PAK index unsorted, invalid flags or out-of-bounds entry')
            if flags & 1 and n:
                raise ValueError('nonempty PAK directory')
            last = h
            entries[h] = dict(offset=off, size=n, crc32=f'{crc:08x}', flags=flags)
            if n: ranges.append((off, off+n))
        for a, b in zip(sorted(ranges), sorted(ranges)[1:]):
            if a[1] > b[0]:
                raise ValueError('overlapping PAK payloads')
    return entries, dict(format='DRPGESP2', version=2, entry_count=count,
                         file_bytes=size, data_offset=data_offset,
                         sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def read_entry(f, entry: dict) -> bytes:
    f.seek(entry['offset'])
    data = f.read(entry['size'])
    if len(data) != entry['size'] or f'{zlib.crc32(data):08x}' != entry['crc32']:
        raise ValueError('PAK resource truncated or CRC mismatch')
    return data


class Cursor:
    def __init__(self, data: bytes):
        self.data, self.pos = data, 0

    def take(self, n: int) -> bytes:
        if n < 0 or n > len(self.data) - self.pos:
            raise ValueError(f'truncated BSP at {self.pos}, requested {n}')
        r = self.data[self.pos:self.pos+n]
        self.pos += n
        return r

    def u16(self) -> int:
        return COUNT.unpack(self.take(2))[0]


def parse_bsp(data: bytes, catalog: list[str]) -> dict:
    cur = Cursor(data)
    name = cur.take(16).split(b'\0',1)[0].decode('latin-1')
    floor_rgb = list(cur.take(3)); ceiling_rgb = list(cur.take(3))
    floor_tex, ceiling_tex = cur.take(1)[0], cur.take(1)[0]
    intro_rgb = list(cur.take(3)); load_id = cur.take(1)[0]
    spawn = cur.u16(); direction = cur.take(1)[0]; camera = cur.u16()
    if cur.pos != 33:
        raise ValueError('BSP header size not 33')

    def records(n: int) -> tuple[int,list[bytes]]:
        count = cur.u16()
        return count, [cur.take(n) for _ in range(count)]

    nodes_n, _ = records(10)
    lines_n, lines = records(10)
    sprites_n, sprites = records(5)
    events_n, raw_events = records(4)
    codes_n, raw_codes = records(9)
    strings_n = cur.u16()
    strings = [cur.take(cur.u16()) for _ in range(strings_n)]
    blockmap = cur.take(256)
    planes = cur.take(2048)
    trailing = len(data) - cur.pos
    codes = [BYTECODE.unpack(x) for x in raw_codes]
    sprite_ids = Counter(x[2] for x in sprites)
    line_textures = Counter(struct.unpack_from('<H', x, 4)[0] for x in lines)
    opcodes = Counter(x[0] for x in codes)
    result = dict(name=name, load_map_id=load_id,
      spawn=dict(tile=spawn, direction=direction, camera_tile=camera),
      colors=dict(floor_rgb=floor_rgb, ceiling_rgb=ceiling_rgb, intro_rgb=intro_rgb,
                  floor_texture=floor_tex, ceiling_texture=ceiling_tex),
      counts=dict(nodes=nodes_n, lines=lines_n, sprites=sprites_n,
                  events=events_n, bytecodes=codes_n, strings=strings_n,
                  strings_bytes=sum(map(len,strings)), blockmap_bytes=len(blockmap),
                  plane_texture_bytes=len(planes), trailing_bytes=trailing,
                  compact_plan_bytes=nodes_n*10+lines_n*10+sprites_n*5+
                  events_n*4+codes_n*9+strings_n*2+256+2048+3*8*4),
      sprite_resource_ids=dict(sorted(sprite_ids.items())),
      line_texture_ids=dict(sorted(line_textures.items())),
      opcode_counts={f'{k:02d}_{OPCODE_NAMES.get(k,"UNKNOWN")}':v
                     for k,v in sorted(opcodes.items())},
      events=[], transitions=[], text_references=[], warnings=[])
    if trailing:
        result['warnings'].append('trailing BSP bytes; native committed spawn may reject')
    if load_id < 1 or load_id > 11:
        result['warnings'].append('gameplay loadMapId outside native transition preflight 1..11')
    if spawn >= 1024:
        result['warnings'].append('header spawn tile is out of map bounds')

    seen_tiles = set(); linked = set(); last_tile = -1
    for i,raw in enumerate(raw_events):
        v = EVENT.unpack(raw)[0]
        tile = v & 0x3ff; first = (v >> 10)&0x1ff
        count = (v >> 19)&0x3f; state = (v >> 25)&0xf; flags = (v>>29)&7
        ev = dict(index=i, tile=tile, x=tile%32, y=tile//32,
                  initial_state=state, flags=flags, first_command=first,
                  command_count=count, commands=[])
        if tile in seen_tiles: result['warnings'].append(f'duplicate event tile {tile}')
        if tile < last_tile: result['warnings'].append(f'unsorted event tile {tile} at {i}')
        seen_tiles.add(tile); last_tile=tile
        if first+count > codes_n:
            result['warnings'].append(f'event {i} exceeds bytecode table')
            result['events'].append(ev)
            continue
        for j in range(first,first+count):
            linked.add(j)
            opcode,arg1,arg2 = codes[j]
            cmd = dict(index=j, offset=j-first, id=opcode,
                       name=OPCODE_NAMES.get(opcode,'UNKNOWN'),
                       arg1=f'{arg1:08x}', arg2=f'{arg2:08x}')
            ev['commands'].append(cmd)
            if opcode in TRANSITION_OPS:
                idx = arg1 & 0xff
                raw_target = strings[idx].decode('latin-1').rstrip('\0') if idx < len(strings) else None
                clean = '/' + norm(raw_target) if raw_target else None
                target_id = catalog.index(clean)+1 if clean in catalog else None
                link = dict(source_event=i, tile=tile, code_index=j,
                            opcode=cmd['name'], target_string_index=idx,
                            target_resource=raw_target, target_catalog_id=target_id,
                            candidate_only=True)
                if opcode == 2:
                    link.update(show_stats=bool(arg1 & 0x80000000),
                                spawn_param=((arg1 << 1)&0xffffffff)>>9)
                result['transitions'].append(link)
            if opcode in TEXT_OPS or opcode == 39:
                idx = arg1 if opcode in TEXT_OPS else arg1 & 0xffff
                result['text_references'].append(dict(event=i, code_index=j,
                       opcode=cmd['name'], string_index=idx,
                       exists=idx<len(strings), bytes=len(strings[idx]) if idx<len(strings) else None))
        result['events'].append(ev)
    result['unlinked_bytecode_indexes'] = sorted(set(range(codes_n))-linked)
    return result


def compare_zip(path: Path, entries: dict[int,dict], names: list[str], f) -> dict:
    extra = []; missed=[]; differences=[]; zip_files=[]
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            name = norm(info.filename)
            h = fnv_name(name)
            e = entries.get(h)
            zip_files.append((name,h))
            if e is None:
                missed.append(name)
            elif (e['size']!=info.file_size or e['crc32']!=f'{info.CRC:08x}' or
                  e['flags']!=int(info.is_dir())):
                differences.append(name)
            if name.endswith('.bsp') and '/'+name not in names:
                extra.append(name)
        zip_hashes = {h for _, h in zip_files}
        unknown_hashes = [f'{h:08x}' for h in entries if h not in zip_hashes]
        for name in names:
            arc = norm(name)
            if arc not in [p for p,_ in zip_files]:
                missed.append('original-zip:'+arc)
    return dict(zip_entry_count=len(zip_files), named_original_bsp_extra=sorted(extra),
                zip_unmatched_names=sorted(set(missed)), zip_pak_mismatches=sorted(set(differences)),
                pak_hashes_unmatched_by_zip=unknown_hashes)


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pak',type=Path,required=True)
    parser.add_argument('--zip',type=Path,help='optional original DoomRPG.zip; compares all pack records')
    parser.add_argument('--catalog',type=Path,default=Path(__file__).resolve().parents[1]/'src'/'esp_map_catalog.c')
    parser.add_argument('--output',type=Path,required=True,help='host JSON report (do not commit asset text)')
    parser.add_argument('--strict',action='store_true',help='fail on missing/invalid maps, warnings or ZIP mismatches')
    args=parser.parse_args()
    try:
        catalog=load_catalog(args.catalog)
        entries, meta=index_pack(args.pak)
        report=dict(schema='GAME-COMPLETE-A0/1', catalog_source=str(args.catalog),
                    source_pak=str(args.pak),pak=meta, maps=[],
                    zip_comparison=None, note='Raw opcode presence is not reachability or runtime PASS')
        with args.pak.open('rb') as f:
            if args.zip:
                report['zip_comparison']=compare_zip(args.zip,entries,catalog,f)
            for map_id,name in enumerate(catalog,1):
                h=fnv_name(name)
                record=dict(map_id=map_id,resource=name,name_hash=f'{h:08x}',present=h in entries)
                if h in entries:
                    record['pak_entry']=entries[h]
                    try:
                        payload=read_entry(f,entries[h]);record.update(parse_bsp(payload,catalog))
                        record['source_sha256']=hashlib.sha256(payload).hexdigest()
                    except ValueError as exc:
                        record['parse_error']=str(exc)
                report['maps'].append(record)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        bad=[m['resource'] for m in report['maps'] if not m['present'] or m.get('parse_error') or m.get('warnings')]
        z=report['zip_comparison']
        print(f'[GAME-A0] maps={len(catalog)} present={sum(m["present"] for m in report["maps"])} '
              f'bad={len(bad)} packEntries={meta["entry_count"]} output={args.output}')
        for m in report['maps']:
            print(f'[GAME-A0] {m["map_id"]:02d} {m["resource"]} '
                  f'{"ERROR " + m.get("parse_error","missing") if not m["present"] or m.get("parse_error") else "BSP-STRUCTURAL-OK"} '
                  f'opcodes={",".join(m.get("opcode_counts",{}))}')
        if z: print(f'[GAME-A0] ZIP compare unmatched={len(z["zip_unmatched_names"])} '
                    f'indexMismatch={len(z["zip_pak_mismatches"])} '
                    f'extraBSP={len(z["named_original_bsp_extra"])}')
        if args.strict and (bad or (z and any(z[k] for k in ('zip_unmatched_names',
                               'zip_pak_mismatches','pak_hashes_unmatched_by_zip')))):
            return 2
        return 0
    except (OSError,ValueError,struct.error,zipfile.BadZipFile) as exc:
        print(f'[GAME-A0] FAILED: {exc}',file=sys.stderr)
        return 1


if __name__=='__main__':
    raise SystemExit(main())
