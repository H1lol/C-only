#!/usr/bin/env python3
"""Reference test for MotionStudio CPU gaussian/box blur (mirrors cpu_effects.cpp)."""
from __future__ import annotations
import struct
import zlib
from pathlib import Path

def write_png(path: Path, w: int, h: int, rgba: bytes) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)

def box_blur(src: bytearray, w: int, h: int, radius: int) -> bytearray:
    R = max(1, radius)
    def axis(data: bytearray, horizontal: bool) -> bytearray:
        out = bytearray(len(data))
        for y in range(h):
            for x in range(w):
                r = g = b = a = c = 0
                for k in range(-R, R + 1):
                    sx = x + k if horizontal else x
                    sy = y if horizontal else y + k
                    if sx < 0 or sy < 0 or sx >= w or sy >= h:
                        continue
                    i = (sy * w + sx) * 4
                    r += data[i]; g += data[i+1]; b += data[i+2]; a += data[i+3]; c += 1
                j = (y * w + x) * 4
                if c:
                    out[j] = r // c; out[j+1] = g // c; out[j+2] = b // c; out[j+3] = a // c
        return out
    return axis(axis(src, True), False)

def main() -> None:
    w, h = 64, 64
    src = bytearray(w * h * 4)
    # white square in center
    for y in range(16, 48):
        for x in range(16, 48):
            i = (y * w + x) * 4
            src[i:i+4] = b"\xff\xff\xff\xff"
    out = box_blur(src, w, h, 4)
    out_dir = Path(__file__).resolve().parents[3] / "examples" / "test_output"
    out_dir.mkdir(parents=True, exist_ok=True)
    write_png(out_dir / "blur_in.png", w, h, bytes(src))
    write_png(out_dir / "blur_out.png", w, h, bytes(out))
    # edge should be soft: center still bright, near edge grayish
    cx = (32 * w + 32) * 4
    edge = (16 * w + 16) * 4
    print(f"center={out[cx]} edge={out[edge]} (expect center high, edge lower)")
    assert out[cx] > out[edge]
    print("OK cpu blur test")

if __name__ == "__main__":
    main()
