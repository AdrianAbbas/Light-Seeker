"""The map's air (final touches): the world map was the most "3D" image of all, high above
and with little air. This pass gives it the same light language as the re-lit scenes,
in 2D on the painted layers, after the light pass (light.py), with restraint so the
landmarks and their labels stay readable:

1. Atmospheric perspective: the land paler, cooler and softer with distance (a depth
   proxy from the horizon line: a ground plane seen from above), warmer towards the sun.
2. Layered haze: the far ranges hazed, more at their foot (low-lying haze) than at their
   summits, so each range stands in front of the next.
3. Cloud banks along the horizon behind the peaks (in the sky layer): warm cream-gold
   tops, lavender undersides, a silver-gold lining towards the sun; and stratus collars
   clinging to the great range's flanks.
4. Mist (a new fog layer, half resolution): a band of haze along the horizon at the foot
   of the foothills, low wisps in the valleys of the middle distance (never over a
   landmark), and mist over the river and the sea.

The plug-in drifts more mist along the horizon, cloud shadows over the land and light
motes on top (PluginEditor.cpp, the map's life).
"""
import numpy as np
from scipy import ndimage

import light

VW, VH = 960.0, 560.0


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def fbm(h, w, cell_x, cell_y, seed, octaves=4):
    """Smooth value noise in [0, 1] (cells in pixels, anisotropic), a few octaves."""
    rng = np.random.default_rng(seed)
    out = np.zeros((h, w), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        cx, cy = max(2.0, cell_x / 2 ** o), max(2.0, cell_y / 2 ** o)
        gh, gw = int(h / cy) + 3, int(w / cx) + 3
        g = rng.random((gh, gw)).astype(np.float32)
        z = ndimage.zoom(g, (cy, cx), order=3)[:h, :w]
        if z.shape != (h, w):
            z = np.pad(z, ((0, h - z.shape[0]), (0, w - z.shape[1])), mode="edge")
        out += amp * z
        total += amp
        amp *= 0.5
    return np.clip(out / total, 0, 1)


def unpre(img):
    return light.unpre(img)


def pre(rgb, a):
    return light.pre(rgb, a)


def air_pass(out, meta, scale):
    sky = out["sky"]
    h, w = sky.shape[:2]
    sp = meta.get("sky", {})
    hz_v = float(sp.get("horizon", 84))
    sun_v = sp.get("moon", [480, 40, 60])
    k = h / VH
    hz = hz_v * k
    sunx, suny = sun_v[0] * w / VW, sun_v[1] * k
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    vy = yy / k
    dsun = np.hypot(xx - sunx, yy - suny) / w
    sunw = np.exp(-dsun / 0.32)[..., None]
    cool = np.array([0.79, 0.83, 0.95], np.float32)
    warm = np.array([1.0, 0.93, 0.80], np.float32)
    haze_c = cool * (1 - sunw) + warm * sunw
    anchors = [(a["x"], a["y"]) for a in meta.get("anchors", []) if a.get("kind") == "landmark"]
    # Keep-clear: landmarks are never fogged over.
    clear = np.ones((h, w), np.float32)
    for ax, ay in anchors:
        d = np.hypot(xx - ax * w / VW, yy - ay * k) / (48 * k)
        clear *= 1 - 0.85 * np.exp(-d * d)

    # 1. Atmospheric perspective on the land (mid): a ground plane seen from above, the
    # distance growing as 1 / (y - horizon).
    dy = np.maximum(yy - hz, 2 * k)
    D = (h - hz) / dy
    aerial = 1 - np.exp(-0.06 * (D - 1))
    aerial = np.where(yy <= hz + 2 * k, 0.8, aerial).astype(np.float32)
    rgb, a = unpre(out["mid"])
    soft = light.blur(rgb, 1.3 * scale)
    t = aerial[..., None]
    rgb = rgb * (1 - 0.55 * t) + soft * 0.55 * t                    # softer with distance
    l = light.lum(rgb)[..., None]
    rgb = rgb * (1 - 0.25 * t) + l * 0.25 * t                        # less saturated
    rgb = rgb * (1 - 0.46 * t) + haze_c * 0.46 * t                   # paler, cooler (warm by the sun)
    out["mid"] = pre(np.clip(rgb, 0, 1), a)

    # 2. The far ranges: hazed, more at their foot than at their summits.
    if "far" in out:
        rgb, a = unpre(out["far"])
        foot = smoothstep(30.0, hz_v + 4, vy)[..., None]
        t = 0.1 + 0.32 * foot
        rgb = light.blur(rgb, 0.9 * scale) * 0.5 + rgb * 0.5
        rgb = rgb * (1 - t) + haze_c * t
        # Stratus collars on the great range's flanks: a band under each column's skyline.
        top = np.argmax(a > 0.5, axis=0).astype(np.float32)          # first row of rock
        has = (a > 0.5).any(axis=0)
        top = np.where(has, top, h).astype(np.float32)
        top = ndimage.gaussian_filter1d(top, 14 * scale)
        below = (yy - top[None, :]) / k                               # virtual px under the skyline
        band = np.exp(-((below - 24) / 7.5) ** 2)
        wisps = fbm(h, w, 140 * scale, 10 * scale, 11)
        collar = np.clip(band * (wisps - 0.38) * 2.4, 0, 1) * 0.55 * a
        lit = np.clip(1 - (below - 18) / 14, 0, 1)[..., None]         # tops lit, undersides shaded
        ccol = np.array([0.80, 0.78, 0.93], np.float32) * (1 - lit) + np.array([1.0, 0.96, 0.88], np.float32) * lit
        ccol = ccol * (1 - 0.35 * sunw) + warm * 0.35 * sunw
        rgb = rgb * (1 - collar[..., None]) + ccol * collar[..., None]
        out["far"] = pre(np.clip(rgb, 0, 1), a)

    # 3. Cloud banks along the horizon, behind the peaks (the sky layer, opaque).
    srgb = sky[..., :3].copy()
    base_v = hz_v - 6 + 10 * (fbm(1, w, 260 * scale, 1, 5)[0] - 0.5)       # the banks' flat base
    height_v = 20 + 34 * fbm(1, w, 180 * scale, 1, 6)[0]
    u = (base_v[None, :] - vy) / height_v[None, :]                       # 0 at the base, 1 at the top
    billow = fbm(h, w, 70 * scale, 34 * scale, 7, octaves=5)
    den = np.clip(billow * 1.5 - u * 1.15 + 0.1, 0, 1) * smoothstep(-0.12, 0.05, u)
    den *= 1 - 0.9 * np.exp(-(np.hypot(xx - sunx, yy - suny) / (90 * k)) ** 2)   # the sun stays clear
    den = light.blur(den, 1.2 * scale)
    # Light from above and towards the sun: density between this point and the sun.
    shift = ndimage.shift(den, (8 * k, (sunx - w / 2) / w * -10 * k), order=1, mode="nearest")
    lit = np.exp(-2.6 * shift)[..., None]
    under = np.array([0.74, 0.70, 0.88], np.float32)                   # lavender undersides
    top_c = np.array([1.0, 0.95, 0.84], np.float32)                     # warm cream-gold tops
    ccol = under * (1 - lit) + top_c * lit
    edge = np.clip(den * (1 - den) * 4, 0, 1)[..., None]
    ccol = ccol + light.GOLD * edge * sunw * 0.45                       # a silver-gold lining by the sun
    ca = (smoothstep(0.05, 0.6, den) * 0.92)[..., None]
    srgb = srgb * (1 - ca) + np.clip(ccol, 0, 1.2) * ca
    out["sky"] = np.dstack([np.clip(srgb, 0, 1), sky[..., 3]])

    # 4. Mist: a new fog layer at half resolution.
    fh, fw = h // 2, w // 2
    fk = fh / VH
    fyy, fxx = np.mgrid[0:fh, 0:fw].astype(np.float32)
    fvy = fyy / fk
    sub = lambda img: ndimage.zoom(img, (fh / h, fw / w), order=1)[:fh, :fw]
    fclear = sub(clear)
    fsunw = sub(sunw[..., 0])[..., None]
    # Along the horizon, at the foot of the foothills.
    band = np.exp(-((fvy - (hz_v + 16)) / 9.0) ** 2)
    streak = fbm(fh, fw, 160 * scale / 2, 9 * scale / 2, 21)
    alpha = band * (0.12 + 0.42 * streak)
    # Low wisps in the valleys of the middle distance (patchy).
    valley = np.exp(-((fvy - 132) / 20.0) ** 2) * np.clip((fbm(fh, fw, 110 * scale / 2, 12 * scale / 2, 22) - 0.48) * 2.6, 0, 1) * 0.3
    alpha = alpha + valley * (1 - alpha)
    # Over the river and the sea: pale water in the land layer, blurred into mist.
    mrgb, ma = unpre(out["mid"])
    water = ((mrgb[..., 2] > mrgb[..., 1] + 0.02) & (mrgb[..., 2] > mrgb[..., 0] + 0.08) & (light.lum(mrgb) > 0.55) & (yy > hz + 30 * k)).astype(np.float32)
    water = sub(light.blur(water * ma, 7 * scale))
    river = np.clip(water * 2.0, 0, 1) * (0.2 + 0.5 * fbm(fh, fw, 60 * scale / 2, 14 * scale / 2, 23))
    alpha = alpha + 0.42 * river * (1 - alpha)
    alpha = np.clip(alpha * fclear, 0, 0.75)
    mist = np.array([0.93, 0.92, 0.99], np.float32) * (1 - fsunw) + np.array([1.0, 0.96, 0.87], np.float32) * fsunw
    mist = np.broadcast_to(mist, (fh, fw, 3))
    out["fog"] = pre(mist, alpha.astype(np.float32))
    return out
