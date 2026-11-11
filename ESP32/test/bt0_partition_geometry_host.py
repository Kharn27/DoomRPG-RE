"""BT-0 draft partition geometry: protect 4-MB flash, NVS, and raw PAK."""
from pathlib import Path
import csv

def read(path):
    parts = {}
    for row in csv.reader(Path(path).read_text().splitlines()):
        if not row or row[0].strip().startswith("#"):
            continue
        name, kind, subtype, offset, size = [s.strip() for s in row[:5]]
        parts[name] = (kind, subtype, int(offset, 0), int(size, 0))
    return parts

old = read("ESP32/partitions_cyd_raw_pak.csv")
new = read("ESP32/partitions_cyd_bt0_raw_pak.csv")
assert set(old) == set(new) == {"nvs", "otadata", "app0", "spiffs", "coredump"}
assert all(new[k] == old[k] for k in ("nvs", "otadata", "coredump"))
a = new["app0"]; p = new["spiffs"]; c = new["coredump"]
assert a[:2] == ("app", "factory")
assert p[:2] == ("data", "spiffs")
assert a[2] == 0x10000 and a[3] == 0x1A0000
assert p[2] == a[2] + a[3] and p[3] == 0x240000
assert p[2] + p[3] == c[2] == 0x3F0000
assert c[2] + c[3] == 0x400000
assert a[3] >= 1658901  # first fully linked A2DP Source ELF
assert p[3] >= 2261031  # historical Entrance staging ONLY; not all-map proof
assert a[3] % 0x10000 == 0 and p[3] % 0x1000 == 0
print("BT0 partition geometry PASS: app=%d rawPAK=%d EntranceKnownMargin=%d (ALL MAPS UNVERIFIED)"
      % (a[3], p[3], p[3] - 2261031))
