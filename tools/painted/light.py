"""The illustration light pass (Direction Update 2, the light-fantasy look).

What makes luminous fantasy illustration read as *light* rather than as a 3D render is
mostly how light is handled, not how detailed the models are:

1. Contre-jour: the sun is in the picture, behind the subject. Everything facing it gets
   a hot rim, the silhouettes glow, the fronts sit in soft, coloured (never grey) shade.
   In 3D the key light now comes *from the painted sun* (render.py, `backlight`); in 2D a
   warm rim is added to every edge that faces the sun.
2. Light wrap: the bright sky bleeds over the edges of everything in front of it, so the
   layers sit *in* the air instead of being cut out and pasted on.
3. Crepuscular rays: shafts of light fan out from the sun wherever the sky shows between
   the towers, trees and cliffs (a radial blur of the unoccluded sky), over everything.
4. Bloom and an Orton glow: highlights spill a soft halo; a blurred, screened copy of the
   image gives the dreamy, watercolour-like glow of the genre without losing the detail.
5. Colour: warm cream-gold highlights, lavender-rose and teal shadows, pastels lifted
   (vibrance, not saturation), a gentle warm vignette that keeps the eye in the light.

All of it runs on the painted layers in display space (after paint.py's tone map), with
the same screen-space maths on every layer, so the plug-in's parallax still lines up.
"""
import numpy as np
from scipy import ndimage

VW, VH = 960.0, 560.0
GOLD = np.array([1.0, 0.82, 0.52], np.float32)
WARM = np.array([1.0, 0.9, 0.72], np.float32)


def blur(x, sigma):
    if sigma <= 0.3:
        return x
    if x.ndim == 3:
        return ndimage.gaussian_filter(x, (sigma, sigma, 0))
    return ndimage.gaussian_filter(x, sigma)


def lum(rgb):
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def unpre(img):
    a = img[..., 3:4]
    return np.where(a > 1e-4, img[..., :3] / np.maximum(a, 1e-4), 0.0), img[..., 3]


def pre(rgb, a):
    return np.dstack([rgb * a[..., None], a])


def over(dst, src):
    return src + dst * (1 - src[..., 3:4])


def resize(img, h, w):
    if img.shape[:2] == (h, w):
        return img
    zy, zx = h / img.shape[0], w / img.shape[1]
    if img.ndim == 3:
        return np.dstack([ndimage.zoom(img[..., c], (zy, zx), order=1)[:h, :w] for c in range(img.shape[2])])
    return ndimage.zoom(img, (zy, zx), order=1)[:h, :w]


def radial_blur(src, cx, cy, length=0.55, steps=28):
    """A zoom blur towards (cx, cy) (pixels): light streaming out from the sun."""
    h, w = src.shape[:2]
    acc = np.zeros_like(src)
    for k in range(steps):
        s = 1.0 - length * k / steps            # sample nearer the sun
        # out(p) = src(c + (p - c) * s)
        mat = np.diag([s, s])
        off = np.array([cy, cx]) * (1 - s)
        if src.ndim == 3:
            for c in range(src.shape[2]):
                acc[..., c] += ndimage.affine_transform(src[..., c], mat, offset=off, order=1, mode="constant")
        else:
            acc += ndimage.affine_transform(src, mat, offset=off, order=1, mode="constant")
    return acc / steps


def vibrance(rgb, amount):
    """More colour where there is little (pastels bloom), little where there is much."""
    l = lum(rgb)[..., None]
    sat = (rgb.max(-1) - rgb.min(-1))[..., None]
    return np.clip(l + (rgb - l) * (1 + amount * (1 - sat) ** 2), 0, 1)


def grade(rgb, shade=(0.62, 0.55, 0.8), light=(1.0, 0.9, 0.72)):
    """Coloured shadows and warm highlights (split toning) and lifted pastels."""
    l = lum(rgb)[..., None]
    s = np.clip(1 - l / 0.55, 0, 1) ** 1.5
    hl = np.clip((l - 0.6) / 0.4, 0, 1)
    out = rgb * (1 - 0.16 * s) + np.array(shade, np.float32) * l * 1.15 * 0.16 * s
    out = out * (1 - 0.12 * hl) + np.array(light, np.float32) * 0.12 * hl
    return vibrance(np.clip(out, 0, 1), 0.45)


def contrast(rgb, amount, pivot=0.5):
    """An S-curve around a pivot: the genre is luminous but never murky or flat."""
    x = np.clip(rgb, 0, 1)
    s = x * x * (3 - 2 * x)
    return np.clip(x + (s - x) * amount * 2, 0, 1)


def orton(rgb, sigma, amount):
    """A blurred copy screened over the image: the dreamy glow of the genre."""
    soft = blur(rgb, sigma)
    soft = np.clip((soft - 0.25) / 0.75, 0, 1)
    return 1 - (1 - rgb) * (1 - soft * amount)


def vignette(h, w, strength=0.14):
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    r2 = ((x / w - 0.5) / 0.62) ** 2 + ((y / h - 0.45) / 0.62) ** 2
    v = 1 - strength * np.clip(r2, 0, 1.6) ** 1.4
    return v


