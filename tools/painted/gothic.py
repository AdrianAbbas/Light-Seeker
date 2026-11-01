"""A recursive gothic architecture generator (the detail pass): towers with corner
buttresses, tiers and string courses, lancet windows with gold frames, pinnacle galleries
that sprout smaller pinnacles, octagonal spires with crockets and gold finials, halls
with window rows, flying buttresses. Geometry is accumulated per material in a builder
and written as a few large meshes (thousands of parts stay fast)."""
import math
import random

import numpy as np

import detail as D


class Builder:
    def __init__(self):
        self.parts = {}   # material key -> (verts list, tris list)

    def _add(self, key, verts, tris):
        vs, ts = self.parts.setdefault(key, ([], []))
        base = sum(len(v) for v in vs)
        vs.append(np.asarray(verts, np.float32))
        ts.append(np.asarray(tris, np.int32) + base)

    def flush(self, layer, materials, name="gothic"):
        for key, (vs, ts) in self.parts.items():
            if not vs:
                continue
            D.big_mesh("%s_%s" % (name, key), np.concatenate(vs), np.concatenate(ts), layer, materials[key])
        self.parts = {}

    # ---------------------------------------------------------------- primitives
    def prism(self, key, x, y, z, r0, r1, h, sides=8, rot=0.0, cap=True):
        a = rot + np.arange(sides) * 2 * np.pi / sides
        bottom = np.stack([x + r0 * np.cos(a), y + r0 * np.sin(a), np.full(sides, z)], 1)
        top = np.stack([x + r1 * np.cos(a), y + r1 * np.sin(a), np.full(sides, z + h)], 1)
        verts = np.concatenate([bottom, top, [[x, y, z + h]]])
        tris = []
        for k in range(sides):
            k1 = (k + 1) % sides
            tris += [(k, k1, sides + k1), (k, sides + k1, sides + k)]
            if cap and r1 > 1e-4:
                tris.append((sides + k, sides + k1, 2 * sides))
        self._add(key, verts, tris)

    def cone(self, key, x, y, z, r, h, sides=8, rot=0.0):
        a = rot + np.arange(sides) * 2 * np.pi / sides
        verts = np.concatenate([np.stack([x + r * np.cos(a), y + r * np.sin(a), np.full(sides, z)], 1), [[x, y, z + h]]])
        tris = [(k, (k + 1) % sides, sides) for k in range(sides)]
        self._add(key, verts, tris)

    def box(self, key, x, y, z, sx, sy, sz, rot=0.0):
        c, s = math.cos(rot), math.sin(rot)
        pts = []
        for dz in (0, sz):
            for dx, dy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                px, py = dx * sx / 2, dy * sy / 2
                pts.append((x + c * px - s * py, y + s * px + c * py, z + dz))
        tris = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4), (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]
        self._add(key, pts, tris)

    def lancet(self, key, cx, cy, z, w, h, ang, depth=0.0):
        """A pointed-arch window shape (a flat fan), facing outwards at angle ang."""
        n = 8
        ox, oy = math.cos(ang), math.sin(ang)            # outward normal
        tx, ty = -math.sin(ang), math.cos(ang)            # along the wall
        pts = [(cx + ox * depth, cy + oy * depth, z + h * 0.45)]   # centre
        outline = [(-w / 2, 0), (w / 2, 0), (w / 2, h * 0.62)]
        for k in range(1, n + 1):                          # the pointed top: two arcs
            t = k / n
            outline.append((w / 2 - w / 2 * (1 - math.cos(t * math.pi / 2)) * 1.0, h * 0.62 + h * 0.38 * math.sin(t * math.pi / 2)))
        for k in range(n - 1, 0, -1):
            t = k / n
            outline.append((-(w / 2 - w / 2 * (1 - math.cos(t * math.pi / 2))), h * 0.62 + h * 0.38 * math.sin(t * math.pi / 2)))
        outline.append((-w / 2, h * 0.62))
        for u, v in outline:
            pts.append((cx + tx * u + ox * depth, cy + ty * u + oy * depth, z + v))
        tris = [(0, i, i + 1) for i in range(1, len(outline))] + [(0, len(outline), 1)]
        self._add(key, pts, tris)


def spire(b, x, y, z, r, h, rng, crockets=True, sides=8):
    """An octagonal spire with crockets up each edge and a gold finial."""
    b.cone("roof", x, y, z, r, h, sides=sides, rot=math.pi / sides)
    if crockets:
        n = max(3, int(h / (r * 0.9)))
        for e in range(sides):
            a = math.pi / sides + e * 2 * math.pi / sides
            for k in range(1, n):
                t = k / n
                rr = r * (1 - t) * 1.02
                cx, cy = x + math.cos(a) * rr, y + math.sin(a) * rr
                s = r * 0.12 * (1 - t * 0.6)
                b.cone("gold" if k % 3 == 0 else "stone", cx, cy, z + h * t, s, s * 2.2, sides=4)
    b.prism("gold", x, y, z + h * 0.97, r * 0.09, r * 0.03, r * 0.5, sides=6)
    b.prism("gold", x, y, z + h * 0.97 + r * 0.5, r * 0.18, r * 0.0, r * 0.9, sides=6)


