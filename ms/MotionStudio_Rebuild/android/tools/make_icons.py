#!/usr/bin/env python3
"""Renders the Motion Studio launcher icon and adaptive-icon foreground.

The mark is the app's own logo: a lavender ring around bold "MS" on near-black
purple. Everything is generated from the bundled Inter Bold face so the icon and
the in-app mark stay identical.

Run from anywhere; paths are resolved against the android/ directory:

    python3 tools/make_icons.py
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ANDROID = Path(__file__).resolve().parent.parent
RES = ANDROID / "app/src/main/res"
FONT_PATH = RES / "font/inter_bold.ttf"

BG = (11, 6, 24, 255)  # #0B0618
RING = (185, 166, 255, 255)  # #B9A6FF

DENSITIES = {
    "mdpi": 48,
    "hdpi": 72,
    "xhdpi": 96,
    "xxhdpi": 144,
    "xxxhdpi": 192,
}


def fit_font(draw, text, max_size, target_width):
    """Largest font size whose rendered text is no wider than target_width."""
    size = max_size
    while size > 8:
        font = ImageFont.truetype(str(FONT_PATH), size)
        left, _, right, _ = draw.textbbox((0, 0), text, font=font)
        if right - left <= target_width:
            return font
        size -= 2
    return ImageFont.truetype(str(FONT_PATH), 8)


def draw_mark(img, ring_inset, ring_width, text_width_ratio, radius_ratio):
    draw = ImageDraw.Draw(img)
    w, h = img.size
    draw.rounded_rectangle(
        [ring_inset, ring_inset, w - 1 - ring_inset, h - 1 - ring_inset],
        radius=int(w * radius_ratio),
        outline=RING,
        width=ring_width,
    )
    text = "MS"
    font = fit_font(draw, text, int(w * 0.9), w * text_width_ratio)
    left, top, right, bottom = draw.textbbox((0, 0), text, font=font)
    draw.text(
        ((w - (right - left)) / 2 - left, (h - (bottom - top)) / 2 - top),
        text,
        font=font,
        fill=RING,
    )


def legacy_icon(size):
    """Full icon used below API 26 and by launchers that ignore adaptive icons."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle(
        [0, 0, size - 1, size - 1], radius=int(size * 0.22), fill=BG
    )
    draw_mark(
        img,
        ring_inset=max(1, int(size * 0.085)),
        ring_width=max(1, int(size * 0.042)),
        text_width_ratio=0.52,
        radius_ratio=0.20,
    )
    return img


def adaptive_foreground(size=432):
    """Ring plus letters only; the launcher supplies the background and mask.

    The ring stays inside the 66% safe zone so no launcher shape crops it.
    """
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw_mark(
        img,
        ring_inset=int(size * 0.166),
        ring_width=max(2, int(size * 0.042)),
        text_width_ratio=0.34,
        radius_ratio=0.22,
    )
    return img


def main():
    for density, size in DENSITIES.items():
        out = RES / f"mipmap-{density}"
        out.mkdir(parents=True, exist_ok=True)
        icon = legacy_icon(size)
        icon.save(out / "ic_launcher.png")
        icon.save(out / "ic_launcher_round.png")

    nodpi = RES / "drawable-nodpi"
    nodpi.mkdir(parents=True, exist_ok=True)
    adaptive_foreground().save(nodpi / "ic_launcher_foreground.png")

    preview = ANDROID.parents[2] / "artifacts" / "motionstudio-icon.png"
    if preview.parent.exists():
        legacy_icon(512).save(preview)

    print(f"icons written under {RES} and {preview}")


if __name__ == "__main__":
    main()
