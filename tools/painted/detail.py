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


CLOUD_GLOW = 0.4


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
    vol.inputs["Anisotropy"].default_value = 0.62   # forward scattering: backlit clouds get silver linings
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
    nt.links.new(dist.outputs["Fac"], billow.inputs[0]); billow.inputs[1].default_value = 1.45; billow.inputs[2].default_value = -0.72
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
          base=(0.07, 0.2, 0.03), tip=(0.42, 0.55, 0.12), name="grass", keep=None):
    """Curved, tapered blades (two quads and a tip each); denser near the viewer (near_y)."""
    rng = np.random.default_rng(seed)
    # Denser near the camera: y drawn with a bias towards y0.
    u = rng.random(count)
    ys = y0 + (y1 - y0) * u ** 1.8
    xs = x0 + (x1 - x0) * rng.random(count)
    if keep is not None:
        inside = np.array([keep(x, y) for x, y in zip(xs, ys)], bool)
        xs, ys = xs[inside], ys[inside]
        count = len(xs)
        if count == 0:
            return None
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
            stem=0.35, keep=None):
    """Meadow flowers: a ring of petals (12 for daisies, 6 for the others) around a domed
    centre, held a little above the ground, facing up and slightly towards the light."""
    rng = random.Random(seed)
    verts, tris, cols = [], [], []
    for _ in range(count):
        u = rng.random()
        y = y0 + (y1 - y0) * u ** 1.6
        x = rng.uniform(x0, x1)
        if keep is not None and not keep(x, y):
            continue
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
    """A volumetric cumulus heap (size = (x, y, z) extents of its body): a broad flat-based
    body and a cluster of rounder puffs piled on it, each with its own billows, so it
    towers and breaks into cauliflower domes instead of reading as one smooth blob
    (Direction Update 2)."""
    x, y, z = location
    sx, sy, sz = size
    rng = random.Random(seed * 31 + 7)

    def box(cx, cy, cz, bx, by, bz, k):
        v = [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0), (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]
        verts = [(cx + a * bx / 2, cy + b * by / 2, cz + c * bz) for a, b, c in v]
        faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        return B.mesh_object(name if k == 0 else "%s_puff%d" % (name, k), verts, faces, layer,
                             mat_cloud("cumulus%d_%d" % (seed, k), density, w=float((seed * 7 + k * 13) % 23) * 1.7))
    ob = box(x, y, z, sx, sy, sz * 0.8, 0)
    # Only clouds in the distant sky tower; banks closer in (a sea of cloud under the
    # islands, mist round the peaks) stay low and flat, or they would rise in front of the eye.
    puffs = 2 + rng.randrange(3) if layer == "far" else 0
    for k in range(1, puffs + 1):
        f = rng.uniform(0.32, 0.6)
        px = x + rng.uniform(-0.32, 0.32) * sx
        py = y + rng.uniform(-0.2, 0.2) * sy
        pz = z + sz * rng.uniform(0.25, 0.65)
        box(px, py, pz, sx * f, sy * f * 1.1, sz * f * 1.9, k)
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
    tz = np.broadcast_to(z + radius * 0.07 * (1 - rho ** 2) - 1.2 * rho ** 8, tx.shape)   # (no noise: the grass must sit on it, see top())
    tv = np.concatenate([[[cx, cy, z + radius * 0.07]], np.stack([tx, ty, tz], -1).reshape(-1, 3)])
    T = [(0, 1 + k, 1 + (k + 1) % nu) for k in range(nu)]
    for j in range(nr - 1):
        for k in range(nu):
            a_ = 1 + j * nu + k; b_ = 1 + j * nu + (k + 1) % nu
            T += [(a_, a_ + nu, b_ + nu), (a_, b_ + nu, b_)]
    big_mesh("isletop", tv, np.array(T), layer, mat_ground("isleground", (0.12, 0.24, 0.06)), smooth=True)
    def top(x, y):
        # The turf's own height at (x, y): the same profile, relative to the outline there.
        a = math.atan2(y - cy, x - cx) % (2 * math.pi)
        q = min(1.0, math.hypot(x - cx, y - cy) / (rim[int(a / (2 * math.pi) * nu) % nu] * 1.02))
        return z + radius * 0.07 * (1 - q * q) - 1.2 * q ** 8
    # Grass and flowers over the whole top, inside its real outline (Direction Update 2:
    # strips left bare patches where the outline reaches further than a circle).
    def inside(x, y):
        a = math.atan2(y - cy, x - cx) % (2 * math.pi)
        return math.hypot(x - cx, y - cy) < rim[int(a / (2 * math.pi) * nu) % nu] * 0.98
    R = float(rim.max()) * 1.02
    for k in range(6):                        # bands front to back (their blades grow a little with distance)
        y0 = cy - R + k * 2 * R / 6; y1 = y0 + 2 * R / 6
        grass(top, cx - R, cx + R, y0, y1, int(R * R * 15), layer, seed + k, height=(0.3, 0.8), keep=inside)
        flowers(top, cx - R, cx + R, y0, y1, int(R * R * 1.1), layer, seed + 40 + k, size=0.25, keep=inside)
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


