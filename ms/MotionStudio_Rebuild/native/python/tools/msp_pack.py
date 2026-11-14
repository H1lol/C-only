#!/usr/bin/env python3
"""Pack / validate MotionStudio .msp plugins."""
from __future__ import annotations

import argparse
import json
import sys
import zipfile
from pathlib import Path

REQUIRED_MANIFEST_KEYS = ("msp_version", "id", "name", "version", "entry")


def load_manifest(folder: Path) -> dict:
    path = folder / "manifest.json"
    if not path.is_file():
        raise SystemExit(f"missing manifest.json in {folder}")
    data = json.loads(path.read_text(encoding="utf-8"))
    for k in REQUIRED_MANIFEST_KEYS:
        if k not in data:
            raise SystemExit(f"manifest missing key: {k}")
    if "type" not in data.get("entry", {}):
        raise SystemExit("entry.type required")
    return data


def pack(folder: Path, out: Path) -> None:
    manifest = load_manifest(folder)
    out = out.with_suffix(".msp") if out.suffix != ".msp" else out
    with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(folder.rglob("*")):
            if path.is_file():
                arc = path.relative_to(folder).as_posix()
                zf.write(path, arcname=arc)
    print(f"packed {manifest['id']} -> {out}")


def validate(msp: Path) -> None:
    if not zipfile.is_zipfile(msp):
        raise SystemExit(f"not a zip/.msp: {msp}")
    with zipfile.ZipFile(msp, "r") as zf:
        names = zf.namelist()
        if "manifest.json" not in names:
            raise SystemExit("manifest.json missing inside .msp")
        data = json.loads(zf.read("manifest.json"))
        for k in REQUIRED_MANIFEST_KEYS:
            if k not in data:
                raise SystemExit(f"manifest missing key: {k}")
        entry = data.get("entry", {})
        et = entry.get("type")
        if et == "shader":
            shader = entry.get("shader")
            if not shader or shader not in names:
                raise SystemExit(f"shader file missing: {shader}")
        elif et == "wasm":
            w = entry.get("wasm") or entry.get("path")
            if not w or w not in names:
                raise SystemExit(f"wasm file missing: {w}")
        print(f"OK  id={data['id']}  name={data['name']}  open_source={data.get('open_source')}")


def main() -> None:
    p = argparse.ArgumentParser(description="MotionStudio .msp packer")
    p.add_argument("path", type=Path, help="plugin folder or .msp file")
    p.add_argument("-o", "--output", type=Path, help="output .msp path")
    p.add_argument("--validate", action="store_true", help="validate existing .msp")
    args = p.parse_args()
    if args.validate or args.path.suffix == ".msp":
        validate(args.path)
    else:
        if not args.path.is_dir():
            raise SystemExit("path must be a plugin folder")
        out = args.output or Path(f"{args.path.name}.msp")
        pack(args.path, out)
        validate(out)


if __name__ == "__main__":
    main()
