#!/usr/bin/env python3
"""R-A2b native CYD full-frame FNV capture and strict metadata-only comparison.
Serial hashes are real hardware witnesses, NOT original-desktop pixel parity.
Neither this tool nor its golden JSON embeds original assets or framebuffer bytes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
GOLDEN_SCHEMA = "doomrpg-ra2b-cyd-frames-v1"
WITNESS = re.compile(r"\[RA2B\] FRAME\s+(.*)")
READY = re.compile(r"\[ENGINESESSION\] READY\s+map=(\d+)")
FIELDS = (
    "v", "seq", "map", "x", "y", "angle", "full", "viewport",
    "world", "hud", "walls", "wallPixels", "planes", "sprites",
    "glows", "worldUs", "spriteUs", "hudUs", "presentUs", "totalUs",
    "reads", "bytes", "rangeHit", "rangeMiss", "backing", "sdRangeUs",
    "flashReadUs",
)
HASHES = ("full", "viewport", "world", "hud")
TIMES = ("worldUs", "spriteUs", "hudUs", "presentUs", "totalUs",
         "sdRangeUs", "flashReadUs")


class CaptureError(ValueError):
    pass


def require(value: bool, message: str) -> None:
    if not value:
        raise CaptureError(message)


def decode_frame(raw: str, lineno: int) -> dict:
    tokens = raw.split()
    require(len(tokens) == len(FIELDS), f"line {lineno}: missing/extra frame fields")
    pairs = {}
    for token in tokens:
        require(token.count("=") == 1, f"line {lineno}: malformed field")
        k, v = token.split("=", 1)
        require(k in FIELDS and k not in pairs and v,
                f"line {lineno}: unknown/duplicate/empty field {k}")
        pairs[k] = v
    require(set(pairs) == set(FIELDS), f"line {lineno}: incomplete frame")
    parsed = {}
    for key in FIELDS:
        value = pairs[key]
        if key == "backing":
            require(value in ("sd", "flash"), "unknown PAK backing")
            parsed[key] = value
        else:
            try:
                parsed[key] = int(value, 16 if key in HASHES else 10)
            except ValueError as exc:
                raise CaptureError(f"line {lineno}: invalid {key}={value}") from exc
    require(parsed["v"] == 1 and 1 <= parsed["seq"] <= 48,
            f"line {lineno}: unsupported probe schema/counter")
    require(1 <= parsed["map"] <= 13 and parsed["angle"] in (0, 64, 128, 192),
            f"line {lineno}: invalid map/cardinal view")
    require(all(0 < parsed[x] < 2**32 for x in HASHES),
            f"line {lineno}: missing/overflowed frame signature")
    require(all(0 <= parsed[x] <= 10_000_000 for x in TIMES),
            f"line {lineno}: improbable frame/read duration")
    require(parsed["reads"] <= parsed["bytes"] <= 4 * 1024 * 1024,
            f"line {lineno}: impossible physical range accounting")
    require(parsed["totalUs"] >= max(parsed[x] for x in TIMES if x != "totalUs"),
            f"line {lineno}: component time > total time")
    require(parsed["backing"] != "flash" or parsed["sdRangeUs"] == 0,
            f"line {lineno}: raw-flash frame reported resident SD range reads")
    require(parsed["backing"] != "sd" or parsed["flashReadUs"] == 0,
            f"line {lineno}: SD frame reported partition reads")
    return parsed


def parse_log(text: str, minimum_poses: int = 3) -> dict:
    session = 0
    active_map = None
    prior_seq = 0
    frames: list[dict] = []
    seen = set()
    ignored_priming = 0
    for lineno, line in enumerate(text.splitlines(), 1):
        match = READY.search(line)
        if match:
            session += 1
            active_map = int(match.group(1))
            continue
        match = WITNESS.search(line)
        if not match:
            continue
        frame = decode_frame(match.group(1), lineno)
        require(frame["seq"] > prior_seq,
                f"line {lineno}: repeated/reordered RA2B seq")
        prior_seq = frame["seq"]
        if active_map is None:
            ignored_priming += 1
            continue
        require(frame["map"] == active_map,
                f"line {lineno}: FRAME map != engine READY map")
        pose = (session, frame["map"], frame["x"],
                frame["y"], frame["angle"])
        if pose not in seen:
            seen.add(pose)
            frames.append({"session": session, **frame})
    require(session > 0, "no real ENGINESESSION READY marker")
    require(len(frames) >= minimum_poses,
            f"insufficient unique settled frames: {len(frames)} < {minimum_poses}")
    return {
        "schema": GOLDEN_SCHEMA,
        "frames": frames,
        "uniquePoses": len(frames),
        "ignoredPriming": ignored_priming,
        "note": "CYD native signatures only; NOT original desktop full-frame pixel parity",
    }


def compare_golden(golden: dict, observed: dict) -> None:
    require(golden.get("schema") == GOLDEN_SCHEMA and
            observed.get("schema") == GOLDEN_SCHEMA,
            "incompatible golden schema")
    require(golden.get("assetSha256") == observed.get("assetSha256"),
            "cannot compare different or unverified PAK assets")
    expected = golden.get("frames", [])
    actual = observed.get("frames", [])
    require(len(expected) >= 3 and len(expected) == len(actual),
            "different capture lengths; comparison must not silently skip poses")
    for index, (a, b) in enumerate(zip(expected, actual), 1):
        for key in ("map", "x", "y", "angle", *HASHES,
                    "walls", "wallPixels", "planes", "sprites", "glows"):
            require(a[key] == b[key],
                    f"pose {index} {key}: golden={a[key]} observed={b[key]}")
    # Timings and cache effectiveness are diagnostic, not pixel golden.


def file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def self_test() -> None:
    def frame(seq: int, x: int, angle: int, full: str = "12345678",
              backing: str = "flash") -> str:
        fields = dict(v="1", seq=str(seq), map="1", x=str(x), y="128",
                      angle=str(angle), full=full, viewport="22334455",
                      world="33445566", hud="44556677", walls="6",
                      wallPixels="1400", planes="700", sprites="30",
                      glows="0", worldUs="22000", spriteUs="12000",
                      hudUs="2000", presentUs="50", totalUs="37000",
                      reads="14", bytes="4064", rangeHit="12",
                      rangeMiss="9", backing=backing, sdRangeUs="0",
                      flashReadUs="7500" if backing == "flash" else "0")
        if backing == "sd":
            fields["sdRangeUs"] = "7500"
        return "[RA2B] FRAME " + " ".join(f"{k}={fields[k]}" for k in FIELDS)

    priming = frame(1, 64, 64) + "\n"
    ready = "[ENGINESESSION] READY map=1 angle=64 residentCache=yes\n"
    testlog = priming + ready + "\n".join((frame(2, 64, 64),
                   frame(3, 64, 64), frame(4, 64, 128),
                   frame(5, 128, 128, backing="sd")))
    golden = parse_log(testlog)
    require(golden["uniquePoses"] == 3 and golden["ignoredPriming"] == 1,
            "priming/unique pose gating incorrect")
    golden["assetSha256"] = "a" * 64
    compare_golden(golden, json.loads(json.dumps(golden)))
    different = json.loads(json.dumps(golden))
    different["frames"][1]["full"] ^= 1
    try:
        compare_golden(golden, different)
        raise AssertionError("pixel mismatch accepted")
    except CaptureError as error:
        require("pose 2 full" in str(error), "unexpected mismatch error")
    different = json.loads(json.dumps(golden))
    different["frames"][1]["totalUs"] = 50000
    compare_golden(golden, different)
    for bad in (testlog.replace("seq=5", "seq=4"),
                testlog.replace("backing=sd sdRangeUs=7500",
                                "backing=flash sdRangeUs=7500"),
                testlog.replace("[ENGINESESSION] READY", "[NOT_READY]"),
                priming + ready + frame(2, 64, 64)):
        try:
            parse_log(bad)
            raise AssertionError("invalid capture accepted")
        except CaptureError:
            pass
    print("PASS R-A2b host capture: priming excluded, 3 unique poses, "
          "strict FNV golden, timings non-golden, invalid/short traces fail-closed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--log", type=Path)
    parser.add_argument("--pak", type=Path,
                        help="private original PAK, SHA256 checked (never exported)")
    parser.add_argument("--out", type=Path, help="write metadata-only golden JSON")
    parser.add_argument("--compare", type=Path,
                        help="compare against an already captured golden JSON")
    args = parser.parse_args()
    try:
        if args.self_test:
            self_test()
            return 0
        require(args.log is not None and args.pak is not None,
                "real capture requires --log and --pak")
        ref_file = HERE / "ra2_original_asset_fingerprints.json"
        original = json.loads(ref_file.read_text())
        expected_sha = original["pak"]["sha256"]
        actual_sha = file_sha256(args.pak)
        require(actual_sha == expected_sha, "real PAK SHA256 differs from R-A2a")
        captured = parse_log(args.log.read_text(errors="replace"))
        captured["assetSha256"] = actual_sha
        captured["logSha256"] = file_sha256(args.log)
        if args.compare is not None:
            compare_golden(json.loads(args.compare.read_text()), captured)
            print("PASS R-A2b golden: exact matched poses + full/viewport/HUD hashes")
        if args.out is not None:
            require(args.out.resolve() != args.pak.resolve() and
                    args.out.resolve() != args.log.resolve(),
                    "refuse to overwrite original assets or Serial")
            args.out.write_text(json.dumps(captured, indent=2, sort_keys=True) + "\n")
        print(f"PASS R-A2b capture uniquePoses={captured['uniquePoses']} "
              f"ignoredPriming={captured['ignoredPriming']} "
              "native-only; desktop-frame parity=pending; hardware Serial=required")
        return 0
    except (OSError, ValueError, KeyError, AssertionError) as exc:
        print("[RA2B] FAIL", exc)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
