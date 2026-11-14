#!/usr/bin/env python3
"""
On-demand Effect Catalog schema and generator.
Produces the public catalog.json that the app downloads.
"""

from __future__ import annotations
import json
import hashlib
from pathlib import Path
from dataclasses import dataclass, asdict, field
from typing import List, Dict, Any, Optional


@dataclass
class ParamDef:
    name: str
    type: str = "float"
    default: float = 0.0
    min: float = 0.0
    max: float = 1.0
    label: str = ""


@dataclass
class RemoteEffect:
    base_id: str
    display_name: str
    version: str
    category: str
    description: str = ""
    author: str = "MotionStudio"
    license: str = "MIT"
    gpu_cost: str = "medium"          # low | medium | high
    min_app_version: str = "2.0.0"
    download_url: str = ""            # relative or absolute
    size_bytes: int = 0
    sha256: str = ""
    thumbnail_url: str = ""
    params: List[ParamDef] = field(default_factory=list)
    tags: List[str] = field(default_factory=list)


def make_sample_catalog() -> Dict[str, Any]:
    effects = [
        RemoteEffect(
            base_id="blur_gaussian",
            display_name="Gaussian Blur",
            version="1.0.0",
            category="Blur",
            description="High-quality separable Gaussian blur",
            download_url="effects/blur_gaussian.zip",
            size_bytes=12400,
            sha256="placeholder_sha256_blur",
            thumbnail_url="effects/blur_gaussian/thumb.png",
            params=[
                ParamDef("radius", "float", 8.0, 0.0, 64.0, "Radius"),
            ],
            tags=["blur", "basic"],
        ),
        RemoteEffect(
            base_id="glitch_rgb",
            display_name="RGB Glitch",
            version="1.2.0",
            category="Stylize",
            description="RGB channel offset glitch with block noise",
            download_url="effects/glitch_rgb.zip",
            size_bytes=18200,
            sha256="placeholder_sha256_glitch",
            thumbnail_url="effects/glitch_rgb/thumb.png",
            params=[
                ParamDef("amount", "float", 0.15, 0.0, 1.0, "Amount"),
                ParamDef("speed", "float", 1.0, 0.0, 5.0, "Speed"),
                ParamDef("block_size", "float", 8.0, 1.0, 64.0, "Block Size"),
            ],
            tags=["glitch", "stylize", "popular"],
        ),
        RemoteEffect(
            base_id="chromatic_aberration",
            display_name="Chromatic Aberration",
            version="1.0.1",
            category="Distort",
            description="Lens-style RGB fringing",
            download_url="effects/chromatic_aberration.zip",
            size_bytes=9800,
            sha256="placeholder_sha256_chromatic",
            thumbnail_url="effects/chromatic_aberration/thumb.png",
            params=[
                ParamDef("amount", "float", 0.02, 0.0, 0.1, "Amount"),
                ParamDef("center_x", "float", 0.5, 0.0, 1.0, "Center X"),
                ParamDef("center_y", "float", 0.5, 0.0, 1.0, "Center Y"),
            ],
            tags=["distort", "lens"],
        ),
        RemoteEffect(
            base_id="film_grain",
            display_name="Film Grain",
            version="1.0.0",
            category="Stylize",
            description="Organic film grain overlay",
            download_url="effects/film_grain.zip",
            size_bytes=15600,
            sha256="placeholder_sha256_grain",
            thumbnail_url="effects/film_grain/thumb.png",
            params=[
                ParamDef("intensity", "float", 0.3, 0.0, 1.0, "Intensity"),
                ParamDef("size", "float", 1.0, 0.5, 3.0, "Size"),
            ],
            tags=["film", "grain", "stylize"],
        ),
        RemoteEffect(
            base_id="vhs_tracking",
            display_name="VHS Tracking",
            version="1.0.0",
            category="Stylize",
            description="Old VHS tracking distortion",
            download_url="effects/vhs_tracking.zip",
            size_bytes=21000,
            sha256="placeholder_sha256_vhs",
            thumbnail_url="effects/vhs_tracking/thumb.png",
            params=[
                ParamDef("amount", "float", 0.4, 0.0, 1.0, "Amount"),
                ParamDef("speed", "float", 1.2, 0.0, 3.0, "Speed"),
            ],
            tags=["vhs", "retro", "stylize"],
        ),
    ]

    return {
        "version": 1,
        "repo": "motionstudio-effects",
        "base_url": "https://raw.githubusercontent.com/your-org/motionstudio-effects/main/",
        "updated_at": "2026-10-23T21:00:00Z",
        "count": len(effects),
        "effects": [asdict(e) for e in effects],
    }


def save_catalog(path: Path):
    path.parent.mkdir(parents=True, exist_ok=True)
    catalog = make_sample_catalog()
    with open(path, "w", encoding="utf-8") as f:
        json.dump(catalog, f, indent=2)
    print(f"Catalog written → {path} ({catalog['count']} effects)")


if __name__ == "__main__":
    out = Path(__file__).parent / "catalog.json"
    save_catalog(out)
