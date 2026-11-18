#!/usr/bin/env python3
"""Asset-free regression tests for the host-only GAME-COMPLETE-A0 inventory."""
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
import zlib

SCRIPT = Path(__file__).resolve().parents[1]/'tools'/'game_complete_a0_inventory.py'
spec = importlib.util.spec_from_file_location('game_complete_a0_inventory', SCRIPT)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def fake_bsp():
    head = (b'Entrance'.ljust(16, b'\0') + b'\0\0\0' + b'\0\0\0' +
            bytes([1, 2]) + b'\0\0\0' + bytes([1]) +
            struct.pack('<H',33) + bytes([192]) + struct.pack('<H',33))
    assert len(head) == 33
    event = 6 | (2 << 19) | (1 << 29)
    return (head + struct.pack('<H',0) +
            struct.pack('<H',1) + b'\0\0\0\0\x09\0\0\0\0\0' +
            struct.pack('<H',1) + b'\0\0\x57\0\0' +
            struct.pack('<H',1) + struct.pack('<I',event) +
            struct.pack('<H',2) + struct.pack('<BII',8,0,0) +
            struct.pack('<BII',2,1,0) + struct.pack('<H',2) +
            struct.pack('<H',5) + b'Hello' +
            struct.pack('<H',12) + b'/level01.bsp' +
            bytes(256) + bytes(2048))


def fake_entities_db():
    return struct.pack('<H',1)+struct.pack('<HBBi16s',87,1,0,294,b'Fixture enemy')


def write_pack(path, assets):
    records = sorted((mod.fnv_name(n),d) for n,d in assets.items())
    with path.open('wb') as f:
        off = mod.HEADER.size + mod.ENTRY.size*len(records)
        f.write(mod.HEADER.pack(b'DRPGESP2',2,len(records),mod.HEADER.size,off))
        for h,d in records:
            f.write(mod.ENTRY.pack(h,off,len(d),zlib.crc32(d),0))
            off += len(d)
        for _,d in records:
            f.write(d)


