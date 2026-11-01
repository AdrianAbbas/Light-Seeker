"""LIGHT SEEKER's painted scenes: one function per scene, building it with blib (and the
light builders below) and returning its metadata (sky painting parameters, haze,
anchors). Daylight throughout: a warm sun, a blue sky fill, white marble, soft gold,
sunlit meadows full of flowers, pastel blossom, crystal and cloud.

Palette (linear): sky blue, cream and white cloud, ivory and pearl marble, soft gold,
blush pink, lavender and mint pastels, sunlit greens, white and yellow meadow flowers.
The developer's mood references informed the palette and light only (never copied).
"""
import math
import random

import numpy as np

import bpy
from mathutils import Vector

import blib as B

SUN = (1.0, 0.9, 0.74)
SKYFILL = (0.55, 0.7, 1.0)
GOLD = (0.95, 0.72, 0.34)
WARM = (1.0, 0.82, 0.5)

REGISTRY = {}


def scene(name):
    def wrap(fn):
        REGISTRY[name] = fn
        return fn
    return wrap


# ------------------------------------------------------------------ light and materials
def daylight(sun_xy, strength=4.2, fill=1.1, ambient=0.9, key=(-0.55, -0.75, 0.55)):
    """A warm key light from behind the viewer's left (the romantic painter's front and
    side light: the painted sun keeps its place in the sky), a blue sky fill from above
    and a pale ambient world, so shadows stay light and lavender, never black."""
    origin, d = B.screen_ray(*sun_xy)
    B.sun(Vector(key), SUN, strength, angle=2.5, name="sun")
    sc = bpy.context.scene
    sc.cycles.diffuse_bounces = 1   # less green bounce from the meadow
    B.sun(Vector((0.2, -0.3, -1.0)), SKYFILL, fill, angle=40.0, name="skyfill")
    B.world_colour((0.62, 0.74, 0.95), ambient)
    return d


def marble(v=1.0):
    return B.mat_stone("marble", (0.80 * v, 0.77 * v, 0.72 * v), rough=0.4, bump=0.12, scale=2.0)


def pearl(v=1.0):
    return B.mat_stone("pearl", (0.88 * v, 0.84 * v, 0.86 * v), rough=0.3, bump=0.05, scale=3.0)


def gold(rough=0.32):
    return B.mat_metal((0.95, 0.7, 0.32), rough, "gold")


def rosegold():
    return B.mat_metal((0.95, 0.62, 0.6), 0.25, "rosegold")


def rock(v=1.0):
    return B.mat_stone("cliff", (0.42 * v, 0.40 * v, 0.40 * v), rough=0.85, bump=0.9, scale=1.3)


def grass(v=1.0, hue=0.0):
    return B.mat_stone("grass%.2f" % hue, ((0.13 + hue) * v, 0.32 * v, (0.05 + hue * 0.5) * v), rough=0.95, bump=0.5, scale=7.0)


def foliage(colour=(0.12, 0.3, 0.07)):
    return B.mat_stone("leaf%.3f" % colour[0], colour, rough=0.9, bump=0.8, scale=5.0)


def cloudmat(v=1.0):
    # Lit white with a breath of self-light so the undersides stay lavender, not grey.
    return B.mat_stone("cloud", (0.92 * v, 0.92 * v, 0.95 * v), rough=1.0, bump=0.3, scale=0.6, haze=0.35, haze_colour=(0.75, 0.72, 0.9))


def windows(strength=2.5):
    return B.mat_emission(WARM, strength, "window")


def crystalmat(colour=(0.7, 0.82, 1.0), glow=0.35):
    return B.mat_crystal(colour, glow)


def water(colour=(0.32, 0.5, 0.62)):
    # Glossy light water: a dielectric with a breath of sky in it (never a black mirror).
    return B.mat_stone("water%.2f" % colour[0], colour, rough=0.35, bump=0.03, scale=8.0, haze=0.6, haze_colour=(0.62, 0.8, 0.98))


FLOWERS = [(0.95, 0.95, 0.9), (0.95, 0.95, 0.9), (0.98, 0.78, 0.12), (0.95, 0.5, 0.68), (0.66, 0.55, 0.92)]


def hotspot(sub, world):
    return {"kind": "hotspot", "sub": sub, "world": tuple(world)}


def lightpt(world, radius, kind):
    """kind: window (a warm breathing glow), fire (glow + rising light motes), candle,
    glint (a twinkle: sun on gold, crystal, water)."""
    return {"kind": kind, "world": tuple(world), "radius": radius}


# --------------------------------------------------------------------------- builders
def hills(fn_height, x0, x1, y0, y1, layer, material, nx=140, ny=90, name="hills"):
    return B.heightfield(fn_height, x0, x1, y0, y1, nx, ny, layer, material, name=name)


def rolling(seed, amp, base=0.0, freq=0.004):
    rnd = random.Random(seed)
    ph = [rnd.random() * 6.28 for _ in range(6)]

    def f(x, y):
        return base + amp * (0.5 * math.sin(x * freq + ph[0]) * math.cos(y * freq * 0.8 + ph[1])
                             + 0.3 * math.sin(x * freq * 2.3 + ph[2]) + 0.2 * math.cos(y * freq * 2.9 + ph[3]))
    return f


def flowers(fn_height, x0, x1, y0, y1, count, layer, seed, size=0.6, colours=FLOWERS, name="flowers"):
    """A meadow of small flower heads (tiny upturned hexagons) on the ground."""
    rnd = random.Random(seed)
    groups = {}
    for _ in range(count):
        x = rnd.uniform(x0, x1)
        y = rnd.uniform(y0, y1)
        c = colours[rnd.randrange(len(colours))]
        s = size * rnd.uniform(0.6, 1.4) * (1.0 + (y - y0) / max(1.0, y1 - y0) * 1.5)
        groups.setdefault(c, []).append((x, y, fn_height(x, y) + 0.35 * s, s))
    for i, (c, pts) in enumerate(groups.items()):
        verts, faces = [], []
        for (x, y, z, s) in pts:
            base = len(verts)
            verts.append((x, y, z + 0.05 * s))
            for k in range(6):
                a = k * math.pi / 3
                verts.append((x + s * math.cos(a), y + s * math.sin(a), z))
            for k in range(6):
                faces.append((base, base + 1 + k, base + 1 + (k + 1) % 6))
        mat = B.mat_stone("petal%d" % i, c, rough=0.6, bump=0.0, scale=10.0, haze=0.15, haze_colour=c)
        B.mesh_object("%s%d" % (name, i), verts, faces, layer, mat)


def tree(location, height, seed, layer, leaves=None, trunk=None, blossom=False, round_=True):
    """A painterly tree: a trunk and a crown of soft lumps (sunlit, rounded)."""
    rnd = random.Random(seed)
    x, y, z = location
    trunk = trunk or B.mat_stone("bark", (0.22, 0.16, 0.11), bump=0.6, scale=4.0)
    B.cylinder((x, y, z), height * 0.05, height * 0.55, layer, trunk, sides=8, top_radius=height * 0.03, name="trunk")
    leaves = leaves or (foliage((0.95, 0.6, 0.72)) if blossom else foliage())
    for k in range(7 if round_ else 5):
        r = height * rnd.uniform(0.18, 0.28)
        off = (rnd.uniform(-0.25, 0.25) * height, rnd.uniform(-0.2, 0.2) * height, height * rnd.uniform(0.55, 0.85))
        B.rock((x + off[0], y + off[1], z + off[2]), r, seed * 31 + k, layer, leaves, squash=(1.1, 1.0, 0.85), detail=2.0, subdiv=4)


def column(location, radius, height, layer, mat, capital=None):
    x, y, z = location
    B.cylinder((x, y, z), radius * 1.3, radius * 0.6, layer, mat, sides=16, name="base")
    B.cylinder((x, y, z + radius * 0.6), radius, height, layer, mat, sides=20, top_radius=radius * 0.92, name="shaft")
    B.box((x, y, z + radius * 0.6 + height), (radius * 2.8, radius * 2.8, radius * 0.7), layer, capital or mat, name="capital")


def colonnade(x0, x1, y, z, count, radius, height, layer, mat, lintel=True):
    for k in range(count):
        x = x0 + (x1 - x0) * k / max(1, count - 1)
        column((x, y, z), radius, height, layer, mat)
    if lintel:
        B.box(((x0 + x1) / 2, y, z + radius * 1.3 + height), (abs(x1 - x0) + radius * 4, radius * 3, radius * 1.6), layer, mat, name="lintel")


def balustrade(x0, x1, y, z, layer, mat, height=3.0, spacing=1.2):
    n = max(2, int(abs(x1 - x0) / spacing))
    for k in range(n):
        x = x0 + (x1 - x0) * (k + 0.5) / n
        B.lathe((x, y, z), [(0.25, 0), (0.42, height * 0.25), (0.22, height * 0.6), (0.3, height * 0.85), (0.0, height * 0.86)], layer, mat, sides=10, name="baluster")
    B.box(((x0 + x1) / 2, y, z + height * 0.92), (abs(x1 - x0), 1.2, height * 0.18), layer, mat, name="rail")
    B.box(((x0 + x1) / 2, y, z - 0.2), (abs(x1 - x0), 1.4, 0.4), layer, mat, name="plinth")


def fountain(location, size, layer, stone=None):
    stone = stone or marble()
    x, y, z = location
    B.lathe((x, y, z), [(size, 0), (size, size * 0.25), (size * 0.92, size * 0.27), (size * 0.9, size * 0.08), (0.0, size * 0.08)], layer, stone, sides=36, name="basin")
    B.cylinder((x, y, z + size * 0.2), size * 0.86, 0.05, layer, water(), sides=36, name="pool")
    B.lathe((x, y, z), [(size * 0.12, 0), (size * 0.08, size * 0.6), (size * 0.38, size * 0.66), (size * 0.36, size * 0.72), (0.0, size * 0.72)], layer, stone, sides=24, name="tier")
    spray = B.mat_emission((0.85, 0.95, 1.0), 1.2, "spray")
    for k in range(8):
        a = k * math.pi / 4
        B.cylinder((x + math.cos(a) * size * 0.15, y + math.sin(a) * size * 0.15, z + size * 0.7), size * 0.015, size * 0.45, layer, spray, sides=5, name="jet")


def mushroom(location, size, layer, cap):
    x, y, z = location
    B.lathe((x, y, z), [(size * 0.18, 0), (size * 0.14, size * 0.7), (0.0, size * 0.72)], layer, pearl(), sides=16, name="stem")
    B.lathe((x, y, z + size * 0.6), [(size * 0.15, 0), (size * 0.62, size * 0.05), (size * 0.55, size * 0.25), (size * 0.3, size * 0.42), (0.0, size * 0.46)], layer, cap, sides=24, name="cap")


def isle(location, radius, depth, seed, layer, top=None, with_tree=True):
    """A floating island: a rocky inverted cone with a grassy top (and a tree)."""
    x, y, z = location
    rnd = random.Random(seed)
    prof = [(0.0, -depth)]
    for k in range(1, 8):
        t = k / 7
        prof.append((radius * (t ** 0.7) * rnd.uniform(0.85, 1.1), -depth * (1 - t)))
    B.lathe((x, y, z), prof, layer, rock(1.1), sides=18, name="isle")
    B.cylinder((x, y, z - 0.2), radius * 1.02, radius * 0.06, layer, top or grass(1.1), sides=24, name="isletop")
    if with_tree:
        tree((x + radius * 0.2, y, z), radius * 0.9, seed + 7, layer, blossom=rnd.random() < 0.5)


def cloud_bank(location, size, seed, layer, count=9, v=1.0):
    rnd = random.Random(seed)
    x, y, z = location
    for k in range(count):
        r = size * rnd.uniform(0.25, 0.5)
        B.rock((x + rnd.uniform(-1, 1) * size, y + rnd.uniform(-0.3, 0.3) * size, z + rnd.uniform(-0.1, 0.3) * size),
               r, seed * 13 + k, layer, cloudmat(v), squash=(1.5, 1.0, 0.75), detail=1.5, subdiv=4)


def white_castle(location, scale, seed, layer, towers=11):
    B.castle(location, scale, seed, layer, marble(), gold(), windows(), towers=towers, spread=1.1, with_crag=False)


def spires(cx, cy, z, layer, count, height, seed, radius=4.0, spread=14.0):
    rnd = random.Random(seed)
    for k in range(count):
        dx = (k - (count - 1) / 2) * spread * rnd.uniform(0.8, 1.2)
        h = height * (1.0 - 0.18 * abs(k - (count - 1) / 2)) * rnd.uniform(0.9, 1.1)
        B.spire((cx + dx, cy + rnd.uniform(-6, 6), z), radius * rnd.uniform(0.8, 1.1), h, layer, marble(), gold(), roof_h=h * 0.45, windows=windows(), seed=seed + k)


def crystals(cx, cy, z, layer, count, height, seed, colour=(0.7, 0.82, 1.0)):
    rnd = random.Random(seed)
    mat = crystalmat(colour)
    for k in range(count):
        B.crystal((cx + rnd.uniform(-1, 1) * height, cy + rnd.uniform(-0.5, 0.5) * height, z), height * rnd.uniform(0.5, 1.2),
                  height * rnd.uniform(0.08, 0.14), layer, mat, tilt=(rnd.uniform(-0.3, 0.3), rnd.uniform(-0.3, 0.3)))


def meadow_scene(seed, z=0.0, amp=8.0, extent=(-600, 600, 20, 1400), flowers_n=2200, hue=0.0):
    """The common foreground: sunlit rolling ground (near and far) full of flowers."""
    x0, x1, y0, y1 = extent
    f = rolling(seed, amp, z)
    hills(f, x0, x1, y0, y0 + (y1 - y0) * 0.25, "near", grass(1.0, hue), name="near_ground")
    hills(f, x0 * 2, x1 * 2, y0 + (y1 - y0) * 0.25, y1 * 2, "mid", grass(0.9, hue), name="mid_ground")
    flowers(f, x0 * 0.5, x1 * 0.5, y0 + 4, y0 + (y1 - y0) * 0.22, flowers_n, "near", seed + 3)
    return f


def far_mountains(seed, y=2600, height=520, snow=True):
    B.ridge(-3000, 3000, y, -40, height, 300, seed, "far", B.mat_stone("farmtn", (0.55, 0.58, 0.72), haze=0.25, haze_colour=(0.7, 0.78, 0.95)), segments=200, rows=4)


def meta(sun, anchors, horizon=300, clouds=0.75, rays=0.6, haze=0.3, exposure=1.0, sky_seed=1):
    return {"sky": {"moon": [sun[0], sun[1], 60], "horizon": horizon, "clouds": clouds, "rays": rays, "stars": 0.0, "seed": sky_seed},
            "haze": {"near": 60, "far": 2600, "amount": haze}, "exposure": exposure, "anchors": anchors}