def _noise_field(x, y, seed, scale, octaves=4):
    """fBm noise in [-1, 1] on numpy arrays (mathutils per point)."""
    from mathutils import noise as mn
    out = np.empty(x.shape, np.float32)
    fx, fy, o = x.ravel(), y.ravel(), out.ravel()
    for i in range(fx.size):
        o[i] = mn.fractal(Vector((fx[i] * scale, fy[i] * scale, seed * 3.17)), 0.5, 2.0, octaves)
    return out


def alpine(seed, y, depth, x0, x1, base_z, height, layer, nx=900, ny=140, peaks=11, rock=(0.44, 0.42, 0.46),
           snow_line=0.5, rough=1.0, detail_scale=None, octaves=7, hills=False, name="alpine"):
    """A believable mountain range (final touches): distinct massifs with pointed summits,
    saddles between them and spurs running down (not a wall of noise). Each massif is a
    cone about as wide as it is tall (slopes of 30-40 degrees, as real ranges); domain-
    warped ridged noise carves arêtes and gullies into it, the gullies deepening with
    height (a cheap stand-in for erosion); the foot fades into the plain in front."""
    rng = np.random.default_rng(seed)
    xs = np.linspace(x0, x1, nx + 1, dtype=np.float32)
    ys = np.linspace(y, y + depth, ny + 1, dtype=np.float32)
    X, Y = np.meshgrid(xs, ys)
    # Massifs: soft-max of cones, the highest towards the middle of the depth.
    M = np.zeros_like(X)
    span = x1 - x0
    for k in range(peaks):
        px = x0 + span * (k + 0.5 + rng.uniform(-0.35, 0.35)) / peaks
        py = y + depth * rng.uniform(0.35, 0.7)
        h = height * rng.uniform(0.5, 1.0) ** 0.7
        if hills:
            # Foothills: broad rounded domes several times wider than tall.
            r = h * rng.uniform(3.5, 5.5)
            d = np.sqrt((X - px) ** 2 + ((Y - py) * 1.15) ** 2) / r
            cone = h * np.clip(1 - d * d, 0, None) ** 1.6
        else:
            r = h * rng.uniform(1.3, 1.9)
            d = np.sqrt((X - px) ** 2 + ((Y - py) * 1.15) ** 2) / r
            cone = h * np.clip(1 - d, 0, None) ** 1.35
        M = np.maximum(M, cone) + 0.12 * np.minimum(M, cone)    # saddles between neighbours
    # A long ridge line joining them (the range, not separate hills).
    spine = height * 0.42 * np.exp(-((Y - (y + depth * 0.52)) / (depth * 0.28)) ** 2)
    M = np.maximum(M, spine * (0.75 + 0.25 * _noise_field(X, Y, seed + 5, 1.0 / (height * 0.9), 3)))
    # Arêtes and gullies: domain-warped ridged noise, stronger higher up.
    f = 1.0 / (detail_scale or height * 0.55)
    wx = _noise_field(X, Y, seed + 1, f * 0.5, 3) * height * 0.35
    wy = _noise_field(X, Y, seed + 2, f * 0.5, 3) * height * 0.35
    rid = np.clip(_ridged((X + wx) * f, (Y + wy) * f, seed, octaves=octaves) / 1.6, 0, None)
    hn = np.clip(M / height, 0, 1)
    if hills:
        # Rolling, wooded: soft fBm bumps (crowns of woods) instead of arêtes.
        M = M * (0.85 + 0.25 * _noise_field(X, Y, seed + 7, 1.0 / (height * 1.6), 4)) + height * 0.06 * _noise_field(X, Y, seed + 8, 1.0 / 90.0, 3)
    Z = M * (1.0 - 0.28 * rough + 0.42 * rough * rid * (0.4 + 0.6 * hn)) + height * 0.05 * rough * rid
    # The foot fades into the plain at the front edge; the back edge stays high (no gap).
    t = np.clip((Y - y) / (depth * 0.22), 0, 1)
    Z = base_z + Z * (t * t * (3 - 2 * t))
    Z[0, :] = base_z - 20
    verts = np.stack([X, Y, Z], -1).reshape(-1, 3)
    i = np.arange(nx); j = np.arange(ny)
    I, J = np.meshgrid(i, j)
    a = (J * (nx + 1) + I).ravel()
    tris = np.concatenate([np.stack([a, a + 1, a + nx + 2], -1), np.stack([a, a + nx + 2, a + nx + 1], -1)])
    top = float(Z.max() - base_z)
    return big_mesh(name, verts, tris, layer, mat_range("alp%d" % seed, rock=rock, snow_line=snow_line, top=top, base_z=base_z), smooth=True)


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


