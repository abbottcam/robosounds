"""Converts the preview PGMs to PNGs and puts all screens side by side."""
import sys
from pathlib import Path
from PIL import Image, ImageOps

out = Path(sys.argv[1])
names = ["lens", "cast", "crew", "notes", "edit"]
imgs = [Image.open(out / f"{n}.pgm").convert("L") for n in names]
for n, im in zip(names, imgs):
    im.save(out / f"{n}.png")
# Contact sheet with a dark bezel around each screen
pad, bezel = 40, 24
framed = [ImageOps.expand(im, border=bezel, fill=30) for im in imgs]
w = sum(f.width for f in framed) + pad * (len(framed) + 1)
h = framed[0].height + pad * 2
sheet = Image.new("L", (w, h), 200)
x = pad
for f in framed:
    sheet.paste(f, (x, pad))
    x += f.width + pad
sheet.save(out / "all_screens.png")
print("wrote", out / "all_screens.png")
