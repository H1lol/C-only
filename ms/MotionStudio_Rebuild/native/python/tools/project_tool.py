#!/usr/bin/env python3
"""
MotionStudio Project Tool — Phase 1 Foundation
Create, inspect, and manage .msproj files.
"""

from __future__ import annotations
import json
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional


def new_id() -> str:
    return str(uuid.uuid4())


def now() -> str:
    return datetime.now(timezone.utc).isoformat()


def create_empty_project(
    name: str = "Untitled",
    width: int = 1080,
    height: int = 1920,
    fps: float = 30.0,
    duration: float = 15.0,
) -> Dict[str, Any]:
    return {
        "id": new_id(),
        "name": name,
        "version": 2,
        "frame_rate": {"num": int(fps), "den": 1},
        "width": width,
        "height": height,
        "duration": duration,
        "created_at": now(),
        "modified_at": now(),
        "media": [],
        "tracks": [
            {
                "id": new_id(),
                "name": "Video 1",
                "track_type": "Video",
                "clips": [],
                "muted": False,
                "locked": False,
                "height": 64.0,
                "visible": True,
            },
            {
                "id": new_id(),
                "name": "Overlay",
                "track_type": "Overlay",
                "clips": [],
                "muted": False,
                "locked": False,
                "height": 48.0,
                "visible": True,
            },
            {
                "id": new_id(),
                "name": "Audio 1",
                "track_type": "Audio",
                "clips": [],
                "muted": False,
                "locked": False,
                "height": 40.0,
                "visible": True,
            },
        ],
    }


def add_media(project: Dict[str, Any], name: str, path: str,
              media_type: str = "Video", width: int = 1080, height: int = 1920,
              duration: float = 10.0, fps: float = 30.0) -> str:
    mid = new_id()
    project["media"].append({
        "id": mid,
        "name": name,
        "media_type": media_type,
        "path": path,
        "width": width,
        "height": height,
        "duration": duration,
        "fps": fps,
        "has_audio": media_type == "Video",
        "proxy_path": None,
        "imported_at": now(),
    })
    project["modified_at"] = now()
    return mid


def add_clip(project: Dict[str, Any], track_name: str, media_id: str,
             name: str, start: float = 0.0, duration: float = 5.0) -> Optional[str]:
    for track in project["tracks"]:
        if track["name"] == track_name:
            cid = new_id()
            track["clips"].append({
                "id": cid,
                "media_id": media_id,
                "name": name,
                "start": start,
                "duration": duration,
                "source_in": 0.0,
                "source_out": duration,
                "speed": 1.0,
                "reversed": False,
                "effects": [],
                "transform": {
                    "position_x": 0.0, "position_y": 0.0,
                    "scale_x": 1.0, "scale_y": 1.0,
                    "rotation": 0.0,
                    "anchor_x": 0.5, "anchor_y": 0.5,
                },
                "opacity": 1.0,
                "blend_mode": "Normal",
            })
            project["modified_at"] = now()
            return cid
    return None


def save_project(project: Dict[str, Any], path: Path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(project, f, indent=2)
    print(f"Saved → {path}")


def load_project(path: Path) -> Dict[str, Any]:
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def print_info(project: Dict[str, Any]):
    print(f"Project  : {project.get('name')}")
    print(f"ID       : {project.get('id')}")
    print(f"Size     : {project.get('width')}×{project.get('height')}")
    print(f"FPS      : {project.get('frame_rate')}")
    print(f"Duration : {project.get('duration')}s")
    print(f"Media    : {len(project.get('media', []))}")
    for m in project.get("media", []):
        print(f"  • {m['name']} ({m['media_type']}) {m.get('width')}x{m.get('height')}")
    print(f"Tracks   : {len(project.get('tracks', []))}")
    for t in project.get("tracks", []):
        print(f"  • {t['name']} ({t['track_type']}) — {len(t.get('clips', []))} clips")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description="MotionStudio Project Tool (Phase 1)")
    parser.add_argument("--create", metavar="NAME", help="Create new empty project")
    parser.add_argument("--out", type=Path, default=Path("untitled.msproj"))
    parser.add_argument("--info", type=Path, help="Show info about a .msproj")
    parser.add_argument("--demo", action="store_true", help="Create a demo project with media + clips")
    args = parser.parse_args()

    if args.demo:
        proj = create_empty_project("Demo Project")
        mid = add_media(proj, "sample_clip.mp4", "/storage/emulated/0/DCIM/sample_clip.mp4")
        add_clip(proj, "Video 1", mid, "Main Clip", start=0.0, duration=8.0)
        add_clip(proj, "Overlay", mid, "Overlay Text Area", start=2.0, duration=4.0)
        save_project(proj, Path("demo_phase1.msproj"))
        print_info(proj)
    elif args.create:
        proj = create_empty_project(name=args.create)
        save_project(proj, args.out)
        print_info(proj)
    elif args.info:
        proj = load_project(args.info)
        print_info(proj)
    else:
        parser.print_help()
