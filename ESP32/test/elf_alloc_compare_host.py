"""Audit comparator tests: ignored debug != ignored gameplay data."""
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location("elf_compare", Path(__file__).resolve().parents[1] /
                                            "tools/compare_elf_alloc.py")
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


def image(text=b"abcd", debug=b"old", address=0x400d0000, bss_size=16, app=b"descriptor"):
    names = b"\0.text\0.bss\0.debug\0.shstrtab\0.appdesc\0"
    payload = text + debug + app + names
    shoff = 52 + len(payload)
    ident = b"\x7fELF\x01\x01\x01" + b"\0" * 9
    header = struct.pack("<16sHHIIIIIHHHHHH", ident, 2, 94, 1, address, 0,
                         shoff, 0, 52, 0, 0, 40, 6, 4)
    def section(name, kind, flags, addr, offset, size):
        return struct.pack("<IIIIIIIIII", names.index(name), kind, flags, addr,
                           offset, size, 0, 0, 4, 0)
    table = (b"\0" * 40 + section(b".text", 1, 6, address, 52, len(text)) +
             section(b".bss", 8, 3, 0x3ffc0000, 0, bss_size) +
             section(b".debug", 1, 0, 0, 52 + len(text), len(debug)) +
             section(b".shstrtab", 3, 0, 0, 52 + len(text) + len(debug) + len(app), len(names)) +
             section(b".appdesc", 1, 2, 0x3f400020, 52 + len(text) + len(debug), len(app)))
    return header + payload + table


class ElfComparisonTest(unittest.TestCase):
    def test_debug_only_change_accepted(self):
        self.assertEqual(len(tool.compare(image(), image(debug=b"new-long-debug"))), 3)

    def test_text_data_address_and_bss_changes_rejected(self):
        for changed in (image(text=b"abce"), image(address=0x400d0004),
                        image(bss_size=20), image(app=b"other-desc")):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                tool.compare(image(), changed)

    def test_invalid_or_truncated_elf_rejected(self):
        for data in (b"not an ELF", image()[:-10], b"\0" * 52):
            with self.subTest(data=data), self.assertRaises(ValueError):
                tool.allocated_sections(data)


if __name__ == "__main__":
    unittest.main()
