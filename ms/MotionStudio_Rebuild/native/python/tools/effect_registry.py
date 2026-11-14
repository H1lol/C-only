#!/usr/bin/env python3
"""
MotionStudio Effect Registry Tool

Reads / generates the effect catalog that both C++ and Flutter use.
Can also validate GLSL shaders and produce parameter metadata.
"""

from __future__ import annotations
import json
import os
from pathlib import Path
from dataclasses import dataclass, asdict, field
from typing import List, Dict, Any, Optional

ROOT = Path(__file__).resolve().parents[3]  # MotionStudio_Rebuild
SHADER_DIR = ROOT / "native" / "cpp" / "effects" / "shaders"  # future
CATALOG_PATH = ROOT / "native" / "python" / "effects" / "vfx_catalog.json"


@dataclass
class ParamDef:
    name: str
    type: str = "float"          # float | int | bool | color | vec2 | enum
    default: float = 0.0
    min: float = 0.0
    max: float = 1.0
    label: str = ""
    group: str = "Main"


@dataclass
class EffectDef:
    base_id: str
    display_name: str
    category: str                # Color, Blur, Distort, Stylize, Transition, Generate...
    description: str = ""
    params: List[ParamDef] = field(default_factory=list)
    shader_vert: str = ""
    shader_frag: str = ""
    gpu_cost: str = "medium"     # low | medium | high
    tags: List[str] = field(default_factory=list)


# ------------------------------------------------------------------
# Built-in starter catalog (will grow to 171+)
# ------------------------------------------------------------------

STARTER_EFFECTS: List[EffectDef] = [
    EffectDef(
        base_id="brightness_contrast",
        display_name="Brightness & Contrast",
        category="Color",
        description="Adjust brightness and contrast",
        params=[
            ParamDef("brightness", "float", 0.0, -1.0, 1.0, "Brightness"),
            ParamDef("contrast",   "float", 1.0,  0.0, 3.0, "Contrast"),
        ],
        tags=["color", "basic"],
    ),
    EffectDef(
        base_id="saturation",
        display_name="Saturation",
        category="Color",
        params=[
            ParamDef("amount", "float", 1.0, 0.0, 3.0, "Saturation"),
        ],
        tags=["color"],
    ),
    EffectDef(
        base_id="blur",
        display_name="Gaussian Blur",
        category="Blur",
        params=[
            ParamDef("radius", "float", 0.0, 0.0, 64.0, "Radius"),
        ],
        gpu_cost="high",
        tags=["blur"],
    ),
    EffectDef(
        base_id="chromatic",
        display_name="Chromatic Aberration",
        category="Distort",
        params=[
            ParamDef("amount", "float", 0.0, 0.0, 0.05, "Amount"),
            ParamDef("center_x", "float", 0.5, 0.0, 1.0, "Center X"),
            ParamDef("center_y", "float", 0.5, 0.0, 1.0, "Center Y"),
        ],
        tags=["distort", "stylize"],
    ),
    EffectDef(
        base_id="vignette",
        display_name="Vignette",
        category="Stylize",
        params=[
            ParamDef("intensity", "float", 0.5, 0.0, 1.0, "Intensity"),
            ParamDef("radius",    "float", 0.7, 0.0, 1.5, "Radius"),
            ParamDef("softness",  "float", 0.3, 0.0, 1.0, "Softness"),
        ],
        tags=["stylize"],
    ),
    EffectDef(
        base_id="glitch_rgb",
        display_name="RGB Glitch",
        category="Stylize",
        params=[
            ParamDef("amount",   "float", 0.1, 0.0, 1.0, "Amount"),
            ParamDef("speed",    "float", 1.0, 0.0, 5.0, "Speed"),
            ParamDef("block_size","float", 8.0, 1.0, 64.0, "Block Size"),
        ],
        tags=["glitch", "stylize"],
    ),
    EffectDef(
        base_id="fade",
        display_name="Fade",
        category="Transition",
        params=[
            ParamDef("progress", "float", 0.0, 0.0, 1.0, "Progress"),
        ],
        tags=["transition"],
    ),
]


def build_catalog() -> Dict[str, Any]:
    effects = []
    for e in STARTER_EFFECTS:
        d = asdict(e)
        effects.append(d)

    catalog = {
        "version": 2,
        "generator": "effect_registry.py",
        "count": len(effects),
        "effects": effects,
    }
    return catalog


def save_catalog(path: Path = CATALOG_PATH):
    path.parent.mkdir(parents=True, exist_ok=True)
    catalog = build_catalog()
    with open(path, "w", encoding="utf-8") as f:
        json.dump(catalog, f, indent=2)
    print(f"Wrote {catalog['count']} effects → {path}")


def load_catalog(path: Path = CATALOG_PATH) -> Dict[str, Any]:
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def print_summary():
    cat = build_catalog()
    print(f"MotionStudio Effect Catalog — {cat['count']} effects\n")
    by_cat: Dict[str, List[str]] = {}
    for e in cat["effects"]:
        by_cat.setdefault(e["category"], []).append(e["display_name"])

    for cat_name, names in sorted(by_cat.items()):
        print(f"  {cat_name}:")
        for n in names:
            print(f"    • {n}")
        print()


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="MotionStudio Effect Registry")
    parser.add_argument("--save", action="store_true", help="Write vfx_catalog.json")
    parser.add_argument("--summary", action="store_true", help="Print summary")
    args = parser.parse_args()

    if args.save:
        save_catalog()
    if args.summary or not any(vars(args).values()):
        print_summary()