# ------------------------------------------------------------- foliage (Direction Update 2)
# Natural trees: a recursive branching skeleton (golden-angle phyllotaxis, tropism, pipe-
# model taper) as continuous smooth tubes, and a crown of leaf cards: quads textured with a
# sprig of leaves (or blossom) drawn here in numpy (alpha-cut, original, no external
# images). Distant woods: the same cards around a dense core, so every crown has a broken,
# leafy silhouette instead of a lumpy sphere.
import os as _os

TEXTURES = _os.path.join(_os.path.dirname(_os.path.abspath(__file__)), "..", "..", "build-art", "textures")


def _sprig_image(kind, size=512, seed=7):
    """A sprig on a transparent ground (RGBA float, rows top to bottom): a thin stem and
    alternate lanceolate leaves with a midrib and a lighter half, or five-petalled blossoms
    in clusters. The colour is mostly a light value (the card's vertex colour gives the hue)."""
    S = size * 2                                   # drawn at twice the size, then averaged
    rng = np.random.default_rng(seed + (0 if kind == "leaf" else 50))
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
    rgb = np.zeros((S, S, 3), np.float32)
    alpha = np.zeros((S, S), np.float32)

    def stem_at(t):
        return np.array([S * (0.5 + 0.07 * math.sin(t * math.pi * 1.2)), S * (0.97 - 0.9 * t)], np.float32)

    # The stem.
    for t in np.linspace(0, 0.97, 220):
        c = stem_at(t)
        r = S * (0.010 * (1 - t) + 0.003)
        m = (xx - c[0]) ** 2 + (yy - c[1]) ** 2 < r * r
        rgb[m] = (0.42, 0.40, 0.26); alpha[m] = 1.0
    if kind == "leaf":
        n = 11
        for i in range(n):
            t = 0.1 + 0.85 * i / (n - 1) + rng.uniform(-0.02, 0.02)
            side = 1 if i % 2 else -1
            base = stem_at(t)
            ang = math.radians(rng.uniform(32, 58)) * side            # from the stem (pointing up)
            d = np.array([math.sin(ang), -math.cos(ang)], np.float32)
            nrm = np.array([-d[1], d[0]], np.float32)
            L = S * rng.uniform(0.2, 0.3) * (1.05 - 0.35 * t)
            W = L * rng.uniform(0.28, 0.36)
            px, py = xx - base[0], yy - base[1]
            u = (px * d[0] + py * d[1]) / L
            v = (px * nrm[0] + py * nrm[1]) / W
            prof = np.clip(np.sin(np.pi * np.clip(u, 0, 1) ** 0.75), 0, 1) ** 0.9
            inside = (u > 0) & (u < 1) & (np.abs(v) < prof)
            # Value: a lighter half, a darker midrib, faint side veins, darker towards the edge.
            val = 0.82 + 0.16 * np.sign(v) * side - 0.18 * np.exp(-(v / 0.07) ** 2)
            val -= 0.07 * (np.abs(v) / np.maximum(prof, 1e-3)) ** 3
            val -= 0.05 * (np.sin((u * 9.0 + np.abs(v) * 3.0) * np.pi) > 0.85)
            val += 0.08 * u
            col = np.stack([val * 0.96, val, val * 0.86], -1)
            rgb[inside] = col[inside]; alpha[inside] = 1.0
    else:
        for k in range(16):
            t = rng.uniform(0.15, 1.0)
            c = stem_at(min(t, 0.97)) + rng.normal(0, S * 0.09, 2).astype(np.float32)
            R = S * rng.uniform(0.055, 0.08)
            rot = rng.uniform(0, 2 * math.pi)
            px, py = xx - c[0], yy - c[1]
            rr = np.hypot(px, py); th = np.arctan2(py, px) - rot
            petal = rr < R * (0.55 + 0.45 * np.abs(np.cos(2.5 * th)) ** 0.6)
            val = 0.9 + 0.1 * (rr / R) - 0.08 * (np.cos(5 * th) > 0.6)
            col = np.stack([val, val, val], -1)
            rgb[petal] = col[petal]; alpha[petal] = 1.0
            centre = rr < R * 0.18
            rgb[centre] = (1.15, 0.95, 0.45); alpha[centre] = 1.0
    # Average down (soft, antialiased edges); colour premultiplied then divided back.
    pre = rgb * alpha[..., None]
    pre = pre.reshape(size, 2, size, 2, 3).mean((1, 3)); a = alpha.reshape(size, 2, size, 2).mean((1, 3))
    col = pre / np.maximum(a[..., None], 1e-4)
    return np.dstack([np.clip(col, 0, 1.2), a])


def _sprig_texture(kind):
    """The sprig as a Blender image (made once per build into build-art/textures)."""
    name = "sprig-%s" % kind
    if name in bpy.data.images:
        return bpy.data.images[name]
    _os.makedirs(TEXTURES, exist_ok=True)
    path = _os.path.abspath(_os.path.join(TEXTURES, name + ".png"))
    if not _os.path.exists(path):
        from PIL import Image
        img = _sprig_image(kind)
        Image.fromarray((np.clip(img, 0, 1) * 255 + 0.5).astype(np.uint8), "RGBA").save(path)
    im = bpy.data.images.load(path)
    im.name = name
    im.alpha_mode = "STRAIGHT"
    return im