def light_pass(out, meta, scale, interior=False):
    """Applies the light to every layer of a scene in place: `out` maps layer names to
    premultiplied RGBA images in display space ('sky' opaque; 'fog' may be half size)."""
    sky = out["sky"]
    h, w = sky.shape[:2]
    p = meta.get("sky", {})
    mx, my, _ = p.get("moon", [480, 80, 60])
    sunx, suny = mx * w / VW, my * h / VH
    rays_amount = float(p.get("rays", 0.5))
    order = [n for n in ("far", "mid", "fog", "near") if n in out]
    full = {n: resize(out[n], h, w) for n in order}
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    dsun = np.hypot(xx - sunx, yy - suny) / w            # distance to the sun, widths
    towards = np.stack([sunx - xx, suny - yy], -1)
    towards /= np.linalg.norm(towards, axis=-1, keepdims=True) + 1e-6
    near_sun = np.exp(-dsun / 0.55)                        # the sun's side of the picture

    # 1. Rays: the sky's brightness where nothing hides it, streaming out from the sun.
    occl = np.zeros((h, w), np.float32)
    for n in order:
        if n != "fog":
            occl = 1 - (1 - occl) * (1 - full[n][..., 3])
    rays = np.zeros((h, w, 3), np.float32)
    if not interior and rays_amount > 0:
        q = 4                                               # at a quarter size: soft anyway
        src_rgb = sky[..., :3] * (1 - occl)[..., None]
        bright = np.clip(lum(src_rgb) - 0.62, 0, None)[..., None] * src_rgb
        small = bright[::q, ::q]
        streaks = radial_blur(small, suny / q, sunx / q, length=0.7, steps=24)
        rays = resize(streaks, h, w) * GOLD * (1.1 * min(1.0, rays_amount + 0.35))
        rays *= (0.35 + 0.65 * near_sun)[..., None]

    # 2. Light wrap and the sun-facing rim, back to front.
    bg = sky.copy()
    for n in order:
        if n == "fog":
            bg = over(bg, full[n])
            continue
        L = full[n]
        rgb, a = unpre(L)
        soft_a = blur(a, 5 * scale)
        band = np.clip(a - soft_a, 0, 1) * 2.2                     # just inside the outline
        thin = np.clip(a - blur(a, 1.6 * scale), 0, 1) * 3.0
        bgl = blur(bg[..., :3], 16 * scale)
        wrap = bgl * (np.clip(lum(bgl) - 0.45, 0, 1) * 1.6)[..., None] * band[..., None] * 0.55
        gy, gx = np.gradient(blur(a, 2.0 * scale))
        nrm = -np.stack([gx, gy], -1)
        nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True) + 1e-6
        facing = np.clip((nrm * towards).sum(-1), 0, 1) ** 1.5
        rim_k = 0.0 if interior else 0.85
        rim = GOLD * (thin * facing * (0.35 + 0.65 * near_sun) * rim_k)[..., None]
        rgb = np.clip(rgb + wrap + rim, 0, 1.4)
        full[n] = pre(rgb, a)
        bg = over(bg, full[n])

    # 3. Air: the rays over everything; bloom; the Orton glow; colour; the vignette.
    vig = vignette(h, w)
    for n in ["sky"] + order:
        img = sky if n == "sky" else full[n]
        rgb, a = unpre(img)
        air = 1.0 if n == "sky" else {"far": 0.8, "mid": 0.55, "fog": 0.6, "near": 0.3}[n]
        rgb = rgb + rays * air
        hot = np.clip(rgb - 0.82, 0, None)
        k = 0.35 if n == "sky" else 1.0                      # the sky has its own glow already
        glow = (blur(hot, 10 * scale) * 0.9 + blur(hot, 30 * scale) * 0.6) * k
        rgb = rgb + glow * WARM
        rgb = orton(np.clip(rgb, 0, 1), 7 * scale, 0.22 if n != "near" else 0.16)
        if n in ("mid", "near") and not interior:
            # Repoussoir: the closest ground (the bottom of the picture) a little deeper and
            # cooler, so the light lives in the distance and the sky.
            fg = (np.clip((yy / h - 0.6) / 0.4, 0, 1) ** 1.3)[..., None]
            rgb = rgb * (1 - 0.2 * fg) + np.array([0.18, 0.26, 0.34], np.float32) * 0.2 * fg * lum(rgb)[..., None] * 2
        rgb = contrast(rgb, 0.28 if n != "sky" else 0.12)
        rgb = grade(rgb)
        rgb = rgb * vig[..., None] + (1 - vig[..., None]) * np.array([0.35, 0.22, 0.25], np.float32) * 0.35
        if n != "sky":
            # Glow spills a little past the outline (its own alpha) so silhouettes shine.
            a = np.clip(np.maximum(a, lum(blur(hot, 10 * scale)) * 0.8), 0, 1)
        res = pre(np.clip(rgb, 0, 1), a)
        if n == "sky":
            out["sky"] = res
        else:
            out[n] = resize(res, *out[n].shape[:2]) if out[n].shape[:2] != (h, w) else res
    return out
