"""Generate KomorebiLauncher branding artwork from one shared geometry spec.

Emits the SVG (used by the Qt resource), the 256px PNG (Linux icon), the multi-size
ICO (Windows exe icon) and the ICNS (macOS bundle) so they can never drift apart.

The mark is komorebilauncher: sunlight coming through leaves - a warm disc crossed by three
leaves, on the dark background the launcher's chrome uses.
"""
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw

APPID = "io.github.komorebilauncher.KomorebiLauncher"
BINARY = "komorebilauncher"

S = 1024
BG = (31, 36, 48)
SUN = (242, 184, 75)
LEAF_MID = (46, 118, 84)
LEAF_LIGHT = (87, 217, 163)
LEAF_DARK = (28, 70, 53)
RAY = (242, 184, 75, 64)

SUN_CENTER = (470, 340)
SUN_RADIUS = 232

# (cx, cy, length, width, degrees, colour) - hand placed so the leaves read as a canopy
LEAVES = [
    (606, 214, 470, 150, -38, LEAF_MID),
    (392, 566, 520, 168, 18, LEAF_DARK),
    (712, 470, 430, 140, 42, LEAF_LIGHT),
]

RAYS = [
    ((470, 340), (860, 250), 16),
    ((470, 340), (890, 470), 12),
    ((470, 340), (700, 700), 10),
]


def leaf_polygon(cx, cy, length, width, degrees, steps=28):
    """A lens shape: two mirrored curves meeting at the tips."""
    rad = math.radians(degrees)
    ux, uy = math.cos(rad), math.sin(rad)
    vx, vy = -uy, ux
    upper, lower = [], []
    for i in range(steps + 1):
        t = i / steps
        along = (t - 0.5) * length
        half = (width / 2) * math.sin(math.pi * t)
        bx, by = cx + ux * along, cy + uy * along
        upper.append((bx + vx * half, by + vy * half))
        lower.append((bx - vx * half, by - vy * half))
    return upper + list(reversed(lower))


def hex_of(colour):
    return "#" + "".join(f"{c:02x}" for c in colour[:3])


def write_svg(path: Path, size=S, background=True, viewbox=None, mask_radius=0.0):
    """Write the mark as SVG. viewBox lets a 12.7pt asset catalogue reuse the geometry."""
    vx, vy, vw, vh = viewbox if viewbox else (0, 0, size, size)
    scale = vw / size

    def sx(value):
        return f"{value * scale:.3f}".rstrip("0").rstrip(".")

    parts = []
    if background:
        if mask_radius:
            parts.append(
                f'  <rect width="{vx + vw}" height="{vy + vh}" rx="{sx(mask_radius)}" fill="{hex_of(BG)}"/>'
            )
        else:
            parts.append(f'  <rect width="{vx + vw}" height="{vy + vh}" fill="{hex_of(BG)}"/>')
    parts.append(
        f'  <circle cx="{sx(SUN_CENTER[0])}" cy="{sx(SUN_CENTER[1])}" r="{sx(SUN_RADIUS)}" fill="{hex_of(SUN)}"/>'
    )
    for start, end, width in RAYS:
        parts.append(
            f'  <line x1="{sx(start[0])}" y1="{sx(start[1])}" x2="{sx(end[0])}" y2="{sx(end[1])}" '
            f'stroke="{hex_of(SUN)}" stroke-opacity="0.25" stroke-width="{sx(width)}" stroke-linecap="round"/>'
        )
    for leave in LEAVES:
        points = " ".join(f"{sx(x)},{sx(y)}" for x, y in leaf_polygon(*leave[:5]))
        parts.append(f'  <polygon points="{points}" fill="{hex_of(leave[5])}"/>')

    body = "\n".join(parts)
    svg = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vx} {vy} {vw} {vh}" '
        f'width="{vw}" height="{vh}">\n'
        "  <!-- KomorebiLauncher mark, original work, SPDX-License-Identifier: CC0-1.0 -->\n"
        f"{body}\n</svg>\n"
    )
    path.write_text(svg, encoding="utf-8", newline="\n")


def render(size: int) -> Image.Image:
    scale = size / S
    img = Image.new("RGB", (size, size), BG)
    d = ImageDraw.Draw(img)

    sun = [((SUN_CENTER[0] - SUN_RADIUS) * scale, (SUN_CENTER[1] - SUN_RADIUS) * scale),
           ((SUN_CENTER[0] + SUN_RADIUS) * scale, (SUN_CENTER[1] + SUN_RADIUS) * scale)]
    d.ellipse(sun, fill=SUN)

    rays = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    rd = ImageDraw.Draw(rays)
    for start, end, width in RAYS:
        rd.line(
            [(start[0] * scale, start[1] * scale), (end[0] * scale, end[1] * scale)],
            fill=RAY,
            width=max(1, round(width * scale)),
        )
    img = Image.alpha_composite(img.convert("RGBA"), rays).convert("RGB")

    d = ImageDraw.Draw(img)
    for leave in LEAVES:
        d.polygon([(x * scale, y * scale) for x, y in leaf_polygon(*leave[:5])], fill=leave[5])
    return img


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    out_dir = Path(args[0] if args else ".")
    out_dir.mkdir(parents=True, exist_ok=True)

    write_svg(out_dir / f"{APPID}.svg")

    big = render(S)
    big.resize((256, 256), Image.LANCZOS).save(out_dir / f"{APPID}_256.png")
    big.save(out_dir / f"{BINARY}.ico", format="ICO", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    try:
        big.save(out_dir / f"{BINARY}.icns", format="ICNS")
    except ValueError as exc:  # Pillow needs a PNG-capable encoder; harmless on Windows-only runs
        print(f"icns skipped: {exc}")

    # The same mark, without the plate, for the instance list icon and the macOS asset catalogue.
    for flag, target in (("--instance", "instance"), ("--asset", "asset")):
        for i, arg in enumerate(sys.argv):
            if arg == flag and i + 1 < len(sys.argv):
                dest = Path(sys.argv[i + 1])
                if target == "instance":
                    write_svg(dest, background=True, mask_radius=180)
                else:
                    write_svg(dest, background=True, viewbox=(0, 0, 12.7, 12.7))
                print(f"wrote {dest}")

    print("\n".join(str(p) for p in sorted(out_dir.glob(f"{APPID}*")) + sorted(out_dir.glob(f"{BINARY}.*"))))


if __name__ == "__main__":
    main()