def mat_leafcard(kind="leaf", translucency=0.4):
    """A leaf card: the sprig's value times the card's colour, cut out by its alpha,
    lit from both sides, a little roughness variation from leaf to leaf."""
    key = ("leafcard", kind)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled("leafcard_" + kind)
    L = nt.links.new
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = _sprig_texture(kind); tex.interpolation = "Linear"
    uv = nt.nodes.new("ShaderNodeUVMap"); uv.uv_map = "UVMap"
    L(uv.outputs["UV"], tex.inputs["Vector"])
    col = _attr_colour(nt)
    mul = nt.nodes.new("ShaderNodeMix"); mul.data_type = "RGBA"; mul.blend_type = "MULTIPLY"; mul.inputs["Factor"].default_value = 1.0
    L(tex.outputs["Color"], mul.inputs["A"]); L(col, mul.inputs["B"])
    L(mul.outputs["Result"], bsdf.inputs["Base Color"])
    rn = nt.nodes.new("ShaderNodeTexNoise"); rn.inputs["Scale"].default_value = 3.0
    rr = nt.nodes.new("ShaderNodeMapRange"); rr.inputs["To Min"].default_value = 0.38; rr.inputs["To Max"].default_value = 0.62
    L(rn.outputs["Fac"], rr.inputs["Value"]); L(rr.outputs["Result"], bsdf.inputs["Roughness"])
    try:
        bsdf.inputs["Specular IOR Level"].default_value = 0.35
    except KeyError:
        pass
    tr = nt.nodes.new("ShaderNodeBsdfTranslucent"); L(mul.outputs["Result"], tr.inputs["Color"])
    leaf = nt.nodes.new("ShaderNodeMixShader"); leaf.inputs["Fac"].default_value = translucency
    L(bsdf.outputs[0], leaf.inputs[1]); L(tr.outputs[0], leaf.inputs[2])
    if kind == "blossom":
        # Blossom is luminous: a little of its own colour glows (petals full of light), so
        # its shaded side stays pale pink instead of turning purple under the blue sky.
        glow = nt.nodes.new("ShaderNodeEmission"); L(mul.outputs["Result"], glow.inputs["Color"]); glow.inputs["Strength"].default_value = 0.35
        lit = nt.nodes.new("ShaderNodeAddShader"); L(leaf.outputs[0], lit.inputs[0]); L(glow.outputs[0], lit.inputs[1])
        leaf = lit
    clear = nt.nodes.new("ShaderNodeBsdfTransparent")
    cut = nt.nodes.new("ShaderNodeMixShader")
    L(tex.outputs["Alpha"], cut.inputs["Fac"]); L(clear.outputs[0], cut.inputs[1]); L(leaf.outputs[0], cut.inputs[2])
    L(cut.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def cards_mesh(name, base, along, across, size, colours, layer, material, aspect=0.9):
    """Leaf cards: one quad per row, from `base` along `along` (unit) for `size`, `aspect`
    times as wide across `across` (unit); UVs over the sprig."""
    base = np.asarray(base, np.float32); a = np.asarray(along, np.float32); b = np.asarray(across, np.float32)
    s = np.asarray(size, np.float32)[:, None]
    w = s * aspect * 0.5
    v = np.stack([base - b * w, base + b * w, base + a * s + b * w, base + a * s - b * w], 1)
    n = len(base)
    tris = (np.array([[0, 1, 2], [0, 2, 3]])[None] + (np.arange(n) * 4)[:, None, None]).reshape(-1, 3)
    ob = big_mesh(name, v.reshape(-1, 3), tris, layer, material, np.repeat(np.asarray(colours, np.float32), 4, axis=0))
    me = ob.data
    uvl = me.uv_layers.new(name="UVMap")
    quad = np.array([[0, 0], [1, 0], [1, 1], [0, 0], [1, 1], [0, 1]], np.float32)
    uvl.data.foreach_set("uv", np.tile(quad, (n, 1)).ravel())
    return ob


def tube_chains(chains, layer, material, name):
    """Continuous smooth tubes, one per chain of points (a ring at each point, oriented by
    parallel transport so they never twist), sides from 12 on the trunk to 4 on twigs."""
    V, T = [], []
    count = 0
    for pts, rads in chains:
        P = np.array([[p.x, p.y, p.z] for p in pts], np.float32)
        R = np.asarray(rads, np.float32)
        sides = int(np.clip(4 + R[0] / max(1e-4, chains[0][1][0]) * 8, 4, 12))
        tang = np.gradient(P, axis=0); tang /= np.linalg.norm(tang, axis=1, keepdims=True) + 1e-9
        ref = np.array([0, 0, 1], np.float32) if abs(tang[0, 2]) < 0.9 else np.array([1, 0, 0], np.float32)
        u = np.cross(tang[0], ref); u /= np.linalg.norm(u) + 1e-9
        ang = np.linspace(0, 2 * np.pi, sides, endpoint=False)
        rings = []
        for i in range(len(P)):
            u = u - tang[i] * np.dot(u, tang[i]); u /= np.linalg.norm(u) + 1e-9
            w = np.cross(tang[i], u)
            rings.append(P[i] + (np.cos(ang)[:, None] * u + np.sin(ang)[:, None] * w) * R[i])
        V.append(np.concatenate(rings))
        for i in range(len(P) - 1):
            for k in range(sides):
                a0 = count + i * sides + k; a1 = count + i * sides + (k + 1) % sides
                b0 = a0 + sides; b1 = a1 + sides
                T += [(a0, a1, b1), (a0, b1, b0)]
        count += len(P) * sides
    return big_mesh(name, np.concatenate(V), np.array(T, np.int32), layer, material, smooth=True)


def mat_bark():
    """Bark: grey-brown, long vertical furrows (stretched noise and cells), lichen patches,
    rough, with a matching bump."""
    key = ("bark3",)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled("bark3")
    L = nt.links.new
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sc = nt.nodes.new("ShaderNodeMapping"); sc.inputs["Scale"].default_value = (3.0, 3.0, 0.6)
    L(tc.outputs["Object"], sc.inputs["Vector"])
    vor = nt.nodes.new("ShaderNodeTexVoronoi"); vor.feature = "DISTANCE_TO_EDGE"; vor.inputs["Scale"].default_value = 2.5
    L(sc.outputs["Vector"], vor.inputs["Vector"])
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 4.0; nz.inputs["Detail"].default_value = 10
    L(sc.outputs["Vector"], nz.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.0; ramp.color_ramp.elements[0].color = (0.10, 0.075, 0.055, 1)
    ramp.color_ramp.elements[1].position = 0.12; ramp.color_ramp.elements[1].color = (0.36, 0.30, 0.24, 1)
    L(vor.outputs["Distance"], ramp.inputs["Fac"])
    lich = nt.nodes.new("ShaderNodeTexNoise"); lich.inputs["Scale"].default_value = 1.6; lich.inputs["Detail"].default_value = 6
    L(tc.outputs["Object"], lich.inputs["Vector"])
    lm = nt.nodes.new("ShaderNodeMapRange"); lm.inputs["From Min"].default_value = 0.58; lm.inputs["From Max"].default_value = 0.68
    L(lich.outputs["Fac"], lm.inputs["Value"])
    mix = nt.nodes.new("ShaderNodeMix"); mix.data_type = "RGBA"
    L(lm.outputs["Result"], mix.inputs["Factor"]); L(ramp.outputs["Color"], mix.inputs["A"]); mix.inputs["B"].default_value = (0.5, 0.55, 0.38, 1)
    tint = nt.nodes.new("ShaderNodeMix"); tint.data_type = "RGBA"; tint.blend_type = "MULTIPLY"; tint.inputs["Factor"].default_value = 0.3
    L(mix.outputs["Result"], tint.inputs["A"]); L(nz.outputs["Color"], tint.inputs["B"])
    L(tint.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.88
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.7
    add = nt.nodes.new("ShaderNodeMath"); add.operation = "ADD"
    L(vor.outputs["Distance"], add.inputs[0]); L(nz.outputs["Fac"], add.inputs[1])
    L(add.outputs[0], bump.inputs["Height"]); L(bump.outputs["Normal"], bsdf.inputs["Normal"])
    L(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


LEAF_GREENS = np.array([[0.16, 0.34, 0.07], [0.23, 0.44, 0.09], [0.32, 0.52, 0.12], [0.13, 0.29, 0.08], [0.4, 0.55, 0.14]], np.float32)
BLOSSOM_PINKS = np.array([[1.0, 0.58, 0.72], [1.0, 0.68, 0.8], [0.99, 0.52, 0.68], [1.0, 0.8, 0.88]], np.float32)   # linear: pink in sRGB
AUTUMN = np.array([[0.85, 0.55, 0.15], [0.9, 0.7, 0.2], [0.6, 0.55, 0.15]], np.float32)


def _skeleton(rng, height, levels, spread):
    """The branches of a broadleaf tree as chains of points with radii, and the terminal
    branches (where leaves grow). Height 1, base at the origin; scaled by the caller."""
    up = Vector((0, 0, 1))
    chains, tips = [], []
    trunk_r = rng.uniform(0.026, 0.034)
    clear = rng.uniform(0.26, 0.4)
    def perp(d, rot):
        ref = Vector((1, 0, 0)) if abs(d.x) < 0.9 else Vector((0, 1, 0))
        p = d.cross(ref).normalized()
        q = d.cross(p).normalized()
        return (p * math.cos(rot) + q * math.sin(rot)).normalized()

    def grow(p, d, length, r0, level):
        n = 3 + int(length / 0.07) if level < 3 else 3
        pts, rads = [p.copy()], [r0]
        cur, dirv = p.copy(), d.normalized()
        taper = 0.55 if level < levels else 0.8
        for i in range(n):
            t = (i + 1) / n
            wob = Vector((rng.gauss(0, 0.13), rng.gauss(0, 0.13), rng.gauss(0, 0.05)))
            trop = up * (0.07 if level <= 1 else 0.02) - up * (0.05 * level * t if level >= 2 else 0.0)
            dirv = (dirv + wob + trop).normalized()
            cur = cur + dirv * (length / n)
            pts.append(cur.copy()); rads.append(max(r0 * (1 - taper * t), 0.0015))
        chains.append((pts, rads, level))
        if level >= levels:
            tips.append((pts, level))
            return
        if level == 0:
            kids = rng.randint(3, 5)
            ts = [rng.uniform(0.82, 1.0) for _ in range(kids)]
        else:
            kids = {1: rng.randint(4, 6), 2: rng.randint(3, 5)}.get(level, rng.randint(2, 4))
            ts = sorted(rng.uniform(0.3, 0.98) for _ in range(kids))
        rot0 = rng.uniform(0, 2 * math.pi)
        for j, t in enumerate(ts):
            i = min(len(pts) - 1, max(1, int(round(t * (len(pts) - 1)))))
            base = pts[i]; pd = (pts[i] - pts[i - 1]).normalized()
            rot = rot0 + j * 2.39996
            a = math.radians(rng.uniform(22, 38) if level == 0 else rng.uniform(32, 58))
            cd = (pd * math.cos(a) + perp(pd, rot) * math.sin(a)).normalized()
            if level == 0:
                cd = (cd + Vector((cd.x, cd.y, 0)) * (spread - 0.4)).normalized()
            cl = length * (rng.uniform(0.95, 1.25) if level == 0 else rng.uniform(0.5, 0.72) * (1.15 - 0.45 * t))
            if level == 0:
                cl = (1.0 - clear) * rng.uniform(0.5, 0.62)
            cr = rads[i] * rng.uniform(0.62, 0.78)
            grow(base, cd, cl, cr, level + 1)
        if level >= levels - 1:
            tips.append((pts, level))

    trunk_dir = Vector((rng.uniform(-0.06, 0.06), rng.uniform(-0.06, 0.06), 1)).normalized()
    grow(Vector((0, 0, -0.01)), trunk_dir, clear, trunk_r, 0)
    # The root flare: the trunk's first ring wider.
    chains[0][1][0] *= 1.6
    return chains, tips


def tree(location, height, seed, layer, kind="green", leaves=9000, spread=0.55, name="tree"):
    """A broadleaf tree (or a blossoming one): a trunk dividing into leaders, branches in a
    golden-angle spiral, twigs, all continuous tapering tubes; a crown of leaf cards on the
    twigs (sunlit and lighter above, cooler and darker inside and below). Every tree is a
    little different in height, spread, lean and how its crown breaks up."""
    rng = random.Random(seed)
    x, y, z = location
    height *= rng.uniform(0.88, 1.12)
    spread *= rng.uniform(0.85, 1.2)
    chains, tips = _skeleton(rng, height, 3, spread)
    # Normalise: the crown's top at `height`.
    top = max(p.z for pts, _, _ in chains for p in pts)
    k = height / max(top, 1e-3)
    origin = Vector((x, y, z))
    scaled = [([origin + p * k for p in pts], [r * k for r in rads]) for pts, rads, _ in chains]
    tube_chains(scaled, layer, mat_bark(), name + "_wood")
    # Leaf cards on the terminal branches (and the tips of the ones before them).
    cards = max(40, leaves // 5)
    blossom = kind == "blossom"
    nr = np.random.default_rng(seed)
    per = max(2, cards // max(1, len(tips)))
    base, along, across, sizes, cols = [], [], [], [], []
    centre = origin + Vector((0, 0, height * 0.65))
    for pts, level in tips:
        n = per if level >= 3 else max(1, per // 3)
        for _ in range(n):
            t = nr.uniform(0.25, 1.0) if level >= 3 else nr.uniform(0.8, 1.0)
            fi = t * (len(pts) - 1); i = int(fi); f = fi - i
            p = pts[min(i, len(pts) - 1)].lerp(pts[min(i + 1, len(pts) - 1)], f)
            P = origin + p * k
            out_ = (P - centre); out_.z *= 0.6
            out_ = out_.normalized() if out_.length > 1e-6 else Vector((0, 0, 1))
            d = Vector(nr.normal(0, 1, 3)).normalized() * 0.7 + out_ * 0.8 + Vector((0, 0, 0.35))
            d.normalize()
            side = d.cross(Vector(nr.normal(0, 1, 3))).normalized()
            s = height * (0.095 if not blossom else 0.08) * nr.uniform(0.7, 1.25)
            base.append(P[:]); along.append(d[:]); across.append(side[:]); sizes.append(s)
            # Light: higher and more outward is brighter and warmer; inside, cooler.
            rel = (P.z - z) / height
            sun = np.clip(0.55 + 0.55 * rel + 0.25 * out_.z + nr.normal(0, 0.06), 0.45, 1.25)
            pal = BLOSSOM_PINKS if blossom else (AUTUMN if kind == "autumn" else LEAF_GREENS)
            c = pal[nr.integers(0, len(pal))] * sun
            if not blossom and nr.random() < 0.08:
                c = c * np.array([1.15, 1.08, 0.7])        # a few sunlit yellow-green sprigs
            cols.append(c)
    if blossom:
        # A blossoming tree keeps a few young green leaves among the flowers.
        for j in range(len(base) // 6):
            i = nr.integers(0, len(base))
            base.append(base[i]); along.append(along[(i + 1) % len(along)]); across.append(across[i]); sizes.append(sizes[i] * 0.8)
            cols.append(LEAF_GREENS[nr.integers(0, len(LEAF_GREENS))] * 1.1)
    if blossom:
        n_flower = len(base) - len(base) // 7
        cards_mesh(name + "_blossom", base[:n_flower], along[:n_flower], across[:n_flower], sizes[:n_flower], cols[:n_flower], layer, mat_leafcard("blossom", 0.55))
        cards_mesh(name + "_leaves", base[n_flower:], along[n_flower:], across[n_flower:], sizes[n_flower:], cols[n_flower:], layer, mat_leafcard("leaf"))
    else:
        cards_mesh(name + "_leaves", base, along, across, sizes, cols, layer, mat_leafcard("leaf"))


def grove(fn, x0, x1, y0, y1, count, layer, seed, size=(10, 22), blossom=0.2, name="grove", clump=0.6, avoid=(), min_z=None):
    """Distant trees, clumped into woods: each a trunk, a dense core of a few soft lobes
    (smooth, darker inside) and a fringe of leaf cards around it that breaks the outline
    into twigs and leaves. One mesh per part: hundreds of trees cheaply."""
    rng = np.random.default_rng(seed)
    import bmesh
    bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)
    sv = np.array([v.co[:] for v in bm.verts], np.float32)
    st = np.array([[v.index for v in f.verts] for f in bm.faces], np.int32)
    bm.free()
    centres = rng.uniform((x0, y0), (x1, y1), (max(1, int(count * (1 - clump) / 6) + 3), 2))
    V, T, C = [], [], []
    TV, TT, TC = [], [], []
    cb, ca, cx_, cs, cc = [], [], [], [], []
    cbb, cab, cxb, csb, ccb = [], [], [], [], []
    base_n = 0; tn = 0
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
        pal = BLOSSOM_PINKS if bl else LEAF_GREENS
        top = pal[rng.integers(0, len(pal))] * rng.uniform(0.85, 1.1)
        # Trunk: a tapered prism, slightly leaning.
        tw = s * 0.045
        lean = rng.normal(0, s * 0.03, 2)
        ring = np.array([[math.cos(a), math.sin(a)] for a in np.linspace(0, 2 * np.pi, 6, endpoint=False)], np.float32)
        lo = np.column_stack([ring * tw * 1.3, np.zeros(6)]); hi = np.column_stack([ring * tw * 0.6 + lean, np.full(6, s * 0.6)])
        TV.append(np.concatenate([lo, hi]) + (x, y, z))
        TT.append(np.array([[i, (i + 1) % 6, 6 + (i + 1) % 6] for i in range(6)] + [[i, 6 + (i + 1) % 6, 6 + i] for i in range(6)], np.int32) + tn)
        tn += 12
        lobes = rng.integers(3, 6)
        for lobe in range(lobes):
            r = s * rng.uniform(0.2, 0.3)
            off = np.array((rng.normal(0, s * 0.14) + lean[0], rng.normal(0, s * 0.14) + lean[1], s * (0.55 + rng.uniform(0, 0.3))), np.float32)
            v = sv * r * (1 + 0.12 * rng.standard_normal(sv.shape).astype(np.float32))
            v[:, 2] *= 0.9
            shade = np.clip(0.35 + 0.45 * (sv[:, 2:3] * 0.5 + 0.5), 0.3, 0.85)       # a darker core
            V.append(v + off + (x, y, z)); T.append(st + base_n); C.append(top * shade); base_n += len(sv)
            # The fringe: cards from the lobe's surface outwards.
            nc = 9
            dirs = rng.normal(0, 1, (nc, 3)).astype(np.float32); dirs[:, 2] = np.abs(dirs[:, 2]) * 0.8 + 0.2
            dirs /= np.linalg.norm(dirs, axis=1, keepdims=True)
            pos = off + (x, y, z) + dirs * r * 0.55
            side = np.cross(dirs, rng.normal(0, 1, (nc, 3))); side /= np.linalg.norm(side, axis=1, keepdims=True) + 1e-6
            light = np.clip(0.75 + 0.4 * dirs[:, 2:3] + rng.normal(0, 0.05, (nc, 1)), 0.5, 1.2)
            if bl:
                cbb.append(pos); cab.append(dirs); cxb.append(side); csb.append(np.full(nc, r * 0.95)); ccb.append(top * light)
            else:
                cb.append(pos); ca.append(dirs); cx_.append(side); cs.append(np.full(nc, r * 0.95)); cc.append(top * light)
    big_mesh(name + "_trunks", np.concatenate(TV), np.concatenate(TT), layer, mat_bark(), colours=None, smooth=True)
    ob = big_mesh(name, np.concatenate(V), np.concatenate(T), layer, mat_vegetation(name + "mat", translucency=0.2), colours=np.concatenate(C), smooth=True)
    if cb:
        cards_mesh(name + "_fringe", np.concatenate(cb), np.concatenate(ca), np.concatenate(cx_), np.concatenate(cs), np.concatenate(cc), layer, mat_leafcard("leaf"))
    if cbb:
        cards_mesh(name + "_bloomfringe", np.concatenate(cbb), np.concatenate(cab), np.concatenate(cxb), np.concatenate(csb), np.concatenate(ccb), layer,
                   mat_leafcard("blossom", 0.3))
    return ob


def mat_goldwood():
    """Golden boughs: bark furrows under a warm metallic gold."""
    key = ("goldwood",)
    if key in _mats:
        return _mats[key]
    m, nt, out, bsdf = _principled("goldwood")
    L = nt.links.new
    tc = nt.nodes.new("ShaderNodeTexCoord")
    vor = nt.nodes.new("ShaderNodeTexVoronoi"); vor.feature = "DISTANCE_TO_EDGE"; vor.inputs["Scale"].default_value = 1.4
    L(tc.outputs["Object"], vor.inputs["Vector"])
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 0.8; nz.inputs["Detail"].default_value = 8
    L(tc.outputs["Object"], nz.inputs["Vector"])
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.2; ramp.color_ramp.elements[0].color = (0.45, 0.28, 0.1, 1)
    ramp.color_ramp.elements[1].position = 0.75; ramp.color_ramp.elements[1].color = (0.95, 0.72, 0.32, 1)
    L(nz.outputs["Fac"], ramp.inputs["Fac"])
    L(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Metallic"].default_value = 0.55
    bsdf.inputs["Roughness"].default_value = 0.38
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.5
    L(vor.outputs["Distance"], bump.inputs["Height"]); L(bump.outputs["Normal"], bsdf.inputs["Normal"])
    L(bsdf.outputs[0], out.inputs["Surface"])
    _mats[key] = m
    return m


def woven_nest(cx, cy, z, radius, height, layer, seed, twigs=220):
    """A great nest woven of golden boughs: curved twigs laid around a ring, each rising and
    dipping as it crosses the others, thicker at the base, finer and looser at the rim, a
    few sticking out; lined inside with golden grass."""
    rng = random.Random(seed)
    chains = []
    for k in range(twigs):
        tier = rng.random() ** 0.8                       # 0 base .. 1 rim
        a0 = rng.uniform(0, 2 * math.pi)
        sweep = rng.uniform(0.5, 1.4) * (1 if rng.random() < 0.5 else -1)
        r0 = radius * (0.72 + 0.3 * tier + rng.uniform(-0.05, 0.05))
        z0 = z + height * tier * rng.uniform(0.85, 1.05)
        thick = (1.5 - 0.9 * tier) * rng.uniform(0.6, 1.2)
        n = 10
        pts, rads = [], []
        weave = rng.uniform(0, 2 * math.pi)
        for i in range(n + 1):
            t = i / n
            a = a0 + sweep * t
            rr = r0 + radius * 0.06 * math.sin(t * 3.0 + weave)
            pts.append(Vector((cx + rr * math.cos(a), cy + rr * math.sin(a), z0 + height * 0.12 * math.sin(t * 7.0 + weave))))
            rads.append(thick * (1.0 - 0.6 * t) + 0.12)
        # Some twigs end sticking out of the rim.
        if tier > 0.7 and rng.random() < 0.35:
            d = Vector((math.cos(a0 + sweep), math.sin(a0 + sweep), rng.uniform(0.2, 0.9))).normalized()
            end = pts[-1]
            for i in range(1, 4):
                pts.append(end + d * radius * 0.06 * i); rads.append(max(0.1, rads[-1] * 0.8))
        chains.append((pts, rads))
    tube_chains(chains, layer, mat_goldwood(), "nest_boughs")
    # The lining: soft golden grass inside the ring.
    def bowl(x, y):
        q = min(1.0, math.hypot(x - cx, y - cy) / (radius * 0.8))
        return z + height * (0.25 + 0.6 * q * q)
    grass(bowl, cx - radius * 0.8, cx + radius * 0.8, cy - radius * 0.8, cy + radius * 0.8, 9000, layer, seed + 1, height=(1.0, 2.6), width=0.12,
          base=(0.5, 0.36, 0.12), tip=(0.95, 0.8, 0.42), name="nestgrass")
