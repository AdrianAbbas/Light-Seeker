"""Renders one painted scene's depth layers with Blender (Cycles, CPU).

    /opt/bvenv/bin/python tools/painted/render.py <scene> <out dir> [--scale 1.0] [--samples 48]

Writes <out>/<layer>.exr (Combined RGBA, Depth, Denoising Albedo/Normal) for every
layer the scene uses and <out>/anchors.json: the scene's sky parameters and its
anchors (hotspots, landmarks, lights, ember sources) projected to the virtual 960 x 560
frame. paint.py turns these into the painted layers.
"""
import argparse
import json
import math
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blib  # noqa: E402
import finish  # noqa: E402
import scenes  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scene")
    ap.add_argument("out")
    ap.add_argument("--scale", type=float, default=1.0, help="1.0 = 1920 x 1120")
    ap.add_argument("--samples", type=int, default=48)
    ap.add_argument("--layers", default="")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    blib.reset()
    build = scenes.REGISTRY[args.scene]
    meta = build()
    finish.finish(verbose=True)   # smooth shading, bevels, subdivision, broken rock (Direction Update 2)
    backlight(meta, args.scene)   # contre-jour: the key light from the painted sun
    width, height = int(round(1920 * args.scale)), int(round(1120 * args.scale))
    blib.configure(width, height, args.samples)
    # Hotfix: a scene of many alpha-cut leaf cards (the map's woods) asks for more
    # transparent bounces, so rays through a crown never stop on a card's clear part.
    if blib.bpy.context.scene.get("transparent_bounces"):
        blib.bpy.context.scene.cycles.transparent_max_bounces = int(blib.bpy.context.scene["transparent_bounces"])

    anchors = []
    for a in meta.get("anchors", []):
        a = dict(a)
        if "world" in a:
            x, y, z = blib.project(a.pop("world"))
            a["x"], a["y"] = round(x, 1), round(y, 1)
        anchors.append(a)
    meta["anchors"] = anchors
    meta["scene"] = args.scene
    layers = [l for l in blib.LAYERS if l != "sky" and any(o.get("layer") == l for o in bpy_objects())]
    meta["rendered"] = layers
    with open(os.path.join(args.out, "anchors.json"), "w") as f:
        json.dump(meta, f, indent=1)

    wanted = [l for l in layers if not args.layers or l in args.layers.split(",")]
    for layer in wanted:
        t = time.time()
        sc = blib.bpy.context.scene
        # The fog layer is soft: render it at half resolution. The far layer (distant ranges
        # and the heaped volumetric cumulus, hazed and softened when painted) at 70 % and
        # fewer samples: the clouds cost most of the render time.
        scale = {"fog": 0.5, "far": 0.7}.get(layer, 1.0)
        sc.render.resolution_x, sc.render.resolution_y = int(width * scale), int(height * scale)
        sc.cycles.samples = args.samples if layer not in ("fog", "far") else max(12, args.samples // 2)
        blib.render_layer(layer, os.path.join(os.path.abspath(args.out), layer + ".exr"))
        print(f"[{args.scene}] {layer}: {time.time() - t:.1f} s", flush=True)


INTERIORS = {"hall-of-mirrors", "throne-of-light", "prism-hall", "diamond-vault", "choir-loft", "nave-of-light", "stained-glass-chapel", "sanctum"}


def backlight(meta, scene):
    """Contre-jour (Direction Update 2). The scenes were lit from behind the viewer while
    their painted sun shone in the sky ahead: light and picture disagreed, and frontal
    light is flat. Now the key light comes from where the sun is painted (raised to at
    least 24 degrees so the fronts are not all in shadow): every edge facing it gets a
    hot rim, shadows fall towards the viewer, the silhouettes glow. The old frontal key
    stays as a soft warm fill (light bounced back from a sunny world), the sky as the
    blue fill. Interiors keep their window light."""
    from mathutils import Vector
    bpy = blib.bpy
    if scene.split("/")[-1] in INTERIORS:
        return
    key = bpy.data.objects.get("sun")
    moon = meta.get("sky", {}).get("moon")
    if key is None or key.type != "LIGHT" or not moon:
        return
    old_dir = (key.rotation_euler.to_matrix() @ Vector((0, 0, 1))).normalized()
    strength = key.data.energy
    _, d = blib.screen_ray(moon[0], moon[1])
    d = Vector(d)
    d.z = max(d.z, 0.0)
    flat = Vector((d.x, d.y, 0)).normalized()
    d = (flat * math.cos(math.radians(24)) + Vector((0, 0, math.sin(math.radians(24))))) if d.z < math.sin(math.radians(24)) else d.normalized()
    key.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
    key.data.energy = strength * 1.25
    key.data.color = (1.0, 0.84, 0.62)
    blib.sun(old_dir, (1.0, 0.88, 0.82), strength * 0.38, angle=14.0, name="frontfill")
    # The physical sky lights the scene from the sun's side too.
    w = bpy.context.scene.world
    if w is not None and w.use_nodes:
        for n in w.node_tree.nodes:
            if n.type == "TEX_SKY":
                n.sun_elevation = math.asin(max(0.05, d.z))
                n.sun_rotation = math.atan2(d.x, d.y)
    print("[backlight] sun from", tuple(round(c, 2) for c in d), flush=True)


def bpy_objects():
    return blib.bpy.context.scene.objects


if __name__ == "__main__":
    main()
