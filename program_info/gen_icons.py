"""Generate Komorebi branding artwork from one shared geometry spec.

Emits the SVG (used by the Qt resource), the 256px PNG (Linux icon) and the
multi-size ICO (Windows exe icon) so all three can never drift apart.
"""
import sys
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw

APPID = "io.github.kujoushiori.Komorebi"
BINARY = "komorebi"
BG = (31, 36, 48)
TOP = (87, 217, 163)
LEFT = (47, 143, 168)
RIGHT = (38, 64, 92)
WHITE = (255, 255, 255)

CX, CY, A, B, H = 512, 420, 250, 125, 250
PTS = {
    "T": (CX, CY - B),
    "R": (CX + A, CY),
    "B": (CX, CY + B),
    "L": (CX - A, CY),
    "L2": (CX - A, CY + H),
    "B2": (CX, CY + B + H),
    "R2": (CX + A, CY + H),
}
FACES = {
    "top": (TOP, ["T", "R", "B", "L"]),
    "left": (LEFT, ["L", "B", "B2", "L2"]),
    "right": (RIGHT, ["B", "R", "R2", "B2"]),
}
CHEVRON = [(352, 332), (432, 440), (512, 352), (592, 440), (672, 332)]
CHEVRON_W = 58
S = 1024


def poly(name):
    return " ".join(f"{x},{y}" for x, y in (PTS[p] for p in FACES[name][1]))


def write_svg(out: Path):
    faces = "\n".join(
        f'  <polygon points="{poly(n)}" fill="#{"".join(f"{c:02x}" for c in FACES[n][0])}"/>'
        for n in ("left", "right", "top")
    )
    pts = " ".join(f"{x},{y}" for x, y in CHEVRON)
    svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {S} {S}" width="{S}" height="{S}">
  <!-- Komorebi mark, original work, SPDX-License-Identifier: CC0-1.0 -->
  <rect width="{S}" height="{S}" fill="#1f2430"/>
{faces}
  <clipPath id="topface"><polygon points="{poly('top')}"/></clipPath>
  <polyline points="{pts}" fill="none" stroke="#ffffff" stroke-width="{CHEVRON_W}"
            stroke-linejoin="miter" stroke-linecap="butt" clip-path="url(#topface)"/>
</svg>
'''
    out.write_text(svg, encoding="utf-8", newline="\n")


def render(size: int) -> Image.Image:
    scale = size / S
    img = Image.new("RGB", (size, size), BG)
    d = ImageDraw.Draw(img)
    for name, (color, _) in FACES.items():
        d.polygon([(x * scale, y * scale) for x, y in (PTS[p] for p in FACES[name][1])], fill=color)
    top_mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(top_mask).polygon(
        [(x * scale, y * scale) for x, y in (PTS[p] for p in FACES["top"][1])], fill=255
    )
    chevron = Image.new("L", (size, size), 0)
    ImageDraw.Draw(chevron).line(
        [(x * scale, y * scale) for x, y in CHEVRON], fill=255, width=max(1, round(CHEVRON_W * scale))
    )
    img.paste(WHITE, (0, 0), ImageChops.multiply(top_mask, chevron))
    return img


def main():
    out_dir = Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    write_svg(out_dir / f"{APPID}.svg")
    big = render(1024)
    big.resize((256, 256), Image.LANCZOS).save(out_dir / f"{APPID}_256.png")
    big.save(out_dir / f"{BINARY}.ico", format="ICO", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])
    try:
        big.save(out_dir / f"{BINARY}.icns", format="ICNS")
    except ValueError as exc:  # Pillow needs a PNG-capable encoder; harmless on Windows-only runs
        print(f"icns skipped: {exc}")
    print("\n".join(str(p) for p in sorted(out_dir.glob(f"{APPID}*")) + sorted(out_dir.glob(f"{BINARY}.*"))))


if __name__ == "__main__":
    main()
