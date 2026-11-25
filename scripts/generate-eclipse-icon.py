#!/usr/bin/env python3
"""Generate Eclipse app icons from the canonical SVG geometry."""
from pathlib import Path
import sys
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
svg = root / "release-overlay/Eclipse/app/gui/icons/eclipse.svg"
out_png = root / "release-overlay/Eclipse/app/gui/icons/eclipse.png"
out_icns = Path(sys.argv[1]) if len(sys.argv) > 1 else None

# Rasterize the supplied HTML design at 4x for clean antialiased edges.
scale = 4
image = Image.new("RGBA", (512 * scale, 512 * scale), (0, 0, 0, 0))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((50*scale, 50*scale, 462*scale, 462*scale),
                       radius=92*scale, fill="#381E72")
draw.ellipse((143*scale, 143*scale, 369*scale, 369*scale), fill="#D0BCFF")
draw.ellipse((163*scale, 163*scale, 370*scale, 370*scale), fill="#381E72")
image = image.resize((512, 512), Image.Resampling.LANCZOS)
out_png.parent.mkdir(parents=True, exist_ok=True)
image.save(out_png)

if out_icns:
    iconset = out_icns.parent / "Eclipse.iconset"
    iconset.mkdir(parents=True, exist_ok=True)
    for size in (16, 32, 64, 128, 256, 512):
        image.resize((size, size), Image.Resampling.LANCZOS).save(
            iconset / f"icon_{size}x{size}.png")
        if size <= 512:
            image.resize((size*2, size*2), Image.Resampling.LANCZOS).save(
                iconset / f"icon_{size}x{size}@2x.png")
    import subprocess
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(out_icns)],
                   check=True)
    import shutil
    shutil.rmtree(iconset)