# ================================================================================ MAP
@scene("map")
def world_map():
    """The bright world from high above at an angle: the sea in the south-west, sunlit
    valleys and a river, cloud banks, snowy peaks in the north-east; each location a
    luminous landmark."""
    B.camera((0, -480, 440), (0, 820, 40), lens=28)
    sun = (760, 40)
    daylight(sun, strength=4.0)
    f = rolling(3, 40, 0, 0.003)
    hills(lambda x, y: f(x, y) + max(0.0, (y - 600) * 0.12), -1600, 1600, -300, 2200, "mid", grass(1.0), nx=200, ny=160, name="land")
    B.cylinder((-900, 0, 8), 700, 1, "mid", water((0.35, 0.58, 0.75)), sides=48, name="sea")
    far_mountains(9, y=2600, height=700)
    flowers(f, -900, 900, -250, 400, 4000, "mid", 5, size=2.2)
    # Landmarks, in Location order (index = the location).
    L = [
        ((0, 1100, 60), "citadel"), ((520, 1300, 60), "temple"), ((-560, 700, 30), "elven"), ((700, 650, 30), "crystal"),
        ((-200, 350, 20), "meadow"), ((300, 500, 30), "cathedral"), ((-420, 1450, 200), "isles"), ((120, 150, 20), "gardens"),
        ((-760, 300, 20), "fairy"), ((900, 1500, 100), "dragon")]
    anchors = []
    for i, (p, kind) in enumerate(L):
        x, y, z = p
        z = f(x, y) + max(0.0, (y - 600) * 0.12)
        if kind == "citadel":
            B.crag((x, y, z - 40), 80, 140, 3, "mid", rock(1.1), spikes=0.2, mesa=0.8)
            spires(x, y, z + 95, "mid", 7, 120, 11, radius=9, spread=26)
        elif kind == "temple":
            B.crag((x, y, z - 30), 60, 120, 5, "mid", rock(1.1), spikes=0.2, mesa=0.9)
            B.lathe((x, y, z + 88), [(40, 0), (40, 30), (34, 30), (0, 62)], "mid", pearl(), sides=24, name="dome")
            cloud_bank((x, y - 40, z + 40), 70, 4, "mid", count=6)
        elif kind == "elven":
            for k in range(6):
                tree((x + (k % 3 - 1) * 45, y + (k // 3) * 40, z), 90, 40 + k, "mid")
            B.cylinder((x, y - 30, z + 2), 22, 2, "mid", water((0.5, 0.7, 0.85)), sides=24, name="well")
        elif kind == "crystal":
            crystals(x, y, z, "mid", 9, 90, 21)
        elif kind == "meadow":
            for k in range(4):
                tree((x + (k - 1.5) * 60, y + 30 * (k % 2), z), 60, 50 + k, "mid", blossom=True)
        elif kind == "cathedral":
            B.cathedral((x, y, z), 2.6, 61, "mid", marble(), gold(), windows(3.0))
        elif kind == "isles":
            for k in range(3):
                isle((x + (k - 1) * 120, y + 40 * k, z + 60 * k), 50, 70, 70 + k, "mid")
        elif kind == "gardens":
            fountain((x, y, z), 30, "mid")
            colonnade(x - 70, x + 70, y + 50, z, 6, 4, 34, "mid", marble())
        elif kind == "fairy":
            for k in range(5):
                mushroom((x + (k - 2) * 26, y + 10 * (k % 2), z), 30, "mid", B.mat_stone("cap", (0.92, 0.45, 0.6), bump=0.1))
            tree((x, y + 60, z), 80, 81, "mid", blossom=True)
        elif kind == "dragon":
            B.crag((x, y, z - 40), 90, 260, 9, "mid", rock(1.0), spikes=0.5)
            spires(x, y, z + 210, "mid", 1, 60, 91, radius=8)
        anchors.append({"kind": "landmark", "index": i, "world": (x, y, z + 80)})
    for k, (x, y, z) in enumerate([(-300, 1900, 260), (500, 2000, 300), (-1000, 1200, 220), (1100, 900, 200)]):
        cloud_bank((x, y, z), 160, 100 + k, "far", count=8)
    anchors += [lightpt((0, 1100, 260), 30, "glint"), lightpt((300, 500, 120), 20, "glint"), lightpt((700, 650, 90), 18, "glint"),
                # Glowing landmarks: light motes rise from them.
                lightpt((0, 1100, 200), 24, "fire"), lightpt((300, 500, 90), 18, "fire"), lightpt((-760, 300, 50), 16, "fire")]
    return meta(sun, anchors, horizon=120, clouds=0.6, rays=0.5, haze=0.5)


# ===================================================================== CELESTIAL CITADEL
def citadel_hall(seed, sun, hall_colour=1.0, mirrors=False, throne=False):
    """A vast bright marble hall seen down its length: two rows of columns, a floor of
    polished marble, tall arched windows full of sky, gold."""
    B.camera((0, -40, 18), (0, 200, 30), lens=24)
    daylight(sun, strength=3.0, fill=1.4, ambient=1.3)
    m = marble(hall_colour)
    B.box((0, 120, -0.5), (90, 320, 1), "near", pearl(1.05), name="floor")
    for side in (-1, 1):
        for k in range(8):
            y = 10 + k * 34
            layer = "near" if k < 2 else "mid"
            column((side * 30, y, 0), 2.6, 52, layer, m, capital=gold())
        B.box((side * 44, 140, 40), (2, 300, 80), "mid", m, name="wall")
        for k in range(5):
            B.arch_shape((side * 42.9, 30 + k * 60, 18), 12, 40, "mid", B.mat_emission((0.85, 0.92, 1.0), 2.2, "skywin"), rotation_z=math.pi / 2 * side)
            if mirrors:
                B.arch_shape((side * 42.8, 30 + k * 60 + 30, 18), 10, 36, "mid", B.mat_metal((0.85, 0.85, 0.9), 0.02, "mirror"), rotation_z=math.pi / 2 * side)
    B.box((0, 290, 40), (90, 2, 80), "far", m, name="endwall")
    B.box((0, 140, 81), (92, 310, 2), "mid", pearl(1.05), name="ceiling")
    for k in range(3):
        B.light("AREA", (0, 40 + k * 100, 74), WARM, 6.0e4, size=50, look_at=(0, 40 + k * 100, 0), name="hall_light")
    for k in range(9):
        B.box((0, 10 + k * 34, 79), (88, 3, 3), "mid", gold(), name="rib")
    B.arch_shape((0, 288.9, 10), 30, 60, "far", B.mat_emission((1.0, 0.92, 0.75), 3.0, "greatwin"))
    if throne:
        B.steps((0, 200, 0), 50, 8, 1.4, 4, "mid", pearl())
        B.box((0, 234, 14), (16, 10, 6), "mid", gold(0.2), name="throne")
        B.box((0, 240, 30), (20, 2.5, 36), "mid", gold(0.2), name="throneback")
        B.cone((0, 240, 48), 6, 14, "mid", gold(0.2), name="crown")
        drape = B.mat_stone("drape", (0.95, 0.72, 0.62), rough=0.8, bump=0.2, haze=0.15, haze_colour=(1.0, 0.8, 0.7))
        for side in (-1, 1):
            B.box((side * 22, 244, 40), (12, 2, 80), "mid", drape, name="drape")
        cam = bpy.context.scene.camera
        cam.location = Vector((0, 60, 8))
        cam.rotation_euler = (Vector((0, 240, 30)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    return m


@scene("celestial-citadel/gates-of-dawn")
def citadel_gates():
    sun = (470, 140)
    B.camera((0, -20, 12), (0, 300, 70), lens=26)
    daylight(sun)
    f = meadow_scene(1, extent=(-400, 400, 10, 900))
    far_mountains(2)
    B.crag((0, 520, -60), 150, 160, 4, "mid", rock(1.15), spikes=0.2, mesa=0.85)
    white_castle((0, 540, 98), 6.0, 7, "mid", towers=14)
    spires(0, 560, 98, "mid", 5, 150, 13, radius=7, spread=34)
    # The golden gates: two gate towers and a great arch with the dawn behind.
    for side in (-1, 1):
        B.spire((side * 26, 140, f(side * 26, 140)), 7, 70, "mid", marble(), gold(), roof_h=30, windows=windows(), seed=17 + side)
    B.wall_with_arch((0, 140, f(0, 140)), 46, 46, 6, 20, 34, "mid", marble())
    B.arch_shape((0, 137, f(0, 140)), 18, 32, "mid", B.mat_metal((0.95, 0.72, 0.32), 0.25, "gategold"))
    tree((-62, 70, f(-62, 70)), 34, 3, "near")
    tree((70, 90, f(70, 90)), 40, 4, "near", blossom=True)
    cloud_bank((-500, 1200, 260), 200, 5, "far")
    cloud_bank((500, 1400, 320), 240, 6, "far")
    return meta(sun, [hotspot(0, (0, 137, 18)), hotspot(1, (-90, 540, 150)), hotspot(2, (0, 540, 180)), hotspot(3, (60, 560, 290)),
                      lightpt((0, 137, 20), 26, "glint"), lightpt((-26, 140, 70), 12, "glint"), lightpt((26, 140, 70), 12, "glint")],
                horizon=330, clouds=0.8)


@scene("celestial-citadel/hall-of-mirrors")
def citadel_mirrors():
    sun = (480, 220)
    citadel_hall(2, sun, mirrors=True)
    # An angled view: across the hall onto the wall of mirrors (unlike the throne's nave).
    cam = bpy.context.scene.camera
    cam.location = Vector((22, -10, 16))
    cam.rotation_euler = (Vector((-40, 150, 26)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    return meta(sun, [hotspot(0, (0, 20, 10)), hotspot(1, (0, 150, 30)), hotspot(2, (0, 280, 25)), hotspot(3, (0, 288, 60)),
                      lightpt((-42, 90, 36), 16, "glint"), lightpt((42, 150, 36), 16, "glint"), lightpt((-42, 210, 36), 14, "glint")],
                horizon=300, clouds=0.0, rays=0.9, haze=0.35)


@scene("celestial-citadel/throne-of-light")
def citadel_throne():
    sun = (480, 180)
    citadel_hall(3, sun, throne=True)
    return meta(sun, [hotspot(0, (-30, 120, 10)), hotspot(1, (-30, 180, 40)), hotspot(2, (0, 234, 20)), hotspot(3, (26, 200, 50)),
                      lightpt((0, 234, 22), 22, "fire"), lightpt((0, 240, 50), 16, "glint")],
                horizon=300, clouds=0.0, rays=1.0, haze=0.35)


@scene("celestial-citadel/spire-summit")
def citadel_summit():
    sun = (300, 120)
    B.camera((0, -10, 400), (0, 400, 300), lens=26)
    daylight(sun)
    balustrade(-60, 60, 30, 380, "near", marble())
    B.box((0, 10, 378), (130, 40, 4), "near", pearl(), name="terrace")
    for side in (-1, 1):
        B.spire((side * 70, 30, 300), 8, 120, "near", marble(), gold(), roof_h=50, windows=windows(), seed=31 + side)
    for k, (x, y, z) in enumerate([(-400, 600, 260), (200, 700, 240), (-100, 1200, 280), (600, 1100, 300), (-700, 1500, 260), (300, 1800, 320)]):
        cloud_bank((x, y, z), 180, 40 + k, "mid" if y < 1000 else "far")
    far_mountains(8, y=3000, height=480)
    B.ground(120, 9000, "far", B.mat_stone("valley", (0.3, 0.45, 0.25), haze=0.4, haze_colour=(0.7, 0.8, 0.95)))
    for k in range(3):
        B.box((-300 + k * 40, 900 - k * 30, 300 + k * 10), (1.5, 6, 0.6), "mid", B.mat_stone("dove", (0.95, 0.95, 0.95), haze=0.5, haze_colour=(1, 1, 1)), name="dove")
    return meta(sun, [hotspot(0, (-70, 30, 430)), hotspot(1, (70, 30, 430)), hotspot(2, (0, 30, 400)), hotspot(3, (0, 600, 380)),
                      lightpt((-70, 30, 470), 14, "glint"), lightpt((70, 30, 470), 14, "glint")],
                horizon=260, clouds=0.9, rays=0.8, haze=0.65)


# ------------------------------------------------------------------ shared templates
def exterior(seed, sun, cam=((0, -20, 14), (0, 300, 60)), lens=26, flowers_n=2200, hue=0.0, mountains=True, amp=8.0):
    B.camera(cam[0], cam[1], lens=lens)
    daylight(sun)
    f = meadow_scene(seed, amp=amp, extent=(-420, 420, 8, 900), flowers_n=flowers_n, hue=hue)
    if mountains:
        far_mountains(seed + 1)
    cloud_bank((-520, 1300, 280), 220, seed + 5, "far")
    cloud_bank((480, 1500, 330), 250, seed + 6, "far")
    return f


def interior(seed, sun, wall=None, floor=None, window=(0.85, 0.92, 1.0), length=300, width=88, height=80, columns=True,
             capital=None, lens=24, cam_z=18, end_window=(1.0, 0.92, 0.75)):
    """A luminous hall seen down its length: columns, tall arched windows full of sky."""
    B.camera((0, -40, cam_z), (0, length * 0.66, cam_z + 12), lens=lens)
    daylight(sun, strength=3.0, fill=1.4, ambient=1.3)
    wall = wall or marble()
    B.box((0, length * 0.4, -0.5), (width, length * 1.1, 1), "near", floor or pearl(1.05), name="floor")
    for side in (-1, 1):
        if columns:
            for k in range(8):
                y = 10 + k * length / 9
                column((side * width * 0.34, y, 0), 2.6, height * 0.65, "near" if k < 2 else "mid", wall, capital=capital or gold())
        B.box((side * width / 2, length * 0.47, height / 2), (2, length, height), "mid", wall, name="wall")
        for k in range(5):
            B.arch_shape((side * (width / 2 - 1.1), length * 0.1 + k * length / 5, height * 0.22), width * 0.14, height * 0.5,
                         "mid", B.mat_emission(window, 2.2, "win%.2f" % window[0]), rotation_z=math.pi / 2 * side)
    B.box((0, length * 0.97, height / 2), (width, 2, height), "far", wall, name="endwall")
    B.box((0, length * 0.47, height + 1), (width + 2, length + 10, 2), "mid", pearl(1.05), name="ceiling")
    for k in range(3):
        y = length * (0.15 + 0.3 * k)
        B.light("AREA", (0, y, height - 6), WARM, 6.0e4 * (height / 80) ** 2, size=width * 0.6, look_at=(0, y, 0), name="hall_light")
    for k in range(9):
        B.box((0, 10 + k * length / 9, height - 1), (width - 2, 3, 3), "mid", capital or gold(), name="rib")
    B.arch_shape((0, length * 0.963, height * 0.12), width * 0.34, height * 0.75, "far", B.mat_emission(end_window, 3.0, "endwin%.2f" % end_window[2]))
    return wall


def place_meta(sun, n, spots, extra=(), **kw):
    """Hotspots for the n places of a location (in order) at the given world points."""
    anchors = [hotspot(k, spots[k]) for k in range(n)] + list(extra)
    return meta(sun, anchors, **kw)


HALL4 = [(0, 30, 10), (-26, 120, 30), (26, 180, 30), (0, 280, 50)]
HALL3 = [(0, 30, 10), (-24, 150, 30), (24, 200, 30)]


# ============================================================================ SKY TEMPLE
def temple_on_clouds(seed, sun, cam=((0, -60, 150), (0, 400, 160)), dome=True):
    B.camera(cam[0], cam[1], lens=26)
    daylight(sun)
    for k, (x, y, z, sz) in enumerate([(-300, 260, 70, 160), (260, 320, 60, 170), (0, 600, 90, 260), (-600, 900, 120, 260), (600, 1000, 130, 280), (0, 140, 40, 120)]):
        cloud_bank((x, y, z), sz, seed * 7 + k, "near" if y < 200 else ("mid" if y < 700 else "far"), count=10)
    B.crag((0, 480, 40), 90, 120, seed, "mid", rock(1.2), spikes=0.1, mesa=0.95)
    if dome:
        B.lathe((0, 480, 158), [(60, 0), (60, 40), (50, 42), (0, 92)], "mid", pearl(), sides=36, name="temple")
        colonnade(-55, 55, 420, 158, 8, 3.2, 38, "mid", marble())
        B.cone((0, 480, 248), 6, 30, "mid", gold(), name="finial")
    return seed


@scene("sky-temple/cloud-steps")
def temple_steps():
    sun = (720, 110)
    temple_on_clouds(11, sun)
    for k in range(14):
        B.box((0, 120 + k * 22, 30 + k * 9), (40 - k * 1.2, 22, 3), "mid" if k > 3 else "near", marble(1.02), name="step")
    return place_meta(sun, 3, [(0, 160, 50), (0, 420, 190), (40, 470, 170)], [lightpt((0, 480, 280), 16, "glint")], horizon=360, clouds=0.9)


@scene("sky-temple/sanctum")
def temple_sanctum():
    """A round, domed sanctum: a ring of columns, an oculus of sky, an altar of light."""
    sun = (480, 60)
    B.camera((0, -70, 16), (0, 40, 30), lens=22)
    daylight(sun, strength=2.6, fill=1.4, ambient=1.3)
    B.cylinder((0, 40, -1), 80, 1, "near", pearl(1.05), sides=64, name="floor")
    for k in range(14):
        a = k / 14 * 2 * math.pi
        column((60 * math.cos(a), 40 + 60 * math.sin(a), 0), 2.8, 46, "mid", marble(), capital=gold())
    B.lathe((0, 40, 52), [(72, 0), (70, 14), (58, 32), (36, 46), (12, 52), (12, 54)], "far", pearl(1.0), sides=64, name="dome")
    B.cylinder((0, 40, 0), 72, 52, "far", pearl(0.96), sides=64, name="drum")
    B.light("AREA", (0, 40, 100), (1.0, 0.95, 0.85), 1.2e5, size=24, look_at=(0, 40, 0), name="oculus")
    B.lathe((0, 40, 0), [(10, 0), (9, 6), (4, 7), (4, 14), (0, 15)], "mid", marble(), sides=24, name="altar")
    B.cylinder((0, 40, 15), 3, 0.5, "mid", B.mat_emission((1.0, 0.92, 0.7), 6.0, "altarlight"), sides=24, name="flame")
    return place_meta(sun, 3, [(0, -20, 10), (0, 40, 20), (40, 60, 30)], [lightpt((0, 40, 18), 24, "fire"), lightpt((0, 40, 104), 20, "glint")],
                      horizon=300, clouds=0.0, rays=1.0, haze=0.35)


@scene("sky-temple/bell-garden")
def temple_bells():
    sun = (260, 120)
    f = exterior(13, sun, cam=((0, -30, 30), (0, 300, 50)), flowers_n=1600)
    for k in range(5):
        x = -80 + k * 40
        B.box((x, 160, f(x, 160) + 40), (24, 3, 3), "mid", marble(), name="beam")
        for side in (-1, 1):
            B.cylinder((x + side * 11, 160, f(x, 160)), 1.6, 40, "mid", marble(), sides=10, name="post")
        B.bell((x, 160, f(x, 160) + 34), 6 + (k % 3) * 2, "mid")
    for k in range(4):
        tree((-140 + k * 90, 260, f(-140 + k * 90, 260)), 50, 130 + k, "mid", blossom=k % 2 == 0)
    return place_meta(sun, 3, [(0, 90, 20), (-80, 400, 60), (0, 160, f(0, 160) + 30)],
                      [lightpt((-40, 160, 40), 12, "glint"), lightpt((40, 160, 40), 12, "glint")], horizon=320)


# ======================================================================= ELVEN SANCTUARY
def grove(f, seed, count=9, near=True):
    rnd = random.Random(seed)
    for k in range(count):
        x = rnd.uniform(-260, 260)
        y = rnd.uniform(110, 520)
        tree((x, y, f(x, y)), rnd.uniform(70, 120), seed * 11 + k, "mid", leaves=foliage((0.1, 0.28 + rnd.random() * 0.08, 0.1)))
    if near:
        tree((-150, 50, f(-150, 50)), 110, seed + 99, "near", leaves=foliage((0.09, 0.26, 0.08)))


@scene("elven-sanctuary/moonwell")
def elven_moonwell():
    sun = (650, 90)
    f = exterior(21, sun, flowers_n=1500, hue=-0.02)
    grove(f, 21)
    B.cylinder((0, 150, f(0, 150) + 0.5), 34, 1.2, "near", water((0.55, 0.72, 0.85)), sides=48, name="well")
    B.lathe((0, 150, f(0, 150)), [(37, 0), (37, 3), (34, 3), (34, 1), (0, 1)], "near", pearl(), sides=48, name="wellrim")
    return place_meta(sun, 3, [(0, 150, 10), (-120, 300, 60), (120, 400, 120)],
                      [lightpt((0, 150, 4), 30, "glint"), lightpt((-10, 160, 4), 14, "glint")], horizon=320)


@scene("elven-sanctuary/leaf-halls")
def elven_leaf_halls():
    sun = (480, 150)
    wood = B.mat_stone("wood", (0.42, 0.28, 0.16), rough=0.7, bump=0.5, scale=3.0)
    interior(22, sun, wall=wood, floor=B.mat_stone("woodfloor", (0.5, 0.36, 0.22), rough=0.6, bump=0.3, scale=6.0),
             window=(0.7, 0.95, 0.6), capital=foliage((0.18, 0.4, 0.1)), end_window=(0.9, 1.0, 0.7))
    for k in range(10):
        B.rock((-30 + (k % 5) * 15, 60 + k * 22, 60), 9, 220 + k, "mid", foliage((0.14, 0.36, 0.09)), squash=(1.6, 1.0, 0.6))
    return place_meta(sun, 3, HALL3, [lightpt((0, 280, 30), 20, "window")], horizon=300, clouds=0.0, rays=1.0, haze=0.35)


@scene("elven-sanctuary/starlit-canopy")
def elven_canopy():
    sun = (480, 60)
    f = exterior(23, sun, cam=((0, -20, 8), (0, 200, 90)), flowers_n=1200, hue=-0.02)
    grove(f, 23, count=12)
    for k in range(18):
        B.cylinder((random.Random(k).uniform(-200, 200), random.Random(k + 50).uniform(120, 400), random.Random(k + 90).uniform(60, 140)),
                   0.8, 0.8, "mid", B.mat_emission((1.0, 0.95, 0.75), 6.0, "mote"), sides=6, name="mote")
    return place_meta(sun, 3, [(0, 100, 10), (-100, 250, 40), (60, 300, 130)],
                      [lightpt((x, 250, 110), 10, "glint") for x in (-120, -40, 50, 130)], horizon=300, clouds=0.5, rays=1.0)


# ======================================================================== CRYSTAL PALACE
@scene("crystal-palace/prism-hall")
def crystal_prism():
    sun = (480, 140)
    glass = crystalmat((0.75, 0.85, 1.0), 0.25)
    interior(31, sun, wall=pearl(1.05), window=(0.75, 0.85, 1.0), capital=glass, end_window=(1.0, 0.9, 1.0))
    crystals(0, 200, 0, "mid", 7, 36, 31, colour=(0.85, 0.75, 1.0))
    for k, c in enumerate([(1.0, 0.5, 0.5), (1.0, 0.85, 0.4), (0.5, 1.0, 0.6), (0.5, 0.7, 1.0), (0.8, 0.5, 1.0)]):
        B.box((-20 + k * 10, 120, 0.1), (9, 80, 0.2), "near", B.mat_emission(c, 0.8, "prism%d" % k), name="ray")
    return place_meta(sun, 3, HALL3, [lightpt((0, 200, 30), 20, "glint"), lightpt((-20, 190, 20), 12, "glint"), lightpt((20, 210, 20), 12, "glint")],
                      horizon=300, clouds=0.0, rays=1.0, haze=0.3)


@scene("crystal-palace/mirror-lake")
def crystal_lake():
    sun = (300, 130)
    B.camera((0, -40, 20), (0, 400, 50), lens=26)
    daylight(sun)
    B.cylinder((0, 400, 0), 900, 0.5, "near", water((0.5, 0.66, 0.82)), sides=64, name="lake")
    B.crag((0, 760, -40), 120, 120, 31, "mid", rock(1.1), spikes=0.1, mesa=0.9)
    crystals(0, 760, 78, "mid", 11, 120, 32, colour=(0.8, 0.85, 1.0))
    spires(0, 780, 78, "mid", 5, 130, 33, radius=6, spread=30)
    far_mountains(34)
    cloud_bank((-600, 1500, 260), 250, 35, "far")
    return place_meta(sun, 3, [(0, 400, 160), (0, 120, 2), (60, 760, 120)],
                      [lightpt((x, 200, 1), 14, "glint") for x in (-60, 0, 70)], horizon=300, clouds=0.7)


@scene("crystal-palace/diamond-vault")
def crystal_vault():
    sun = (480, 200)
    B.camera((0, 70, 14), (0, 190, 22), lens=22)
    daylight(sun, strength=2.5, fill=1.5, ambient=1.4)
    B.box((0, 120, -0.5), (160, 200, 1), "near", pearl(), name="floor")
    B.lathe((0, 120, 0), [(70, 0), (70, 30), (55, 60), (20, 78), (0, 80)], "far", B.mat_stone("vaultglow", (0.85, 0.84, 0.9), rough=0.4, bump=0.1, haze=0.25, haze_colour=(0.9, 0.88, 1.0)), sides=48, name="vault")
    B.light("AREA", (0, 140, 70), (0.9, 0.92, 1.0), 4.0e4, size=60, look_at=(0, 140, 0), name="vault_light")
    for k in range(14):
        crystals(random.Random(k).uniform(-45, 45), random.Random(k + 7).uniform(60, 170), 0, "mid", 3, 20, 300 + k,
                 colour=[(0.8, 0.9, 1.0), (1.0, 0.85, 0.95), (0.85, 1.0, 0.95)][k % 3])
    return place_meta(sun, 3, [(0, 100, 10), (-30, 150, 20), (25, 170, 25)],
                      [lightpt((x, 150, 18), 12, "glint") for x in (-35, -10, 15, 40)], horizon=300, clouds=0.0, rays=0.9, haze=0.3)


# ======================================================================== BLOSSOM MEADOW
@scene("blossom-meadow/sunlit-meadow")
def meadow_sunlit():
    sun = (640, 90)
    f = exterior(41, sun, flowers_n=3500, amp=10)
    for k in range(5):
        tree((-200 + k * 100, 320 + (k % 2) * 60, f(-200 + k * 100, 320)), 70, 410 + k, "mid", blossom=k % 2 == 1)
    return place_meta(sun, 3, [(0, 80, 6), (-140, 260, 30), (160, 420, 60)], horizon=330, clouds=0.85)


@scene("blossom-meadow/butterfly-glade")
def meadow_glade():
    sun = (480, 70)
    f = exterior(42, sun, flowers_n=3000, amp=6)
    for k in range(7):
        tree((-240 + k * 80, 200 + (k % 3) * 40, f(-240 + k * 80, 200)), 80, 420 + k, "mid", blossom=True)
    wing = [B.mat_stone("wing%d" % i, c, haze=0.4, haze_colour=c) for i, c in enumerate([(0.98, 0.8, 0.3), (0.95, 0.6, 0.8), (0.7, 0.75, 1.0)])]
    rnd = random.Random(42)
    for k in range(16):
        B.box((rnd.uniform(-80, 80), rnd.uniform(40, 160), rnd.uniform(6, 30)), (1.6, 0.1, 1.2), "near", wing[k % 3], rotation=(0, 0, rnd.random() * 3), name="butterfly")
    return place_meta(sun, 3, [(-60, 120, 10), (0, 200, 40), (120, 300, 50)], [lightpt((0, 120, 16), 30, "fire")], horizon=330, clouds=0.6)


@scene("blossom-meadow/wildflower-hill")
def meadow_hill():
    sun = (250, 120)
    B.camera((0, -20, 60), (0, 700, -20), lens=26)
    daylight(sun)
    slope = rolling(43, 6, 0)
    f = lambda x, y: slope(x, y) - max(0.0, y - 120) * 0.35
    hills(f, -420, 420, 8, 330, "near", grass(1.0), name="hill")
    flowers(f, -210, 210, 12, 200, 3500, "near", 46)
    B.ground(-80, 9000, "far", B.mat_stone("valley", (0.25, 0.42, 0.2), haze=0.25, haze_colour=(0.7, 0.8, 0.95)))
    B.cylinder((150, 1200, -78), 120, 1, "far", water((0.5, 0.7, 0.85)), sides=32, name="river")
    far_mountains(44, y=2600, height=420)
    white_castle((-260, 1150, -80), 5.0, 45, "far", towers=11)
    for k in range(10):
        tree((-380 + k * 85, 520 + (k % 3) * 160, -80), 60, 440 + k, "mid", blossom=k % 3 == 0)
    tree((120, 60, f(120, 60)), 60, 430, "near", blossom=True)
    return place_meta(sun, 3, [(-50, 120, f(-50, 120) + 8), (60, 300, 20), (0, 900, -40)], horizon=250, clouds=0.8)


# ====================================================================== GOLDEN CATHEDRAL
@scene("golden-cathedral/nave-of-light")
def cathedral_nave():
    sun = (480, 120)
    interior(51, sun, wall=marble(1.03), window=(1.0, 0.9, 0.65), length=380, height=110, capital=gold(), end_window=(1.0, 0.88, 0.6), lens=22)
    for k in range(6):
        B.box((0, 40 + k * 40, 3), (40, 6, 6), "mid", B.mat_stone("pew", (0.55, 0.38, 0.22), bump=0.3), name="pew")
    return place_meta(sun, 3, [(0, 30, 10), (-30, 200, 60), (30, 260, 40)],
                      [lightpt((0, 360, 40), 30, "window"), lightpt((-38, 180, 50), 14, "window"), lightpt((38, 220, 50), 14, "window")],
                      horizon=300, clouds=0.0, rays=1.0, haze=0.4)


@scene("golden-cathedral/choir-loft")
def cathedral_loft():
    sun = (480, 140)
    interior(52, sun, wall=marble(1.03), window=(1.0, 0.9, 0.65), length=380, height=110, capital=gold(), end_window=(1.0, 0.88, 0.6), cam_z=60)
    balustrade(-40, 40, 10, 50, "near", marble())
    B.box((0, 0, 49), (90, 30, 2), "near", pearl(), name="loft")
    for k in range(9):
        B.cylinder((-24 + k * 6, 360, 30), 1.2, 30 + 10 * math.sin(k), "far", gold(0.2), sides=10, name="pipe")
    return place_meta(sun, 3, [(0, 200, 10), (0, 10, 58), (-30, 300, 50)], [lightpt((0, 360, 50), 26, "window")],
                      horizon=300, clouds=0.0, rays=1.0, haze=0.4)


@scene("golden-cathedral/stained-glass-chapel")
def cathedral_chapel():
    sun = (480, 150)
    interior(53, sun, wall=pearl(), window=(0.9, 0.6, 0.9), length=200, height=80, end_window=(0.6, 0.75, 1.0))
    for k, c in enumerate([(1.0, 0.45, 0.45), (1.0, 0.8, 0.35), (0.45, 0.9, 0.55), (0.45, 0.6, 1.0), (0.8, 0.5, 1.0)]):
        B.box((-30 + k * 15, 100, 0.1), (12, 60, 0.2), "near", B.mat_emission(c, 0.9, "glasslight%d" % k), name="colour")
    return place_meta(sun, 3, [(0, 210, 40), (-26, 80, 30), (0, 30, 10)],
                      [lightpt((0, 192, 40), 26, "window"), lightpt((-43, 100, 30), 14, "window")], horizon=300, clouds=0.0, rays=1.0, haze=0.35)


# ======================================================================== FLOATING ISLES
def sky_isles(seed, sun, cam=((0, -40, 200), (0, 400, 200))):
    B.camera(cam[0], cam[1], lens=26)
    daylight(sun)
    for k, (x, y, z, r) in enumerate([(-220, 420, 170, 60), (240, 520, 230, 70), (-40, 900, 280, 90), (420, 1100, 160, 60), (-500, 1000, 240, 70)]):
        isle((x, y, z), r, r * 1.4, seed + k, "mid" if y < 800 else "far")
    for k, (x, y, z) in enumerate([(-300, 300, 60), (300, 400, 80), (0, 1300, 120), (-700, 1500, 160), (700, 1600, 180)]):
        cloud_bank((x, y, z), 200, seed * 5 + k, "mid" if y < 800 else "far")


@scene("floating-isles/cloud-bridge")
def isles_bridge():
    sun = (720, 100)
    sky_isles(61, sun)
    for k in range(20):
        t = k / 19
        B.box((-220 + 460 * t, 420 + 100 * t, 185 + 45 * t + 18 * math.sin(t * math.pi)), (24, 9, 1.5), "mid", marble(), name="plank")
    return place_meta(sun, 3, [(0, 470, 230), (-220, 420, 200), (240, 520, 260)], horizon=380, clouds=0.9)


@scene("floating-isles/windswept-isle")
def isles_windswept():
    sun = (300, 90)
    sky_isles(62, sun, cam=((-200, 300, 200), (-220, 420, 180)))
    for k in range(3):
        tree((-240 + k * 15, 410, 170), 40, 620 + k, "mid")
    return place_meta(sun, 3, [(200, 600, 260), (-220, 420, 200), (0, 900, 300)], horizon=360, clouds=1.0, rays=0.4, haze=0.7)


@scene("floating-isles/sky-harbour")
def isles_harbour():
    sun = (640, 120)
    sky_isles(63, sun)
    B.box((240, 470, 228), (80, 20, 3), "mid", marble(), name="pier")
    for k in range(3):
        x = 200 + k * 30
        B.cylinder((x, 470, 230), 1.5, 30, "mid", gold(), sides=8, name="mast")
        B.box((x + 5, 470, 248), (12, 0.5, 16), "mid", B.mat_stone("sail", (0.95, 0.93, 0.88), haze=0.3, haze_colour=(1, 1, 1)), name="sail")
        B.lathe((x, 470, 222), [(0, 0), (8, 3), (10, 8), (0, 9)], "mid", B.mat_stone("hull", (0.7, 0.55, 0.38)), sides=16, name="hull")
    return place_meta(sun, 3, [(-200, 420, 200), (0, 900, 300), (230, 470, 250)], [lightpt((230, 470, 260), 16, "glint")], horizon=380, clouds=0.85)


# ========================================================================= ROYAL GARDENS
def garden(f, seed):
    for k in range(6):
        for side in (-1, 1):
            tree((side * 90, 80 + k * 50, f(side * 90, 80 + k * 50)), 34, seed + k * 2 + (side > 0), "mid", blossom=(k + side) % 2 == 0)
    B.box((0, 250, f(0, 250) + 0.2), (40, 420, 0.4), "near", B.mat_stone("path", (0.85, 0.8, 0.72), bump=0.2), name="path")


@scene("royal-gardens/rose-garden")
def gardens_rose():
    sun = (640, 110)
    f = exterior(71, sun, flowers_n=1500, amp=2)
    garden(f, 71)
    roses = [(0.9, 0.3, 0.42), (0.98, 0.6, 0.7), (0.95, 0.95, 0.92)]
    for side in (-1, 1):
        flowers(f, side * 25, side * 70, 30, 300, 900, "near", 72 + side, size=1.1, colours=roses, name="roses")
    white_castle((0, 900, f(0, 900)), 3.5, 73, "far", towers=10)
    return place_meta(sun, 3, [(0, 60, 6), (0, 400, 30), (90, 300, 30)], horizon=330)


@scene("royal-gardens/fountain-court")
def gardens_fountain():
    sun = (480, 100)
    f = exterior(72, sun, flowers_n=900, amp=1)
    B.box((0, 200, f(0, 200) + 0.1), (220, 300, 0.4), "near", B.mat_stone("court", (0.86, 0.82, 0.76), bump=0.15), name="court")
    fountain((0, 180, f(0, 180)), 36, "near")
    colonnade(-100, 100, 330, f(0, 330), 9, 3, 40, "mid", marble())
    return place_meta(sun, 3, [(-80, 300, 20), (0, 180, 30), (80, 330, 40)],
                      [lightpt((0, 180, 26), 26, "glint"), lightpt((-10, 175, 8), 14, "glint"), lightpt((12, 185, 8), 14, "glint")], horizon=330)


@scene("royal-gardens/marble-colonnade")
def gardens_colonnade():
    sun = (300, 110)
    f = exterior(73, sun, cam=((-60, -20, 16), (20, 300, 30)), flowers_n=1500, amp=1)
    for k in range(12):
        column((-40 + k * 3, 30 + k * 26, f(0, 30 + k * 26)), 2.6, 42, "near" if k < 3 else "mid", marble())
    B.box((-22, 170, 48), (6, 300, 4), "mid", marble(), name="architrave")
    balustrade(10, 90, 60, f(50, 60), "near", marble())
    return place_meta(sun, 3, [(60, 100, 10), (80, 300, 20), (-30, 200, 30)], horizon=320)


# =========================================================================== FAIRY GLADE
CAPS = [(0.95, 0.45, 0.55), (0.95, 0.75, 0.35), (0.75, 0.55, 0.95)]


@scene("fairy-glade/mushroom-ring")
def fairy_ring():
    sun = (480, 90)
    f = exterior(81, sun, cam=((0, -10, 14), (0, 200, 20)), flowers_n=2500, amp=3, mountains=False)
    grove(f, 81, count=10, near=False)
    for k in range(10):
        a = k / 10 * 2 * math.pi
        x, y = 40 * math.cos(a), 120 + 26 * math.sin(a)
        mushroom((x, y, f(x, y)), 8 + 3 * (k % 3), "mid", B.mat_stone("cap%d" % (k % 3), CAPS[k % 3], bump=0.1))
    return place_meta(sun, 3, [(0, 120, 6), (-100, 240, 30), (100, 260, 30)],
                      [lightpt((x, 120, 12), 10, "fire") for x in (-30, 0, 30)], horizon=320, clouds=0.5, rays=1.0)


@scene("fairy-glade/firefly-hollow")
def fairy_hollow():
    sun = (480, 50)
    f = exterior(82, sun, cam=((0, -10, 10), (0, 200, 30)), flowers_n=1500, amp=4, mountains=False)
    grove(f, 82, count=14)
    rnd = random.Random(82)
    mote = B.mat_emission((1.0, 0.92, 0.55), 8.0, "firefly")
    for k in range(40):
        B.cylinder((rnd.uniform(-90, 90), rnd.uniform(60, 260), rnd.uniform(4, 40)), 0.5, 0.5, "mid", mote, sides=6, name="firefly")
    return place_meta(sun, 3, [(-80, 160, 10), (0, 140, 14), (90, 220, 20)],
                      [lightpt((x, 150, 20), 12, "fire") for x in (-60, -10, 40, 80)], horizon=300, clouds=0.3, rays=1.0, haze=0.6)


@scene("fairy-glade/fairy-court")
def fairy_court():
    sun = (480, 120)
    B.camera((0, -20, 8), (0, 80, 14), lens=24)
    daylight(sun, strength=3.0, fill=1.5, ambient=1.3)
    f = rolling(83, 1.0)
    hills(f, -80, 80, -10, 140, "near", grass(1.05), name="moss")
    flowers(f, -60, 60, 0, 120, 1500, "near", 84, size=0.5)
    for k in range(8):
        a = k / 8 * math.pi
        mushroom((-60 * math.cos(a), 80 + 40 * math.sin(a), 0), 26, "mid", B.mat_stone("cap%d" % (k % 3), CAPS[k % 3], bump=0.1))
    # A small harp of gold.
    B.box((0, 60, 6), (1, 1, 12), "mid", gold(), name="harp")
    for k in range(7):
        B.cylinder((-3 + k, 60, 1), 0.08, 10 - abs(k - 3), "mid", gold(0.15), sides=4, name="string")
    return place_meta(sun, 3, [(-40, 100, 20), (40, 110, 20), (0, 60, 10)], [lightpt((0, 60, 10), 18, "glint"), lightpt((0, 90, 30), 22, "fire")],
                      horizon=300, clouds=0.3, rays=1.0, haze=0.5)


# ========================================================================== DRAGON'S PEAK
def golden_dragon(location, scale, layer, rotation_z=0.0):
    before = set(bpy.data.objects.keys())
    B.dragon(location, scale, layer, rotation_z=rotation_z, eye_strength=6.0)
    mat = B.mat_metal((0.92, 0.7, 0.4), 0.35, "dragongold")
    for name in set(bpy.data.objects.keys()) - before:
        ob = bpy.data.objects[name]
        if ob.type in ("MESH", "META") and ob.data.materials and "eye" not in ob.name:
            for i in range(len(ob.data.materials)):
                if "eye" not in ob.data.materials[i].name:
                    ob.data.materials[i] = mat


def peak(seed, sun, cam=((0, -60, 80), (0, 500, 180))):
    B.camera(cam[0], cam[1], lens=26)
    daylight(sun)
    B.crag((0, 600, -100), 220, 520, seed, "mid", rock(1.15), spikes=0.45, mesa=0.2)
    B.crag((-300, 900, -100), 160, 380, seed + 1, "far", rock(1.1), spikes=0.4)
    B.crag((320, 1000, -100), 180, 430, seed + 2, "far", rock(1.1), spikes=0.4)
    far_mountains(seed + 3, y=2800, height=600)
    for k, (x, y, z) in enumerate([(-400, 500, 100), (300, 400, 60), (0, 1400, 300)]):
        cloud_bank((x, y, z), 200, seed * 3 + k, "mid" if y < 800 else "far")


@scene("dragons-peak/eyrie")
def dragon_eyrie():
    sun = (700, 90)
    peak(91, sun)
    golden_dragon((0, 560, 425), 7.0, "mid", rotation_z=0.4)
    return place_meta(sun, 3, [(0, 560, 460), (-120, 500, 250), (0, 600, 520)], [lightpt((0, 560, 460), 20, "glint")], horizon=350, clouds=0.8)


@scene("dragons-peak/golden-nest")
def dragon_nest():
    sun = (650, 110)
    B.camera((0, -40, 40), (0, 160, 20), lens=26)
    daylight(sun)
    B.crag((0, 160, -160), 140, 160, 92, "mid", rock(1.15), spikes=0.1, mesa=0.95)
    B.lathe((0, 150, 0), [(46, 0), (50, 6), (40, 10), (0, 8)], "mid", B.mat_stone("nest", (0.62, 0.48, 0.3), bump=0.8, scale=6.0), sides=40, name="nest")
    rnd = random.Random(92)
    for k in range(90):
        B.cylinder((rnd.uniform(-38, 38), 150 + rnd.uniform(-30, 30), rnd.uniform(8, 12)), rnd.uniform(1, 2.5), 0.4, "mid", gold(0.2), sides=10, name="coin")
    golden_dragon((0, 160, 10), 4.0, "mid", rotation_z=-0.6)
    peakfar = rock(1.05)
    B.crag((-400, 900, -200), 180, 420, 93, "far", peakfar, spikes=0.4)
    B.crag((380, 1000, -200), 200, 460, 94, "far", peakfar, spikes=0.4)
    far_mountains(95, y=2800, height=600)
    cloud_bank((0, 600, -40), 260, 96, "far")
    return place_meta(sun, 3, [(-100, 300, 60), (0, 150, 30), (100, 300, 80)], [lightpt((x, 150, 12), 12, "glint") for x in (-25, 0, 25)],
                      horizon=300, clouds=0.8)


@scene("dragons-peak/summit-of-winds")
def dragon_summit():
    sun = (360, 110)
    peak(93, sun, cam=((0, -40, 420), (0, 500, 360)))
    B.rock((0, 120, 380), 30, 97, "near", rock(1.1), squash=(1.6, 1.2, 0.6))
    golden_dragon((10, 140, 400), 3.5, "near", rotation_z=2.6)
    return place_meta(sun, 3, [(0, 500, 300), (-120, 500, 300), (0, 40, 420)], [lightpt((-30, 120, 420), 18, "glint")],
                      horizon=330, clouds=0.9, rays=0.9, haze=0.65)


# ================================================================ DETAIL PASS (v2)
import detail as D  # noqa: E402


def daylight2(sun_dir=(-0.6, 0.55, 0.32), strength=5.0, sky_strength=0.9):
    """Golden-morning daylight: a low warm sun from the side and behind (rim light on
    every tower and leaf), a physical multiple-scattering sky lighting the shadows blue,
    one diffuse bounce kept warm."""
    d = Vector(sun_dir).normalized()
    B.sun(d, (1.0, 0.86, 0.66), strength, angle=1.5, name="sun")
    w = bpy.context.scene.world
    w.use_nodes = True
    nt = w.node_tree
    for n in list(nt.nodes):
        if n.type != "OUTPUT_WORLD":
            nt.nodes.remove(n)
    sky = nt.nodes.new("ShaderNodeTexSky")
    sky.sky_type = "MULTIPLE_SCATTERING"
    sky.sun_elevation = math.asin(max(0.05, d.z))
    sky.sun_rotation = math.atan2(d.x, d.y)
    try:
        sky.sun_disc = False
    except Exception:
        pass
    bg = nt.nodes.new("ShaderNodeBackground")
    bg.inputs["Strength"].default_value = sky_strength
    nt.links.new(sky.outputs[0], bg.inputs[0])
    nt.links.new(bg.outputs[0], nt.nodes["World Output"].inputs["Surface"])
    return d


def haze2(y0, y1, z0, z1, layer, density=0.0025, colour=(0.82, 0.88, 1.0), width=6000):
    """A sunlit atmosphere between the layers: depth, and beams where the sun is blocked."""
    fog = B.mat_fog(colour=colour, density=density, noise_scale=0.004, z0=z0, z1=z1, anisotropy=0.55, emission=0.0, emit_colour=colour)
    B.fog_box((0, (y0 + y1) / 2, (z0 + z1) / 2), (width, y1 - y0, z1 - z0), layer, fog)


@scene("celestial-citadel/gates-of-dawn")
def citadel_gates_v2():
    """The Gates of Dawn: a meadow thick with flowers below tall layered cliffs; at their
    foot the golden gates between two gothic gate towers; above, the many-spired
    white-gold citadel; cumulus towering behind; light motes in the air."""
    import gothic as G
    sun_xy = (230, 70)
    B.camera((0, -10, 4.0), (0, 400, 120), lens=24)
    daylight2((-0.55, -0.35, 0.75), strength=5.5, sky_strength=0.2)
    B.sun(Vector((0.5, 0.8, 0.35)), (1.0, 0.85, 0.6), 1.4, angle=2.0, name="rim")   # warm rim from behind right
    f = rolling(7, 1.5, 0, 0.03)
    ground = lambda x, y: f(x, y) + max(0.0, y - 10) * 0.07
    # Near: the meadow, thick with flowers.
    B.heightfield(ground, -80, 80, -10, 70, 160, 110, "near", D.mat_ground())
    D.grass(ground, -45, 45, -8, 70, 220000, "near", 11, height=(0.2, 0.55))
    D.flowers(ground, -12, 12, -4, 10, 12000, "near", 15, size=0.16,
              kinds=(D.BUTTERCUP, D.BUTTERCUP, D.DAISY, D.PINK, D.LAVENDER, D.CORAL), stem=0.3)
    D.flowers(ground, -40, 40, -6, 60, 16000, "near", 12, size=0.22,
              kinds=(D.BUTTERCUP, D.BUTTERCUP, D.DAISY, D.PINK, D.LAVENDER, D.CORAL), stem=0.45)
    D.tree((-24, 34, ground(-24, 34)), 20, 3, "near", kind="green", leaves=18000)
    D.tree((26, 44, ground(26, 44)), 17, 4, "near", kind="blossom", leaves=26000)
    # Mid: more meadow, the cliffs, the gate towers, the citadel.
    B.heightfield(ground, -400, 400, 70, 300, 140, 70, "mid", D.mat_ground())
    D.flowers(ground, -160, 160, 70, 220, 9000, "mid", 14, size=0.35, kinds=(D.BUTTERCUP, D.DAISY, D.PINK, D.LAVENDER))
    gz = ground(0, 260)
    D.mesa(0, 380, gz - 10, 120, 140, "mid", 4)
    D.mesa(-150, 430, gz - 20, 80, 110, "mid", 5)
    D.mesa(155, 440, gz - 20, 85, 115, "mid", 6)
    # Bushes and small trees on the ledges and the plateau rims.
    rr = random.Random(31)
    for k in range(26):
        a_ = rr.uniform(math.pi * 1.1, math.pi * 1.9)
        rad = rr.uniform(0.75, 0.95)
        vz = rr.uniform(0.35, 0.98)
        bx, by = 120 * rad * (1 - 0.28 * vz ** 1.6) * math.cos(a_), 380 + 90 * rad * (1 - 0.28 * vz ** 1.6) * math.sin(a_) * 0.75
        D.tree((bx, by, gz - 10 + 140 * vz), rr.uniform(6, 12), 300 + k, "mid", kind="blossom" if k % 4 == 0 else "green", leaves=2500)
    b = G.Builder()
    rng = random.Random(9)
    for side in (-1, 1):
        G.tower(b, side * 16, 230, gz, 4.0, 34, rng, tiers=3, spire_ratio=1.4, depth=1)
    G.hall(b, -12, 12, 232, gz, 16, 5, rng, bays=3)
    G.citadel(b, 0, 385, gz + 128, 1.05, 7)
    b.flush("mid", G.materials(), "citadel")
    B.arch_shape((0, 229.2, gz), 6, 11, "mid", D.mat_gold(), name="gate")
    D.sparkles((0, 60, 8), (60, 80, 14), 140, "near", 21, size=0.12)
    D.sparkles((0, 300, 90), (260, 120, 120), 90, "mid", 22, size=0.5)
    # Far: valley, mountains, cumulus.
    v2_sky(2, gz)
    return meta(sun_xy, [hotspot(0, (0, 229, gz + 8)), hotspot(1, (-60, 380, gz + 200)), hotspot(2, (0, 380, gz + 280)),
                         hotspot(3, (0, 380, gz + 420)),
                         lightpt((0, 229, gz + 6), 20, "glint"), lightpt((0, 380, gz + 330), 14, "glint"), lightpt((-16, 230, gz + 40), 10, "fire")],
                horizon=330, clouds=0.35, rays=0.9, haze=0.25)


# ------------------------------------------------------------------ v2 templates
import gothic as G  # noqa: E402

BLOOM = (D.BUTTERCUP, D.BUTTERCUP, D.DAISY, D.PINK, D.LAVENDER, D.CORAL)
WHITE = (D.DAISY, D.DAISY, D.DAISY, D.BUTTERCUP)
PASTEL = (D.PINK, D.LAVENDER, D.DAISY, D.CORAL, D.PINK)


def v2_light(key=(-0.55, -0.35, 0.75), strength=5.5, sky=0.2, rim=1.4):
    daylight2(key, strength=strength, sky_strength=sky)
    if rim > 0:
        B.sun(Vector((0.5, 0.8, 0.35)), (1.0, 0.85, 0.6), rim, angle=2.0, name="rim")


def v2_meadow(seed, ground, kinds=BLOOM, width=45, depth=70, grass_n=200000, flowers_n=14000, trees=((-24, 34, 20, "green"), (26, 44, 17, "blossom"))):
    B.heightfield(ground, -width * 2, width * 2, -10, depth, 160, 110, "near", D.mat_ground())
    D.grass(ground, -width, width, -8, depth, grass_n, "near", seed, height=(0.2, 0.55))
    D.flowers(ground, -12, 12, -4, 10, flowers_n // 2, "near", seed + 1, size=0.16, kinds=kinds, stem=0.3)
    D.flowers(ground, -width * 0.9, width * 0.9, -6, depth * 0.85, flowers_n, "near", seed + 2, size=0.22, kinds=kinds, stem=0.45)
    for k, (x, y, h, kind) in enumerate(trees):
        D.tree((x, y, ground(x, y)), h, seed * 10 + k, "near", kind=kind, leaves=18000 if kind == "green" else 24000)


def v2_mid_meadow(seed, ground, kinds=BLOOM, y0=70, y1=260, width=400):
    B.heightfield(ground, -width, width, y0, y1 + 40, 140, 70, "mid", D.mat_ground())
    D.flowers(ground, -width * 0.4, width * 0.4, y0, y1 * 0.85, 9000, "mid", seed + 5, size=0.35, kinds=kinds)


def v2_sky(seed, gz, clouds=((-900, 2200, 380, 700), (350, 2800, 560, 1000), (1100, 2300, 340, 650), (-500, 3300, 720, 1100)), mountains=True, valley=True):
    if valley:
        h = D.valley(seed, gz - 60, y0=280)
        D.grove(h, -2600, 2600, 500, 2400, 260, "far", seed + 7, size=(16, 34), blossom=0.15, name="farwood%d" % seed)
    if mountains:
        D.mountains(seed, y=2800, height=620, base_z=gz - 80)
    for k, (x, y, z, s_) in enumerate(clouds):
        D.cumulus((x, y, z), (s_ * 1.6, s_, s_ * 0.9), "far", seed * 10 + k, density=0.012, name="fogcloud%d_%d" % (seed, k))


def v2_interior(seed, length=300, width=80, height=90, stone=None, window=(1.0, 0.85, 0.55), bays=7, cam=None, rose=True,
                beams=True, floor=None, columns=True, wall_colour=(0.9, 0.85, 0.76)):
    """A gothic interior: clustered columns, pointed arcades, clerestory lancets with gold
    frames, rib vaults, a rose window, a polished chequered floor, sunbeams through haze."""
    rng = random.Random(seed)
    cam = cam or ((0, -20, 14), (0, length * 0.8, height * 0.45))
    B.camera(cam[0], cam[1], lens=22)
    sun = daylight2((-0.6, 0.4, 0.55), strength=2.0, sky_strength=0.15)
    stone = stone or D.mat_marble("hallmarble", colour=(0.9, 0.84, 0.74))
    gold = D.mat_gold()
    glass = D.mat_stained(window, 1.9, "stained%d" % seed)
    wall = D.mat_ashlar("ashlar%d" % seed, colour=wall_colour)
    B.box((0, length / 2, -0.5), (width * 1.4, length * 1.1, 1), "mid", floor or D.mat_floor(), name="floor")
    b = G.Builder()
    half = width / 2
    bay = length / bays
    for side in (-1, 1):
        B.box((side * (half + 14), length / 2, height / 2), (2, length, height * 1.1), "mid", wall, name="wall")
        for k in range(bays + 1):
            y = k * bay
            x = side * half * 0.62
            if columns:
                # A clustered column: a core and four shafts, gilt capitals and bases.
                b.prism("stone", x, y, 0, 3.2, 3.2, 1.6, sides=8)
                b.prism("stone", x, y, 1.6, 2.4, 2.3, height * 0.5, sides=12)
                for c in range(8):
                    a = c * math.pi / 4 + math.pi / 8
                    r_ = 0.85 if c % 2 == 0 else 0.55
                    b.prism("stone", x + math.cos(a) * 2.3, y + math.sin(a) * 2.3, 1.6, r_, r_, height * 0.5, sides=8)
                    b.prism("gold", x + math.cos(a) * 2.3, y + math.sin(a) * 2.3, 1.6 + height * 0.25, r_ * 1.25, r_ * 1.25, 0.6, sides=8)
                b.prism("gold", x, y, 1.6 + height * 0.5, 3.4, 3.0, 1.6, sides=12)
            # Clerestory lancets in the outer wall.
            if k < bays:
                wy = y + bay / 2
                b.lancet("gold", side * (half + 12.9), wy, height * 0.18, bay * 0.42, height * 0.62, math.pi if side > 0 else 0.0, depth=0.0)
                # Triforium: a row of small gilt arches below the clerestory, on the arcade wall.
                for t in range(3):
                    b.lancet("gold", side * (half * 0.62 + 2.6), y + bay * (t + 1) / 4, height * 0.62, bay * 0.16, height * 0.12,
                             math.pi if side > 0 else 0.0, depth=0.0)
                if beams:
                    B.light("SPOT", (side * (half + 30), wy - 10, height * 0.75), (1.0, 0.88, 0.62), 6.0e5, size=3,
                            look_at=(-side * half * 0.2, wy + 15, 0), name="beam", spot=24)
        for k in range(bays):
            y = k * bay
            D.pointed_arch(y, y + bay, side * half * 0.62, 1.6 + height * 0.53, bay * 0.55, 1.2, "mid", stone)
    b.flush("mid", {"stone": stone, "gold": gold, "glass": glass, "roof": stone}, "hall")
    # The arcades run along y: rotate the arches into place (they were built along x).
    for ob in [o for o in bpy.data.objects if o.name.startswith("archrib")]:
        me = ob.data
        for v in me.vertices:
            x0, y0 = v.co.x, v.co.y
            v.co.x, v.co.y = y0, x0
    for side in (-1, 1):
        for k in range(bays):
            wy = k * bay + bay / 2
            B.arch_shape((side * (half + 12.8), wy, height * 0.2), bay * 0.34, height * 0.56, "mid", glass, rotation_z=-side * math.pi / 2, name="lancetglass")
            # Tracery: two mullions, a transom and a quatrefoil disc in the head.
            ix = side * (half + 12.4)
            for m in (-1, 1):
                B.box((ix, wy + m * bay * 0.06, height * 0.2), (0.6, 0.8, height * 0.42), "mid", gold, name="mullion")
            B.box((ix, wy, height * 0.45), (0.6, bay * 0.32, 0.8), "mid", gold, name="transom")
            B.cylinder((ix, wy, height * 0.66), bay * 0.07, 0.6, "mid", gold, sides=24, name="quatrefoil", rotation=(0, math.pi / 2, 0))
            B.box((side * (half + 12.6), wy, height * 0.12), (0.8, bay, 1.2), "mid", gold, name="course")
    # Rib vault: diagonal ribs across each bay, a gilt ridge.
    for k in range(bays):
        y0, y1 = k * bay, (k + 1) * bay
        top = height * 1.02
        for sx in (-1, 1):
            D.arc(lambda t, y0=y0, y1=y1, sx=sx: (sx * half * 0.62 * (1 - 2 * t), y0 + (y1 - y0) * t, 1.6 + height * 0.53 + (top - 1.6 - height * 0.53) * math.sin(t * math.pi)),
                  14, 1.0, "mid", gold, "rib")
    B.box((0, length / 2, height * 1.05), (width * 1.3, length, 2), "mid", wall, name="vault")
    if rose:
        B.box((0, length + 1, height / 2), (width * 1.4, 2, height * 1.2), "far", wall, name="endwall")
        B.cylinder((0, length - 0.2, height * 0.55), height * 0.28, 0.3, "far", glass, sides=48, name="rose", rotation=(math.pi / 2, 0, 0))
        for k in range(16):
            a = k * 2 * math.pi / 16
            B.box((math.cos(a) * height * 0.14, length - 0.5, height * 0.55 + math.sin(a) * height * 0.14), (height * 0.28, 0.4, 0.8), "far", gold,
                  rotation=(0, -a, 0), name="tracery")
        B.cylinder((0, length - 0.6, height * 0.55), height * 0.29, 0.5, "far", gold, sides=48, name="roserim", rotation=(math.pi / 2, 0, 0))
    B.light("AREA", (0, length * 0.5, height * 0.95), (1.0, 0.92, 0.8), 2.0e4 * (height / 90) ** 2, size=width * 0.8, look_at=(0, length * 0.5, 0), name="fill")
    if beams:
        fog = B.mat_fog(colour=(1.0, 0.95, 0.85), density=0.0008, noise_scale=0.01, z0=0, z1=height, anisotropy=0.6, emission=0.0, emit_colour=(1, 1, 1))
        B.fog_box((0, length / 2, height / 2), (width * 1.4, length, height), "mid", fog)
    return b


def spots3(n, pts):
    return [hotspot(k, pts[k]) for k in range(n)]


@scene("golden-cathedral/nave-of-light")
def v2_nave():
    v2_interior(51, length=320, width=80, height=110, window=(1.0, 0.85, 0.5), bays=8, cam=((0, -20, 30), (0, 300, 55)))
    pew = B.mat_stone("pew2", (0.42, 0.27, 0.15), rough=0.45, bump=0.3, scale=6.0)
    for k in range(9):
        for side in (-1, 1):
            B.box((side * 13, 30 + k * 26, 1.6), (18, 2.5, 3.2), "mid", pew, name="pew")
    D.sparkles((0, 150, 50), (60, 250, 80), 200, "mid", 51, size=0.25)
    return meta((480, 120), spots3(3, [(0, 30, 10), (-28, 200, 60), (28, 260, 40)]) +
                [lightpt((0, 318, 60), 30, "window"), lightpt((-30, 150, 40), 16, "window"), lightpt((30, 220, 40), 16, "window")],
                horizon=300, clouds=0.0, rays=0.6, haze=0.2)


def gentle(seed, amp=1.5, rise=0.07, start=10):
    f = rolling(seed, amp, 0, 0.03)
    return lambda x, y: f(x, y) + max(0.0, y - start) * rise


# ============================================================ CELESTIAL CITADEL (v2)
@scene("celestial-citadel/hall-of-mirrors")
def v2_mirrors():
    v2_interior(2, length=260, width=70, height=80, window=(0.85, 0.92, 1.0), bays=7, cam=((6, -10, 20), (-30, 190, 30)))
    mirror = B.mat_metal((0.92, 0.92, 0.95), 0.03, "mirror2")
    for k in range(7):
        y = k * 260 / 7 + 260 / 14
        B.arch_shape((-34.6, y, 4), 14, 44, "mid", mirror, rotation_z=math.pi / 2, name="mirror")
        D.pointed_arch(y - 9, y + 9, -34.4, 48, 9, 0.8, "mid", D.mat_gold(), name="mirrorframe")
    for ob in [o for o in bpy.data.objects if o.name.startswith("mirrorframe")]:
        for v in ob.data.vertices:
            x0, y0 = v.co.x, v.co.y
            v.co.x, v.co.y = y0, x0
    for k in range(5):
        B.lathe((0, 40 + k * 45, 60), [(0.0, -6), (5, -4), (7, 0), (5, 2), (0.0, 3)], "mid", D.mat_gold(), sides=24, name="chandelier")
        D.sparkles((0, 40 + k * 45, 56), (14, 14, 6), 40, "mid", 200 + k, size=0.3)
    return meta((480, 220), spots3(4, [(0, 30, 10), (-30, 120, 30), (0, 200, 30), (0, 255, 50)]) +
                [lightpt((-34, 90, 30), 16, "glint"), lightpt((-34, 150, 30), 16, "glint"), lightpt((0, 85, 58), 12, "glint")],
                horizon=300, clouds=0.0, rays=0.6, haze=0.2)


@scene("celestial-citadel/throne-of-light")
def v2_throne():
    v2_interior(3, length=240, width=80, height=100, window=(1.0, 0.86, 0.55), bays=6, cam=((0, -10, 22), (0, 220, 40)))
    gold = D.mat_gold(); m = D.mat_marble()
    B.steps((0, 190, 0), 50, 8, 1.6, 4, "mid", m)
    B.box((0, 224, 12.8), (16, 10, 6), "mid", gold, name="throneseat")
    B.box((0, 230, 30), (20, 2.5, 40), "mid", gold, name="throneback")
    b = G.Builder(); rng = random.Random(3)
    for side in (-1, 1):
        G.pinnacle(b, side * 9, 230, 50, 1.6, 22, rng, 1)
    G.pinnacle(b, 0, 230, 50, 2.2, 30, rng, 1)
    b.flush("mid", G.materials(), "throne")
    drape = B.mat_stone("drape2", (0.95, 0.7, 0.6), rough=0.7, bump=0.2)
    for side in (-1, 1):
        for k in range(6):
            B.cylinder((side * (24 + k * 0.6), 238, 0), 1.0, 90, "mid", drape, sides=10, name="drapefold")
    B.light("SPOT", (0, 160, 120), (1.0, 0.9, 0.65), 3.0e5, size=4, look_at=(0, 226, 10), name="throne_beam", spot=18)
    D.sparkles((0, 210, 40), (60, 50, 60), 200, "mid", 31, size=0.35)
    return meta((480, 180), spots3(4, [(-30, 100, 10), (-30, 160, 40), (0, 224, 22), (26, 190, 50)]) +
                [lightpt((0, 224, 22), 22, "fire"), lightpt((0, 230, 52), 16, "glint")], horizon=300, clouds=0.0, rays=0.6, haze=0.2)


@scene("celestial-citadel/spire-summit")
def v2_summit():
    B.camera((0, 22, 410), (0, 600, 372), lens=24)
    v2_light((-0.55, -0.35, 0.65), strength=5.0, sky=0.25)
    m, gold = D.mat_marble(), D.mat_gold()
    B.box((0, 10, 397), (120, 46, 6), "near", D.mat_floor("terracefloor"), name="terrace")
    b = G.Builder(); rng = random.Random(4)
    for x in range(-56, 60, 3):
        b.prism("stone", x, 32, 400, 0.45, 0.35, 3.2, sides=8)
    b.box("stone", 0, 32, 403.2, 120, 1.6, 0.8)
    b.box("gold", 0, 31.2, 403.9, 120, 0.4, 0.2)
    for side in (-1, 1):
        G.tower(b, side * 70, 40, 300, 9, 100, rng, tiers=4, spire_ratio=2.0, depth=1)
    G.tower(b, -150, 300, 200, 14, 230, rng, tiers=6, spire_ratio=2.2, depth=1)
    G.tower(b, 190, 380, 200, 12, 200, rng, tiers=5, spire_ratio=2.0, depth=1)
    b.flush("near", G.materials(), "summit")
    for k, (x, y, z, s_) in enumerate([(-400, 600, 250, 300), (200, 700, 240, 340), (-100, 1200, 260, 420), (600, 1100, 270, 380),
                                       (-700, 1500, 250, 420), (300, 1800, 290, 500), (-1100, 900, 240, 380), (1000, 800, 250, 360)]):
        D.cumulus((x, y, z), (s_ * 1.8, s_, s_ * 0.6), "mid" if y < 1000 else "far", 40 + k, density=0.014, name="fogcloud_s%d" % k)
    v2_sky(8, 120, clouds=(), mountains=True, valley=True)
    D.sparkles((0, 60, 410), (100, 60, 30), 80, "near", 41, size=0.15)
    return meta((300, 120), spots3(4, [(-150, 300, 380), (190, 380, 380), (0, 60, 402), (0, 600, 380)]) +
                [lightpt((-150, 300, 420), 14, "glint"), lightpt((190, 380, 400), 14, "glint")], horizon=260, clouds=0.5, rays=0.7, haze=0.2)


# ================================================================== SKY TEMPLE (v2)
@scene("sky-temple/cloud-steps")
def v2_cloud_steps():
    B.camera((0, -40, 70), (0, 400, 190), lens=24)
    v2_light((-0.6, -0.3, 0.7), strength=5.0, sky=0.25)
    m, gold = D.mat_marble(), D.mat_gold()
    for k in range(26):
        y = 40 + k * 16; z = 40 + k * 7
        B.box((0, y, z), (26 - k * 0.3, 16, 2.2), "mid" if k > 4 else "near", m, name="step")
        if k % 3 == 0:
            for side in (-1, 1):
                B.cylinder((side * (13 - k * 0.15), y, z + 1.1), 0.6, 9, "mid", m, sides=12, name="post")
                B.lathe((side * (13 - k * 0.15), y, z + 10), [(0.0, 0), (0.9, 0.8), (0.0, 1.8)], "mid", gold, sides=12, name="postfinial")
    b = G.Builder(); rng = random.Random(11)
    zt = 40 + 26 * 7
    b.prism("stone", 0, 470, zt - 6, 46, 46, 6, sides=32)
    for k in range(16):
        a = k * 2 * math.pi / 16
        b.prism("stone", math.cos(a) * 36, 470 + math.sin(a) * 36, zt, 1.8, 1.7, 30, sides=12)
        b.prism("gold", math.cos(a) * 36, 470 + math.sin(a) * 36, zt + 30, 2.4, 2.0, 1.6, sides=12)
    b.prism("stone", 0, 470, zt + 31.6, 40, 40, 4, sides=32)
    b.box("gold", 0, 470 - 40, zt + 35.6, 6, 0.4, 0.6)
    G.spire(b, 0, 470, zt + 35.6, 30, 70, rng)
    for k in range(8):
        a = k * 2 * math.pi / 8
        G.pinnacle(b, math.cos(a) * 40, 470 + math.sin(a) * 40, zt + 35.6, 2.0, 18, rng, 1)
    b.flush("mid", G.materials(), "temple")
    for k, (x, y, z, s_) in enumerate([(-260, 300, 0, 300), (260, 340, 10, 320), (0, 650, -20, 500), (-600, 900, 20, 520), (600, 1000, 30, 520), (0, 150, -10, 220)]):
        D.cumulus((x, y, z), (s_ * 1.8, s_, s_ * 0.55), "near" if y < 200 else ("mid" if y < 700 else "far"), 110 + k, density=0.016, name="fogcloud_c%d" % k)
    D.sparkles((0, 300, 160), (200, 300, 120), 160, "mid", 12, size=0.5)
    return meta((720, 110), spots3(3, [(0, 160, 70), (0, 440, 240), (36, 470, 230)]) + [lightpt((0, 470, 330), 16, "glint")], horizon=360, clouds=0.5)


@scene("sky-temple/sanctum")
def v2_sanctum():
    """Inside the temple's rotunda: a ring of clustered columns, tall stained-glass
    windows all round, a gilt-ribbed dome with an oculus pouring light on the altar."""
    B.camera((0, -18, 12), (0, 70, 36), lens=20)
    daylight2((-0.5, 0.3, 0.8), strength=2.0, sky_strength=0.15)
    m, gold = D.mat_marble(), D.mat_gold()
    glass = D.mat_stained((0.8, 0.88, 1.0), 1.9, "sanctumglass")
    wall = D.mat_ashlar("sanctumashlar", colour=(0.92, 0.88, 0.8))
    B.cylinder((0, 40, -1), 90, 1, "mid", D.mat_floor("sanctumfloor", (0.9, 0.87, 0.82), (0.75, 0.66, 0.55), 0.18), sides=64, name="floor")
    b = G.Builder(); rng = random.Random(12)
    for k in range(14):
        a_ = k / 14 * 2 * math.pi
        x, y = 50 * math.cos(a_), 40 + 50 * math.sin(a_)
        b.prism("stone", x, y, 0, 3.4, 3.4, 1.8, sides=8)
        b.prism("stone", x, y, 1.8, 2.4, 2.2, 44, sides=16)
        for c in range(8):
            ca = c * math.pi / 4
            b.prism("stone", x + math.cos(ca) * 2.3, y + math.sin(ca) * 2.3, 1.8, 0.6, 0.55, 44, sides=6)
        b.prism("gold", x, y, 45.8, 3.6, 3.0, 2, sides=16)
        # A window in the outer wall behind each bay.
        wa = a_ + math.pi / 14
        wx, wy = 70.5 * math.cos(wa), 40 + 70.5 * math.sin(wa)
        B.arch_shape((wx, wy, 8), 13, 40, "mid", glass, rotation_z=wa - math.pi / 2, name="sanctumglass")
        b.lancet("gold", 70.0 * math.cos(wa), 40 + 70.0 * math.sin(wa), 7, 15, 42, wa + math.pi, depth=0.0)
    b.flush("mid", G.materials(), "sanctum")
    # The wall (open at the top and bottom), the dome above it.
    nu = 96
    V, T = [], []
    for k in range(nu):
        a_ = k / nu * 2 * math.pi
        V += [(72 * math.cos(a_), 40 + 72 * math.sin(a_), -1), (72 * math.cos(a_), 40 + 72 * math.sin(a_), 56)]
        j = 2 * k; jn = 2 * ((k + 1) % nu)
        T += [(j, jn, jn + 1), (j, jn + 1, j + 1)]
    D.big_mesh("drum", np.array(V), np.array(T), "mid", wall)
    B.lathe((0, 40, 54), [(72, 0), (70, 14), (58, 32), (36, 46), (12, 52), (12, 54)], "mid", m, sides=64, name="dome")
    for k in range(16):
        a_ = k * 2 * math.pi / 16
        D.arc(lambda t, a_=a_: (math.cos(a_) * (70 - 58 * t), 40 + math.sin(a_) * (70 - 58 * t), 54 + 52 * math.sin(t * math.pi / 2)), 12, 1.0, "mid", gold, "domerib")
    B.light("SPOT", (0, 40, 160), (1.0, 0.94, 0.8), 6.0e5, size=6, look_at=(0, 40, 0), name="oculus", spot=14)
    B.light("AREA", (0, 40, 50), (1.0, 0.93, 0.82), 1.6e4, size=60, look_at=(0, 40, 0), name="fill")
    fog = B.mat_fog(colour=(1.0, 0.96, 0.88), density=0.0012, noise_scale=0.02, z0=0, z1=110, anisotropy=0.6, emission=0.0, emit_colour=(1, 1, 1))
    B.fog_box((0, 40, 55), (140, 140, 110), "mid", fog)
    B.lathe((0, 40, 0), [(12, 0), (11, 6), (5, 7), (5, 15), (0, 16)], "mid", m, sides=32, name="altar")
    B.cylinder((0, 40, 16), 3, 0.6, "mid", B.mat_emission((1.0, 0.92, 0.7), 8.0, "altarflame"), sides=24, name="flame")
    D.sparkles((0, 40, 40), (50, 50, 60), 220, "mid", 13, size=0.25)
    return meta((480, 60), spots3(3, [(-30, 60, 12), (0, 40, 20), (30, 70, 30)]) + [lightpt((0, 40, 18), 24, "fire"), lightpt((0, 40, 100), 20, "glint")],
                horizon=300, clouds=0.0, rays=0.0, haze=0.15)


@scene("sky-temple/bell-garden")
def v2_bell_garden():
    B.camera((0, -12, 4.5), (0, 300, 60), lens=24)
    v2_light()
    ground = gentle(13, rise=0.05)
    v2_meadow(13, ground, kinds=PASTEL)
    v2_mid_meadow(13, ground, kinds=PASTEL)
    b = G.Builder(); rng = random.Random(13)
    for k in range(5):
        x = -60 + k * 30; y = 110; gz = ground(x, y)
        for side in (-1, 1):
            b.prism("stone", x + side * 9, y, gz, 1.2, 1.1, 24, sides=8)
            G.pinnacle(b, x + side * 9, y, gz + 24, 1.0, 8, rng, 0)
        b.box("stone", x, y, gz + 24, 20, 2.2, 2.2)
        b.box("gold", x, y - 1.15, gz + 25, 20, 0.2, 0.4)
    b.flush("mid", G.materials(), "bellgarden")
    for k in range(5):
        x = -60 + k * 30
        B.bell((x, 110, ground(x, 110) + 21), 4 + (k % 3), "mid")
    gz = ground(0, 400)
    b = G.Builder()
    G.tower(b, 0, 420, gz, 10, 110, rng, tiers=5, spire_ratio=2.0, depth=1)
    G.tower(b, -60, 440, gz, 6, 70, rng, tiers=3, spire_ratio=1.8, depth=1)
    b.flush("mid", G.materials(), "belltower")
    v2_sky(14, ground(0, 300))
    return meta((260, 120), spots3(3, [(0, 40, 6), (-60, 420, 80), (0, 110, 30)]) +
                [lightpt((-30, 110, 30), 12, "glint"), lightpt((30, 110, 30), 12, "glint")], horizon=320, clouds=0.35)


# ============================================================= ELVEN SANCTUARY (v2)
def forest(ground, seed, count, x=(-160, 160), y=(60, 400), h=(40, 75), layer="mid", blossom=0.15):
    rng = random.Random(seed)
    for k in range(count):
        tx, ty = rng.uniform(*x), rng.uniform(*y)
        if abs(tx) < 30 and ty < 200:
            continue
        D.tree((tx, ty, ground(tx, ty)), rng.uniform(*h), seed * 100 + k, layer, kind="blossom" if rng.random() < blossom else "green", leaves=9000)


@scene("elven-sanctuary/moonwell")
def v2_moonwell():
    B.camera((0, -14, 6), (0, 200, 30), lens=24)
    v2_light((-0.5, 0.3, 0.6), strength=4.0, sky=0.25, rim=2.0)
    ground = gentle(21, amp=1.0, rise=0.02)
    v2_meadow(21, ground, kinds=WHITE, trees=((-28, 30, 34, "green"), (30, 40, 30, "green")))
    v2_mid_meadow(21, ground, kinds=WHITE)
    forest(ground, 21, 22)
    gz = ground(0, 120)
    B.cylinder((0, 120, gz + 0.4), 30, 0.8, "mid", water((0.55, 0.75, 0.9)), sides=64, name="well")
    b = G.Builder(); rng = random.Random(21)
    b.prism("stone", 0, 120, gz, 32, 32, 2.2, sides=48)
    for k in range(8):
        a = k * 2 * math.pi / 8
        b.prism("stone", math.cos(a) * 31, 120 + math.sin(a) * 31, gz, 1.2, 1.1, 14, sides=8)
        G.pinnacle(b, math.cos(a) * 31, 120 + math.sin(a) * 31, gz + 14, 1.0, 7, rng, 0)
    b.flush("mid", G.materials(), "moonwell")
    D.sparkles((0, 120, 8), (60, 40, 14), 220, "mid", 22, size=0.3)
    v2_sky(22, gz, mountains=False)
    return meta((650, 90), spots3(3, [(0, 120, 8), (-90, 260, 50), (90, 300, 80)]) +
                [lightpt((0, 120, 2), 30, "glint"), lightpt((-10, 125, 2), 14, "glint"), lightpt((0, 120, 10), 20, "fire")], horizon=300, clouds=0.3)


@scene("elven-sanctuary/leaf-halls")
def v2_leaf_halls():
    B.camera((0, -10, 14), (0, 220, 40), lens=22)
    daylight2((-0.4, 0.6, 0.65), strength=4.0, sky_strength=0.25)
    ground = gentle(22, amp=0.4, rise=0.0)
    B.heightfield(ground, -120, 120, -10, 320, 120, 160, "mid", D.mat_ground())
    D.grass(ground, -60, 60, -8, 250, 160000, "mid", 22, height=(0.15, 0.4))
    D.flowers(ground, -50, 50, -6, 250, 8000, "mid", 23, size=0.2, kinds=WHITE)
    # Living columns: tall trunks in two rows, their crowns meeting overhead as a roof.
    for k in range(7):
        for side in (-1, 1):
            D.tree((side * 26, 20 + k * 38, 0), 70, 220 + k * 2 + (side > 0), "mid", kind="green", leaves=14000)
    wood = B.mat_stone("woodhall", (0.45, 0.3, 0.18), rough=0.6, bump=0.6, scale=4.0)
    for k in range(6):
        D.pointed_arch(-26, 26, 39 + k * 38, 30, 18, 1.4, "mid", wood, name="woodarch")
    B.light("SPOT", (-60, 120, 140), (1.0, 0.92, 0.65), 6.0e5, size=5, look_at=(0, 140, 0), name="beam", spot=20)
    fog = B.mat_fog(colour=(1.0, 0.97, 0.88), density=0.0015, noise_scale=0.02, z0=0, z1=80, anisotropy=0.6, emission=0.0, emit_colour=(1, 1, 1))
    B.fog_box((0, 150, 40), (160, 320, 80), "mid", fog)
    D.sparkles((0, 150, 20), (50, 250, 30), 220, "mid", 24, size=0.25)
    return meta((480, 150), spots3(3, [(0, 30, 8), (-20, 150, 20), (20, 200, 20)]) + [lightpt((0, 250, 20), 20, "window")], horizon=300, clouds=0.0, rays=0.0)


@scene("elven-sanctuary/starlit-canopy")
def v2_canopy():
    B.camera((0, -14, 4), (0, 200, 70), lens=22)
    v2_light((-0.3, 0.8, 0.25), strength=3.5, sky=0.3, rim=2.5)
    ground = gentle(23, amp=1.0, rise=0.02)
    v2_meadow(23, ground, kinds=(D.LAVENDER, D.DAISY, D.PINK), trees=((-24, 30, 36, "green"), (26, 36, 34, "blossom")))
    v2_mid_meadow(23, ground, kinds=(D.LAVENDER, D.DAISY))
    forest(ground, 23, 26, h=(50, 90), blossom=0.3)
    D.sparkles((0, 120, 40), (240, 220, 90), 600, "mid", 25, size=0.35)
    v2_sky(24, ground(0, 300), mountains=False)
    return meta((480, 60), spots3(3, [(0, 60, 6), (-90, 220, 40), (60, 260, 90)]) + [lightpt((x, 200, 60), 10, "glint") for x in (-80, 0, 80)],
                horizon=300, clouds=0.25, rays=1.0)


# ============================================================== CRYSTAL PALACE (v2)
@scene("crystal-palace/prism-hall")
def v2_prism():
    glass = B.mat_crystal((0.78, 0.86, 1.0), 0.2)
    v2_interior(31, length=240, width=74, height=90, window=(0.75, 0.88, 1.0), bays=6, stone=D.mat_marble("prismmarble", colour=(0.9, 0.9, 0.95)),
                floor=D.mat_floor("prismfloor", (0.92, 0.93, 0.98), (0.72, 0.76, 0.86)), wall_colour=(0.88, 0.9, 0.96))
    D.crystal_cluster(0, 200, 0, 26, 14, "mid", 31, colour=(0.85, 0.78, 1.0))
    for k, c in enumerate([(1.0, 0.45, 0.45), (1.0, 0.8, 0.35), (0.45, 0.95, 0.55), (0.45, 0.6, 1.0), (0.8, 0.5, 1.0)]):
        B.light("SPOT", (-60 + k * 4, 100 + k * 8, 120), c, 1.2e5, size=2, look_at=(-12 + k * 6, 130, 0), name="prismlight", spot=10)
    D.sparkles((0, 150, 30), (50, 160, 50), 260, "mid", 32, size=0.25)
    return meta((480, 140), spots3(3, [(0, 30, 10), (-24, 150, 30), (24, 200, 30)]) +
                [lightpt((0, 200, 30), 20, "glint"), lightpt((-12, 195, 20), 12, "glint"), lightpt((12, 205, 20), 12, "glint")], horizon=300, clouds=0.0, rays=0.0)


@scene("crystal-palace/mirror-lake")
def v2_lake():
    B.camera((0, -40, 12), (0, 400, 60), lens=24)
    v2_light((-0.6, -0.2, 0.5), strength=5.0, sky=0.3)
    lake = B.mat_metal((0.6, 0.72, 0.85), 0.02, "lake2")
    B.cylinder((0, 500, 0), 1200, 0.5, "near", lake, sides=96, name="lake")
    D.mesa(0, 700, -40, 150, 85, "mid", 31, squash=0.55)
    D.grass(lambda x, y: 45.5, -60, 60, 660, 740, 60000, "mid", 31, height=(0.6, 1.4))
    b = G.Builder()
    G.citadel(b, 0, 700, 45, 0.85, 33)
    b.flush("mid", {"stone": D.mat_marble("crystalstone", colour=(0.88, 0.9, 0.98)), "gold": D.mat_gold(), "glass": B.mat_crystal((0.7, 0.85, 1.0), 0.6),
                    "roof": B.mat_crystal((0.8, 0.88, 1.0), 0.15)}, "crystalcastle")
    D.crystal_cluster(-150, 680, 0, 40, 10, "mid", 34)
    D.crystal_cluster(170, 690, 0, 46, 10, "mid", 35, colour=(0.95, 0.8, 1.0))
    v2_sky(34, 0, valley=False)
    return meta((300, 130), spots3(3, [(0, 400, 160), (0, 120, 2), (60, 700, 120)]) + [lightpt((x, 200, 1), 14, "glint") for x in (-60, 0, 70)],
                horizon=300, clouds=0.4)


@scene("crystal-palace/diamond-vault")
def v2_vault():
    B.camera((0, 60, 16), (0, 190, 24), lens=22)
    daylight2((-0.4, 0.3, 0.8), strength=1.5, sky_strength=0.15)
    B.box((0, 120, -0.5), (180, 220, 1), "mid", D.mat_floor("vaultfloor", (0.95, 0.94, 0.97), (0.78, 0.76, 0.86)), name="floor")
    B.lathe((0, 120, 0), [(80, 0), (80, 30), (62, 62), (24, 82), (0, 84)], "far", D.mat_marble("vaultdome", colour=(0.9, 0.9, 0.95)), sides=64, name="vault")
    for k in range(24):
        a = k * 2 * math.pi / 24
        D.arc(lambda t, a=a: (math.cos(a) * (80 - 76 * t ** 1.5), 120 + math.sin(a) * (80 - 76 * t ** 1.5), 30 + 54 * math.sin(t * math.pi / 2)), 14, 0.8, "far", D.mat_gold(), "vaultrib")
    rng = random.Random(36)
    for k in range(18):
        D.crystal_cluster(rng.uniform(-55, 55), rng.uniform(130, 190), 0, rng.uniform(8, 22), 6, "mid", 360 + k,
                          colour=[(0.8, 0.9, 1.0), (1.0, 0.85, 0.95), (0.85, 1.0, 0.95)][k % 3])
    B.light("SPOT", (0, 150, 120), (1.0, 0.95, 0.9), 5.0e5, size=8, look_at=(0, 160, 0), name="oculus", spot=30)
    B.light("AREA", (0, 140, 70), (0.92, 0.95, 1.0), 1.4e4, size=90, look_at=(0, 140, 0), name="fill")
    glass = D.mat_stained((0.75, 0.85, 1.0), 2.0, "vaultglass", tint_mix=0.3)
    gold = D.mat_gold()
    for k in range(18):
        a = k * 2 * math.pi / 18
        if math.sin(a) < -0.3:
            continue
        wx, wy = 79.2 * math.cos(a), 120 + 79.2 * math.sin(a)
        B.arch_shape((wx, wy, 3), 14, 26, "far", glass, rotation_z=a - math.pi / 2, name="vaultglass")
    for k in range(10):
        D.crystal_cluster(rng.uniform(-70, 70), rng.uniform(170, 196), 0, rng.uniform(20, 34), 7, "far", 380 + k,
                          colour=[(0.8, 0.9, 1.0), (1.0, 0.85, 0.95), (0.85, 1.0, 0.95)][k % 3])
    D.sparkles((0, 160, 20), (90, 60, 40), 300, "mid", 37, size=0.2)
    return meta((480, 200), spots3(3, [(0, 100, 10), (-30, 150, 20), (25, 170, 25)]) + [lightpt((x, 150, 18), 12, "glint") for x in (-35, -10, 15, 40)],
                horizon=300, clouds=0.0, rays=0.0)


# ============================================================== BLOSSOM MEADOW (v2)
@scene("blossom-meadow/sunlit-meadow")
def v2_sunlit():
    B.camera((0, -12, 4.5), (0, 300, 40), lens=24)
    v2_light()
    ground = gentle(41, amp=2.5, rise=0.03)
    v2_meadow(41, ground, kinds=BLOOM, trees=((-30, 40, 24, "green"), (34, 56, 20, "blossom")))
    v2_mid_meadow(41, ground)
    forest(ground, 41, 12, y=(220, 420), h=(40, 60), blossom=0.4)
    gz = ground(-260, 1100)
    b = G.Builder()
    G.citadel(b, -260, 1100, gz, 0.6, 42)
    b.flush("far", G.materials(), "farcastle")
    v2_sky(42, ground(0, 300))
    D.butterflies((0, 30, 2), (40, 40, 3), 30, "near", 43)
    return meta((640, 90), spots3(3, [(0, 40, 6), (-140, 260, 30), (160, 420, 60)]), horizon=330, clouds=0.35)


@scene("blossom-meadow/butterfly-glade")
def v2_glade():
    B.camera((0, -12, 4), (0, 200, 30), lens=24)
    v2_light((-0.5, 0.2, 0.7), strength=5.0, sky=0.25, rim=2.0)
    ground = gentle(42, amp=1.2, rise=0.02)
    v2_meadow(42, ground, kinds=PASTEL, trees=((-26, 34, 26, "blossom"), (28, 40, 24, "blossom")))
    v2_mid_meadow(42, ground, kinds=PASTEL)
    rng = random.Random(44)
    for k in range(14):
        a = math.pi * (0.1 + 0.8 * k / 13)
        x, y = -math.cos(a) * 120, 90 + math.sin(a) * 90
        D.tree((x, y, ground(x, y)), rng.uniform(30, 44), 440 + k, "mid", kind="blossom" if k % 3 else "green", leaves=12000)
    D.butterflies((0, 40, 4), (50, 60, 6), 90, "near", 45)
    D.butterflies((0, 120, 10), (120, 80, 20), 120, "mid", 46)
    D.sparkles((0, 80, 10), (100, 80, 20), 120, "mid", 47, size=0.25)
    v2_sky(45, ground(0, 300), mountains=False)
    return meta((480, 70), spots3(3, [(-60, 120, 10), (0, 200, 40), (120, 300, 50)]) + [lightpt((0, 60, 8), 30, "fire")], horizon=330, clouds=0.3)


@scene("blossom-meadow/wildflower-hill")
def v2_hill():
    """High on a flowering hillside: the slope falls away to rolling sunlit country, a
    river winding through it, woods, and a white castle on a far hill; mountains beyond."""
    B.camera((0, -16, 14), (0, 900, -30), lens=26)
    v2_light((-0.5, -0.3, 0.6), strength=5.2, sky=0.22)
    slope = rolling(43, 1.6, 0, 0.03)
    f = lambda x, y: slope(x, y) - max(0.0, y - 30) * 0.12
    B.heightfield(f, -160, 160, -10, 175, 170, 100, "near", D.mat_ground())
    D.grass(f, -70, 70, -8, 160, 240000, "near", 43, height=(0.25, 0.6))
    D.flowers(f, -14, 14, -4, 12, 9000, "near", 47, size=0.17, kinds=BLOOM, stem=0.32)
    D.flowers(f, -70, 70, -6, 150, 22000, "near", 44, size=0.24, kinds=BLOOM, stem=0.5)
    D.tree((34, 46, f(34, 46)), 24, 430, "near", kind="blossom", leaves=26000)
    D.tree((-46, 90, f(-46, 90)), 20, 431, "near", kind="green", leaves=16000)
    D.flowers(f, -160, 160, 150, 300, 14000, "mid", 48, size=0.5, kinds=BLOOM)
    B.heightfield(f, -900, 900, 150, 440, 180, 70, "mid", D.mat_ground(), name="lowerslope")
    vz = f(0, 330) + 6
    h = D.valley(43, vz, y0=260, amp=40)
    # The river: a ribbon winding away through the valley.
    ys = np.linspace(320, 3200, 220)
    xs = 160 * np.sin(ys * 0.0021 + 0.6) + 60 * np.sin(ys * 0.006)
    wid = 10 + ys * 0.012
    V, T = [], []
    for k, (x, y, w) in enumerate(zip(xs, ys, wid)):
        V += [(x - w, y, h(x - w, y) + 1.2), (x + w, y, h(x + w, y) + 1.2)]
        if k:
            a_ = 2 * (k - 1)
            T += [(a_, a_ + 1, a_ + 3), (a_, a_ + 3, a_ + 2)]
    D.big_mesh("river", np.array(V), np.array(T), "far", water((0.55, 0.74, 0.9)))
    D.grove(h, -1600, 1600, 400, 2600, 320, "far", 49, size=(14, 30), blossom=0.25, name="valleywood")
    cx, cy = -300, 1300
    b = G.Builder()
    G.citadel(b, cx, cy, h(cx, cy) - 5, 0.95, 45)
    b.flush("far", G.materials(), "valleycastle")
    v2_sky(44, vz, valley=False, clouds=((-900, 2600, 520, 600), (500, 3000, 640, 800), (1300, 2600, 480, 560)))
    return meta((250, 120), spots3(3, [(-50, 70, f(-50, 70) + 6), (60, 220, f(60, 220) + 8), (cx, cy, h(cx, cy) + 60)]), horizon=250, clouds=0.4, haze=0.3)


# ============================================================ GOLDEN CATHEDRAL (v2)
@scene("golden-cathedral/choir-loft")
def v2_loft():
    v2_interior(52, length=320, width=80, height=110, window=(1.0, 0.85, 0.5), bays=8, cam=((0, -15, 58), (0, 300, 40)))
    B.box((0, 0, 49), (90, 34, 2), "near", D.mat_floor("loftfloor"), name="loft")
    b = G.Builder(); rng = random.Random(52)
    for x in range(-40, 42, 2):
        b.prism("stone", x, 14, 50, 0.4, 0.3, 4, sides=8)
    b.box("stone", 0, 14, 54, 82, 1.6, 0.9)
    b.box("gold", 0, 13.2, 54.9, 82, 0.3, 0.3)
    b.flush("near", G.materials(), "loftrail")
    b = G.Builder()
    for k in range(15):
        h = 30 + 18 * math.sin(k * 0.42) ** 2
        b.prism("gold", -28 + k * 4, 318, 20, 1.4, 1.4, h, sides=16)
        b.cone("gold", -28 + k * 4, 318, 20 + h, 1.4, 3, sides=16)
    b.flush("far", G.materials(), "organ")
    D.sparkles((0, 150, 40), (60, 250, 70), 200, "mid", 53, size=0.25)
    return meta((480, 140), spots3(3, [(0, 200, 10), (0, 10, 58), (-30, 300, 50)]) + [lightpt((0, 318, 50), 26, "window")],
                horizon=300, clouds=0.0, rays=0.0)


@scene("golden-cathedral/stained-glass-chapel")
def v2_chapel():
    b = v2_interior(53, length=200, width=70, height=80, window=(0.9, 0.55, 0.85), bays=5, cam=((-18, -12, 16), (34, 150, 46)))
    B.steps((0, 170, 0), 30, 6, 1.4, 3, "mid", D.mat_marble())
    B.box((0, 186, 6.6), (14, 6, 6), "mid", D.mat_gold(), name="altar")
    for k, c in enumerate([(1.0, 0.35, 0.35), (1.0, 0.75, 0.25), (0.35, 0.9, 0.45), (0.35, 0.55, 1.0), (0.75, 0.4, 1.0)]):
        for side in (-1, 1):
            B.light("SPOT", (side * 70, 20 + k * 40, 70), c, 1.5e5, size=3, look_at=(-side * 10, 30 + k * 40, 0), name="glasslight", spot=16)
    D.sparkles((0, 100, 30), (50, 160, 50), 160, "mid", 54, size=0.25)
    return meta((480, 150), spots3(3, [(0, 190, 40), (-26, 80, 30), (0, 30, 10)]) + [lightpt((0, 198, 44), 26, "window"), lightpt((-43, 100, 30), 14, "window")],
                horizon=300, clouds=0.0, rays=0.0)


# =============================================================== FLOATING ISLES (v2)
def isles_world(seed, cam, sun=(-0.55, -0.3, 0.65)):
    B.camera(cam[0], cam[1], lens=24)
    v2_light(sun, strength=5.0, sky=0.3)
    for k, (x, y, z, r) in enumerate([(-220, 420, 170, 50), (240, 520, 230, 60), (-40, 900, 280, 80), (420, 1100, 160, 50), (-500, 1000, 240, 60)]):
        D.isle2(x, y, z, r, r * 1.6, "mid" if y < 800 else "far", seed + k, trees=3)
    for k, (x, y, z, s_) in enumerate([(-300, 300, 20, 260), (300, 400, 40, 280), (0, 1300, 60, 500), (-700, 1500, 100, 500), (700, 1600, 120, 520)]):
        D.cumulus((x, y, z), (s_ * 1.8, s_, s_ * 0.6), "mid" if y < 800 else "far", seed * 9 + k, density=0.015, name="fogcloud_i%d_%d" % (seed, k))


@scene("floating-isles/cloud-bridge")
def v2_bridge():
    isles_world(61, ((0, -40, 200), (0, 400, 200)))
    b = G.Builder(); rng = random.Random(61)
    for k in range(24):
        t = k / 23
        x, y, z = -220 + 460 * t, 420 + 100 * t, 172 + 58 * t + 22 * math.sin(t * math.pi)
        b.box("stone", x, y, z, 20, 9, 1.6, rot=math.atan2(100, 460))
        if k % 2 == 0:
            for side in (-1, 1):
                b.prism("stone", x, y + side * 4.2, z + 1.6, 0.4, 0.35, 4, sides=8)
                b.prism("gold", x, y + side * 4.2, z + 5.6, 0.6, 0.0, 1.2, sides=8)
    for side in (-1, 1):
        G.tower(b, side * 225 - 5 * side, 420 + (side + 1) * 50, 170 + (side + 1) * 30, 6, 40, rng, tiers=2, spire_ratio=1.6, depth=1)
    b.flush("mid", G.materials(), "bridge")
    D.sparkles((0, 470, 230), (400, 120, 60), 120, "mid", 62, size=0.5)
    return meta((720, 100), spots3(3, [(0, 470, 230), (-220, 420, 200), (240, 520, 260)]), horizon=380, clouds=0.35)


@scene("floating-isles/windswept-isle")
def v2_windswept():
    isles_world(62, ((-200, 300, 200), (-220, 420, 180)), sun=(-0.3, -0.5, 0.5))
    b = G.Builder(); rng = random.Random(62)
    G.tower(b, -235, 425, 170, 5, 30, rng, tiers=2, spire_ratio=1.4, depth=0)
    b.flush("mid", G.materials(), "ruin")
    return meta((300, 90), spots3(3, [(200, 600, 260), (-220, 420, 200), (0, 900, 300)]), horizon=360, clouds=0.6, rays=0.4, haze=0.5)


@scene("floating-isles/sky-harbour")
def v2_harbour():
    isles_world(63, ((150, 330, 238), (262, 500, 236)))
    m, gold = D.mat_marble(), D.mat_gold()
    B.box((320, 470, 229), (130, 14, 3), "mid", m, name="pier")
    sail = D.mat_vegetation("sailcloth", 0.5, 0.6)
    for k in range(3):
        x = 318 + k * 34
        B.lathe((x, 452, 221), [(0, 0), (9, 3), (11, 8), (0, 9)], "mid", B.mat_stone("hull2", (0.62, 0.42, 0.25), rough=0.5, bump=0.3, scale=5.0), sides=24, name="hull")
        B.cylinder((x, 452, 229), 0.8, 34, "mid", gold, sides=10, name="mast")
        cols = np.array([[0.97, 0.95, 0.9]] * 4)
        D.big_mesh("sail", np.array([(x - 0.5, 447, 236), (x - 0.5, 457, 236), (x - 2, 457, 260), (x - 2, 447, 260)]), np.array([(0, 1, 2), (0, 2, 3)]), "mid", sail, cols)
        B.cylinder((x, 452, 263), 0.2, 6, "mid", gold, sides=6, name="pennant")
    b = G.Builder(); rng = random.Random(63)
    G.tower(b, 285, 520, 230, 7, 50, rng, tiers=3, spire_ratio=1.8, depth=1)
    b.flush("mid", G.materials(), "harbourtower")
    D.sparkles((250, 480, 250), (120, 60, 40), 80, "mid", 64, size=0.3)
    return meta((640, 120), spots3(3, [(352, 452, 250), (285, 520, 290), (420, 1100, 200)]) + [lightpt((352, 452, 260), 16, "glint")], horizon=380, clouds=0.35)


# ================================================================ ROYAL GARDENS (v2)
def hedges(ground, seed, rows, layer="mid"):
    rng = random.Random(seed)
    for side in (-1, 1):
        for k in range(rows):
            y = 60 + k * 30
            D.tree((side * 30, y, ground(side * 30, y)), 9, seed * 50 + k * 2 + (side > 0), layer, kind="green", leaves=5000)


@scene("royal-gardens/rose-garden")
def v2_rose():
    B.camera((0, -12, 5), (0, 300, 45), lens=24)
    v2_light()
    ground = gentle(71, amp=0.3, rise=0.0)
    roses = ((0.92, 0.25, 0.38), (0.98, 0.6, 0.7), (0.98, 0.97, 0.94))
    rosekinds = (roses, roses, D.PINK, D.DAISY)
    v2_meadow(71, ground, kinds=rosekinds, grass_n=150000, trees=((-28, 30, 18, "blossom"), (30, 34, 18, "blossom")))
    B.heightfield(lambda x, y: ground(x, y) + 0.05, -8, 8, -10, 400, 4, 100, "mid", B.mat_stone("gravel", (0.85, 0.8, 0.7), rough=0.9, bump=0.5, scale=8.0), name="path")
    v2_mid_meadow(71, ground, kinds=rosekinds)
    hedges(ground, 71, 9)
    b = G.Builder()
    G.citadel(b, 0, 700, 0, 0.75, 72)
    b.flush("mid", G.materials(), "palace")
    v2_sky(72, 0, mountains=True)
    return meta((640, 110), spots3(3, [(0, 60, 6), (0, 400, 30), (90, 300, 30)]), horizon=330, clouds=0.35)


@scene("royal-gardens/fountain-court")
def v2_fountain():
    B.camera((0, -10, 8), (0, 200, 30), lens=24)
    v2_light()
    ground = gentle(72, amp=0.2, rise=0.0)
    v2_meadow(72, ground, kinds=BLOOM, grass_n=160000, flowers_n=9000, trees=((-36, 40, 22, "blossom"), (38, 50, 20, "blossom")))
    v2_mid_meadow(72, ground)
    B.cylinder((0, 160, -0.1), 48, 0.5, "mid", B.mat_stone("paving2", (0.82, 0.78, 0.7), rough=0.7, bump=0.5, scale=1.5), sides=64, name="paving")
    B.box((0, 90, 0.0), (12, 140, 0.5), "mid", B.mat_stone("gravel2", (0.86, 0.8, 0.68), rough=0.9, bump=0.6, scale=6.0), name="path")
    b = G.Builder(); rng = random.Random(72)
    # A three-tier fountain with a gilt crown.
    for k, (r, z) in enumerate([(30, 0), (15, 12), (7, 22)]):
        b.prism("stone", 0, 160, z, r, r * 0.9, 3, sides=48)
        b.prism("gold", 0, 160, z + 3, r * 0.92, r * 0.92, 0.4, sides=48)
        b.prism("stone", 0, 160, z, r * 0.2, r * 0.15, 12 if k < 2 else 8, sides=24)
    G.pinnacle(b, 0, 160, 30, 1.6, 10, rng, 1)
    # A gothic arcade around the court.
    for k in range(11):
        x = -100 + k * 20
        b.prism("stone", x, 300, 0, 2.2, 2.0, 30, sides=12)
        b.prism("gold", x, 300, 30, 2.6, 2.4, 1.2, sides=12)
        if k < 10:
            b.lancet("glass", x + 10, 299, 4, 14, 24, -math.pi / 2, depth=0.0)
    b.box("stone", 0, 300, 31, 220, 4, 10)
    b.flush("mid", G.materials(), "fountain")
    for k, (r, z) in enumerate([(28, 3.1), (13.5, 15.1), (6, 25.1)]):
        B.cylinder((0, 160, z), r, 0.3, "mid", water((0.55, 0.75, 0.9)), sides=48, name="pool")
    spray = B.mat_emission((0.85, 0.95, 1.0), 1.5, "spray2")
    for k in range(24):
        a = k * 2 * math.pi / 24
        D.arc(lambda t, a=a: (math.cos(a) * 6 * t * 2.4, 160 + math.sin(a) * 6 * t * 2.4, 34 + 8 * math.sin(t * math.pi) - 10 * t), 10, 0.15, "mid", spray, "jet")
    hedges(ground, 72, 6)
    D.sparkles((0, 160, 30), (60, 40, 30), 200, "mid", 73, size=0.25)
    v2_sky(73, 0)
    return meta((480, 100), spots3(3, [(-80, 300, 20), (0, 160, 30), (80, 300, 40)]) +
                [lightpt((0, 160, 26), 26, "glint"), lightpt((-10, 155, 8), 14, "glint"), lightpt((12, 165, 8), 14, "glint")], horizon=330, clouds=0.35)


@scene("royal-gardens/marble-colonnade")
def v2_colonnade():
    """A long double colonnade of fluted marble columns with gilt capitals and pointed
    arches, seen from the flowering garden beside it, receding into the light."""
    B.camera((-34, -14, 9), (14, 260, 26), lens=24)
    v2_light((-0.6, -0.5, 0.5), strength=5.5, sky=0.2)
    ground = gentle(73, amp=0.3, rise=0.0)
    # The garden beside the walk only (grass under the walk's floor would be shadowed black
    # in its own layer's pass).
    B.heightfield(ground, -220, 7, -10, 90, 170, 80, "near", D.mat_ground())
    D.grass(ground, -130, 6.5, -8, 90, 300000, "near", 73, height=(0.2, 0.55))
    D.flowers(ground, -40, 6, -4, 12, 6000, "near", 74, size=0.16, kinds=BLOOM, stem=0.3)
    D.flowers(ground, -120, 6, -6, 80, 16000, "near", 75, size=0.22, kinds=BLOOM, stem=0.45)
    D.tree((-44, 60, ground(-44, 60)), 22, 730, "near", kind="blossom", leaves=24000)
    v2_mid_meadow(73, ground)
    marble = D.mat_marble()
    b = G.Builder(); rng = random.Random(73)
    n = 16
    for row, x in enumerate((12, 30)):
        for k in range(n):
            y = 20 + k * 22
            b.prism("stone", x, y, 0, 2.6, 2.6, 1.6, sides=8)
            b.prism("stone", x, y, 1.6, 1.7, 1.55, 30, sides=20)
            for c in range(10):
                ca = c * math.pi / 5
                b.prism("stone", x + math.cos(ca) * 1.65, y + math.sin(ca) * 1.65, 1.6, 0.22, 0.2, 30, sides=5)
            b.prism("gold", x, y, 31.6, 2.6, 2.1, 1.6, sides=16)
            b.prism("gold", x, y, 1.6, 2.0, 2.0, 0.5, sides=16)
            if k < n - 1:
                D.pointed_arch(y, y + 22, x, 33.2, 9, 0.8, "mid", marble, name="colarch")
        b.box("stone", x, 20 + (n - 1) * 11, 44, 4, (n - 1) * 22 + 6, 3)
        b.box("gold", x - 2.1, 20 + (n - 1) * 11, 44.5, 0.3, (n - 1) * 22 + 6, 0.8)
    b.box("stone", 21, 20 + (n - 1) * 11, 46, 22, (n - 1) * 22 + 6, 1.2)
    for k in range(n):
        G.pinnacle(b, 12, 20 + k * 22, 45.5, 0.8, 6, rng, 0)
    b.box("stone", 21, 20 + (n - 1) * 11, 0.0, 26, (n - 1) * 22 + 8, 1.2)
    b.flush("mid", G.materials(), "colonnade")
    for ob in [o for o in bpy.data.objects if o.name.startswith("colarch")]:
        for v in ob.data.vertices:
            x0, y0 = v.co.x, v.co.y
            v.co.x, v.co.y = y0, x0
    v2_sky(74, 0)
    return meta((300, 110), spots3(3, [(-20, 60, 6), (12, 140, 20), (30, 300, 30)]), horizon=320, clouds=0.35)


# ================================================================== FAIRY GLADE (v2)
CAPS = ((0.92, 0.38, 0.45), (0.95, 0.72, 0.35), (0.75, 0.55, 0.95))


@scene("fairy-glade/mushroom-ring")
def v2_ring():
    B.camera((0, -10, 5), (0, 200, 20), lens=24)
    v2_light((-0.4, 0.5, 0.6), strength=4.5, sky=0.25, rim=2.0)
    ground = gentle(81, amp=1.0, rise=0.01)
    v2_meadow(81, ground, kinds=PASTEL, trees=((-30, 40, 36, "green"), (34, 44, 32, "green")))
    v2_mid_meadow(81, ground, kinds=PASTEL)
    forest(ground, 81, 18)
    for k in range(12):
        a = k / 12 * 2 * math.pi
        x, y = 34 * math.cos(a), 110 + 22 * math.sin(a)
        D.mushroom2(x, y, ground(x, y), 7 + 3 * (k % 3), "mid", 810 + k, cap=CAPS[k % 3])
    D.sparkles((0, 110, 8), (80, 50, 16), 300, "mid", 82, size=0.3)
    v2_sky(82, 0, mountains=False)
    return meta((480, 90), spots3(3, [(0, 110, 6), (-100, 240, 30), (100, 260, 30)]) + [lightpt((x, 110, 12), 10, "fire") for x in (-30, 0, 30)],
                horizon=320, clouds=0.3)


@scene("fairy-glade/firefly-hollow")
def v2_hollow():
    B.camera((0, -10, 4), (0, 200, 30), lens=24)
    v2_light((-0.2, 0.9, 0.2), strength=3.0, sky=0.25, rim=2.5)
    ground = gentle(82, amp=1.5, rise=0.01)
    v2_meadow(82, ground, kinds=(D.LAVENDER, D.DAISY), trees=((-26, 30, 40, "green"), (28, 34, 36, "green")))
    v2_mid_meadow(82, ground, kinds=(D.LAVENDER, D.DAISY))
    forest(ground, 82, 26, h=(50, 90))
    D.sparkles((0, 120, 15), (180, 200, 40), 900, "mid", 83, size=0.3)
    D.sparkles((0, 30, 3), (40, 40, 6), 120, "near", 84, size=0.08)
    v2_sky(83, 0, mountains=False)
    return meta((480, 50), spots3(3, [(-80, 160, 10), (0, 140, 14), (90, 220, 20)]) + [lightpt((x, 150, 20), 12, "fire") for x in (-60, -10, 40, 80)],
                horizon=300, clouds=0.2, rays=1.0)


@scene("fairy-glade/fairy-court")
def v2_court():
    B.camera((0, -20, 6), (0, 80, 14), lens=24)
    v2_light((-0.4, 0.3, 0.7), strength=4.5, sky=0.25)
    ground = gentle(83, amp=0.4, rise=0.0)
    B.heightfield(ground, -90, 90, -20, 160, 140, 140, "mid", D.mat_ground())
    D.grass(ground, -60, 60, -18, 140, 220000, "mid", 83, height=(0.3, 0.75))
    D.flowers(ground, -50, 50, -16, 130, 14000, "mid", 84, size=0.2, kinds=BLOOM, stem=0.4)
    forest(ground, 830, 22, x=(-200, 200), y=(130, 360), h=(40, 70), blossom=0.3)
    for k in range(9):
        a = k / 8 * math.pi
        x, y = -55 * math.cos(a), 80 + 40 * math.sin(a)
        D.mushroom2(x, y, ground(x, y), 22 + 4 * (k % 2), "mid", 830 + k, cap=CAPS[k % 3])
    gold = D.mat_gold()
    B.lathe((0, 60, 0), [(5, 0), (4, 2), (1, 3), (1, 12), (0, 12.5)], "mid", gold, sides=24, name="harpbase")
    D.arc(lambda t: (-5 + 10 * t, 60, 3 + 14 * math.sin(t * math.pi * 0.9)), 16, 0.5, "mid", gold, "harpneck")
    for k in range(9):
        x = -4 + k
        B.cylinder((x, 60, 3), 0.06, 13 * math.sin((x + 5) / 10 * math.pi * 0.9), "mid", gold, sides=4, name="string")
    D.sparkles((0, 80, 15), (100, 80, 30), 300, "mid", 85, size=0.3)
    v2_sky(84, 0, mountains=False)
    return meta((480, 120), spots3(3, [(-40, 100, 20), (40, 110, 20), (0, 60, 10)]) + [lightpt((0, 60, 10), 18, "glint"), lightpt((0, 90, 30), 22, "fire")],
                horizon=300, clouds=0.3)


# =============================================================== DRAGON'S PEAK (v2)
def peak_world(seed, cam, sun=(-0.6, -0.3, 0.6)):
    B.camera(cam[0], cam[1], lens=24)
    v2_light(sun, strength=5.5, sky=0.25)
    D.mesa(0, 600, -100, 170, 520, "mid", seed)
    D.mesa(-320, 950, -100, 130, 400, "far", seed + 1)
    D.mesa(330, 1050, -100, 140, 440, "far", seed + 2)
    v2_sky(seed + 3, -100, clouds=((-500, 1800, 300, 700), (400, 2200, 400, 800), (0, 2800, 600, 1000)))
    for k, (x, y, z) in enumerate([(-400, 500, 80), (300, 420, 50)]):
        D.cumulus((x, y, z), (420, 260, 160), "mid", seed * 3 + k, density=0.015, name="fogcloud_p%d_%d" % (seed, k))


@scene("dragons-peak/eyrie")
def v2_eyrie():
    peak_world(91, ((0, -60, 80), (0, 500, 230)))
    b = G.Builder(); rng = random.Random(91)
    G.tower(b, -60, 560, 418, 10, 90, rng, tiers=4, spire_ratio=1.8, depth=1)
    G.tower(b, 50, 580, 418, 7, 60, rng, tiers=3, spire_ratio=1.6, depth=1)
    b.flush("mid", G.materials(), "eyrie")
    golden_dragon((0, 540, 420), 7.0, "mid", rotation_z=0.4)
    return meta((700, 90), spots3(3, [(0, 540, 450), (-120, 500, 250), (0, 600, 520)]) + [lightpt((0, 540, 450), 20, "glint")], horizon=350, clouds=0.3)


@scene("dragons-peak/golden-nest")
def v2_nest():
    B.camera((0, -10, 40), (0, 160, 16), lens=24)
    v2_light((-0.5, -0.4, 0.6), strength=5.0, sky=0.25)
    D.mesa(0, 160, -170, 160, 170, "mid", 92)
    B.lathe((0, 150, 0), [(46, 0), (50, 6), (40, 10), (0, 8)], "mid", B.mat_stone("nest2", (0.6, 0.46, 0.3), bump=0.9, scale=6.0), sides=64, name="nest")
    rng = random.Random(92)
    for k in range(200):
        B.cylinder((rng.uniform(-38, 38), 150 + rng.uniform(-30, 30), rng.uniform(8, 12)), rng.uniform(0.8, 2.0), 0.35, "mid", D.mat_gold(rough=0.18), sides=12, name="coin")
    golden_dragon((-70, 420, 90), 7.0, "mid", rotation_z=0.9)
    # A heap of gold under the dragon: a mound and coins strewn over it.
    B.lathe((0, 150, 7), [(40, 0), (34, 3), (24, 8), (12, 12.5), (0, 14)], "mid", D.mat_gold(rough=0.3), sides=64, name="hoard")
    for k in range(420):
        rr = 36 * math.sqrt(rng.random()); a = rng.uniform(0, 2 * math.pi)
        hz = 7 + 14 * max(0.0, 1 - rr / 40) ** 1.3
        B.cylinder((rr * math.cos(a), 150 + rr * math.sin(a), hz), rng.uniform(0.8, 1.6), 0.3, "mid", D.mat_gold(rough=0.18), sides=12, name="coin",
                   rotation=(rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.4), 0))
    D.mesa(-400, 900, -200, 150, 420, "far", 93)
    D.mesa(380, 1000, -200, 160, 460, "far", 94)
    v2_sky(95, -200)
    D.sparkles((0, 150, 20), (90, 70, 30), 200, "mid", 96, size=0.3)
    return meta((650, 110), spots3(3, [(-40, 200, 40), (0, 150, 22), (40, 210, 40)]) + [lightpt((x, 150, 12), 12, "glint") for x in (-25, 0, 25)],
                horizon=300, clouds=0.35)


@scene("dragons-peak/summit-of-winds")
def v2_winds():
    """The highest peak: a wind-carved ridge of rock and snow in front, a sea of cloud far
    below with other sharp peaks rising through it, the golden dragon soaring past."""
    B.camera((0, -30, 560), (0, 700, 480), lens=24)
    v2_light((-0.4, -0.5, 0.45), strength=5.5, sky=0.25)
    D.peak(230, 160, 300, 220, 210, "near", 97, snow_line=0.6, name="ridge")
    for k, (x, y, r, hgt) in enumerate([(-460, 900, 380, 640), (420, 1150, 420, 700), (-60, 1800, 520, 820), (1000, 1900, 460, 620), (-1100, 1700, 480, 660)]):
        D.peak(x, y, 0, r, hgt, "mid" if y < 1200 else "far", 100 + k, name="peak%d" % k)
    for k in range(9):
        x = -1400 + k * 350; y = 600 + (k % 3) * 500
        D.cumulus((x, y, 240), (520, 380, 170), "mid" if y < 1200 else "far", 970 + k, density=0.016, name="fogcloud_w%d" % k)
    D.mountains(99, y=2600, height=900, base_z=0)
    for k, (cx_, cy_) in enumerate([(-120, 380), (160, 420), (0, 560)]):
        D.cumulus((cx_, cy_, 250), (620, 360, 160), "mid", 990 + k, density=0.018, name="fogcloud_sea%d" % k)
    golden_dragon((-60, 420, 520), 6.0, "mid", rotation_z=2.2)
    D.sparkles((0, 200, 540), (200, 120, 60), 60, "near", 96, size=0.3)
    return meta((360, 110), spots3(3, [(-60, 420, 540), (-460, 900, 600), (30, 90, 545)]) + [lightpt((-60, 420, 530), 18, "glint")],
                horizon=330, clouds=0.4, rays=0.9, haze=0.3)


# ========================================================================= MAP (v2)
@scene("map")
def v2_map():
    """The bright world from high above: a patchwork of sunlit fields and woods, a river
    winding from the snowy mountains in the north to a sea in the south-west, cumulus
    drifting over the edges; each location a luminous landmark."""
    B.camera((0, -520, 470), (0, 820, 40), lens=28)
    v2_light((-0.5, -0.4, 0.6), strength=5.0, sky=0.3)
    f = rolling(3, 30, 0, 0.003)

    def land(x, y):
        d = math.hypot(x + 1150, y + 120)
        sea = 90 * min(1.0, max(0.0, (820 - d) / 260))
        return f(x, y) + max(0.0, (y - 600) * 0.12) - sea
    B.heightfield(land, -1700, 1700, -400, 2300, 300, 240, "mid", D.mat_fields("mapfields", (0.22, 0.38, 0.12), scale=0.009))
    B.box((-900, 0, -42), (1800, 1600, 2), "mid", water((0.38, 0.62, 0.8)), name="sea")
    # The river: from the mountains to the sea.
    ys = np.linspace(2200, -60, 200)
    xs = -420 + 260 * np.sin(ys * 0.0019 + 1.2) - 300 * np.clip((600 - ys) / 660, 0, 1)
    V, T = [], []
    for k, (x, y) in enumerate(zip(xs, ys)):
        w = 7 + (2200 - y) * 0.006
        V += [(x - w, y, land(x - w, y) + 1.5), (x + w, y, land(x + w, y) + 1.5)]
        if k:
            a_ = 2 * (k - 1); T += [(a_, a_ + 1, a_ + 3), (a_, a_ + 3, a_ + 2)]
    D.big_mesh("river", np.array(V), np.array(T), "mid", water((0.5, 0.72, 0.9)))
    L = [(0, 1100), (520, 1300), (-560, 700), (700, 650), (-200, 350), (300, 500), (-420, 1450), (120, 150), (-760, 300), (900, 1500)]
    # Woods everywhere (clumped), not on the landmarks or in the sea.
    D.grove(land, -1600, 1600, -200, 2000, 2200, "mid", 41, size=(22, 40), blossom=0.12, name="mapwood", clump=0.85,
            avoid=[(x, y, 120) for x, y in L], min_z=-30)
    D.mountains(9, y=2080, height=820, base_z=land(0, 2080) - 30, x0=-3000, x1=3000, layer="mid")   # same layer as the land: no shadow seam
    anchors = []
    rng = random.Random(3)
    b = G.Builder()
    for i, (x, y) in enumerate(L):
        z = land(x, y)
        if i == 0:
            D.mesa(x, y, z - 30, 100, 120, "mid", 300)
            G.citadel(b, x, y, z + 90, 0.85, 301)
        elif i == 1:
            D.cumulus((x, y, z + 40), (190, 130, 60), "mid", 302, density=0.02, name="fogcloud_temple")
            G.tower(b, x, y, z + 60, 18, 45, rng, tiers=2, spire_ratio=1.6, depth=1)
            b.prism("stone", x, y, z + 52, 36, 36, 8, sides=32)
        elif i == 2:
            D.grove(land, x - 90, x + 90, y - 60, y + 60, 70, "mid", 303, size=(40, 70), blossom=0.0, name="elvenwood", clump=0.2)
            B.cylinder((x, y - 70, z + 2), 20, 2, "mid", water((0.5, 0.7, 0.85)), sides=48, name="well")
        elif i == 3:
            D.crystal_cluster(x, y, z, 140, 16, "mid", 304)
        elif i == 4:
            D.grove(land, x - 90, x + 90, y - 40, y + 40, 40, "mid", 305, size=(18, 30), blossom=1.0, name="blossomwood", clump=0.3)
            D.flowers(land, x - 100, x + 100, y - 60, y + 60, 6000, "mid", 5, size=1.4, kinds=BLOOM, stem=0.5)
        elif i == 5:
            G.tower(b, x, y, z, 22, 100, rng, tiers=3, spire_ratio=2.2, depth=1)
            G.tower(b, x - 40, y + 10, z, 9, 50, rng, tiers=2, spire_ratio=2.0, depth=0)
            G.tower(b, x + 40, y + 10, z, 9, 50, rng, tiers=2, spire_ratio=2.0, depth=0)
        elif i == 6:
            for k in range(3):
                D.isle2(x + (k - 1) * 130, y + 40 * k, z + 120 + 50 * k, 60, 90, "mid", 306 + k, trees=2)
        elif i == 7:
            hedge = B.mat_stone("maphedge", (0.1, 0.26, 0.07), rough=0.9, bump=0.8, scale=0.5)
            for k in range(-3, 4):
                B.box((x + k * 22, y, z + 3), (16, 70, 6), "mid", hedge, name="parterre")
            b.prism("stone", x, y - 50, z, 16, 14, 5, sides=32)
            G.pinnacle(b, x, y - 50, z + 5, 1.6, 16, rng, 0)
        elif i == 8:
            for k in range(5):
                D.mushroom2(x + (k - 2) * 34, y + 10 * (k % 2), z, 44, "mid", 320 + k, cap=CAPS[k % 3])
            D.grove(land, x - 80, x + 80, y + 30, y + 90, 25, "mid", 321, size=(20, 34), name="fairywood")
        elif i == 9:
            D.peak(x, y, z - 40, 160, 300, "mid", 330, snow_line=0.7, name="dragonpeak")
            G.tower(b, x, y, z + 215, 10, 40, rng, tiers=2, spire_ratio=1.6, depth=1)
        anchors.append({"kind": "landmark", "index": i, "world": (x, y, z + 80)})
    b.flush("mid", G.materials(), "maplandmarks")
    for k, (x, y, z) in enumerate([(-300, 1900, 260), (500, 2000, 300), (-1000, 1200, 220), (1100, 900, 200)]):
        D.cumulus((x, y, z), (400, 260, 180), "far", 340 + k, density=0.012, name="fogcloud_m%d" % k)
    anchors += [lightpt((0, 1100, 260), 30, "glint"), lightpt((300, 500, 120), 20, "glint"), lightpt((700, 650, 90), 18, "glint"),
                lightpt((0, 1100, 200), 24, "fire"), lightpt((300, 500, 90), 18, "fire"), lightpt((-760, 300, 50), 16, "fire")]
    return meta((760, 40), anchors, horizon=120, clouds=0.3, rays=0.5, haze=0.3)
