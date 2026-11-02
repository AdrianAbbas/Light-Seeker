"""The finishing pass before a scene renders (Direction Update 2: less "low-poly game").

Every scene is built from scripted primitives; this pass gives them what a hand-finished
model would have, at render time only (nothing is saved, the renders cost nothing at run
time in the plug-in):

- smooth shading everywhere, sharp only where an edge turns more than 40 degrees (columns,
  domes, mushrooms and towers stop showing their facets; boxes keep crisp corners);
- small bevels on the architecture's hard edges (they catch the light, as real stone does);
- subdivision on the soft, organic shapes (mushroom caps and stems, the nest, the hoard,
  pools, island tops);
- rock and cliffs broken up: a Voronoi crackle and a cloud noise displace the surface
  (fractured faces, chipped ledges) on top of a subdivision;
- skipped: vegetation (grass, flowers, leaf cards: their shapes are already fine), volumes,
  glass and mirror planes, water, sparkles.
"""
import math

import bpy

VEGETATION = ("grass", "flower", "leaves", "blossom", "fringe", "petal", "sparkle", "butterfl", "wing", "mapwood", "grove",
              "farwood", "valleywood", "elvenwood", "blossomwood", "fairywood", "_trunks", "_wood", "boughs")
SKIP = ("fogcloud", "cloud", "fog", "glass", "mirror", "water", "sea", "river", "lake", "jet", "rays", "sky", "gate")
ROCK = ("mesa", "isle", "peak", "ridge", "crag", "rock", "menhir", "lowerslope", "range", "cliff")
SOFT = ("nest", "hoard", "cap", "stem", "pool", "well", "mushroom", "bell", "dome", "spot", "egg")


def _tex(name, kind, size, **props):
    tex = bpy.data.textures.get(name) or bpy.data.textures.new(name, kind)
    tex.noise_scale = size
    for k, v in props.items():
        setattr(tex, k, v)
    return tex


def _volume_only(ob):
    for slot in ob.material_slots:
        m = slot.material
        if m is None or not m.use_nodes:
            continue
        out = next((n for n in m.node_tree.nodes if n.type == "OUTPUT_MATERIAL"), None)
        if out is not None and out.inputs["Volume"].is_linked and not out.inputs["Surface"].is_linked:
            return True
    return False


def finish(verbose=False):
    counts = {"smooth": 0, "bevel": 0, "soft": 0, "rock": 0}
    for ob in list(bpy.context.scene.objects):
        if ob.type != "MESH":
            continue
        name = ob.name.lower()
        me = ob.data
        polys = len(me.polygons)
        if polys <= 2 or "isletop" in name or any(v in name for v in VEGETATION) or any(s in name for s in SKIP) or _volume_only(ob):
            continue
        dims = sorted(ob.dimensions)
        size = max(dims[1], 1e-3)          # the middle extent: a fair "how big"
        # Smooth shading with sharp edges kept above 40 degrees.
        try:
            me.set_sharp_from_angle(angle=math.radians(40.0))
        except AttributeError:
            pass
        me.polygons.foreach_set("use_smooth", [True] * polys)
        me.update()
        counts["smooth"] += 1
        if any(r in name for r in ROCK) and "top" not in name:
            # Fractured stone: a simple subdivision (where it is not already dense), then a
            # Voronoi crackle (cells: broken faces) and a cloud noise (lumps, chips).
            if polys < 150000:
                sub = ob.modifiers.new("subdiv", "SUBSURF")
                sub.subdivision_type = "SIMPLE"; sub.levels = 0; sub.render_levels = 1
            cell = _tex("rock_cells_%d" % int(size), "VORONOI", max(4.0, size * 0.08), distance_metric="DISTANCE", color_mode="INTENSITY",
                        noise_intensity=1.0, weight_1=1.0, weight_2=-0.6)
            lump = _tex("rock_lumps_%d" % int(size), "CLOUDS", max(4.0, size * 0.12), noise_depth=5)
            for tex, strength in ((cell, 0.008), (lump, 0.03)):
                d = ob.modifiers.new("displace", "DISPLACE")
                d.texture = tex; d.texture_coords = "OBJECT"; d.strength = size * strength; d.mid_level = 0.5
            counts["rock"] += 1
        elif any(sft in name for sft in SOFT) and polys < 20000:
            sub = ob.modifiers.new("subdiv", "SUBSURF")
            sub.levels = 0; sub.render_levels = 2 if polys < 3000 else 1
            counts["soft"] += 1
        elif polys < 40000:
            # Architecture: a small rounded bevel on hard edges only.
            bev = ob.modifiers.new("bevel", "BEVEL")
            bev.width = min(size * 0.012, 0.6)
            bev.segments = 2
            bev.limit_method = "ANGLE"; bev.angle_limit = math.radians(40.0)
            bev.harden_normals = False
            try:
                bev.use_clamp_overlap = True
            except AttributeError:
                pass
            counts["bevel"] += 1
    if verbose:
        print("[finish]", counts, flush=True)
    return counts
