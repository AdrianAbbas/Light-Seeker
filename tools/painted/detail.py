"""High-detail builders for LIGHT SEEKER's painted scenes (the detail pass).

Everything fine is built directly as large meshes with numpy (no particle systems: full
control of orientation, and Cycles renders a few million triangles quickly): grass blades,
meadow flowers (daisies, buttercups, blossoms, lavender), trees with branches and thousands
of leaves or blossom petals, volumetric cumulus clouds, detailed white-gold towers and
castles, layered cliffs with moss. Colour variation is a per-vertex colour attribute
("col") read by the materials.
"""
import math
import random

import bpy
import numpy as np
from mathutils import Vector

import blib as B

_mats = {}


# ------------------------------------------------------------------------------- meshes
def big_mesh(name, verts, tris, layer, material, colours=None, smooth=False):
    """A mesh from numpy arrays: verts (N, 3), tris (M, 3), colours (N, 3) optional."""
    verts = np.asarray(verts, np.float32).reshape(-1, 3)
    tris = np.asarray(tris, np.int32).reshape(-1, 3)
    me = bpy.data.meshes.new(name)
    me.vertices.add(len(verts))
    me.vertices.foreach_set("co", verts.ravel())
    me.loops.add(len(tris) * 3)
    me.loops.foreach_set("vertex_index", tris.ravel())
    me.polygons.add(len(tris))
    me.polygons.foreach_set("loop_start", np.arange(0, len(tris) * 3, 3, dtype=np.int32))
    me.update(calc_edges=True)
    if colours is not None:
        attr = me.color_attributes.new("col", "FLOAT_COLOR", "POINT")
        rgba = np.ones((len(verts), 4), np.float32)
        rgba[:, :3] = np.asarray(colours, np.float32).reshape(-1, 3)
        attr.data.foreach_set("color", rgba.ravel())
    if smooth:
        me.polygons.foreach_set("use_smooth", np.ones(len(tris), bool))
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    me.materials.append(material)
    return B.put(ob, layer)


def _principled(name):
    m = bpy.data.materials.new(name)
    nt, out = B._nodes(m)
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    return m, nt, out, bsdf


def _attr_colour(nt, scale=1.0):
    a = nt.nodes.new("ShaderNodeAttribute")
    a.attribute_name = "col"
    if scale == 1.0:
        return a.outputs["Color"]
    mul = nt.nodes.new("ShaderNodeMix"); mul.data_type = "RGBA"; mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    nt.links.new(a.outputs["Color"], mul.inputs["A"])
    mul.inputs["B"].default_value = (scale, scale, scale, 1)
    return mul.outputs["Result"]


