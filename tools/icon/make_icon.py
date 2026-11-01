"""Draws LIGHT SEEKER's icon (1024 px): a radiant gold sun with rays over a pearl sky and
a white spire. Original artwork, generated. Usage: python3 tools/icon/make_icon.py out.png"""
import math, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

S = 1024
y, x = np.mgrid[0:S, 0:S].astype(np.float32) / S
# Pearl-to-sky gradient with a warm glow in the centre.
sky = np.stack([0.62 + 0.36 * y, 0.80 + 0.17 * y, 0.95 + 0.03 * y], -1)
r = np.hypot(x - 0.5, y - 0.44)
glow = np.exp(-(r / 0.28) ** 2)[..., None]
img = sky * (1 - 0.6 * glow) + np.array([1.0, 0.93, 0.72]) * 0.6 * glow
# Rays.
ang = np.arctan2(y - 0.44, x - 0.5)
rays = (0.5 + 0.5 * np.cos(ang * 16)) ** 6 * np.exp(-r / 0.45)
img = img + rays[..., None] * np.array([0.25, 0.2, 0.08])
im = Image.fromarray((np.clip(img, 0, 1) * 255).astype(np.uint8), "RGB")
d = ImageDraw.Draw(im)
# The sun disc, gold.
c, rad = (S * 0.5, S * 0.44), S * 0.16
d.ellipse([c[0] - rad, c[1] - rad, c[0] + rad, c[1] + rad], fill=(244, 206, 110))
d.ellipse([c[0] - rad * 0.8, c[1] - rad * 0.8, c[0] + rad * 0.8, c[1] + rad * 0.8], fill=(252, 232, 160))
# A white spire in front of the sun, and a cloud bank.
d.polygon([(S * 0.44, S * 0.86), (S * 0.56, S * 0.86), (S * 0.5, S * 0.30)], fill=(250, 246, 236))
d.polygon([(S * 0.36, S * 0.86), (S * 0.43, S * 0.86), (S * 0.395, S * 0.52)], fill=(240, 234, 222))
d.polygon([(S * 0.57, S * 0.86), (S * 0.64, S * 0.86), (S * 0.605, S * 0.52)], fill=(240, 234, 222))
clouds = Image.new("L", (S, S), 0)
cd = ImageDraw.Draw(clouds)
for k in range(9):
    cx = S * (0.05 + 0.11 * k)
    cd.ellipse([cx - S * 0.12, S * 0.78, cx + S * 0.12, S * 0.95], fill=255)
clouds = clouds.filter(ImageFilter.GaussianBlur(S * 0.02))
im = Image.composite(Image.new("RGB", (S, S), (255, 252, 246)), im, clouds)
# Rounded-square mask (macOS style).
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.22), fill=255)
out = Image.new("RGBA", (S, S), (0, 0, 0, 0))
out.paste(im, (0, 0), mask)
out.save(sys.argv[1])
