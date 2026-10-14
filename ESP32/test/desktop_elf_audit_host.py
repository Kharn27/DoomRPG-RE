"""Reject discarded/debug/non-ALLOC map attribution and local-name collisions."""
import importlib.util
from pathlib import Path
import json
import tempfile
import unittest
from unittest.mock import patch
from elf_alloc_compare_host import image

spec = importlib.util.spec_from_file_location("desktop_audit", Path(__file__).resolve().parents[1] /
                                            "tools/audit_desktop_elf.py")
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


class DesktopAuditTest(unittest.TestCase):
    def test_stale_object_is_not_a_compiled_unit(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory(prefix="doomrpg-elf-admission-") as tmp:
            build = Path(tmp)
            (build / "firmware.elf").write_bytes(image())
            (build / "firmware.map").write_text("Linker script and memory map\n")
            (build / "doomrpg_engine_patched").mkdir()
            (build / "doomrpg_engine_patched/Render.o").write_bytes(b"stale object")
            db = build / "compiledb.json"
            db.write_text(json.dumps([{"directory": str(root), "file": "ESP32/src/main.cpp"}]))
            with patch.object(tool, "symbols", return_value={}) as nm:
                report = tool.audit(build, root, tool.NM, db)
                nm.assert_called_once()  # Final ELF only; stale object not inspected.
            self.assertFalse(report["Render"]["admitted"])
            self.assertTrue(report["Render"]["object_present"])
            self.assertEqual(report["Render"]["defined_symbols"], 0)
            db.write_text("[]")
            with patch.object(tool, "symbols", return_value={}), self.assertRaises(ValueError):
                tool.audit(build, root, tool.NM, db)

    def test_loaded_ranges_not_discarded_or_debug(self):
        allocated = {".flash.text": (1, 6, 0x400d0000, 64, 4, b"x" * 64),
                     ".dram0.bss": (8, 3, 0x3ffc0000, 16, 4, None)}
        obj = ".pio/build/esp32-cyd/doomrpg_engine_patched/Render.o"
        text = "\n".join((
            "Discarded input sections",
            " .text.dead 0x00000000 0x20 " + obj,
            # Even a coincident nonzero row BEFORE the loaded map is rejected.
            " .text.false 0x400d0000 0x10 " + obj,
            "Linker script and memory map",
            " .text.live",
            "                0x400d0000 0x10 " + obj,
            " .literal.live 0x400d0010 0x4 " + obj,
            " .text.overrun 0x400d0030 0x20 " + obj,
            " .debug_info 0x00000000 0x100 " + obj,
            " .debug_other 0x00008000 0x10 " + obj,
            " .text.empty 0x400d0020 0x0 " + obj,
            " COMMON 0x3ffc0000 0x8 " + obj,
            " .text.unrelated 0x400d0020 0x10 .pio/build/esp32-cyd/src/other.o"))
        rows = tool.map_sections(text, allocated)["Render"]
        loaded = [r for r in rows if r["loaded"]]
        self.assertEqual([r["section"] for r in loaded], [".text.live", ".literal.live", "COMMON"])
        self.assertEqual(sum(r["size"] for r in loaded), 28)

    def test_same_name_different_owner_is_not_linkage(self):
        loaded = [{"address": 0x1000, "size": 16}]
        wrong = (0x2000, 4, "b")
        right = (0x1008, 4, "b")
        self.assertIsNone(tool.owned_symbol([wrong], loaded))
        self.assertEqual(tool.owned_symbol([wrong, right], loaded), right)
        self.assertIsNone(tool.owned_symbol([(0x1010, 4, "b")], loaded))

    def test_ambiguous_named_owner_rejected(self):
        with self.assertRaises(ValueError):
            tool.owned_symbol([(4, 4, "b"), (8, 4, "b")], [{"address": 0, "size": 16}])


if __name__ == "__main__":
    unittest.main()
