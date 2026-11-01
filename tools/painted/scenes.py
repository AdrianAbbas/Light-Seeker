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


def meta(sun, anchors, horizon=300, clouds=0.75, rays=0.6, haze=0.55, exposure=1.0, sky_seed=1):
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
    anchors += [lightpt((0, 1100, 260), 30, "glint"), lightpt((300, 500, 120), 20, "glint"), lightpt((700, 650, 90), 18, "glint")]
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
        B.steps((0, 250, 0), 30, 6, 1.2, 3, "mid", pearl())
        B.box((0, 262, 7.2), (8, 6, 16), "mid", gold(0.2), name="throne")
        B.box((0, 265, 18), (10, 1.5, 22), "mid", gold(0.2), name="throneback")
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
    return meta(sun, [hotspot(0, (0, 20, 10)), hotspot(1, (0, 150, 30)), hotspot(2, (0, 280, 25)), hotspot(3, (0, 288, 60)),
                      lightpt((-42, 90, 36), 16, "glint"), lightpt((42, 150, 36), 16, "glint"), lightpt((-42, 210, 36), 14, "glint")],
                horizon=300, clouds=0.0, rays=0.9, haze=0.35)


@scene("celestial-citadel/throne-of-light")
def citadel_throne():
    sun = (480, 180)
    citadel_hall(3, sun, throne=True)
    return meta(sun, [hotspot(0, (0, 30, 10)), hotspot(1, (-30, 150, 40)), hotspot(2, (0, 262, 20)), hotspot(3, (0, 288, 66)),
                      lightpt((0, 262, 18), 22, "fire"), lightpt((0, 265, 30), 16, "glint")],
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
    sun = (480, 160)
    interior(12, sun, wall=pearl(), window=(0.95, 0.9, 1.0), end_window=(1.0, 0.95, 0.85))
    B.lathe((0, 230, 0), [(10, 0), (9, 6), (4, 7), (4, 14), (0, 15)], "mid", marble(), sides=24, name="altar")
    for k in range(5):
        B.cylinder((-12 + k * 6, 222, 15), 0.6, 3, "mid", pearl(), sides=8, name="candle")
    return place_meta(sun, 3, HALL3, [lightpt((0, 230, 20), 24, "candle"), lightpt((-12, 222, 19), 8, "candle"), lightpt((12, 222, 19), 8, "candle")],
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