# ---------------------------------------------------------------------------- materials
def mat_vegetation(name="foliage", translucency=0.35, rough=0.55, sheen=0.0):
    """Leaves, blades, petals: the vertex colour, lit from both sides (translucent)."""
    key = ("veg", name, translucency, rough)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    col = _attr_colour(nt)
    nt.links.new(col, bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = rough
    tr = nt.nodes.new("ShaderNodeBsdfTranslucent")
    nt.links.new(col, tr.inputs["Color"])
    mix = nt.nodes.new("ShaderNodeMixShader")
    mix.inputs["Fac"].default_value = translucency
    nt.links.new(bsdf.outputs[0], mix.inputs[1])
    nt.links.new(tr.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def mat_marble(name="marble2", colour=(0.86, 0.83, 0.78), vein=(0.62, 0.6, 0.6), rough=0.32):
    """White marble: soft grey veins (a distorted wave), slight subsurface, fine bump."""
    key = ("marble", name, colour)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    noise = nt.nodes.new("ShaderNodeTexNoise"); noise.inputs["Scale"].default_value = 0.6; noise.inputs["Detail"].default_value = 8
    nt.links.new(tc.outputs["Object"], noise.inputs["Vector"])
    wave = nt.nodes.new("ShaderNodeTexWave"); wave.inputs["Scale"].default_value = 0.35
    wave.inputs["Distortion"].default_value = 9.0; wave.inputs["Detail"].default_value = 6
    nt.links.new(tc.outputs["Object"], wave.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.0; ramp.color_ramp.elements[0].color = (*vein, 1)
    ramp.color_ramp.elements[1].position = 0.12; ramp.color_ramp.elements[1].color = (*colour, 1)
    nt.links.new(wave.outputs["Fac"], ramp.inputs["Fac"])
    tint = nt.nodes.new("ShaderNodeMix"); tint.data_type = "RGBA"; tint.blend_type = "MULTIPLY"
    tint.inputs["Factor"].default_value = 0.12
    nt.links.new(ramp.outputs["Color"], tint.inputs["A"]); nt.links.new(noise.outputs["Color"], tint.inputs["B"])
    nt.links.new(tint.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = rough
    try:
        bsdf.inputs["Subsurface Weight"].default_value = 0.08
        bsdf.inputs["Subsurface Radius"].default_value = (0.4, 0.3, 0.25)
    except KeyError:
        pass
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.08
    fine = nt.nodes.new("ShaderNodeTexNoise"); fine.inputs["Scale"].default_value = 40; fine.inputs["Detail"].default_value = 4
    nt.links.new(tc.outputs["Object"], fine.inputs["Vector"])
    nt.links.new(fine.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def mat_gold(name="gold2", rough=0.24):
    key = ("gold", name, rough)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    bsdf.inputs["Base Color"].default_value = (1.0, 0.72, 0.32, 1)
    bsdf.inputs["Metallic"].default_value = 1.0
    tc = nt.nodes.new("ShaderNodeTexCoord")
    noise = nt.nodes.new("ShaderNodeTexNoise"); noise.inputs["Scale"].default_value = 8; noise.inputs["Detail"].default_value = 6
    nt.links.new(tc.outputs["Object"], noise.inputs["Vector"])
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.inputs["To Min"].default_value = rough * 0.6; mr.inputs["To Max"].default_value = rough * 1.6
    nt.links.new(noise.outputs["Fac"], mr.inputs["Value"])
    nt.links.new(mr.outputs["Result"], bsdf.inputs["Roughness"])
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def mat_glass_window(name="window2"):
    """A window: deep sky-blue glass catching a warm glow from within."""
    key = ("win", name)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    bsdf.inputs["Base Color"].default_value = (0.25, 0.36, 0.5, 1)
    bsdf.inputs["Roughness"].default_value = 0.08
    bsdf.inputs["Metallic"].default_value = 0.4
    bsdf.inputs["Emission Color"].default_value = (1.0, 0.78, 0.45, 1)
    bsdf.inputs["Emission Strength"].default_value = 0.35
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def mat_rock(name="rock2", colour=(0.55, 0.52, 0.48), moss=(0.2, 0.32, 0.1), moss_amount=0.55):
    """Layered cliff stone: strata, crevices (bump), green moss and grass on the tops."""
    key = ("rock", name, colour, moss_amount)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    noise = nt.nodes.new("ShaderNodeTexNoise"); noise.inputs["Scale"].default_value = 0.08; noise.inputs["Detail"].default_value = 12
    noise.inputs["Roughness"].default_value = 0.62
    nt.links.new(tc.outputs["Object"], noise.inputs["Vector"])
    strata = nt.nodes.new("ShaderNodeTexWave"); strata.wave_type = "BANDS"; strata.bands_direction = "Z"
    strata.inputs["Scale"].default_value = 0.05; strata.inputs["Distortion"].default_value = 4; strata.inputs["Detail"].default_value = 4
    nt.links.new(tc.outputs["Object"], strata.inputs["Vector"])
    stone = nt.nodes.new("ShaderNodeMix"); stone.data_type = "RGBA"
    stone.inputs["A"].default_value = (*[c * 0.62 for c in colour], 1); stone.inputs["B"].default_value = (*colour, 1)
    nt.links.new(strata.outputs["Fac"], stone.inputs["Factor"])
    # Moss where the surface faces up (geometry normal z), broken up by noise.
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(geo.outputs["Normal"], sep.inputs["Vector"])
    up = nt.nodes.new("ShaderNodeMapRange")
    up.inputs["From Min"].default_value = 0.55; up.inputs["From Max"].default_value = 0.85
    nt.links.new(sep.outputs["Z"], up.inputs["Value"])
    mm = nt.nodes.new("ShaderNodeMath"); mm.operation = "MULTIPLY"
    nt.links.new(up.outputs["Result"], mm.inputs[0]); mm.inputs[1].default_value = moss_amount
    mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"
    nt.links.new(mm.outputs[0], mix.inputs["Factor"])
    nt.links.new(stone.outputs["Result"], mix.inputs["A"]); mix.inputs["B"].default_value = (*moss, 1)
    nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.85
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.9; bump.inputs["Distance"].default_value = 0.6
    nt.links.new(noise.outputs["Fac"], bump.inputs["Height"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def mat_ground(name="ground2", base=(0.16, 0.32, 0.07)):
    """Meadow ground under the grass: patches of lighter, darker and golden green."""
    key = ("ground", name, base)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    noise = nt.nodes.new("ShaderNodeTexNoise"); noise.inputs["Scale"].default_value = 0.02; noise.inputs["Detail"].default_value = 6
    nt.links.new(tc.outputs["Object"], noise.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (base[0] * 0.7, base[1] * 0.75, base[2] * 0.6, 1)
    ramp.color_ramp.elements[1].color = (base[0] * 1.6 + 0.08, base[1] * 1.25, base[2] * 0.9, 1)
    nt.links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.95
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


CLOUD_GLOW = 0.65


def mat_cloud(name="cumulus", density=6.0, w=0.0):
    """A cumulus volume: a billowing shape from distorted noise inside the object's box,
    flat-bottomed, bright white scattering with a faint warm forward glow."""
    key = ("cloud", name, density, w)
    if key in _mats:
        return _mats[key]
    m = bpy.data.materials.new(name)
    nt, out = B._nodes(m)
    vol = nt.nodes.new("ShaderNodeVolumePrincipled")
    vol.inputs["Color"].default_value = (1.0, 0.99, 0.97, 1)
    vol.inputs["Anisotropy"].default_value = 0.35
    tc = nt.nodes.new("ShaderNodeTexCoord")
    # Shape: 1 - |p| (p in the box's -1..1), distorted by noise: billows.
    dist = nt.nodes.new("ShaderNodeTexNoise"); dist.inputs["Scale"].default_value = 1.6; dist.inputs["Detail"].default_value = 6
    dist.noise_dimensions = "4D"; dist.inputs["W"].default_value = w   # each cloud its own billows
    dist.inputs["Roughness"].default_value = 0.6
    nt.links.new(tc.outputs["Generated"], dist.inputs["Vector"])
    centre = nt.nodes.new("ShaderNodeVectorMath"); centre.operation = "SUBTRACT"
    nt.links.new(tc.outputs["Generated"], centre.inputs[0]); centre.inputs[1].default_value = (0.5, 0.5, 0.35)
    scale = nt.nodes.new("ShaderNodeVectorMath"); scale.operation = "MULTIPLY"
    nt.links.new(centre.outputs[0], scale.inputs[0]); scale.inputs[1].default_value = (2.0, 2.0, 2.6)
    length = nt.nodes.new("ShaderNodeVectorMath"); length.operation = "LENGTH"
    nt.links.new(scale.outputs[0], length.inputs[0])
    shape = nt.nodes.new("ShaderNodeMath"); shape.operation = "SUBTRACT"; shape.inputs[0].default_value = 1.0
    nt.links.new(length.outputs["Value"], shape.inputs[1])
    billow = nt.nodes.new("ShaderNodeMath"); billow.operation = "MULTIPLY_ADD"
    nt.links.new(dist.outputs["Fac"], billow.inputs[0]); billow.inputs[1].default_value = 1.1; billow.inputs[2].default_value = -0.55
    add = nt.nodes.new("ShaderNodeMath"); add.operation = "ADD"
    nt.links.new(shape.outputs[0], add.inputs[0]); nt.links.new(billow.outputs[0], add.inputs[1])
    # A flatter base: fade out below 30 % of the height.
    sep = nt.nodes.new("ShaderNodeSeparateXYZ"); nt.links.new(tc.outputs["Generated"], sep.inputs["Vector"])
    base = nt.nodes.new("ShaderNodeMapRange")
    base.inputs["From Min"].default_value = 0.18; base.inputs["From Max"].default_value = 0.32
    nt.links.new(sep.outputs["Z"], base.inputs["Value"])
    mul = nt.nodes.new("ShaderNodeMath"); mul.operation = "MULTIPLY"
    nt.links.new(add.outputs[0], mul.inputs[0]); nt.links.new(base.outputs["Result"], mul.inputs[1])
    dens = nt.nodes.new("ShaderNodeMapRange")
    dens.inputs["From Min"].default_value = 0.0; dens.inputs["From Max"].default_value = 0.25
    dens.inputs["To Min"].default_value = 0.0; dens.inputs["To Max"].default_value = density
    nt.links.new(mul.outputs[0], dens.inputs["Value"])
    nt.links.new(dens.outputs["Result"], vol.inputs["Density"])
    # A soft self-glow in proportion to the density: stands in for the many bounces of
    # light inside a real cloud (one volume bounce alone leaves them grey).
    glow = nt.nodes.new("ShaderNodeMath"); glow.operation = "MULTIPLY"
    nt.links.new(dens.outputs["Result"], glow.inputs[0]); glow.inputs[1].default_value = CLOUD_GLOW   # radiance inside = CLOUD_GLOW
    nt.links.new(glow.outputs[0], vol.inputs["Emission Strength"])
    vol.inputs["Emission Color"].default_value = (1.0, 0.97, 0.93, 1)
    nt.links.new(vol.outputs[0], out.inputs["Volume"])
    _mats[key] = m
    return m


# ------------------------------------------------------------------------- vegetation
def grass(fn, x0, x1, y0, y1, count, layer, seed, height=(0.25, 0.7), width=0.025, near_y=None,
          base=(0.07, 0.2, 0.03), tip=(0.42, 0.55, 0.12), name="grass"):
    """Curved, tapered blades (two quads and a tip each); denser near the viewer (near_y)."""
    rng = np.random.default_rng(seed)
    # Denser near the camera: y drawn with a bias towards y0.
    u = rng.random(count)
    ys = y0 + (y1 - y0) * u ** 1.8
    xs = x0 + (x1 - x0) * rng.random(count)
    zs = np.array([fn(x, y) for x, y in zip(xs, ys)], np.float32)
    grow = 1.0 + (ys - y0) / max(1.0, y1 - y0) * 3.0            # far blades bigger (still a few px)
    h = (height[0] + (height[1] - height[0]) * rng.random(count)) * grow
    w = width * grow * (0.7 + 0.6 * rng.random(count))
    yaw = rng.random(count) * 2 * np.pi
    lean = (rng.random(count) * 0.6 + 0.15)
    lean_dir = rng.random(count) * 2 * np.pi
    # Profile along the blade (t = 0, 0.5, 1): offsets along lean, widths.
    ts = np.array([0.0, 0.5, 1.0], np.float32)
    nv = 5
    verts = np.zeros((count, nv, 3), np.float32)
    cx, cy = np.cos(yaw), np.sin(yaw)          # across the blade
    lx, ly = np.cos(lean_dir), np.sin(lean_dir)
    for i, t in enumerate(ts[:2]):
        bend = lean * h * t ** 1.6
        hw = w * (1.0 - 0.45 * t)
        px = xs + lx * bend; py = ys + ly * bend; pz = zs + h * t * (1 - 0.25 * lean * t)
        verts[:, 2 * i, 0] = px - cx * hw; verts[:, 2 * i, 1] = py - cy * hw; verts[:, 2 * i, 2] = pz
        verts[:, 2 * i + 1, 0] = px + cx * hw; verts[:, 2 * i + 1, 1] = py + cy * hw; verts[:, 2 * i + 1, 2] = pz
    bend = lean * h
    verts[:, 4, 0] = xs + lx * bend; verts[:, 4, 1] = ys + ly * bend; verts[:, 4, 2] = zs + h * (1 - 0.25 * lean)
    tris = np.array([[0, 1, 3], [0, 3, 2], [2, 3, 4]], np.int32)
    tris = (tris[None, :, :] + (np.arange(count) * nv)[:, None, None]).reshape(-1, 3)
    # Colour: dark base to a sunlit tip, each blade a little different (some golden).
    var = rng.random(count)[:, None]
    b = np.array(base)[None, :] * (0.8 + 0.4 * var)
    tp = np.array(tip)[None, :] * (0.75 + 0.5 * var) + np.array([0.12, 0.05, 0.0]) * (rng.random(count)[:, None] < 0.12)
    cols = np.zeros((count, nv, 3), np.float32)
    cols[:, 0] = b; cols[:, 1] = b; cols[:, 2] = (b + tp) * 0.5; cols[:, 3] = (b + tp) * 0.5; cols[:, 4] = tp
    return big_mesh(name, verts.reshape(-1, 3), tris, layer, mat_vegetation("grassblade", 0.3, 0.5), cols.reshape(-1, 3))


DAISY = ((0.97, 0.96, 0.92), (0.98, 0.72, 0.1))       # petals, centre
BUTTERCUP = ((0.98, 0.8, 0.1), (0.95, 0.6, 0.05))
PINK = ((0.96, 0.55, 0.7), (0.98, 0.85, 0.3))
LAVENDER = ((0.62, 0.52, 0.9), (0.95, 0.85, 0.5))
CORAL = ((0.98, 0.62, 0.5), (0.98, 0.8, 0.3))


def flowers(fn, x0, x1, y0, y1, count, layer, seed, size=0.12, kinds=(DAISY, DAISY, DAISY, BUTTERCUP, PINK, LAVENDER), name="flowers",
            stem=0.35):
    """Meadow flowers: a ring of petals (12 for daisies, 6 for the others) around a domed
    centre, held a little above the ground, facing up and slightly towards the light."""
    rng = random.Random(seed)
    verts, tris, cols = [], [], []
    for _ in range(count):
        u = rng.random()
        y = y0 + (y1 - y0) * u ** 1.6
        x = rng.uniform(x0, x1)
        grow = 1.0 + (y - y0) / max(1.0, y1 - y0) * 3.0
        s = size * grow * rng.uniform(0.7, 1.3)
        kind = kinds[rng.randrange(len(kinds))]
        petals = 13 if kind is DAISY else 6
        z = fn(x, y) + stem * grow * rng.uniform(0.6, 1.2)
        tilt = rng.uniform(0.1, 0.5)
        ta = rng.uniform(0, 2 * math.pi)
        nx, ny = math.cos(ta) * tilt, math.sin(ta) * tilt
        base = len(verts)
        # Centre (a small dome: hub + 6 rim points).
        verts.append((x, y, z + s * 0.18)); cols.append(kind[1])
        for k in range(6):
            a = k * math.pi / 3
            rx, ry = math.cos(a) * s * 0.28, math.sin(a) * s * 0.28
            verts.append((x + rx, y + ry, z + (rx * nx + ry * ny))); cols.append(kind[1])
        for k in range(6):
            tris.append((base, base + 1 + k, base + 1 + (k + 1) % 6))
        # Petals: thin rounded quads radiating, slightly cupped.
        for k in range(petals):
            a = k * 2 * math.pi / petals + rng.uniform(-0.1, 0.1)
            ca, sa = math.cos(a), math.sin(a)
            pw = s * (0.16 if kind is DAISY else 0.32)
            pl = s * (1.0 if kind is DAISY else 0.8) * rng.uniform(0.85, 1.1)
            p0 = len(verts)
            for (r, side, lift) in ((0.25, -1, 0.0), (0.25, 1, 0.0), (pl * 0.8 / s, -1, 0.06), (pl * 0.8 / s, 1, 0.06), (pl / s, 0, 0.1)):
                px = x + ca * r * s - sa * side * pw * (0.6 if r > 0.3 else 0.3)
                py = y + sa * r * s + ca * side * pw * (0.6 if r > 0.3 else 0.3)
                dx, dy = px - x, py - y
                verts.append((px, py, z + dx * nx + dy * ny + lift * s))
                cols.append(tuple(c * rng.uniform(0.92, 1.03) for c in kind[0]))
            tris.extend([(p0, p0 + 1, p0 + 3), (p0, p0 + 3, p0 + 2), (p0 + 2, p0 + 3, p0 + 4)])
    return big_mesh(name, np.array(verts), np.array(tris), layer, mat_vegetation("petal2", 0.4, 0.45), np.array(cols))


def tree(location, height, seed, layer, kind="green", leaves=9000, spread=0.55, name="tree"):
    """A broadleaf tree: a trunk and branches (tapered, gently curved), and a crown of
    leaves (or blossom petals) in clumps at the branch ends, densest on the outer shell."""
    rng = random.Random(seed)
    x, y, z = location
    bark = mat_bark()
    # Trunk and branches as tapered segments.
    segments = []

    def branch(p, d, length, radius, depth):
        n = 4
        cur = Vector(p)
        dirv = Vector(d).normalized()
        for k in range(n):
            nxt = cur + dirv * (length / n)
            segments.append((cur.copy(), nxt.copy(), radius * (1 - 0.18 * k), radius * (1 - 0.18 * (k + 1))))
            dirv = (dirv + Vector((rng.uniform(-0.25, 0.25), rng.uniform(-0.25, 0.25), rng.uniform(-0.05, 0.2)))).normalized()
            cur = nxt
        tips.append(cur.copy())
        if depth > 0:
            for _ in range(rng.randint(2, 3)):
                nd = (dirv + Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(0.2, 0.9))) * 0.9).normalized()
                branch(cur, nd, length * rng.uniform(0.55, 0.7), radius * 0.55, depth - 1)

    tips = []
    trunk_h = height * 0.42
    branch((x, y, z), (rng.uniform(-0.08, 0.08), rng.uniform(-0.08, 0.08), 1), trunk_h, height * 0.035, 0)
    top = tips.pop()
    for k in range(rng.randint(4, 6)):
        a = k * 2 * math.pi / 5 + rng.uniform(-0.4, 0.4)
        branch(top, (math.cos(a) * 0.8, math.sin(a) * 0.8, 0.9), height * 0.3, height * 0.022, 2)
    tube_mesh(segments, layer, bark, name + "_wood")
    # Leaves: clumps around the tips (and a few inner ones), as small diamonds.
    rng2 = np.random.default_rng(seed)
    centres = np.array([[t.x, t.y, t.z] for t in tips], np.float32)
    per = max(1, leaves // len(centres))
    clump_r = height * 0.13
    offs = rng2.normal(size=(len(centres) * per, 3)).astype(np.float32)
    offs /= np.linalg.norm(offs, axis=1, keepdims=True) + 1e-6
    rad = (rng2.random((len(offs), 1)) ** 0.35) * clump_r          # towards the shell
    pos = np.repeat(centres, per, axis=0) + offs * rad * np.array([1.0, 1.0, 0.8], np.float32)
    leaf = height * (0.012 if kind != "blossom" else 0.009)
    n = len(pos)
    a1 = rng2.normal(size=(n, 3)).astype(np.float32); a1 /= np.linalg.norm(a1, axis=1, keepdims=True)
    a2 = np.cross(a1, offs); a2 /= np.linalg.norm(a2, axis=1, keepdims=True) + 1e-6
    verts = np.stack([pos - a1 * leaf, pos - a2 * leaf * 0.5, pos + a1 * leaf, pos + a2 * leaf * 0.5], 1)
    tris = (np.array([[0, 1, 2], [0, 2, 3]])[None] + (np.arange(n) * 4)[:, None, None]).reshape(-1, 3)
    shade = (offs[:, 2:3] * 0.5 + 0.5) * 0.35 + 0.75                 # upper leaves lighter
    if kind == "blossom":
        palette = np.array([[0.98, 0.74, 0.82], [0.99, 0.86, 0.9], [0.96, 0.62, 0.74], [1.0, 0.95, 0.95]], np.float32)
    elif kind == "autumn":
        palette = np.array([[0.85, 0.55, 0.15], [0.9, 0.7, 0.2], [0.6, 0.55, 0.15]], np.float32)
    else:
        palette = np.array([[0.13, 0.32, 0.06], [0.2, 0.42, 0.08], [0.3, 0.5, 0.1], [0.1, 0.26, 0.07]], np.float32)
    c = palette[rng2.integers(0, len(palette), n)] * shade
    cols = np.repeat(c, 4, axis=0)
    big_mesh(name + "_leaves", verts.reshape(-1, 3), tris, layer, mat_vegetation("leaf2", 0.45, 0.5), cols)


def mat_bark():
    key = ("bark",)
    if key in _mats:
        return _mats[key]
    m = B.mat_stone("bark2", (0.25, 0.18, 0.12), rough=0.9, bump=0.9, scale=6.0)
    _mats[key] = m
    return m


def tube_mesh(segments, layer, material, name, sides=8):
    """Tapered tubes for branches: each segment (a, b, ra, rb) a ring-to-ring sleeve."""
    verts, tris = [], []
    for a, b, ra, rb in segments:
        d = (b - a).normalized()
        ref = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
        u = d.cross(ref).normalized(); v = d.cross(u)
        base = len(verts)
        for ring, (p, r) in enumerate(((a, ra), (b, rb))):
            for k in range(sides):
                ang = 2 * math.pi * k / sides
                q = p + (u * math.cos(ang) + v * math.sin(ang)) * r
                verts.append((q.x, q.y, q.z))
        for k in range(sides):
            k1 = (k + 1) % sides
            tris.append((base + k, base + k1, base + sides + k1))
            tris.append((base + k, base + sides + k1, base + sides + k))
    return big_mesh(name, np.array(verts), np.array(tris), layer, material, smooth=True)


def cumulus(location, size, layer, seed, density=6.0, name="cloud"):
    """A volumetric cumulus cloud in a box (size = (x, y, z) extents)."""
    x, y, z = location
    sx, sy, sz = size
    v = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0), (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]
    verts = [(x + a * sx / 2, y + b * sy / 2, z + c * sz) for a, b, c in v]
    faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    ob = B.mesh_object(name, verts, faces, layer, mat_cloud("cumulus%d" % seed, density, w=float(seed % 17) * 1.7))
    return ob


# ------------------------------------------------------------------------ architecture
def ring(location, radius, height, layer, material, sides=32, name="ring"):
    return B.cylinder(location, radius, height, layer, material, sides=sides, name=name)


def tower(location, radius, height, layer, seed, roof="gold", window_rows=3, crenels=True, slender=1.0, facing=-math.pi / 2,
          marble=None, gold=None, glass=None, roof_h=None):
    """A detailed white-gold tower: a plinth, string courses, rows of arched windows with
    gold frames (on the side facing the viewer), a corbelled crenellated crown, a tall
    roof with gold ribs and a gold finial, little pinnacles round the roof's foot."""
    rng = random.Random(seed)
    m = marble or mat_marble()
    g = gold or mat_gold()
    gl = glass or mat_glass_window()
    x, y, z = location
    sides = 32
    B.cylinder((x, y, z), radius * 1.12, height * 0.06, layer, m, sides=sides, name="plinth")
    B.cylinder((x, y, z + height * 0.06), radius, height * 0.94, layer, m, sides=sides, top_radius=radius * 0.96, name="shaft")
    # String courses.
    for k in range(1, 4):
        ring((x, y, z + height * k / 4), radius * 1.04, height * 0.012, layer, m, sides=sides, name="course")
    # Windows: rows of arches in the front half.
    for row in range(window_rows):
        wz = z + height * (0.22 + 0.6 * row / max(1, window_rows))
        count = 5 if radius > 3 else 3
        for k in range(count):
            a = facing + (k - (count - 1) / 2) * (math.pi / (count + 1.2))
            wx, wy = x + math.cos(a) * radius * 1.0, y + math.sin(a) * radius * 1.0
            w, h = radius * 0.22, radius * 0.55
            B.arch_shape((wx, wy, wz), w * 1.25, h * 1.15, layer, g, rotation_z=a + math.pi / 2, name="frame")
            fx, fy = x + math.cos(a) * radius * 1.012, y + math.sin(a) * radius * 1.012
            B.arch_shape((fx, fy, wz + h * 0.06), w, h, layer, gl, rotation_z=a + math.pi / 2, name="glass")
    top = z + height
    # Corbelled crown and crenellations.
    B.cylinder((x, y, top - radius * 0.5), radius * 0.96, radius * 0.5, layer, m, sides=sides, top_radius=radius * 1.18, name="corbel")
    B.cylinder((x, y, top), radius * 1.18, radius * 0.25, layer, m, sides=sides, name="parapet")
    ring((x, y, top + radius * 0.25), radius * 1.19, radius * 0.04, layer, g, sides=sides, name="giltband")
    if crenels:
        n = 16
        for k in range(n):
            if k % 2:
                continue
            a = 2 * math.pi * k / n
            B.box((x + math.cos(a) * radius * 1.1, y + math.sin(a) * radius * 1.1, top + radius * 0.25 + radius * 0.18),
                  (radius * 0.3, radius * 0.3, radius * 0.36), layer, m, rotation=(0, 0, a), name="merlon")
    # Roof: a tall cone with gold ribs, a finial.
    rh = roof_h if roof_h is not None else radius * 4.2 * slender
    roof_mat = g if roof == "gold" else (B.mat_stone("slate2", (0.42, 0.5, 0.66), rough=0.35, bump=0.3, scale=8.0) if roof == "blue" else m)
    B.cone((x, y, top + radius * 0.25), radius * 1.02, rh, layer, roof_mat, sides=sides, name="roof")
    if roof != "gold":
        for k in range(8):
            a = 2 * math.pi * k / 8
            segs = [(Vector((x + math.cos(a) * radius * 1.03, y + math.sin(a) * radius * 1.03, top + radius * 0.25)),
                     Vector((x, y, top + radius * 0.25 + rh)), radius * 0.05, radius * 0.01)]
            tube_mesh(segs, layer, g, "rib", sides=5)
    B.lathe((x, y, top + radius * 0.25 + rh * 0.92), [(radius * 0.12, 0), (radius * 0.22, radius * 0.25), (radius * 0.1, radius * 0.5),
                                                       (radius * 0.04, radius * 1.4), (0.0, radius * 1.9)], layer, g, sides=12, name="finial")
    for k in range(4):
        a = 2 * math.pi * (k + 0.5) / 4
        px, py = x + math.cos(a) * radius * 1.05, y + math.sin(a) * radius * 1.05
        B.cone((px, py, top + radius * 0.25), radius * 0.16, rh * 0.35, layer, roof_mat, sides=8, name="pinnacle")
        B.lathe((px, py, top + radius * 0.25 + rh * 0.33), [(radius * 0.03, 0), (radius * 0.06, radius * 0.1), (0.0, radius * 0.5)], layer, g, sides=8, name="pinfinial")


def wall(x0, x1, y, z, height, thickness, layer, marble=None, gold=None, arches=0):
    """A curtain wall with crenellations, a gilt band and optional arcade of windows."""
    m = marble or mat_marble()
    g = gold or mat_gold()
    length = abs(x1 - x0)
    cx = (x0 + x1) / 2
    B.box((cx, y, z + height / 2), (length, thickness, height), layer, m, name="wall")
    B.box((cx, y - thickness * 0.5, z + height * 0.98), (length, thickness * 0.3, height * 0.04), layer, g, name="wallband")
    n = max(2, int(length / (thickness * 0.9)))
    for k in range(n):
        if k % 2:
            continue
        mx = x0 + (x1 - x0) * (k + 0.5) / n
        B.box((mx, y, z + height + thickness * 0.3), (length / n * 0.9, thickness, thickness * 0.6), layer, m, name="merlon")
    for k in range(arches):
        ax = x0 + (x1 - x0) * (k + 0.5) / arches
        B.arch_shape((ax, y - thickness / 2 - 0.02, z + height * 0.35), height * 0.12, height * 0.38, layer, mat_glass_window(), name="gallery")


def castle(cx, cy, z, scale, layer, seed, towers=14, facing=-math.pi / 2):
    """A white-gold castle of many spires: a tall central keep, a ring of towers of
    graded heights, walls between them, all with windows, crowns and gilt roofs."""
    rng = random.Random(seed)
    m, g = mat_marble(), mat_gold()
    # The keep.
    tower((cx, cy, z), 9 * scale, 120 * scale, layer, seed, roof="gold", window_rows=5, slender=1.25, facing=facing, marble=m, gold=g)
    pts = []
    for k in range(towers):
        a = 2 * math.pi * k / towers + rng.uniform(-0.15, 0.15)
        r = (36 + rng.uniform(-8, 12)) * scale
        tx, ty = cx + math.cos(a) * r, cy + math.sin(a) * r * 0.7
        front = math.sin(a) < 0
        h = (55 + rng.uniform(0, 45)) * scale * (1.0 if front else 1.15)
        rad = (4.2 + rng.uniform(0, 2.2)) * scale
        roof = "gold" if rng.random() < 0.55 else "blue"
        tower((tx, ty, z), rad, h, layer, seed * 31 + k, roof=roof, window_rows=3 if h > 60 * scale else 2, slender=rng.uniform(0.9, 1.3),
              facing=facing, marble=m, gold=g)
        pts.append((tx, ty))
    # Inner towers between the keep and the ring.
    for k in range(towers // 2):
        a = 2 * math.pi * (k + 0.5) / (towers // 2)
        tx, ty = cx + math.cos(a) * 18 * scale, cy + math.sin(a) * 13 * scale
        tower((tx, ty, z), 5 * scale, (85 + rng.uniform(0, 25)) * scale, layer, seed * 77 + k, roof="gold", window_rows=3,
              slender=1.2, facing=facing, marble=m, gold=g)
    # Walls between neighbouring front towers.
    front = sorted([p for p in pts if p[1] < cy], key=lambda p: p[0])
    for a, b in zip(front, front[1:]):
        wall(a[0], b[0], (a[1] + b[1]) / 2, z, 28 * scale, 4 * scale, layer, m, g, arches=3)


def sparkles(centre, extent, count, layer, seed, size=0.25, name="sparkles"):
    """Glowing motes of light hanging in the air (small bright diamonds)."""
    rng = np.random.default_rng(seed)
    c = np.array(centre, np.float32); e = np.array(extent, np.float32)
    pos = c + (rng.random((count, 3)).astype(np.float32) - 0.5) * e
    s = (size * (0.5 + rng.random(count))).astype(np.float32)[:, None]
    up = np.array([0, 0, 1], np.float32); side = np.array([1, 0, 0], np.float32); fwd = np.array([0, 1, 0], np.float32)
    verts = np.stack([pos + up * s, pos + side * s * 0.5, pos - up * s, pos - side * s * 0.5, pos + fwd * s * 0.5, pos - fwd * s * 0.5], 1)
    tris = np.array([[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4], [0, 5, 1], [1, 5, 2], [2, 5, 3], [3, 5, 0]])
    tris = (tris[None] + (np.arange(count) * 6)[:, None, None]).reshape(-1, 3)
    return big_mesh(name, verts.reshape(-1, 3), tris, layer, B.mat_emission((1.0, 0.9, 0.6), 12.0, "sparkle"))


def _fbm2(u, v, seed, octaves=6):
    """Cheap periodic-in-u fractal noise on numpy grids (sum of random sinusoids)."""
    rng = np.random.default_rng(seed)
    out = np.zeros_like(u)
    amp, freq = 1.0, 1.0
    for _ in range(octaves):
        for _ in range(3):
            ku = rng.integers(1, 4) * freq; kv = rng.uniform(0.5, 1.5) * freq
            ph = rng.random() * 6.28
            out += amp * np.sin(ku * u + kv * v * 6.28 + ph) * np.cos(kv * 0.7 * v * 6.28 + ku * 0.3 * u + ph * 1.3)
        amp *= 0.5; freq *= 2.0
    return out


def mesa(cx, cy, z0, radius, height, layer, seed, material=None, squash=0.75, name="mesa"):
    """A tall cliff mesa: a dense displaced shell (strata ledges, buttresses, overhangs),
    narrowing to the top, with a flat plateau."""
    nu, nv = 360, 150
    u = np.linspace(0, 2 * np.pi, nu, endpoint=False)[None, :].repeat(nv, 0)
    v = np.linspace(0, 1, nv)[:, None].repeat(nu, 1)
    n = _fbm2(u, v, seed)
    ridge = 1.0 - np.abs(_fbm2(u * 4, v * 1.5, seed + 9, 5)) * 0.6
    strata = 0.02 * np.sin(v * 52 + n * 3.0) + 0.07 * (ridge - 0.7) + 0.03 * _fbm2(u * 9, v * 6, seed + 17, 3)   # ridged buttresses, irregular strata
    r = radius * (1.0 - 0.28 * v ** 1.6) * (1.0 + 0.16 * n + strata)
    x = cx + r * np.cos(u)
    y = cy + r * np.sin(u) * squash
    zz = z0 + height * v
    verts = np.stack([x, y, zz], -1).reshape(-1, 3)
    tris = []
    idx = np.arange(nv * nu).reshape(nv, nu)
    a = idx[:-1, :]; b = np.roll(idx[:-1, :], -1, 1); c = np.roll(idx[1:, :], -1, 1); d = idx[1:, :]
    tris = np.concatenate([np.stack([a, b, c], -1).reshape(-1, 3), np.stack([a, c, d], -1).reshape(-1, 3)])
    # The plateau: a fan.
    top = len(verts)
    verts = np.concatenate([verts, [[cx, cy, z0 + height]]])
    ring = idx[-1]
    fan = np.stack([ring, np.roll(ring, -1), np.full(nu, top)], -1)
    tris = np.concatenate([tris, fan])
    return big_mesh(name, verts, tris, layer, material or mat_cliff("cliff%d" % seed, seed), smooth=True)


def mat_cliff(name="cliff3", seed=0):
    """Realistic cliff stone: three warm tones in patches, dark crevices (Voronoi cracks),
    vertical water stains, fine grain; moss and grass only on true ledges, in patches."""
    key = ("cliff3", name, seed)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    L = nt.links.new
    tc = nt.nodes.new("ShaderNodeTexCoord")
    def noise(scale, detail=8, rough=0.6, dims="3D", w=0.0):
        n = nt.nodes.new("ShaderNodeTexNoise"); n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail; n.inputs["Roughness"].default_value = rough
        n.noise_dimensions = "4D"; n.inputs["W"].default_value = w + seed * 3.1
        L(tc.outputs["Object"], n.inputs["Vector"]); return n
    patches = noise(0.03, 6, 0.55)
    tones = nt.nodes.new("ShaderNodeValToRGB")
    els = tones.color_ramp.elements
    els[0].position = 0.35; els[0].color = (0.46, 0.43, 0.4, 1)
    els[1].position = 0.65; els[1].color = (0.78, 0.74, 0.68, 1)
    e = els.new(0.5); e.color = (0.68, 0.57, 0.43, 1)          # ochre
    L(patches.outputs["Fac"], tones.inputs["Fac"])
    # Cracks: Voronoi distance to the edge, thin dark lines.
    vor = nt.nodes.new("ShaderNodeTexVoronoi"); vor.feature = "DISTANCE_TO_EDGE"; vor.inputs["Scale"].default_value = 0.6
    warp = noise(0.2, 4, 0.5, w=7)
    mixv = nt.nodes.new("ShaderNodeVectorMath"); mixv.operation = "ADD"
    sc = nt.nodes.new("ShaderNodeVectorMath"); sc.operation = "SCALE"; sc.inputs["Scale"].default_value = 3.0
    L(warp.outputs["Color"], sc.inputs[0]); L(tc.outputs["Object"], mixv.inputs[0]); L(sc.outputs[0], mixv.inputs[1])
    L(mixv.outputs[0], vor.inputs["Vector"])
    crack = nt.nodes.new("ShaderNodeMapRange"); crack.inputs["From Min"].default_value = 0.0; crack.inputs["From Max"].default_value = 0.04
    L(vor.outputs["Distance"], crack.inputs["Value"])
    # Vertical stains: noise stretched along z.
    stretch = nt.nodes.new("ShaderNodeVectorMath"); stretch.operation = "MULTIPLY"; stretch.inputs[1].default_value = (1.0, 1.0, 0.08)
    L(tc.outputs["Object"], stretch.inputs[0])
    stain = nt.nodes.new("ShaderNodeTexNoise"); stain.inputs["Scale"].default_value = 0.4; stain.inputs["Detail"].default_value = 5
    L(stretch.outputs[0], stain.inputs["Vector"])
    stainr = nt.nodes.new("ShaderNodeMapRange"); stainr.inputs["From Min"].default_value = 0.45; stainr.inputs["From Max"].default_value = 0.7
    stainr.inputs["To Min"].default_value = 1.0; stainr.inputs["To Max"].default_value = 0.72
    L(stain.outputs["Fac"], stainr.inputs["Value"])
    c1 = nt.nodes.new("ShaderNodeMix"); c1.data_type = "RGBA"; c1.blend_type = "MULTIPLY"; c1.inputs["Factor"].default_value = 1.0
    L(tones.outputs["Color"], c1.inputs["A"])
    cr = nt.nodes.new("ShaderNodeCombineColor"); L(crack.outputs["Result"], cr.inputs[0]); L(crack.outputs["Result"], cr.inputs[1]); L(crack.outputs["Result"], cr.inputs[2])
    crack_tone = nt.nodes.new("ShaderNodeMix"); crack_tone.data_type = "RGBA"
    crack_tone.inputs["A"].default_value = (0.25, 0.22, 0.2, 1); crack_tone.inputs["B"].default_value = (1, 1, 1, 1)
    L(crack.outputs["Result"], crack_tone.inputs["Factor"])
    L(crack_tone.outputs["Result"], c1.inputs["B"])
    c2 = nt.nodes.new("ShaderNodeMix"); c2.data_type = "RGBA"; c2.blend_type = "MULTIPLY"; c2.inputs["Factor"].default_value = 1.0
    L(c1.outputs["Result"], c2.inputs["A"])
    st = nt.nodes.new("ShaderNodeCombineColor")
    for i in range(3): L(stainr.outputs["Result"], st.inputs[i])
    L(st.outputs["Color"], c2.inputs["B"])
    # Moss: true ledges only (normal z > 0.8), in patches.
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ"); L(geo.outputs["Normal"], sep.inputs["Vector"])
    up = nt.nodes.new("ShaderNodeMapRange"); up.inputs["From Min"].default_value = 0.8; up.inputs["From Max"].default_value = 0.95
    L(sep.outputs["Z"], up.inputs["Value"])
    mp = noise(0.15, 5, 0.6, w=13)
    mpr = nt.nodes.new("ShaderNodeMapRange"); mpr.inputs["From Min"].default_value = 0.45; mpr.inputs["From Max"].default_value = 0.6
    L(mp.outputs["Fac"], mpr.inputs["Value"])
    mm = nt.nodes.new("ShaderNodeMath"); mm.operation = "MULTIPLY"; L(up.outputs["Result"], mm.inputs[0]); L(mpr.outputs["Result"], mm.inputs[1])
    moss = nt.nodes.new("ShaderNodeMix"); moss.data_type = "RGBA"
    L(mm.outputs[0], moss.inputs["Factor"]); L(c2.outputs["Result"], moss.inputs["A"]); moss.inputs["B"].default_value = (0.2, 0.32, 0.08, 1)
    L(moss.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.88
    # Relief: cracks cut in, a coarse and a fine grain.
    h1 = nt.nodes.new("ShaderNodeMath"); h1.operation = "MULTIPLY_ADD"
    fine = noise(3.0, 6, 0.7, w=21)
    L(fine.outputs["Fac"], h1.inputs[0]); h1.inputs[1].default_value = 0.25; L(crack.outputs["Result"], h1.inputs[2])
    h2 = nt.nodes.new("ShaderNodeMath"); h2.operation = "ADD"; L(h1.outputs[0], h2.inputs[0]); L(patches.outputs["Fac"], h2.inputs[1])
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.85; bump.inputs["Distance"].default_value = 1.2
    L(h2.outputs[0], bump.inputs["Height"]); L(bump.outputs["Normal"], bsdf.inputs["Normal"])
    L(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def mat_floor(name="floor3", a=(0.88, 0.85, 0.8), b=(0.7, 0.62, 0.5), scale=0.12):
    """A polished chequered marble floor (reflections add richness)."""
    key = ("floor", name)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    ch = nt.nodes.new("ShaderNodeTexChecker"); ch.inputs["Scale"].default_value = scale
    ch.inputs["Color1"].default_value = (*a, 1); ch.inputs["Color2"].default_value = (*b, 1)
    nt.links.new(tc.outputs["Object"], ch.inputs["Vector"])
    nt.links.new(ch.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.12
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def arc(points_fn, n, radius, layer, material, name="arc", sides=6):
    """A tube along a curve given by points_fn(t), t in 0..1."""
    pts = [Vector(points_fn(k / n)) for k in range(n + 1)]
    segs = [(pts[k], pts[k + 1], radius, radius) for k in range(n)]
    return tube_mesh(segs, layer, material, name, sides=sides)


def pointed_arch(x0, x1, y, z, rise, radius, layer, material, name="archrib"):
    """A pointed (two-centred) arch from (x0, y, z) to (x1, y, z) rising by `rise`."""
    def p(t):
        if t <= 0.5:
            u = t / 0.5
            return (x0 + (x1 - x0) / 2 * math.sin(u * math.pi / 2), y, z + rise * (1 - (1 - u) ** 2) ** 0.5 * 1.0 if False else z + rise * math.sin(u * math.pi / 2))
        u = (t - 0.5) / 0.5
        return (x0 + (x1 - x0) / 2 + (x1 - x0) / 2 * (1 - math.cos(u * math.pi / 2)), y, z + rise * math.cos(u * math.pi / 2))
    return arc(p, 12, radius, layer, material, name)


def crystal_cluster(cx, cy, z, size, count, layer, seed, colour=(0.75, 0.85, 1.0)):
    """A cluster of faceted crystals: hexagonal prisms with pointed tips, fanning out."""
    rng = random.Random(seed)
    mat = B.mat_crystal(colour, 0.25)
    for k in range(count):
        h = size * rng.uniform(0.4, 1.3)
        B.crystal((cx + rng.uniform(-0.5, 0.5) * size, cy + rng.uniform(-0.3, 0.3) * size, z), h, h * rng.uniform(0.09, 0.15), layer, mat,
                  tilt=(rng.uniform(-0.45, 0.45), rng.uniform(-0.45, 0.45)))


def isle2(cx, cy, z, radius, depth, layer, seed, trees=3, blossom=0.4):
    """A floating island: a ridged rock underside hanging below a grassy, flowered top."""
    nu, nv = 200, 60
    u = np.linspace(0, 2 * np.pi, nu, endpoint=False)[None, :].repeat(nv, 0)
    v = np.linspace(0, 1, nv)[:, None].repeat(nu, 1)
    n = _fbm2(u, v, seed)
    r = radius * (v ** 0.55) * (1.0 + 0.18 * n + 0.06 * _fbm2(u * 5, v * 3, seed + 4, 3))
    zz = z - depth * (1 - v) * (1.0 + 0.15 * n)
    verts = np.stack([cx + r * np.cos(u), cy + r * np.sin(u), zz], -1).reshape(-1, 3)
    idx = np.arange(nv * nu).reshape(nv, nu)
    a = idx[:-1, :]; b = np.roll(idx[:-1, :], -1, 1); c = np.roll(idx[1:, :], -1, 1); d = idx[1:, :]
    tris = np.concatenate([np.stack([a, c, b], -1).reshape(-1, 3), np.stack([a, d, c], -1).reshape(-1, 3)])
    big_mesh("isle", verts, tris, layer, mat_cliff("islecliff%d" % seed, seed), smooth=True)
    # The top: a gentle grassy dome following the rock's own outline, its rim rolling over.
    rim = r[-1]                                            # the outline radius per angle
    nr = 24
    rho = np.linspace(0, 1, nr + 1)[1:, None]
    tx = cx + rho * rim[None, :] * 1.02 * np.cos(u[0])[None, :]
    ty = cy + rho * rim[None, :] * 1.02 * np.sin(u[0])[None, :]
    tz = z + radius * 0.07 * (1 - rho ** 2) - 1.2 * rho ** 8 + 0.4 * _fbm2(u[:nr] * 3, rho.repeat(nu, 1) * 2, seed + 21, 3)
    tv = np.concatenate([[[cx, cy, z + radius * 0.07]], np.stack([tx, ty, tz], -1).reshape(-1, 3)])
    T = [(0, 1 + k, 1 + (k + 1) % nu) for k in range(nu)]
    for j in range(nr - 1):
        for k in range(nu):
            a_ = 1 + j * nu + k; b_ = 1 + j * nu + (k + 1) % nu
            T += [(a_, a_ + nu, b_ + nu), (a_, b_ + nu, b_)]
    big_mesh("isletop", tv, np.array(T), layer, mat_ground("isleground", (0.12, 0.24, 0.06)), smooth=True)
    def top(x, y):
        q = min(1.0, math.hypot(x - cx, y - cy) / (radius * 0.98))
        return z + radius * 0.07 * (1 - q * q) - 1.2 * q ** 8
    # Grass and flowers in strips that fill the round top (not a square patch).
    for k in range(12):
        y0 = -1.2 + k * 0.2; y1 = y0 + 0.2
        half = math.sqrt(max(0.0, 1.3 - min(y0 ** 2, y1 ** 2)))
        if half <= 0.05:
            continue
        grass(top, cx - radius * half, cx + radius * half, cy + radius * y0, cy + radius * y1, int(radius * radius * 9 * half), layer, seed + k,
              height=(0.3, 0.8))
        flowers(top, cx - radius * half, cx + radius * half, cy + radius * y0, cy + radius * y1, int(radius * radius * 0.7 * half), layer, seed + 40 + k,
                size=0.25)
    rng = random.Random(seed)
    for k in range(trees):
        tx_, ty_ = cx + rng.uniform(-0.5, 0.5) * radius, cy + rng.uniform(-0.4, 0.4) * radius
        tree((tx_, ty_, top(tx_, ty_)), radius * rng.uniform(0.4, 0.7), seed * 7 + k, layer,
             kind="blossom" if rng.random() < blossom else "green", leaves=6000)


def mushroom2(x, y, z, size, layer, seed, cap=(0.92, 0.42, 0.5)):
    """A fairy-tale mushroom: a curved stem, a domed cap with white spots, gills below."""
    rng = random.Random(seed)
    stem = mat_marble("stem%.2f" % size, colour=(0.95, 0.92, 0.85), vein=(0.85, 0.82, 0.76))
    B.lathe((x, y, z), [(size * 0.2, 0), (size * 0.15, size * 0.35), (size * 0.13, size * 0.7), (size * 0.18, size * 0.82), (0.0, size * 0.83)], layer, stem, sides=24, name="stem")
    capm = mat_cap(cap)
    B.lathe((x, y, z + size * 0.72), [(size * 0.16, 0), (size * 0.7, size * 0.04), (size * 0.66, size * 0.2), (size * 0.5, size * 0.38),
                                      (size * 0.25, size * 0.48), (0.0, size * 0.5)], layer, capm, sides=40, name="cap")
    spot = B.mat_stone("spot", (0.98, 0.97, 0.94), rough=0.5, bump=0.1)
    for k in range(14):
        a = rng.uniform(0, 2 * math.pi); t = rng.uniform(0.15, 0.85)
        rr = size * (0.68 - 0.55 * t); zz = z + size * 0.72 + size * 0.5 * (1 - (1 - t) ** 2) * 0.95
        B.cylinder((x + math.cos(a) * rr, y + math.sin(a) * rr, zz), size * rng.uniform(0.04, 0.08), size * 0.02, layer, spot, sides=10, name="spot")
    for k in range(36):
        a = k * 2 * math.pi / 36
        B.box((x + math.cos(a) * size * 0.42, y + math.sin(a) * size * 0.42, z + size * 0.7), (size * 0.5, size * 0.01, size * 0.03), layer,
              stem, rotation=(0, 0, a), name="gill")


def butterflies(centre, extent, count, layer, seed):
    rng = random.Random(seed)
    cols = [(0.98, 0.78, 0.25), (0.95, 0.55, 0.75), (0.62, 0.72, 1.0), (1.0, 1.0, 0.95)]
    verts, tris, colsv = [], [], []
    for k in range(count):
        x = centre[0] + rng.uniform(-0.5, 0.5) * extent[0]; y = centre[1] + rng.uniform(-0.5, 0.5) * extent[1]
        z = centre[2] + rng.uniform(-0.5, 0.5) * extent[2]
        s = rng.uniform(0.25, 0.45); a = rng.uniform(0, 2 * math.pi); open_ = rng.uniform(0.2, 0.9)
        ca, sa = math.cos(a), math.sin(a); c = cols[rng.randrange(4)]
        for side in (-1, 1):
            b0 = len(verts)
            pts = [(0, 0, 0), (0.3, side * 1.0, open_ * 0.6), (1.0, side * 0.9, open_ * 0.5), (0.6, side * 0.2, 0.05),
                   (-0.6, side * 0.8, open_ * 0.5), (-0.3, side * 0.2, 0.0)]
            for px, py, pz in pts:
                verts.append((x + (ca * px - sa * py) * s, y + (sa * px + ca * py) * s, z + pz * s)); colsv.append(c)
            tris += [(b0, b0 + 1, b0 + 2), (b0, b0 + 2, b0 + 3), (b0, b0 + 4, b0 + 1), (b0, b0 + 5, b0 + 4)]
    return big_mesh("butterflies", np.array(verts), np.array(tris), layer, mat_vegetation("wing", 0.5, 0.4), np.array(colsv))


# ------------------------------------------------------------------ distance (v3)
def _ridged(x, y, seed, octaves=6):
    """Ridged fractal noise on numpy arrays (mathutils per vertex: fine for ~100k)."""
    from mathutils import noise as mn
    out = np.empty(x.shape, np.float32)
    fx, fy = x.ravel(), y.ravel()
    o = out.ravel()
    for i in range(fx.size):
        o[i] = mn.ridged_multi_fractal(Vector((fx[i], fy[i], seed * 7.31)), 0.9, 2.1, octaves, 1.0, 2.0)
    return out


def mat_range(name="range3", rock=(0.38, 0.39, 0.46), snow=(0.97, 0.96, 0.98), snow_line=0.62, top=700.0, base_z=0.0):
    """Distant mountains: blue-grey rock with darker gullies (noise), snow on the upper
    slopes where they are not too steep, a soft grassy foot."""
    key = ("range", name, rock, snow_line, top)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    L = nt.links.new
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ"); L(geo.outputs["Position"], sep.inputs[0])
    nsep = nt.nodes.new("ShaderNodeSeparateXYZ"); L(geo.outputs["Normal"], nsep.inputs[0])
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 0.012; nz.inputs["Detail"].default_value = 8
    L(geo.outputs["Position"], nz.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB"); els = ramp.color_ramp.elements
    els[0].position = 0.35; els[0].color = (rock[0] * 0.55, rock[1] * 0.55, rock[2] * 0.62, 1)
    els[1].position = 0.7; els[1].color = (rock[0] * 1.25, rock[1] * 1.22, rock[2] * 1.15, 1)
    L(nz.outputs["Fac"], ramp.inputs["Fac"])
    # Snow: height above the snow line (jittered by noise) and a not-too-steep slope.
    h = nt.nodes.new("ShaderNodeMapRange")
    h.inputs["From Min"].default_value = base_z + top * snow_line; h.inputs["From Max"].default_value = base_z + top * (snow_line + 0.08)
    L(sep.outputs["Z"], h.inputs["Value"])
    jit = nt.nodes.new("ShaderNodeMath"); jit.operation = "MULTIPLY_ADD"
    L(nz.outputs["Fac"], jit.inputs[0]); jit.inputs[1].default_value = 0.9; jit.inputs[2].default_value = -0.45
    hh = nt.nodes.new("ShaderNodeMath"); hh.operation = "ADD"; hh.use_clamp = True
    L(h.outputs["Result"], hh.inputs[0]); L(jit.outputs[0], hh.inputs[1])
    flat = nt.nodes.new("ShaderNodeMapRange")
    flat.inputs["From Min"].default_value = 0.45; flat.inputs["From Max"].default_value = 0.7
    L(nsep.outputs["Z"], flat.inputs["Value"])
    sn = nt.nodes.new("ShaderNodeMath"); sn.operation = "MULTIPLY"
    L(hh.outputs[0], sn.inputs[0]); L(flat.outputs["Result"], sn.inputs[1])
    mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"
    L(sn.outputs[0], mix.inputs["Factor"]); L(ramp.outputs["Color"], mix.inputs["A"])
    mix.inputs["B"].default_value = (*snow, 1)
    # A green foot.
    foot = nt.nodes.new("ShaderNodeMapRange")
    foot.inputs["From Min"].default_value = base_z + top * 0.12; foot.inputs["From Max"].default_value = base_z + top * 0.02
    L(sep.outputs["Z"], foot.inputs["Value"])
    mix2 = nt.nodes.new("ShaderNodeMix"); mix2.data_type = "RGBA"
    L(foot.outputs["Result"], mix2.inputs["Factor"]); L(mix.outputs["Result"], mix2.inputs["A"])
    mix2.inputs["B"].default_value = (0.2, 0.32, 0.14, 1)
    L(mix2.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.9
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.5
    L(nz.outputs["Fac"], bump.inputs["Height"]); L(bump.outputs["Normal"], bsdf.inputs["Normal"])
    L(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def mountains(seed, y=2600, height=700, x0=-4200, x1=4200, depth=900, base_z=-40, layer="far", nx=520, ny=90, name="range"):
    """A real mountain range: a ridged-noise height field with peaks, ridges and gullies,
    rising from a foot at base_z, snow-capped, receding in depth (three staggered ridges)."""
    xs = np.linspace(x0, x1, nx + 1, dtype=np.float32)
    ys = np.linspace(y, y + depth, ny + 1, dtype=np.float32)
    X, Y = np.meshgrid(xs, ys)
    f = 1.6 / height
    r = _ridged(X * f, Y * f, seed)
    r = np.clip(r / 1.6, 0, None) ** 1.5
    # Ridges: an envelope rising from the front edge, with staggered crests in depth.
    t = (Y - y) / depth
    env = np.clip(np.sin(np.clip(t, 0, 1) * np.pi * 0.9) * 1.1, 0, 1)
    crest = 0.55 + 0.45 * np.sin(X * 0.0011 + seed) * np.cos(X * 0.0023 + seed * 2)
    Z = base_z + height * env * (0.25 + 0.95 * r * crest)
    Z[0, :] = base_z - 20
    verts = np.stack([X, Y, Z], -1).reshape(-1, 3)
    i = np.arange(nx); j = np.arange(ny)
    I, J = np.meshgrid(i, j)
    a = (J * (nx + 1) + I).ravel()
    tris = np.concatenate([np.stack([a, a + 1, a + nx + 2], -1), np.stack([a, a + nx + 2, a + nx + 1], -1)])
    return big_mesh(name, verts, tris, layer, mat_range("range%d" % seed, top=height * 1.2, base_z=base_z), smooth=True)


def mat_fields(name="fields3", base=(0.2, 0.36, 0.12), scale=0.004):
    """A sunlit valley floor from far away: meadows in several greens and gold, darker
    groves, at two scales."""
    key = ("fields", name, base)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    L = nt.links.new
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    vor = nt.nodes.new("ShaderNodeTexVoronoi"); vor.inputs["Scale"].default_value = scale
    # Field edges wander (noise-warped cells), never straight polygons.
    warpn = nt.nodes.new("ShaderNodeTexNoise"); warpn.inputs["Scale"].default_value = scale * 3; warpn.inputs["Detail"].default_value = 4
    L(geo.outputs["Position"], warpn.inputs["Vector"])
    wsc = nt.nodes.new("ShaderNodeVectorMath"); wsc.operation = "SCALE"; wsc.inputs["Scale"].default_value = 0.45 / scale
    L(warpn.outputs["Color"], wsc.inputs[0])
    wadd = nt.nodes.new("ShaderNodeVectorMath"); wadd.operation = "ADD"
    L(geo.outputs["Position"], wadd.inputs[0]); L(wsc.outputs[0], wadd.inputs[1])
    L(wadd.outputs[0], vor.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB"); els = ramp.color_ramp.elements
    els[0].position = 0.0; els[0].color = (base[0] * 0.8, base[1] * 0.85, base[2] * 0.7, 1)
    els[1].position = 1.0; els[1].color = (base[0] * 1.9 + 0.1, base[1] * 1.3 + 0.04, base[2] * 0.8, 1)
    e = els.new(0.55); e.color = (base[0] * 1.2, base[1] * 1.15, base[2] * 1.1, 1)
    bw = nt.nodes.new("ShaderNodeRGBToBW"); L(vor.outputs["Color"], bw.inputs[0]); L(bw.outputs[0], ramp.inputs["Fac"])
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 0.02; nz.inputs["Detail"].default_value = 6
    L(geo.outputs["Position"], nz.inputs["Vector"])
    dark = nt.nodes.new("ShaderNodeMapRange"); dark.inputs["From Min"].default_value = 0.58; dark.inputs["From Max"].default_value = 0.66
    L(nz.outputs["Fac"], dark.inputs["Value"])
    mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"
    L(dark.outputs["Result"], mix.inputs["Factor"]); L(ramp.outputs["Color"], mix.inputs["A"])
    mix.inputs["B"].default_value = (0.05, 0.13, 0.04, 1)
    # Softer: the cells blended with the fine noise (meadow texture inside each field).
    soft = nt.nodes.new("ShaderNodeMix"); soft.data_type = "RGBA"; soft.blend_type = "OVERLAY"; soft.inputs["Factor"].default_value = 0.45
    L(mix.outputs["Result"], soft.inputs["A"]); L(nz.outputs["Color"], soft.inputs["B"])
    L(soft.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.95
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.4; bump.inputs["Distance"].default_value = 2.0
    L(nz.outputs["Fac"], bump.inputs["Height"]); L(bump.outputs["Normal"], bsdf.inputs["Normal"])
    L(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def valley(seed, z0, x0=-5000, x1=5000, y0=300, y1=4000, amp=45, layer="far", nx=240, ny=160, name="valley"):
    """Rolling sunlit country: low hills (ridged and smooth noise) with fields and groves."""
    from mathutils import noise as mn
    xs = np.linspace(x0, x1, nx + 1, dtype=np.float32)
    # Rows denser near the viewer.
    ys = (y0 + (y1 - y0) * np.linspace(0, 1, ny + 1) ** 1.6).astype(np.float32)
    X, Y = np.meshgrid(xs, ys)
    Z = np.empty(X.shape, np.float32)
    for idx in np.ndindex(X.shape):
        x, y_ = float(X[idx]), float(Y[idx])
        Z[idx] = z0 + amp * (mn.noise(Vector((x * 0.0016, y_ * 0.0016, seed))) + 0.4 * mn.noise(Vector((x * 0.005, y_ * 0.005, seed + 3))))
    verts = np.stack([X, Y, Z], -1).reshape(-1, 3)
    I, J = np.meshgrid(np.arange(nx), np.arange(ny))
    a = (J * (nx + 1) + I).ravel()
    tris = np.concatenate([np.stack([a, a + 1, a + nx + 2], -1), np.stack([a, a + nx + 2, a + nx + 1], -1)])
    big_mesh(name, verts, tris, layer, mat_fields("fields%d" % seed), smooth=True)

    def h(x, y_):
        return z0 + amp * (mn.noise(Vector((x * 0.0016, y_ * 0.0016, seed))) + 0.4 * mn.noise(Vector((x * 0.005, y_ * 0.005, seed + 3))))
    return h


def grove(fn, x0, x1, y0, y1, count, layer, seed, size=(10, 22), blossom=0.2, name="grove", clump=0.6, avoid=(), min_z=None):
    """Distant trees: each a trunk and a crown of 4-7 lumpy lobes (vertex colours: sunlit
    tops, shaded undersides); clumped into woods. One mesh: hundreds of trees cheaply."""
    rng = np.random.default_rng(seed)
    # A low-poly lumpy sphere template.
    import bmesh
    bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)
    sv = np.array([v.co[:] for v in bm.verts], np.float32)
    st = np.array([[v.index for v in f.verts] for f in bm.faces], np.int32)
    bm.free()
    centres = rng.uniform((x0, y0), (x1, y1), (max(1, int(count * (1 - clump) / 6) + 3), 2))
    V, T, C = [], [], []
    base_n = 0
    for k in range(count):
        if rng.random() < clump:
            c = centres[rng.integers(len(centres))]
            x, y = c[0] + rng.normal(0, (x1 - x0) * 0.03), c[1] + rng.normal(0, (y1 - y0) * 0.03)
        else:
            x, y = rng.uniform(x0, x1), rng.uniform(y0, y1)
        z = fn(x, y)
        s = rng.uniform(*size)
        bl = rng.random() < blossom
        if any(math.hypot(x - ax, y - ay) < ar for ax, ay, ar in avoid) or (min_z is not None and z < min_z):
            continue
        top = np.array((1.0, 0.78, 0.86) if bl else (0.26 + rng.uniform(-0.05, 0.08), 0.45 + rng.uniform(-0.06, 0.06), 0.12), np.float32)
        # Trunk: a thin box.
        tw = s * 0.06
        tv = np.array([[-tw, -tw, 0], [tw, -tw, 0], [tw, tw, 0], [-tw, tw, 0], [-tw, -tw, s * 0.55], [tw, -tw, s * 0.55], [tw, tw, s * 0.55], [-tw, tw, s * 0.55]], np.float32)
        tt = np.array([[0, 1, 5], [0, 5, 4], [1, 2, 6], [1, 6, 5], [2, 3, 7], [2, 7, 6], [3, 0, 4], [3, 4, 7]], np.int32)
        V.append(tv + (x, y, z)); T.append(tt + base_n); C.append(np.tile((0.22, 0.15, 0.09), (8, 1))); base_n += 8
        for lobe in range(rng.integers(4, 8)):
            r = s * rng.uniform(0.22, 0.36)
            off = np.array((rng.normal(0, s * 0.16), rng.normal(0, s * 0.16), s * (0.55 + rng.uniform(0, 0.35))), np.float32)
            v = sv * r * (1 + 0.18 * rng.standard_normal(sv.shape).astype(np.float32))
            v[:, 2] *= 0.85
            shade = np.clip(0.55 + 0.45 * sv[:, 2:3], 0.3, 1.0)
            V.append(v + off + (x, y, z)); T.append(st + base_n); C.append(top * shade); base_n += len(sv)
    return big_mesh(name, np.concatenate(V), np.concatenate(T), layer, mat_vegetation(name + "mat", translucency=0.2), colours=np.concatenate(C))


# ------------------------------------------------------------------ interiors (v3)
STAINED = ((1.0, 0.78, 0.3), (0.95, 0.42, 0.5), (0.35, 0.55, 1.0), (1.0, 0.92, 0.7), (0.55, 0.85, 0.7), (0.8, 0.5, 0.95), (1.0, 0.6, 0.25))


def mat_stained(tint=(1.0, 0.85, 0.55), strength=1.6, name="stained3", scale=0.42, tint_mix=0.45):
    """Stained glass: leaded cells of jewel colours (gold, rose, sapphire, cream, mint,
    violet, amber) mixed towards the window's tint, dark lead lines between the cells,
    glowing (the daylight behind it)."""
    key = ("stained", name, tint, strength)
    if key in _mats:
        return _mats[key]
    m = bpy.data.materials.new(name)
    nt, out = B._nodes(m)
    L = nt.links.new
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    vor = nt.nodes.new("ShaderNodeTexVoronoi"); vor.inputs["Scale"].default_value = scale
    L(geo.outputs["Position"], vor.inputs["Vector"])
    edge = nt.nodes.new("ShaderNodeTexVoronoi"); edge.feature = "DISTANCE_TO_EDGE"; edge.inputs["Scale"].default_value = scale
    L(geo.outputs["Position"], edge.inputs["Vector"])
    bw = nt.nodes.new("ShaderNodeSeparateColor"); L(vor.outputs["Color"], bw.inputs[0])
    ramp = nt.nodes.new("ShaderNodeValToRGB"); ramp.color_ramp.interpolation = "CONSTANT"
    els = ramp.color_ramp.elements
    els[0].position = 0.0; els[0].color = (*STAINED[0], 1)
    els[1].position = 0.9; els[1].color = (*STAINED[-1], 1)
    for k, c in enumerate(STAINED[1:-1]):
        e = els.new((k + 1) / len(STAINED)); e.color = (*c, 1)
    L(bw.outputs[0], ramp.inputs["Fac"])
    mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"; mix.inputs["Factor"].default_value = tint_mix
    L(ramp.outputs["Color"], mix.inputs["A"]); mix.inputs["B"].default_value = (*tint, 1)
    lead = nt.nodes.new("ShaderNodeMapRange"); lead.inputs["From Min"].default_value = 0.02; lead.inputs["From Max"].default_value = 0.07
    L(edge.outputs["Distance"], lead.inputs["Value"])
    # Brighter in the middle of each cell (the glass's own sheen).
    sheen = nt.nodes.new("ShaderNodeMapRange"); sheen.inputs["From Min"].default_value = 0.0; sheen.inputs["From Max"].default_value = 0.6
    sheen.inputs["To Min"].default_value = 0.75; sheen.inputs["To Max"].default_value = 1.15
    L(edge.outputs["Distance"], sheen.inputs["Value"])
    st = nt.nodes.new("ShaderNodeMath"); st.operation = "MULTIPLY"
    L(lead.outputs["Result"], st.inputs[0]); L(sheen.outputs["Result"], st.inputs[1])
    st2 = nt.nodes.new("ShaderNodeMath"); st2.operation = "MULTIPLY"; st2.inputs[1].default_value = strength
    L(st.outputs[0], st2.inputs[0])
    em = nt.nodes.new("ShaderNodeEmission")
    L(mix.outputs["Result"], em.inputs["Color"]); L(st2.outputs[0], em.inputs["Strength"])
    L(em.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def mat_ashlar(name="ashlar3", colour=(0.88, 0.83, 0.74), block=(3.2, 1.6), rough=0.6):
    """Cut-stone walls: courses of blocks (each a slightly different tone), fine mortar
    joints, worn faces (noise bump), faint warm staining."""
    key = ("ashlar", name, colour, block)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    L = nt.links.new
    geo = nt.nodes.new("ShaderNodeNewGeometry")
    # Walls face x or y: brick coordinates from (y + x, z) so both read as courses.
    sep = nt.nodes.new("ShaderNodeSeparateXYZ"); L(geo.outputs["Position"], sep.inputs[0])
    add = nt.nodes.new("ShaderNodeMath"); add.operation = "ADD"; L(sep.outputs["X"], add.inputs[0]); L(sep.outputs["Y"], add.inputs[1])
    comb = nt.nodes.new("ShaderNodeCombineXYZ"); L(add.outputs[0], comb.inputs["X"]); L(sep.outputs["Z"], comb.inputs["Y"])
    br = nt.nodes.new("ShaderNodeTexBrick")
    br.inputs["Scale"].default_value = 1.0
    br.inputs["Brick Width"].default_value = block[0]; br.inputs["Row Height"].default_value = block[1]
    br.inputs["Mortar Size"].default_value = 0.05; br.inputs["Mortar Smooth"].default_value = 0.4
    br.offset = 0.5
    c = colour
    br.inputs["Color1"].default_value = (c[0], c[1], c[2], 1)
    br.inputs["Color2"].default_value = (c[0] * 0.88, c[1] * 0.86, c[2] * 0.84, 1)
    br.inputs["Mortar"].default_value = (c[0] * 0.62, c[1] * 0.6, c[2] * 0.58, 1)
    L(comb.outputs[0], br.inputs["Vector"])
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 0.6; nz.inputs["Detail"].default_value = 8
    L(geo.outputs["Position"], nz.inputs["Vector"])
    stain = nt.nodes.new("ShaderNodeMix"); stain.data_type = "RGBA"; stain.blend_type = "MULTIPLY"
    fac = nt.nodes.new("ShaderNodeMapRange"); fac.inputs["From Min"].default_value = 0.45; fac.inputs["From Max"].default_value = 0.75
    fac.inputs["To Max"].default_value = 0.35
    L(nz.outputs["Fac"], fac.inputs["Value"]); L(fac.outputs["Result"], stain.inputs["Factor"])
    L(br.outputs["Color"], stain.inputs["A"]); stain.inputs["B"].default_value = (0.8, 0.7, 0.55, 1)
    L(stain.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = rough
    h = nt.nodes.new("ShaderNodeMath"); h.operation = "MULTIPLY_ADD"
    L(br.outputs["Fac"], h.inputs[0]); h.inputs[1].default_value = -0.6
    L(nz.outputs["Fac"], h.inputs[2])
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.45; bump.inputs["Distance"].default_value = 0.15
    L(h.outputs[0], bump.inputs["Height"]); L(bump.outputs["Normal"], bsdf.inputs["Normal"])
    L(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def peak(cx, cy, z0, radius, height, layer, seed, snow_line=0.62, name="peak", n=220):
    """A single sharp mountain: a ridged-noise cone (arêtes, gullies, a pointed summit),
    snow above the snow line on the gentler faces."""
    xs = np.linspace(-radius, radius, n + 1, dtype=np.float32)
    X, Y = np.meshgrid(xs, xs)
    r = np.sqrt(X ** 2 + Y ** 2) / radius
    rid = _ridged(X / radius * 2.2, Y / radius * 2.2, seed)
    rid = np.clip(rid / 1.6, 0, None)
    cone = np.clip(1 - r, 0, 1) ** 1.25
    Z = z0 + height * cone * (0.75 + 0.45 * rid) - height * 1.5 * np.clip(r - 1, 0, None)
    verts = np.stack([X + cx, Y + cy, Z], -1).reshape(-1, 3)
    I, J = np.meshgrid(np.arange(n), np.arange(n))
    a = (J * (n + 1) + I).ravel()
    tris = np.concatenate([np.stack([a, a + 1, a + n + 2], -1), np.stack([a, a + n + 2, a + n + 1], -1)])
    return big_mesh(name, verts, tris, layer, mat_range("peak%d" % seed, snow_line=snow_line, top=height * 1.15, base_z=z0), smooth=True)


def mat_cap(cap, name=None):
    """A mushroom cap: a deeper tone at the crown fading to a pale rim, mottled, with a
    soft waxy sheen and fine bumps."""
    name = name or "cap3_%.2f_%.2f" % (cap[0], cap[2])
    key = ("cap", name)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled(name)
    L = nt.links.new
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ"); L(tc.outputs["Generated"], sep.inputs[0])
    ramp = nt.nodes.new("ShaderNodeValToRGB"); els = ramp.color_ramp.elements
    els[0].position = 0.2; els[0].color = (min(1, cap[0] * 1.15 + 0.12), min(1, cap[1] * 1.15 + 0.12), min(1, cap[2] * 1.15 + 0.12), 1)
    els[1].position = 0.95; els[1].color = (cap[0] * 0.7, cap[1] * 0.62, cap[2] * 0.7, 1)
    L(sep.outputs["Z"], ramp.inputs["Fac"])
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 6.0; nz.inputs["Detail"].default_value = 8
    L(tc.outputs["Generated"], nz.inputs["Vector"])
    mott = nt.nodes.new("ShaderNodeMix"); mott.data_type = "RGBA"; mott.blend_type = "OVERLAY"; mott.inputs["Factor"].default_value = 0.35
    L(ramp.outputs["Color"], mott.inputs["A"]); L(nz.outputs["Color"], mott.inputs["B"])
    L(mott.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.35
    bsdf.inputs["Subsurface Weight"].default_value = 0.15
    bsdf.inputs["Subsurface Radius"].default_value = (0.5, 0.25, 0.2)
    bsdf.inputs["Coat Weight"].default_value = 0.3
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.25
    L(nz.outputs["Fac"], bump.inputs["Height"]); L(bump.outputs["Normal"], bsdf.inputs["Normal"])
    L(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m