def pinnacle(b, x, y, z, r, h, rng, depth):
    """A small square pinnacle: shaft, gable, spire; at depth > 0 it sprouts four more."""
    b.prism("stone", x, y, z, r, r * 0.95, h * 0.45, sides=4, rot=math.pi / 4)
    b.prism("gold", x, y, z + h * 0.45, r * 1.06, r * 1.06, h * 0.03, sides=4, rot=math.pi / 4)
    spire(b, x, y, z + h * 0.48, r * 1.0, h * 0.62, rng, crockets=depth > 0, sides=4)
    if depth > 0:
        for k in range(4):
            a = k * math.pi / 2
            pinnacle(b, x + math.cos(a) * r * 1.15, y + math.sin(a) * r * 1.15, z + h * 0.3, r * 0.32, h * 0.45, rng, depth - 1)


def tower(b, x, y, z, r, h, rng, facing=-math.pi / 2, tiers=4, spire_ratio=1.6, depth=1, windows=True):
    """A gothic tower: an octagonal shaft with corner buttresses, tiers with string courses
    and lancet windows (the faces towards the viewer), a crown of pinnacles and a spire."""
    sides = 8
    rot = math.pi / sides
    b.prism("stone", x, y, z, r * 1.08, r * 1.08, h * 0.04, sides=sides, rot=rot)
    b.prism("stone", x, y, z, r, r * 0.97, h, sides=sides, rot=rot)
    # Corner buttresses: stepped, in two stages.
    for e in range(sides):
        a = e * 2 * math.pi / sides
        bx, by = x + math.cos(a) * r * 1.02, y + math.sin(a) * r * 1.02
        b.box("stone", bx, by, z, r * 0.16, r * 0.16, h * 0.62, rot=a)
        bx2, by2 = x + math.cos(a) * r * 0.99, y + math.sin(a) * r * 0.99
        b.box("stone", bx2, by2, z + h * 0.62, r * 0.12, r * 0.12, h * 0.38, rot=a)
    th = h / tiers
    for t in range(tiers):
        tz = z + t * th
        b.prism("stone", x, y, tz + th - th * 0.04, r * 1.05, r * 1.05, th * 0.04, sides=sides, rot=rot)   # string course
        if t % 2 == 1:
            b.prism("gold", x, y, tz + th, r * 1.055, r * 1.055, th * 0.012, sides=sides, rot=rot)
        if not windows:
            continue
        for e in range(sides):
            a = rot + e * 2 * math.pi / sides + math.pi / sides
            if math.cos(a - facing) < 0.15:
                continue                                # only the faces we see
            fr = r * math.cos(math.pi / sides) * 0.99
            wx, wy = x + math.cos(a) * fr, y + math.sin(a) * fr
            ww, wh = r * 0.42, th * 0.6
            b.lancet("gold", wx, wy, tz + th * 0.15, ww * 1.22, wh * 1.12, a, depth=0.02 * r)
            b.lancet("glass", wx, wy, tz + th * 0.17, ww, wh, a, depth=0.05 * r)
            # A mullion and a small rose above (tracery hint).
            tx, ty = -math.sin(a), math.cos(a)
            b.box("stone", wx + math.cos(a) * 0.07 * r, wy + math.sin(a) * 0.07 * r, tz + th * 0.17, r * 0.04, r * 0.04, wh * 0.6, rot=a)
    top = z + h
    # The crown: corbel, parapet, pinnacles at every corner (recursive).
    b.prism("stone", x, y, top - r * 0.3, r * 0.97, r * 1.2, r * 0.3, sides=sides, rot=rot)
    b.prism("stone", x, y, top, r * 1.2, r * 1.2, r * 0.35, sides=sides, rot=rot)
    b.prism("gold", x, y, top + r * 0.35, r * 1.21, r * 1.21, r * 0.04, sides=sides, rot=rot)
    for e in range(sides):
        a = e * 2 * math.pi / sides
        pinnacle(b, x + math.cos(a) * r * 1.12, y + math.sin(a) * r * 1.12, top + r * 0.35, r * 0.13, r * 1.5, rng, depth)
    # A drum of lancets, then the spire.
    b.prism("stone", x, y, top + r * 0.35, r * 0.8, r * 0.78, r * 1.1, sides=sides, rot=rot)
    for e in range(sides):
        a = rot + e * 2 * math.pi / sides + math.pi / sides
        if math.cos(a - facing) < 0.2:
            continue
        fr = r * 0.78 * math.cos(math.pi / sides)
        b.lancet("glass", x + math.cos(a) * fr, y + math.sin(a) * fr, top + r * 0.5, r * 0.28, r * 0.8, a, depth=0.03 * r)
    spire(b, x, y, top + r * 1.45, r * 0.82, r * spire_ratio * 4.0, rng)


