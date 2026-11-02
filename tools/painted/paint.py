"""Painterly post-process for the rendered layers (numpy + scipy; OIDN via ctypes).

    /opt/bvenv/bin/python tools/painted/paint.py <render dir> <asset dir> [--preview file.jpg]

Per layer: denoise (Intel Open Image Denoise, bundled with Blender), atmospheric haze
by depth layer, bloom on the emissive lights, tone map and colour grade (near-black,
slate and purple mists, crimson kept intense), a generalised Kuwahara (oil-paint)
filter, brush streaks along the image's structure (line-integral convolution), grain
and speckle. The sky and moon layer is painted here from the scene's parameters.

Writes <asset dir>/sky.jpg, <layer>.jpg + <layer>.png (colour + alpha mask) for the
other layers, and scene.json (anchors, layer list, moon) for the manifest.
"""
import argparse
import ctypes
import glob
import json
import os

import numpy as np
import OpenEXR
from PIL import Image
from scipy import ndimage, signal

VW, VH = 960.0, 560.0

# ------------------------------------------------------------------------- OIDN
_lib = None


def _oidn():
    global _lib
    if _lib is None:
        import site
        roots = site.getsitepackages() + [os.path.dirname(__import__("bpy").__file__)]
        path = [p for r in roots for p in glob.glob(os.path.join(r, "bpy", "lib", "libOpenImageDenoise.so.2"))]
        if not path:
            path = glob.glob("/opt/bvenv/lib/python3*/site-packages/bpy/lib/libOpenImageDenoise.so.2")
        lib = ctypes.CDLL(path[0])
        lib.oidnNewDevice.restype = ctypes.c_void_p
        lib.oidnNewDevice.argtypes = [ctypes.c_int]
        lib.oidnCommitDevice.argtypes = [ctypes.c_void_p]
        lib.oidnNewFilter.restype = ctypes.c_void_p
        lib.oidnNewFilter.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        lib.oidnSetSharedFilterImage.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int] + [ctypes.c_size_t] * 5
        lib.oidnSetFilterBool.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_bool]
        lib.oidnCommitFilter.argtypes = [ctypes.c_void_p]
        lib.oidnExecuteFilter.argtypes = [ctypes.c_void_p]
        lib.oidnReleaseFilter.argtypes = [ctypes.c_void_p]
        lib.oidnReleaseDevice.argtypes = [ctypes.c_void_p]
        _lib = lib
    return _lib


def denoise(rgb, hdr=True):
    lib = _oidn()
    h, w, _ = rgb.shape
    c = np.ascontiguousarray(rgb[..., :3], dtype=np.float32)
    out = np.zeros_like(c)
    dev = lib.oidnNewDevice(0)
    lib.oidnCommitDevice(dev)
    f = lib.oidnNewFilter(dev, b"RT")
    lib.oidnSetSharedFilterImage(f, b"color", c.ctypes.data, 3, w, h, 0, 0, 0)
    lib.oidnSetSharedFilterImage(f, b"output", out.ctypes.data, 3, w, h, 0, 0, 0)
    lib.oidnSetFilterBool(f, b"hdr", hdr)
    lib.oidnCommitFilter(f)
    lib.oidnExecuteFilter(f)
    lib.oidnReleaseFilter(f)
    lib.oidnReleaseDevice(dev)
    return out


# ------------------------------------------------------------------------ helpers
def load_exr(path):
    with OpenEXR.File(path, separate_channels=True) as f:
        ch = f.channels()
        get = lambda c: ch["ViewLayer.Combined." + c].pixels.astype(np.float32)
        return np.dstack([get("R"), get("G"), get("B"), get("A")])


# Aerial perspective inside a layer (Direction Update 2: soft atmospheric depth): what
# lies towards the horizon melts a little more into the air than what is close (below
# it on screen); things standing above the horizon count as distant. (Blender 5 writes
# only the Combined pass here, so the horizon line stands in for depth.)
DEPTH_HAZE = {"far": 0.10, "mid": 0.12, "near": 0.04}


def depth_haze(img, horizon, amount, hc):
    if amount <= 0 or horizon is None:
        return img
    h, w = img.shape[:2]
    rgb, a = unpremultiply(img)
    _, sy = screen_xy(h, w)
    t = np.clip(1.0 - (sy - horizon) / max(40.0, VH - horizon), 0, 1)
    k = (amount * t ** 1.6)[..., None]
    rgb = rgb * (1 - k) + hc * k
    return np.dstack([rgb * a[..., None], a])


