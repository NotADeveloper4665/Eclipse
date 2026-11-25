#!/usr/bin/env python3
"""Rasterize the canonical Eclipse SVG and build the macOS ICNS asset."""
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
svg_path = root / "release-overlay/Eclipse/app/gui/icons/eclipse.svg"
png_path = root / "release-overlay/Eclipse/app/gui/icons/eclipse.png"
icon = ET.parse(svg_path).getroot()
view = icon.attrib.get("viewBox", "0 0 512 512").split()
width, height = int(float(view[2])), int(float(view[3]))
scale = 4
image = Image.new("RGBA", (width * scale, height * scale), (0, 0, 0, 0))
draw = ImageDraw.Draw(image)

for shape in icon:
    tag = shape.tag.rsplit("}", 1)[-1]
    fill = shape.attrib.get("fill", "#000000")
    if tag == "rect":
        x, y = float(shape.attrib["x"]), float(shape.attrib["y"])
        w, h = float(shape.attrib["width"]), float(shape.attrib["height"])
        radius = float(shape.attrib.get("rx", "0"))
        draw.rounded_rectangle((round(x*scale), round(y*scale),
                                round((x+w)*scale), round((y+h)*scale)),
                               radius=round(radius*scale), fill=fill)
    elif tag == "circle":
        cx, cy, radius = (float(shape.attrib[k]) for k in ("cx", "cy", "r"))
        draw.ellipse((round((cx-radius)*scale), round((cy-radius)*scale),
                      round((cx+radius)*scale), round((cy+radius)*scale)),
                     fill=fill)

image = image.resize((width, height), Image.Resampling.LANCZOS)
png_path.parent.mkdir(parents=True, exist_ok=True)
image.save(png_path)

if len(sys.argv) > 1:
    icns_path = Path(sys.argv[1])
    iconset = icns_path.parent / "Eclipse.iconset"
    iconset.mkdir(parents=True, exist_ok=True)
    for size in (16, 32, 64, 128, 256, 512):
        image.resize((size, size), Image.Resampling.LANCZOS).save(
            iconset / f"icon_{size}x{size}.png")
        image.resize((size*2, size*2), Image.Resampling.LANCZOS).save(
            iconset / f"icon_{size}x{size}@2x.png")
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(icns_path)],
                   check=True)
    import shutil
    shutil.rmtree(iconset)