def hall(b, x0, x1, y, z, height, depth, rng, facing=-math.pi / 2, bays=6):
    """A hall between towers: buttressed bays with tall lancets, a gilt cornice, a steep roof."""
    length = abs(x1 - x0)
    cx = (x0 + x1) / 2
    b.box("stone", cx, y, z, length, depth, height)
    b.box("gold", cx, y - depth / 2 - 0.02, z + height * 0.97, length, 0.4, height * 0.025)
    # Roof: a ridge prism.
    rh = height * 0.45
    pts = [(x0, y - depth / 2, z + height), (x1, y - depth / 2, z + height), (x1, y + depth / 2, z + height), (x0, y + depth / 2, z + height),
           (x0, y, z + height + rh), (x1, y, z + height + rh)]
    b._add("roof", pts, [(0, 1, 5), (0, 5, 4), (3, 4, 5), (3, 5, 2), (0, 4, 3), (1, 2, 5)])
    for k in range(bays):
        bx = x0 + (x1 - x0) * (k + 0.5) / bays
        b.lancet("gold", bx, y - depth / 2, z + height * 0.2, length / bays * 0.5, height * 0.62, -math.pi / 2, depth=0.02)
        b.lancet("glass", bx, y - depth / 2, z + height * 0.22, length / bays * 0.4, height * 0.56, -math.pi / 2, depth=0.06)
        if k > 0:
            ex = x0 + (x1 - x0) * k / bays
            b.box("stone", ex, y - depth / 2 - depth * 0.08, z, depth * 0.12, depth * 0.22, height * 1.05)
            pinnacle(b, ex, y - depth / 2 - depth * 0.08, z + height * 1.05, depth * 0.07, depth * 0.8, rng, 0)


def citadel(b, cx, cy, z, s, seed, facing=-math.pi / 2):
    """The many-spired citadel: a soaring central tower, two rings of towers in graded
    heights, halls between the front ones, flying buttresses to the keep."""
    rng = random.Random(seed)
    tower(b, cx, cy, z, 14 * s, 150 * s, rng, facing, tiers=6, spire_ratio=2.2, depth=1)
    ring1, ring2 = [], []
    for k in range(10):
        a = 2 * math.pi * k / 10 + rng.uniform(-0.1, 0.1)
        x, y = cx + math.cos(a) * 34 * s, cy + math.sin(a) * 26 * s
        h = (95 + rng.uniform(-10, 20)) * s
        tower(b, x, y, z, 7 * s, h, rng, facing, tiers=5, spire_ratio=1.9, depth=1)
        ring1.append((x, y, h))
    for k in range(14):
        a = 2 * math.pi * (k + 0.5) / 14 + rng.uniform(-0.08, 0.08)
        x, y = cx + math.cos(a) * 62 * s, cy + math.sin(a) * 44 * s
        h = (60 + rng.uniform(-10, 25)) * s * (0.85 if math.sin(a) < 0 else 1.15)
        tower(b, x, y, z, 5.2 * s, h, rng, facing, tiers=4, spire_ratio=1.7, depth=1 if math.sin(a) < 0 else 0)
        ring2.append((x, y, h))
    front = sorted([p for p in ring2 if p[1] < cy], key=lambda p: p[0])
    for a, c in zip(front, front[1:]):
        hall(b, a[0] + 4 * s, c[0] - 4 * s, (a[1] + c[1]) / 2, z, 34 * s, 12 * s, rng, facing, bays=3)
    # Flying buttresses from the inner ring to the keep.
    for (x, y, h) in ring1:
        for t in range(6):
            u = t / 6
            px, py = x + (cx - x) * u * 0.6, y + (cy - y) * u * 0.6
            pz = z + h * 0.55 + math.sin(u * math.pi) * 10 * s + u * 20 * s
            b.box("stone", px, py, pz, 2.2 * s, 2.2 * s, 2.0 * s, rot=math.atan2(cy - y, cx - x))


def materials():
    return {"stone": D.mat_marble("gothmarble", colour=(0.84, 0.8, 0.74)), "gold": D.mat_gold("gothgold"),
            "glass": D.mat_glass_window("gothglass"), "roof": D.mat_marble("gothroof", colour=(0.9, 0.86, 0.8), vein=(0.75, 0.7, 0.65))}