def fbm(h, w, cells_y, cells_x, octaves, seed, persistence=0.5):
    """Smooth fractal value noise in [0, 1] (cubic upsampled random grids)."""
    rng = np.random.default_rng(seed)
    out = np.zeros((h, w), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        gy, gx = max(2, int(cells_y * 2 ** o)), max(2, int(cells_x * 2 ** o))
        grid = rng.random((gy + 3, gx + 3)).astype(np.float32)
        up = ndimage.zoom(grid, ((h + 3 * h / gy) / (gy + 3), (w + 3 * w / gx) / (gx + 3)), order=3)
        out += amp * up[:h, :w]
        total += amp
        amp *= persistence
    out /= total
    return (out - out.min()) / max(1e-6, out.max() - out.min())


def lum(rgb):
    return rgb[..., 0] * 0.2126 + rgb[..., 1] * 0.7152 + rgb[..., 2] * 0.0722


def unpremultiply(img):
    a = img[..., 3:4]
    rgb = np.where(a > 1e-4, img[..., :3] / np.maximum(a, 1e-4), 0.0)
    return rgb, img[..., 3]


def screen_xy(h, w):
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    return x * (VW / w), y * (VH / h)


# ------------------------------------------------------------------ tone + grade
def tonemap(rgb, exposure=1.0, white=6.0):
    """A soft filmic curve for daylight: highlights roll off into cream, the shadows are
    lifted (never black), mid-tones stay luminous."""
    x = rgb * exposure
    l = lum(x)[..., None]
    lt = l * (1 + l / (white * white)) / (1 + l)
    out = x * (lt / np.maximum(l, 1e-6))
    out = np.clip(out, 0, None) ** (1 / 2.2)
    # Lift the floor: the darkest shade is a soft lavender-grey, not black.
    return np.clip(0.035 + 0.97 * out, 0, 1)


def grade(rgb):
    """High-key romantic daylight: warm cream highlights, lavender-blue shadows, the
    pastels (blush, lavender, mint, sky) kept and gently enriched, greens sunlit."""
    L = lum(rgb)[..., None]
    grey = np.repeat(L, 3, axis=2)
    out = grey + (rgb - grey) * 1.3                        # richer colour
    out = np.clip(out, 0, 1)
    out = out * out * (3 - 2 * out) * 0.35 + out * 0.65        # a gentle S-curve: more contrast
    shadow = np.clip(1 - L / 0.5, 0, 1)
    light = np.clip((L - 0.55) / 0.45, 0, 1)
    out = out * (1 - 0.05 * shadow) + np.array([0.45, 0.42, 0.6], np.float32) * shadow * 0.05   # lavender shadows
    out = out * (1 - 0.14 * light) + np.array([1.0, 0.92, 0.75], np.float32) * light * 0.14     # warm cream highlights
    return np.clip(out, 0, 1)


# ------------------------------------------------------------------ the painting
def kuwahara(img, radius, sectors=8, q=8.0):
    """Generalised Kuwahara (Papari et al.): eight overlapping sectors, weighted by the
    inverse of their luminance variance. Works on premultiplied RGBA."""
    if radius < 1:
        return img
    size = 2 * radius + 1
    yy, xx = np.mgrid[-radius:radius + 1, -radius:radius + 1].astype(np.float32)
    ang = np.arctan2(yy, xx)
    dist = np.sqrt(xx * xx + yy * yy)
    gauss = np.exp(-(dist ** 2) / (2 * (radius * 0.6) ** 2)) * (dist <= radius + 0.5)
    L = lum(img[..., :3])
    num = np.zeros_like(img)
    den = np.zeros(img.shape[:2], np.float32)
    for k in range(sectors):
        a0 = -np.pi + 2 * np.pi * k / sectors
        d = np.angle(np.exp(1j * (ang - (a0 + np.pi / sectors))))
        w = gauss * np.exp(-(d ** 2) / (2 * (np.pi / sectors) ** 2))
        w[radius, radius] = gauss[radius, radius]
        w /= w.sum()
        conv = lambda ch: signal.oaconvolve(ch, w, mode="same")
        mean = np.dstack([conv(img[..., c]) for c in range(img.shape[2])])
        m1 = conv(L)
        m2 = conv(L * L)
        var = np.maximum(m2 - m1 * m1, 0)
        wk = 1.0 / (1.0 + (var * 255.0) ** (q / 2) * 1e-2)
        num += mean * wk[..., None]
        den += wk
    return num / np.maximum(den, 1e-8)[..., None]


def structure_angle(L, sigma=3.0):
    gx = ndimage.sobel(L, 1)
    gy = ndimage.sobel(L, 0)
    jxx = ndimage.gaussian_filter(gx * gx, sigma)
    jyy = ndimage.gaussian_filter(gy * gy, sigma)
    jxy = ndimage.gaussian_filter(gx * gy, sigma)
    # Tangent = direction of least change.
    theta = 0.5 * np.arctan2(2 * jxy, jxx - jyy) + np.pi / 2
    coherence = np.sqrt((jxx - jyy) ** 2 + 4 * jxy ** 2) / (jxx + jyy + 1e-6)
    return theta, coherence


def brush_texture(L, length, seed, scale, horizontal=False):
    """Streaks of noise smeared along the structure (line-integral convolution): the
    traces of brush strokes. Where the image has no structure the strokes follow a
    slowly varying default direction."""
    h, w = L.shape
    rng = np.random.default_rng(seed)
    base = rng.random((h, w)).astype(np.float32)
    base = ndimage.gaussian_filter(base, 0.7 * scale)
    theta, coh = structure_angle(ndimage.gaussian_filter(L, 1.5 * scale), 4.0 * scale)
    wander = (fbm(h, w, 2, 3, 2, seed + 1) - 0.5) * 0.7 + 0.12
    theta = np.where(coh > 0.45, theta, wander)
    if horizontal:
        # Long sweeping strokes, mostly horizontal, turning with the clouds and mist.
        theta = (fbm(h, w, 3, 4, 2, seed + 4) - 0.5) * 1.0 + 0.5 * np.where(coh > 0.5, np.sin(2 * theta) * 0.3, 0)
    dx, dy = np.cos(theta), np.sin(theta)
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    acc = np.zeros_like(base)
    steps = max(3, int(length))
    for s in range(-steps, steps + 1):
        acc += ndimage.map_coordinates(base, [y + dy * s, x + dx * s], order=1, mode="reflect")
    acc /= 2 * steps + 1
    acc = (acc - acc.mean()) / (acc.std() + 1e-6)
    return acc


def dabs(img, seed, scale):
    """Soft, slightly uneven paint dabs: low-frequency value and warmth variation."""
    h, w = img.shape[:2]
    v = fbm(h, w, 18, 30, 3, seed + 21) - 0.5
    warm = fbm(h, w, 10, 16, 2, seed + 22) - 0.5
    out = img.copy()
    out[..., :3] *= (1 + 0.16 * v)[..., None]
    out[..., 0] *= 1 + 0.06 * warm
    out[..., 2] *= 1 - 0.06 * warm
    return out


def painterly(img, radius, stroke, seed, scale, streak=0.015, sharpen=0.0, horizontal=False):
    """img: premultiplied RGBA in display space."""
    out = kuwahara(dabs(img, seed, scale), radius)
    if stroke > 0:
        rgb, a = unpremultiply(out)
        tex = brush_texture(lum(rgb), stroke, seed, scale, horizontal)
        rgb = rgb * (1 + (streak * 0.6 if horizontal else streak) * tex[..., None])
        out = np.dstack([rgb * a[..., None], a])
    if sharpen > 0:
        blur = ndimage.gaussian_filter(out, (1.2 * scale, 1.2 * scale, 0))
        out = out + sharpen * (out - blur)
    return np.clip(out, 0, 1)


def grain(img, seed, amount=0.018, speckle=0.0012, scale=1.0):
    h, w = img.shape[:2]
    rng = np.random.default_rng(seed + 99)
    n = rng.normal(0, amount, (h, w)).astype(np.float32)
    n = ndimage.gaussian_filter(n, 0.5 * scale) * 1.6
    a = img[..., 3]
    out = img.copy()
    for c in range(3):
        out[..., c] += n * a
    if speckle > 0:
        dots = rng.random((h, w)) < speckle
        size = ndimage.binary_dilation(dots, iterations=max(1, int(round(scale)))) if scale > 1.2 else dots
        val = rng.random((h, w)).astype(np.float32)
        light = size & (val > 0.55)
        dark = size & (val < 0.35)
        for c in range(3):
            out[..., c] = np.where(light, out[..., c] + 0.07 * a, out[..., c])
            out[..., c] = np.where(dark, out[..., c] * 0.55, out[..., c])
    return np.clip(out, 0, 1)


# --------------------------------------------------------------------------- sky
def paint_sky(h, w, p, seed):
    """The daylight sky (linear light): clear blue above fading to cream and blush at the
    horizon, a radiant gold sun with a wide glow, soft cumulus lit from the sun's side
    with lavender undersides, faint god rays. (`moon` holds the sun's place.)"""
    sx, sy = screen_xy(h, w)
    horizon = p.get("horizon", 300)
    t = np.clip(sy / max(horizon, 1), 0, 1.4)
    zenith = np.array([0.10, 0.30, 0.85], np.float32)
    hor = np.array([0.85, 0.86, 0.9], np.float32)
    out = zenith + (hor - zenith) * (np.clip(t, 0, 1)[..., None] ** 2.6)
    mx, my, mr = p.get("moon", [480, 80, 60])
    d = np.sqrt((sx - mx) ** 2 + (sy - my) ** 2)
    glow = np.exp(-d / 260.0) * 0.55 + np.exp(-d / 70.0) * 0.6
    out = out + np.array([1.0, 0.85, 0.55], np.float32) * glow[..., None]
    # Cumulus: puffy masses (thresholded fbm), lit on the sun's side.
    cl = p.get("clouds", 0.7)
    if cl > 0:
        n1 = fbm(h, w, 5, 3, 5, seed)
        n2 = fbm(h, w, 12, 7, 4, seed + 7)
        mass = n1 * 0.7 + n2 * 0.3
        cover = np.clip((mass - (0.66 - 0.2 * cl)) * 4.5, 0, 1)
        cover *= np.clip(1.15 - sy / (horizon * 1.05), 0, 1)
        # Light from the sun's side: compare with the mass shifted towards the sun.
        dx, dy = (mx - w * VW / w / 2) / VW, (my - horizon) / VH
        shift = ndimage.shift(mass, (-dy * 0.03 * h, -dx * 0.03 * w), order=1, mode="nearest")
        lit = np.clip(0.55 + (mass - shift) * 9.0, 0.25, 1.15)
        cloud = np.array([1.0, 0.97, 0.93], np.float32) * lit[..., None] + np.array([0.62, 0.6, 0.82], np.float32) * (1 - np.clip(lit, 0, 1))[..., None] * 0.8
        cloud = cloud + np.array([1.0, 0.8, 0.5], np.float32) * (np.exp(-d / 220.0) * 0.5)[..., None]
        # Silver linings (Direction Update 2): the edges of each cloud that face the sun
        # glow gold, brightest near it; warm tops, cool lavender bases.
        soft = ndimage.gaussian_filter(cover, h * 0.006)
        inner = np.clip(cover - ndimage.gaussian_filter(cover, h * 0.012), 0, 1) * 3.0
        gy, gx = np.gradient(soft)
        nlen = np.hypot(gx, gy) + 1e-6
        tx, ty = (mx - sx), (my - sy)
        tlen = np.hypot(tx, ty) + 1e-6
        facing = np.clip(-(gx * tx + gy * ty) / (nlen * tlen), 0, 1)
        silver = inner * facing * (0.4 + 1.6 * np.exp(-d / 380.0))
        cloud = cloud + np.array([1.0, 0.86, 0.6], np.float32) * silver[..., None]
        base = np.clip(gy / (nlen + 1e-3), -1, 1)                 # density growing downwards: a top edge
        cloud = cloud * (1 + 0.08 * base[..., None]) + np.array([0.55, 0.52, 0.78], np.float32) * np.clip(-base, 0, 1)[..., None] * 0.08
        out = out * (1 - cover[..., None]) + cloud * cover[..., None]
    # The sun: a bright disc with a soft rim.
    disc = np.clip((mr * 0.55 - d) / 2.0 + 0.5, 0, 1)
    out = out * (1 - disc[..., None]) + np.array([2.4, 2.1, 1.5], np.float32) * disc[..., None]
    # God rays: soft radial streaks from the sun.
    rays = p.get("rays", 0.5)
    if rays > 0:
        ang = np.arctan2(sy - my, sx - mx)
        streak = fbm(1, 720, 1, 48, 3, seed + 21)[0]
        idx = ((ang + np.pi) / (2 * np.pi) * 719).astype(int) % 720
        out = out + np.array([1.0, 0.9, 0.7], np.float32) * (streak[idx] ** 3 * np.exp(-d / 500.0) * 0.35 * rays)[..., None]
    return np.dstack([out, np.ones((h, w), np.float32)])


# -------------------------------------------------------------------- per layer
LAYER_STYLE = {
    #        haze  kuwahara stroke sharpen grain speckle   (detail pass: detail first)
    "sky":  (0.00, 4, 6, 0.0, 0.010, 0.0),
    "far":  (0.10, 2, 4, 0.2, 0.008, 0.0),
    "mid":  (0.03, 1, 2, 0.6, 0.006, 0.0),
    "near": (0.00, 1, 2, 0.6, 0.006, 0.0),
    "fog":  (0.00, 3, 6, 0.0, 0.006, 0.0),
}


def haze_colour(h, w, meta):
    """Aerial perspective: a pale blue-cream air, golden towards the sun."""
    sx, sy = screen_xy(h, w)
    col = np.broadcast_to(np.array([0.72, 0.8, 0.95], np.float32), (h, w, 3)).copy()
    sun = meta.get("sky", {}).get("moon")
    if sun:
        d = np.sqrt((sx - sun[0]) ** 2 + (sy - sun[1]) ** 2)
        col += np.array([0.35, 0.25, 0.08], np.float32) * np.exp(-d / 320.0)[..., None]
    return col


def bloom(img, scale):
    """Glow around emissive light (linear, premultiplied)."""
    bright = np.maximum(img[..., :3] - 1.2, 0)
    glow = np.zeros_like(bright)
    for sigma, wgt in ((3, 0.25), (10, 0.15), (28, 0.1)):
        glow += ndimage.gaussian_filter(bright, (sigma * scale, sigma * scale, 0)) * wgt
    out = img.copy()
    out[..., :3] += glow
    out[..., 3] = np.clip(np.maximum(out[..., 3], lum(glow) * 1.5), 0, 1)
    return out


def process_layer(name, img, meta, seed, scale):
    haze, rad, stroke, sharp, gr, sp = LAYER_STYLE[name]
    h, w = img.shape[:2]
    if name != "sky":
        rgb = denoise(img[..., :3], hdr=True)
        a = denoise(np.repeat(img[..., 3:4], 3, axis=2), hdr=False)[..., 0]
        a = np.clip(a, 0, 1)
        img = np.dstack([np.clip(rgb, 0, None), a])
    amount = meta.get("haze", {}).get("amount", 0.7)
    if name in DEPTH_HAZE:
        img = depth_haze(img, meta.get("sky", {}).get("horizon"), DEPTH_HAZE[name] * min(1.0, amount / 0.5 + 0.3), haze_colour(h, w, meta))
    if haze > 0:
        rgb, a = unpremultiply(img)
        hc = haze_colour(h, w, meta)
        k = haze * amount
        rgb = rgb * (1 - k) + hc * k
        img = np.dstack([rgb * a[..., None], a])
    img = bloom(img, scale)
    rgb, a = unpremultiply(img)
    exposure = meta.get("exposure", 1.0)
    rgb = grade(tonemap(rgb, exposure))
    if name == "fog":
        # Mist: a soft bright veil.
        a = np.clip(a * meta.get("fog_opacity", 0.6), 0, 1)
    img = np.dstack([rgb * a[..., None], a])
    img = painterly(img, max(1, int(round(rad * scale))), stroke * scale, seed, scale, sharpen=sharp, horizontal=name in ("fog", "sky"))
    if name in ("sky", "far"):
        img = ndimage.gaussian_filter(img, (0.8 * scale, 0.8 * scale, 0))
    img = grain(img, seed, gr, sp, scale)
    return img


def save_layer(img, path_base, opaque=False, quality=88):
    rgb, a = unpremultiply(img) if not opaque else (img[..., :3], img[..., 3])
    if not opaque:
        # Bleed the colour outwards under transparent pixels so JPEG blocks and
        # resampling never bring in black fringes.
        known = a > 0.02
        filled = rgb.copy()
        if known.any():
            idx = ndimage.distance_transform_edt(~known, return_distances=False, return_indices=True)
            filled = rgb[idx[0], idx[1]]
        rgb = np.where(known[..., None], rgb, filled)
        Image.fromarray((np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8), "L").save(path_base + ".png", optimize=True)
    Image.fromarray((np.clip(rgb, 0, 1) * 255 + 0.5).astype(np.uint8), "RGB").save(path_base + ".jpg", quality=quality, optimize=True, progressive=False, subsampling=0)


def over(dst, src):
    return src + dst * (1 - src[..., 3:4])


# Scenes indoors: no sun in the picture (no rays or sun rim; the rest of the light pass).
INTERIORS = {"hall-of-mirrors", "throne-of-light", "prism-hall", "diamond-vault", "choir-loft", "nave-of-light", "stained-glass-chapel", "sanctum"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("render")
    ap.add_argument("assets")
    ap.add_argument("--preview", default="")
    ap.add_argument("--width", type=int, default=0, help="output width (default: the render's)")
    ap.add_argument("--no-light", action="store_true", help="skip the illustration light pass (comparisons)")
    args = ap.parse_args()
    meta = json.load(open(os.path.join(args.render, "anchors.json")))
    os.makedirs(args.assets, exist_ok=True)
    seed = abs(hash(meta["scene"])) % 10000
    layers = {}
    size = None
    for name in meta["rendered"]:
        img = load_exr(os.path.join(args.render, name + ".exr"))
        layers[name] = img
        if name not in ("fog", "far"):
            size = img.shape[:2]
    if size is None:
        size = (int(round(layers["far"].shape[0] / 0.7)), int(round(layers["far"].shape[1] / 0.7))) if "far" in layers else (1120, 1920)
    h, w = size
    if "far" in layers and layers["far"].shape[:2] != (h, w):
        # Rendered at 70 % (render.py): back to full size before painting.
        f = layers["far"]
        layers["far"] = np.dstack([ndimage.zoom(f[..., c], (h / f.shape[0], w / f.shape[1]), order=3)[:h, :w] for c in range(4)])
        layers["far"][..., 3] = np.clip(layers["far"][..., 3], 0, 1)
    scale = w / 1920.0
    sky_params = meta.get("sky", {})
    out = {}
    out["sky"] = process_layer("sky", paint_sky(h, w, sky_params, sky_params.get("seed", 1) + seed), meta, seed, scale)
    for name, img in layers.items():
        s = scale * (0.5 if name == "fog" else 1.0)
        out[name] = process_layer(name, img, meta, seed + len(name), s)
    # The illustration light (light.py): rays, light wrap, sun rim, glow, colour, vignette.
    if not args.no_light:
        import light
        light.light_pass(out, meta, scale, interior=meta["scene"].split("/")[-1] in INTERIORS)
        if meta.get("air"):
            # The map's air (final touches): perspective, haze, cloud banks, a mist layer.
            import map_air
            map_air.air_pass(out, meta, scale)
            if "fog" in out and "fog" not in layers:
                layers["fog"] = None
    save_layer(out["sky"], os.path.join(args.assets, "sky"), opaque=True)
    for name in layers:
        save_layer(out[name], os.path.join(args.assets, name), quality=86 if name != "fog" else 80)
    info = {k: meta[k] for k in ("scene", "anchors") if k in meta}
    info["layers"] = ["sky"] + [l for l in ("far", "mid", "fog", "near") if l in layers]
    info["moon"] = sky_params.get("moon", [0, 0, 0])
    info["glows"] = sky_params.get("glows", [])
    json.dump(info, open(os.path.join(args.assets, "scene.json"), "w"), indent=1)
    if args.preview:
        comp = out["sky"]
        for name in ("far", "mid", "fog", "near"):
            if name in out:
                layer = out[name]
                if layer.shape[:2] != comp.shape[:2]:
                    layer = np.dstack([ndimage.zoom(layer[..., c], (comp.shape[0] / layer.shape[0], comp.shape[1] / layer.shape[1]), order=1) for c in range(4)])
                comp = over(comp, np.clip(layer, 0, 1))
        Image.fromarray((np.clip(comp[..., :3], 0, 1) * 255).astype(np.uint8)).save(args.preview, quality=90)


if __name__ == "__main__":
    main()