class Tests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.catalog=self.root/'esp_map_catalog.c'
        self.catalog.write_text('static const char* const mapNames[ESP_MAP_CATALOG_COUNT] = {\n'
                                '"/intro.bsp", "/level01.bsp"\n};\n')
        self.pak=self.root/'DoomRPG-ESP32.pak'
        self.assets={'intro.bsp':fake_bsp(), 'level01.bsp':fake_bsp(),
                     'entities.db':fake_entities_db()}
        write_pack(self.pak,self.assets)

    def test_parser_and_graph(self):
        m=mod.parse_bsp(fake_bsp(),['/intro.bsp','/level01.bsp'])
        self.assertEqual(m['name'],'Entrance')
        self.assertEqual(m['counts']['events'],1)
        self.assertEqual(m['opcode_counts'],{'02_CHANGEMAP':1,'08_DIALOG':1})
        self.assertEqual(m['transitions'][0]['target_catalog_id'],2)
        self.assertEqual(m['transitions'][0]['target_resource'],'/level01.bsp')
        self.assertEqual(m['text_references'][0]['string_index'],0)
        self.assertEqual(m['events'][0]['tile'],6)
        self.assertEqual(m['warnings'],[])
        self.assertEqual(m['unlinked_bytecode_indexes'],[])

    def test_truncated_and_unresolved(self):
        with self.assertRaises(ValueError):mod.parse_bsp(fake_bsp()[:-3],['/intro.bsp'])
        raw=fake_bsp()
        self.assertIsNone(mod.parse_bsp(raw,['/intro.bsp'])['transitions'][0]['target_catalog_id'])
        # Malformed event references do not produce fabricated commands.
        damaged=bytearray(raw)
        offset=33+2+2+10+2+5+2
        struct.pack_into('<I',damaged,offset, 6 | (63<<19) | (500<<10))
        result=mod.parse_bsp(bytes(damaged),['/intro.bsp'])
        self.assertIn('exceeds bytecode table',result['warnings'][0])

    def test_preflight_id_bounds_and_strict_invalid_references(self):
        # esp_map_transition_preflight.h admits gameplay load IDs 1..32,
        # distinct from catalog resource IDs 1..13.
        raw=bytearray(fake_bsp())
        for load_id in (12,32):
            raw[27]=load_id
            parsed=mod.parse_bsp(bytes(raw),['/intro.bsp','/level01.bsp'])
            self.assertFalse(any('loadMapId outside' in w for w in parsed['warnings']))
        raw[27]=33
        parsed=mod.parse_bsp(bytes(raw),['/intro.bsp','/level01.bsp'])
        self.assertTrue(any('loadMapId outside native transition preflight 1..32' in w
                            for w in parsed['warnings']))
        raw[27]=1
        code_start=33+2+(2+10)+(2+5)+(2+4)+2
        struct.pack_into('<I',raw,code_start+1,99) # EV_DIALOG invalid string
        struct.pack_into('<I',raw,code_start+mod.BYTECODE.size+1,254) # CHANGEMAP invalid string
        parsed=mod.parse_bsp(bytes(raw),['/intro.bsp','/level01.bsp'])
        self.assertTrue(any('missing text string 99' in w for w in parsed['warnings']))
        self.assertTrue(any('missing map string 254' in w for w in parsed['warnings']))
        self.assets['intro.bsp']=bytes(raw)
        write_pack(self.pak,self.assets)
        cmd=[sys.executable,str(SCRIPT),'--pak',str(self.pak),
             '--catalog',str(self.catalog),'--output',str(self.root/'bad.json'),'--strict']
        self.assertEqual(subprocess.run(cmd,capture_output=True,text=True).returncode,2)
        parsed=mod.parse_bsp(fake_bsp(),['/intro.bsp'])
        self.assertTrue(any('target outside map catalog' in w for w in parsed['warnings']))


    def test_special_bsp_and_capacity_census(self):
        raw=bytearray(fake_bsp())
        raw[27]=0
        special=mod.parse_bsp(bytes(raw),['/items.bsp','/endgame.bsp'],
                              '/endgame.bsp',mod.parse_entity_defs(fake_entities_db()))
        self.assertEqual(special['map_role'],'special-non-gameplay')
        self.assertFalse(any('loadMapId outside' in w for w in special['warnings']))
        ordinary=mod.parse_bsp(bytes(raw),['/intro.bsp'],'/intro.bsp')
        self.assertTrue(any('loadMapId outside' in w for w in ordinary['warnings']))
        a=mod.classify_initial_entities([b'\\0\\0\\x57\\0\\0']*101,
                                         {87:(1,0)})
        self.assertEqual(a['enemies'],101)
        self.assertEqual(a['topology_snapshot_tracked'],101)
        self.assertTrue(a['topology_snapshot_overflow'])
        # Raw type classification cannot be excused by an inactive sprite flag.
        no_entity=mod.classify_initial_entities([b'\\0\\0\\x57\\0\\x01'],{87:(1,0)})
        self.assertEqual(no_entity['enemies'],0)


    def test_pack_index_and_crc(self):
        idx,head=mod.index_pack(self.pak)
        self.assertEqual(head['entry_count'],3)
        with self.pak.open('rb') as f:
            d=mod.read_entry(f,idx[mod.fnv_name('/intro.bsp')])
        self.assertEqual(d,fake_bsp())
        damaged=bytearray(self.pak.read_bytes())
        damaged[idx[mod.fnv_name('level01.bsp')]['offset']] ^= 255
        self.pak.write_bytes(damaged)
        idx,_=mod.index_pack(self.pak)
        with self.pak.open('rb') as f:
            with self.assertRaisesRegex(ValueError,'CRC'):
                mod.read_entry(f,idx[mod.fnv_name('level01.bsp')])
        damaged[0]=0
        self.pak.write_bytes(damaged)
        with self.assertRaisesRegex(ValueError,'header'):
            mod.index_pack(self.pak)

    def test_cli_and_zip(self):
        zpath=self.root/'DoomRPG.zip'
        with zipfile.ZipFile(zpath,'w') as z:
            for name,data in self.assets.items():z.writestr(name,data)
        output=self.root/'inventory.json'
        cmd=[sys.executable,str(SCRIPT),'--pak',str(self.pak),'--zip',str(zpath),
             '--catalog',str(self.catalog),'--output',str(output),'--strict']
        self.assertEqual(subprocess.run(cmd,capture_output=True,text=True).returncode,0)
        result=json.loads(output.read_text())
        self.assertEqual(result['maps'][0]['counts']['bytecodes'],2)
        self.assertEqual(result['zip_comparison']['zip_pak_mismatches'],[])
        self.assertEqual(result['zip_comparison']['zip_pak_payload_mismatches'],[])
        self.assertEqual(result['maps'][0]['initial_entities']['enemies'],1)
        self.assertEqual(result['zip_comparison']['pak_hashes_unmatched_by_zip'],[])
        with zipfile.ZipFile(zpath,'a') as z:z.writestr('unexpected.bsp',b'abcd')
        self.assertEqual(subprocess.run(cmd,capture_output=True,text=True).returncode,2)


if __name__=='__main__':
    unittest.main(verbosity=2)
