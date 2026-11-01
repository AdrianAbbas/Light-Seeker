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
    vor = nt.nodes.new("ShaderNodeTexVoronoi"); vor.feature = "DISTANCE_TO_EDGE"; vor.inputs["Scale"].default_value = 0.35
    warp = noise(0.2, 4, 0.5, w=7)
    mixv = nt.nodes.new("ShaderNodeVectorMath"); mixv.operation = "ADD"
    sc = nt.nodes.new("ShaderNodeVectorMath"); sc.operation = "SCALE"; sc.inputs["Scale"].default_value = 3.0
    L(warp.outputs["Color"], sc.inputs[0]); L(tc.outputs["Object"], mixv.inputs[0]); L(sc.outputs[0], mixv.inputs[1])
    L(mixv.outputs[0], vor.inputs["Vector"])
    crack = nt.nodes.new("ShaderNodeMapRange"); crack.inputs["From Min"].default_value = 0.0; crack.inputs["From Max"].default_value = 0.06
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
    top = lambda x, y: z + 0.6 * math.sin(x * 0.3) * math.cos(y * 0.3)
    B.cylinder((cx, cy, z - 1.2), radius * 1.01, 1.4, layer, mat_ground(), sides=64, name="isletop")
    grass(lambda x, y: z + 0.2, cx - radius * 0.7, cx + radius * 0.7, cy - radius * 0.7, cy + radius * 0.7, int(radius * radius * 60), layer, seed,
          height=(0.3, 0.8))
    flowers(lambda x, y: z + 0.2, cx - radius * 0.6, cx + radius * 0.6, cy - radius * 0.6, cy + radius * 0.6, int(radius * radius * 4), layer, seed + 1,
            size=0.25)
    rng = random.Random(seed)
    for k in range(trees):
        tree((cx + rng.uniform(-0.5, 0.5) * radius, cy + rng.uniform(-0.4, 0.4) * radius, z), radius * rng.uniform(0.4, 0.7), seed * 7 + k, layer,
             kind="blossom" if rng.random() < blossom else "green", leaves=6000)


def mushroom2(x, y, z, size, layer, seed, cap=(0.92, 0.42, 0.5)):
    """A fairy-tale mushroom: a curved stem, a domed cap with white spots, gills below."""
    rng = random.Random(seed)
    stem = mat_marble("stem%.2f" % size, colour=(0.95, 0.92, 0.85), vein=(0.85, 0.82, 0.76))
    B.lathe((x, y, z), [(size * 0.2, 0), (size * 0.15, size * 0.35), (size * 0.13, size * 0.7), (size * 0.18, size * 0.82), (0.0, size * 0.83)], layer, stem, sides=24, name="stem")
    capm = B.mat_stone("cap%.2f" % cap[0], cap, rough=0.4, bump=0.2, scale=4.0)
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
