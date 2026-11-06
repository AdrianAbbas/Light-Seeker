#!/usr/bin/env python3
"""Prepares the brass and woodwinds of VSCO 2 Community Edition (CC0) for the instrument's sampler.

The library (https://github.com/sgossner/VSCO-2-CE, tag 1.1.0; licence and source in
docs/LICENSES/VSCO-2-CE.txt) is never committed to this repository: CI fetches its brass
and woodwind folders (FOLDERS below) and SFZ maps (scripts/ci-fetch-vsco.sh) and this script writes, beside the
developer's prepared recordings, mono 16-bit FLAC notes and one mapping file per set in
the format the sample bank reads (Source/Synth/wb/SampleBank.cpp), and adds the sets to
index.json (creating it when the developer's recordings are not there).

    python3 tools/sample-prep/vsco.py --source ../VSCO-2-CE --out build/samples

What it does, per SFZ region (horn, trumpet, tenor trombone, tuba; sustain and staccato; since
the real woodwinds update the flute's sustain, vibrato, expressive vibrato and staccato, the
oboe's vibrato sustain and staccato, the piccolo's sustain and staccato, the clarinet's and the
bassoon's sustains; the piccolo moved up an octave to where it sounds: the library's map plays
it an octave low, measured; the vibrato sustains loop over a longer stretch, 4.5 s, with a
150 ms crossfade, and keep their pitch measurement with a vibrato's spread):

1. **Decode**: 44.1 kHz WAV. The two channels are a spaced pair (correlation 0.2-0.8, the
   tuba's sustains even negative: summed they would comb-filter), so each set uses one
   channel, the louder over the set, for all its notes; the region's own `volume` applied
   (the library's map undoes its per-file normalisation with it), a 15 Hz high-pass for DC.
   Where two velocity splits overlap on one key range (the SFZ layers both there), the
   finer split is kept.
2. **Attack trim**: the pre-roll before the note speaks is cut (from 2 ms before the level
   first reaches 40 dB under the note's peak, with a 1 ms fade-in), so the attack lands
   on the beat and stacks tightly with the synthesis.
3. **Pitch from the audio**: YIN within 6 semitones of the map's key centre on the steady
   part (sustains 0.3-1.3 s, staccatos 40-260 ms), refined on the harmonics. The sampler
   is tuned to the measured pitch; a note more than 60 cents from its key centre is
   reported (and mapped at the key centre: a measurement that far off is not trusted), as
   is a note whose pitch moves more than 20 cents over the measured part (a short low
   staccato sliding into its note).
4. **Sustains**: a sustain loop in the steady part (the best-matching waveforms,
   crossfaded and levelled) within its first 2.5 s, then the recorded release (where the
   player stops) appended after the loop: each note is its attack, its loop and its own
   release tail (tools/sample-prep/prepare.py's loop search). **Staccatos**: trimmed at
   60 dB under their peak (1.6 s at most: the room after a short note), faded out.
5. **Levels**: every note's level (RMS of its loudest 400 ms, unweighted). The velocity
   layers of a key are levelled to each other, each 0.9 dB over the one under it (the
   dynamics then come from the instrument's velocity response, continuous across a layer
   change: the timbre changes there, the level only rises a little), and the notes along
   the range evened out (a quarter of the register's natural tilt kept, from a quadratic
   fit in the key: the synthesis beside it is about level across its range). One gain per set: its median note at -20 dB RMS (a staccato set as
   loud as its sustain set), no peak over -0.5 dBFS.
   Staccatos are measured by their hit (the loudest 100 ms of their first 400 ms: the takes
   run 0.6-1.6 s with their hall, so a longer window measured their length). The map's
   per-note `loudness` is that same level (the instrument levels each staccato front to its
   sustain note by it).
6. **Mapping**: key ranges and velocity ranges from the library's SFZ; velocity layers are
   numbered per key from soft to loud (contiguous, as the bank's lookup needs);
   round-robins from `lorand`/`seq_position`.

Writes report-vsco.json in the output folder and prints a readable report.
"""

import argparse
import json
import math
import os
import re
import sys

import numpy as np
import soundfile as sf
from scipy import signal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prepare  # noqa: E402  (the same folder: the loop search, YIN, loudness)

RATE = prepare.RATE
SOURCE_COMMIT = "28092772094b2d9f1148d84cea97f4545b8c687d"

# The sets: id, SFZ map, sustained (loops) or short, a readable name, options:
# `loop` (seconds of the note the sustain loop is looked for in), `xfade` (the loop's
# crossfade, seconds), `transpose` (semitones: the library maps its piccolo an octave under
# where it sounds, measured), `family` (brass or woodwinds, for the report).
# `spread`: how far (cents, the standard deviation over the note) the pitch may move and still
# be measured (a vibrato moves 20-40 cents by design; a staccato sliding into its note does not).
VIB = {"loop": 4.5, "xfade": 0.15, "spread": 45.0, "family": "woodwinds"}
WW = {"family": "woodwinds"}
SETS = [
    ("vsco-horn-sus", "FHornSus.sfz", True, "F horn, sustain (VSCO 2 CE)", {}),
    ("vsco-horn-stac", "FHornStac.sfz", False, "F horn, staccato (VSCO 2 CE)", {}),
    ("vsco-trumpet-sus", "TrumpetSus.sfz", True, "Trumpet, sustain (VSCO 2 CE)", {}),
    ("vsco-trumpet-stac", "TrumpetStac.sfz", False, "Trumpet, staccato (VSCO 2 CE)", {}),
    ("vsco-trombone-sus", "TromboneSus.sfz", True, "Tenor trombone, sustain (VSCO 2 CE)", {}),
    ("vsco-trombone-stac", "TromboneStac.sfz", False, "Tenor trombone, staccato (VSCO 2 CE)", {}),
    ("vsco-tuba-sus", "TubaSus.sfz", True, "Tuba, sustain (VSCO 2 CE)", {}),
    ("vsco-tuba-stac", "TubaStac.sfz", False, "Tuba, staccato (VSCO 2 CE)", {}),
    # The woodwinds (the real woodwinds update). The vibrato sustains get a longer loop
    # window and crossfade (whole vibrato cycles; a short loop would pulse).
    ("vsco-flute-sus", "FluteSusNV.sfz", True, "Flute, sustain without vibrato (VSCO 2 CE)", WW),
    ("vsco-flute-vib", "FluteSusVib.sfz", True, "Flute, sustain with vibrato (VSCO 2 CE)", VIB),
    ("vsco-flute-expvib", "FluteExpVib.sfz", True, "Flute, expressive vibrato (VSCO 2 CE)", VIB),
    ("vsco-flute-stac", "FluteStac.sfz", False, "Flute, staccato (VSCO 2 CE)", WW),
    ("vsco-oboe-vib", "OboeSusVib.sfz", True, "Oboe, sustain with vibrato (VSCO 2 CE)", VIB),
    ("vsco-oboe-stac", "OboeStac.sfz", False, "Oboe, staccato (VSCO 2 CE)", WW),
    ("vsco-piccolo-sus", "PiccoloSus.sfz", True, "Piccolo, sustain (VSCO 2 CE)", {**WW, "transpose": 12}),
    ("vsco-piccolo-stac", "PiccoloStac.sfz", False, "Piccolo, staccato (VSCO 2 CE)", {**WW, "transpose": 12}),
    ("vsco-clarinet-sus", "ClarinetSus.sfz", True, "Clarinet, sustain (VSCO 2 CE)", WW),
    ("vsco-bassoon-sus", "BassoonSus.sfz", True, "Bassoon, sustain (VSCO 2 CE)", WW),
]
# The folders those maps read (scripts/ci-fetch-vsco.sh fetches only these).
FOLDERS = ["Brass/F Horn/sus", "Brass/F Horn/stac", "Brass/Trumpet/sus", "Brass/Trumpet/stac", "Brass/Tenor Trombone/sus",
           "Brass/Tenor Trombone/stac", "Brass/Tuba/sus", "Brass/Tuba/stac", "Woodwinds/Flute/susNV", "Woodwinds/Flute/susvib",
           "Woodwinds/Flute/expvib", "Woodwinds/Flute/stac", "Woodwinds/Oboe/Vib", "Woodwinds/Oboe/Stacc", "Woodwinds/Piccolo/Sus",
           "Woodwinds/Piccolo/Stac", "Woodwinds/Clarinet/susLong", "Woodwinds/Bassoon/sus"]


def parse_sfz(path):
    """The regions of an SFZ file, each with its <global>/<control>/<group> opcodes."""
    with open(path, encoding="latin-1") as f:
        text = re.sub(r"//[^\n]*", "", f.read())
    regions, top, group, current, section = [], {}, {}, None, None
    for token in re.split(r"(<\w+>)", text):
        m = re.fullmatch(r"<(\w+)>", token)
        if m:
            section = m.group(1)
            if section == "group":
                group = {}
            elif section == "region":
                current = {}
                regions.append((current, dict(group)))
            continue
        target = {"global": top, "control": top, "group": group, "region": current}.get(section)
        if target is None:
            continue
        # Values may contain spaces (file names): a value runs until the next opcode.
        for k, v in re.findall(r"(\w+)=(.*?)(?=\s+\w+=|\s*$)", token.strip(), flags=re.S):
            target[k] = v.strip()
    out = []
    for region, grp in regions:
        r = dict(top)
        r.update(grp)
        r.update(region)
        out.append(r)
    return out


def sample_path(source, region):
    folder = region.get("default_path", "").replace("\\", "/")
    return os.path.join(source, folder, region["sample"].replace("\\", "/"))


def attack_start(x):
    """Where the note starts speaking: 2 ms before the 1 ms level first reaches 40 dB
    under its peak."""
    hop = 44
    n = len(x) // hop
    env = prepare.db(np.sqrt((x[:n * hop].reshape(n, hop) ** 2).mean(axis=1) + 1e-30))
    above = np.nonzero(env >= env.max() - 40.0)[0]
    first = int(above[0]) * hop if len(above) else 0
    return max(0, first - int(0.002 * RATE))


def measure_pitch(x, centre, sustained):
    # Staccatos: their steady middle (low notes get longer: more periods to measure).
    a, b = (0.3, 1.3) if sustained else (0.04, 0.26 if centre >= 48 else 0.4)
    seg = x[int(a * RATE):int(b * RATE)]
    if len(seg) < 2048 + int(RATE / 30):
        seg = x[int(0.02 * RATE):int(0.6 * RATE)]
    fmin = 440 * 2 ** ((centre - 6 - 69) / 12)
    fmax = 440 * 2 ** ((centre + 6 - 69) / 12)
    seg = signal.sosfiltfilt(signal.butter(4, 0.7 * fmin, "highpass", fs=RATE, output="sos"), seg)
    frame = 2048 if fmin > 60 else 4096
    frames = prepare.yin(seg, fmin=fmin, fmax=fmax, frame=frame, hop=256)
    good = [f for f, ap in frames if ap < 0.3]
    if len(good) < 3:
        return None, None
    f0 = float(np.median(good))
    # How steady the pitch is over the frames (cents): a short low note that slides is
    # not a measurement to tune by.
    spread = float(np.std(1200 * np.log2(np.array(good) / f0)))
    f0 = prepare.refine(seg[:int(0.5 * RATE)], f0, 4)
    return 69 + 12 * math.log2(f0 / 440.0), spread


def prepare_note(x, sustained, loop_seconds=2.5, xfade=0.08):
    """(audio, loop, release, notes) for one region's mono audio."""
    s = attack_start(x)
    y = x[s:].copy()
    fade_in = int(0.001 * RATE)
    y[:fade_in] *= np.linspace(0.0, 1.0, fade_in)
    notes = {"preRollMs": round(1000 * s / RATE, 1)}
    if not sustained:
        env = prepare.envelope(y)
        last = np.nonzero(env >= env.max() - 60.0)[0]
        end = min(len(y), (int(last[-1]) + 1) * prepare.ENV_HOP if len(last) else len(y), int(1.6 * RATE))
        y = y[:end]
        fo = min(int(0.02 * RATE), len(y) // 4)
        y[-fo:] *= np.linspace(1.0, 0.0, fo)
        return y, None, None, notes
    region = prepare.sustain_region(y)
    if region is None:
        notes["loop"] = "no steady part"
        return fade_end(y[:int(6 * RATE)]), None, None, notes
    a, rel = region
    b = min(rel, a + int(loop_seconds * RATE))
    hop = prepare.ENV_HOP
    probe = y[a:b]
    f0 = None
    if len(probe) > 4096:
        frames = prepare.yin(probe[:int(0.6 * RATE)], fmin=30.0, fmax=1500.0, frame=4096, hop=1024)
        good = [f for f, ap in frames if ap < 0.3]
        if good:
            f0 = float(np.median(good))
    found = prepare.find_loop(y, f0 or 220.0, (a, b)) if f0 else None
    if found is None:
        notes["loop"] = "none found"
        return fade_end(y[:int(6 * RATE)]), None, None, notes
    ls, le, cost, _ = found
    y = prepare.crossfade_loop(y, ls, le, int(xfade * RATE))
    # The recorded release: from 30 ms before the level leaves the sustain, until it has
    # fallen 60 dB under the note's peak (at most 1.5 s).
    env = prepare.envelope(y)
    tail_from = max(le + 1, rel - int(0.03 * RATE))
    tail_env = env[tail_from // hop:]
    quiet = np.nonzero(tail_env < env.max() - 60.0)[0]
    tail_to = min(len(y), tail_from + (int(quiet[0]) * hop if len(quiet) else len(y)), tail_from + int(1.5 * RATE))
    tail = y[tail_from:tail_to].copy()
    if len(tail) < int(0.08 * RATE):
        tail = y[le:min(len(y), le + int(0.4 * RATE))].copy()
    tail = fade_end(tail)
    spacer = np.zeros(16)
    out = np.concatenate([y[:le], spacer, tail, np.zeros(8)])
    notes["loop"] = f"{ls / RATE:.2f}-{le / RATE:.2f} s (match {cost:.3f})"
    notes["tailSeconds"] = round(len(tail) / RATE, 2)
    return out, (int(ls), int(le)), int(le + len(spacer)), notes


def plain_loudness(audio):
    """A note's level (RMS, not weighted: a louder layer is brighter, so at equal weighted
    loudness it would measure quieter): the mean in dB of its first 600 ms (the attack, which
    a soft layer takes longer to reach) and its loudest 400 ms in its first 1.5 s (the
    sustain), so a layer change steps neither."""
    y = audio[:int(1.9 * RATE)]
    win = int(0.4 * RATE)
    head = float(10 * np.log10(np.mean(y[:int(0.6 * RATE)] ** 2) + 1e-24))
    if len(y) <= win:
        return head
    total = np.cumsum(np.concatenate(([0.0], y ** 2)))
    power = ((total[win:] - total[:-win]) / win)[:int(1.5 * RATE)]
    return 0.5 * (head + float(10 * np.log10(max(power.max(), 0.0) + 1e-24)))


def hit_level(audio):
    """A staccato's level: its loudest 100 ms in its first 400 ms (RMS): the hit itself (the
    takes run 0.6-1.6 s with their hall, so a longer window would measure their length)."""
    y = audio[:int(0.4 * RATE)]
    win = int(0.1 * RATE)
    if len(y) <= win:
        return float(10 * np.log10(np.mean(y ** 2) + 1e-24))
    total = np.cumsum(np.concatenate(([0.0], y ** 2)))
    return float(10 * np.log10(max(((total[win:] - total[:-win]) / win).max(), 0.0) + 1e-24))


def fade_end(y):
    fo = min(int(0.03 * RATE), len(y) // 4)
    y = y.copy()
    y[-fo:] *= np.linspace(1.0, 0.0, fo)
    return y


def process_set(sid, sfz, sustained, name, source, out_dir, report, opts=None):
    opts = opts or {}
    shift = int(opts.get("transpose", 0))
    regions = parse_sfz(os.path.join(source, sfz))
    for r in regions:
        for k in ("lokey", "hikey", "pitch_keycenter"):
            r[k] = str(int(r[k]) + shift)
    # Overlapping velocity sets on one key range (the SFZ sounds both at once there): the
    # finer split is kept, the other left out (reported).
    by_range = {}
    for r in regions:
        by_range.setdefault((int(r["lokey"]), int(r["hikey"])), []).append(r)
    kept = []
    for rng, group in by_range.items():
        splits = {}
        for r in group:
            splits.setdefault(int(r.get("lovel", 0)), []).append(r)
        # Partitions: chains of velocity ranges from 0 to 127.
        ranges = sorted({(int(r.get("lovel", 0)), int(r.get("hivel", 127))) for r in group})
        chains = []
        def walk(at, chain):
            if at > 127:
                chains.append(chain)
                return
            for lo, hi in ranges:
                if lo == at:
                    walk(hi + 1, chain + [(lo, hi)])
        walk(0, [])
        best = max(chains, key=len) if chains else ranges
        dropped = [r for r in group if (int(r.get("lovel", 0)), int(r.get("hivel", 127))) not in best]
        for r in dropped:
            report["warnings"].append(f"{sid} {r['sample']}: velocity {r.get('lovel', 0)}-{r.get('hivel', 127)} overlaps a finer split "
                                      f"on keys {rng[0]}-{rng[1]}; left out")
        kept += [r for r in group if r not in dropped]
    regions = kept
    # The two microphones are a spaced pair (low or even negative correlation: summing them
    # would comb-filter): one channel, the louder one over the set, for every note.
    energy = np.zeros(2)
    for r in regions[::2]:
        info = sf.info(sample_path(source, r))
        if info.channels > 1:
            raw, _ = sf.read(sample_path(source, r), always_2d=True)
            energy += np.mean(raw[:, :2] ** 2, axis=0)
    channel = int(np.argmax(energy))
    report.setdefault("channels", {})[sid] = ["left", "right"][channel] + f" ({10 * np.log10((energy[channel] + 1e-30) / (energy[1 - channel] + 1e-30)):+.1f} dB over the other)"
    zones = []
    for r in regions:
        path = sample_path(source, r)
        raw, rate = sf.read(path, always_2d=True)
        if rate != RATE:
            raw = np.stack([prepare.to_rate(raw[:, c], rate) for c in range(raw.shape[1])], axis=1)
        if raw.shape[1] > 1:
            l, rr = raw[:, 0], raw[:, 1]
            corr = float(np.corrcoef(l, rr)[0, 1]) if np.std(l) > 0 and np.std(rr) > 0 else 1.0
            x = raw[:, channel]
        else:
            corr, x = 1.0, raw[:, 0]
        gain_db = float(r.get("volume", 0)) if "volume" in r else 0.0
        x = x * 10 ** (gain_db / 20)
        x = signal.sosfilt(signal.butter(2, 15.0, "highpass", fs=RATE, output="sos"), x)
        centre = int(r["pitch_keycenter"])
        tune = float(r.get("tune", 0)) / 100.0
        y, loop, release, notes = prepare_note(x, sustained, float(opts.get("loop", 2.5)), float(opts.get("xfade", 0.08)))
        measured, spread = measure_pitch(y, centre, sustained)
        root = float(centre)
        cents = None
        if measured is None:
            report["warnings"].append(f"{sid} {os.path.basename(path)}: no clear pitch; mapped at its key centre {prepare.name_of(centre)}")
        elif spread > float(opts.get("spread", 20.0)):
            report["warnings"].append(f"{sid} {os.path.basename(path)}: its pitch moves ({spread:.0f} cents over the note, "
                                      f"{100 * (measured - centre):+.0f} on average); mapped at its key centre {prepare.name_of(centre)}")
        elif abs(measured - centre - tune) > 0.6:
            report["warnings"].append(f"{sid} {os.path.basename(path)}: measured {measured - centre:+.2f} semitones from its key centre "
                                      f"{prepare.name_of(centre)}; mapped at the key centre")
        else:
            root = measured
            cents = round(100 * (measured - centre), 1)
        lovel, hivel = int(r.get("lovel", 0)), int(r.get("hivel", 127))
        if "lorand" in r or "hirand" in r:
            rr_index = float(r.get("lorand", 0))
        else:
            rr_index = float(r.get("seq_position", 1))
        zones.append({"path": path, "audio": y, "loop": loop, "release": release, "root": root, "centre": centre, "cents": cents,
                      "loKey": int(r["lokey"]), "hiKey": int(r["hikey"]), "loVel": lovel, "hiVel": hivel, "rrKey": rr_index,
                      "corr": round(corr, 3), "volume": gain_db, "notes": notes})
    # Velocity layers per key centre (contiguous, soft to loud) and round-robins.
    by_key = {}
    for z in zones:
        by_key.setdefault((z["loKey"], z["hiKey"]), []).append(z)
    for group in by_key.values():
        vels = sorted({(z["loVel"], z["hiVel"]) for z in group})
        for z in group:
            z["layer"] = vels.index((z["loVel"], z["hiVel"]))
        for z in group:
            takes = sorted({q["rrKey"] for q in group if q["layer"] == z["layer"]})
            z["rr"] = takes.index(z["rrKey"])
    level_of = plain_loudness if sustained else hit_level
    for z in zones:
        z["loud0"] = level_of(z["audio"])
    # The layers of a key levelled to each other (their mean), then the keys evened out along
    # the range.
    for group in by_key.values():
        layer_level = {}
        for z in group:
            layer_level.setdefault(z["layer"], []).append(z["loud0"])
        mean = float(np.mean([np.mean(v) for v in layer_level.values()]))
        top = max(layer_level)
        for z in group:
            # Each layer a little louder than the one under it (0.9 dB): rising, never a step.
            z["layerTrim"] = mean - float(np.mean(layer_level[z["layer"]])) + 0.9 * (z["layer"] - top / 2.0)
            z["keyLevel"] = mean
    keys = sorted(by_key.keys())
    centres = np.array([np.mean([z["root"] for z in by_key[k]]) for k in keys])
    levels = np.array([by_key[k][0]["keyLevel"] for k in keys])
    deg = 2 if len(keys) >= 5 else (1 if len(keys) >= 2 else 0)
    fit = np.polyval(np.polyfit(centres, levels, deg), centres) if deg else levels
    # A quarter of the register's tilt kept (the instrument's synthesis is about level across
    # its range: at full tilt the top notes would stand 3 dB over the calibrated middle).
    flat = float(np.mean(levels))
    target = flat + 0.25 * (fit - flat)
    for k, c, lv, ft in zip(keys, target, levels, fit):
        for z in by_key[k]:
            z["keyTrim"] = float(np.clip(c - lv, -12.0, 12.0))
    for z in zones:
        trim = float(np.clip(z["layerTrim"] + z["keyTrim"], -18.0, 18.0))
        z["audio"] = z["audio"] * 10 ** (trim / 20)
        z["trim"] = trim
    # One gain per set: its median note level at -20 dB RMS (so a staccato and its sustain
    # sit at the same level), never a peak over -0.5 dBFS.
    peak = max(float(np.abs(z["audio"]).max()) for z in zones)
    median = float(np.median([level_of(z["audio"]) for z in zones]))
    gain = min(10 ** ((-20.0 - median) / 20), 10 ** (-0.5 / 20) / max(peak, 1e-9))
    os.makedirs(os.path.join(out_dir, sid), exist_ok=True)
    layers = max(z["layer"] for z in zones) + 1
    mapping_zones = []
    for z in zones:
        a = z["audio"] * gain
        label = prepare.name_of(z["centre"])
        fname = f"{label.replace('#', 's')}-v{z['layer'] + 1}-{z['rr'] + 1}.flac"
        sf.write(os.path.join(out_dir, sid, fname), a.astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
        env_z = prepare.envelope(a[:int(1.5 * RATE)])
        top = int(np.argmax(env_z)) * prepare.ENV_HOP if len(env_z) else 0
        head = a[top:top + int(0.3 * RATE)]
        mz = {"file": f"{sid}/{fname}", "label": label, "root": round(z["root"], 3), "loKey": z["loKey"], "hiKey": z["hiKey"],
              "loVel": round(z["loVel"] / 127.0, 4), "hiVel": round(min(1.0, (z["hiVel"] + 1) / 128.0), 4) if z["hiVel"] < 127 else 1.0,
              "layer": z["layer"], "rr": z["rr"], "length": len(a),
              "rms": round(float(prepare.db(np.sqrt(np.mean(head ** 2) + 1e-30))), 2),
              "loudness": round(level_of(a), 2), "peak": round(float(prepare.db(np.abs(a).max())), 2),
              "source": os.path.relpath(z["path"], source)}
        if z["loop"] is not None:
            mz["loopStart"], mz["loopEnd"] = z["loop"]
            mz["release"] = z["release"]
        mapping_zones.append(mz)
        z["entry"] = mz
    mapping_zones.sort(key=lambda m: (m["root"], m["layer"], m["rr"]))
    mapping = {"id": sid, "name": name, "kind": "sustain" if sustained else "short", "rate": RATE, "channels": 1,
               "layers": [f"v{k + 1}" for k in range(layers)], "gainDb": round(20 * math.log10(gain), 2),
               "source": f"VSCO 2 Community Edition (CC0 1.0), https://github.com/sgossner/VSCO-2-CE @ {SOURCE_COMMIT}, {sfz}",
               "zones": mapping_zones}
    with open(os.path.join(out_dir, f"{sid}.json"), "w") as f:
        json.dump(mapping, f, indent=1)
    # The report.
    cents = [z["cents"] for z in zones if z["cents"] is not None]
    entry = {"sfz": sfz, "family": opts.get("family", "brass"), "transpose": shift, "zones": len(zones), "layers": layers, "gainDb": mapping["gainDb"],
             "seconds": round(sum(len(z["audio"]) for z in zones) / RATE, 1),
             "loops": sum(1 for z in zones if z["loop"] is not None),
             "keys": f"{prepare.name_of(min(z['loKey'] for z in zones))}-{prepare.name_of(max(z['hiKey'] for z in zones))}",
             "tuningCents": {"min": min(cents) if cents else None, "max": max(cents) if cents else None,
                             "meanAbs": round(float(np.mean(np.abs(cents))), 1) if cents else None},
             "stereoCorrelationMin": min(z["corr"] for z in zones), "channel": report["channels"][sid],
             "preRollMsMax": max(z["notes"]["preRollMs"] for z in zones),
             "notes": [{"file": os.path.basename(z["path"]), "key": z["entry"]["label"], "layer": z["layer"], "rr": z["rr"],
                        "cents": z["cents"], "trimDb": round(z["trim"], 1), "seconds": round(len(z["audio"]) / RATE, 2),
                        **z["notes"]} for z in sorted(zones, key=lambda q: (q["centre"], q["layer"], q["rr"]))]}
    report["sets"][sid] = entry
    return mapping


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--source", required=True, help="the VSCO 2 CE checkout (its SFZ files and Brass folder)")
    ap.add_argument("--out", required=True, help="the prepared samples folder (index.json is added to)")
    ap.add_argument("--only", help="comma-separated set ids")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    licence = os.path.join(args.source, "LICENSE")
    if not os.path.isfile(licence) or "CC0 1.0 Universal" not in open(licence, encoding="latin-1").read(200):
        print("VSCO 2 CE: the LICENSE is missing or not CC0 1.0 Universal: not prepared", file=sys.stderr)
        return 2
    only = set(args.only.split(",")) if args.only else None
    report = {"source": f"https://github.com/sgossner/VSCO-2-CE @ {SOURCE_COMMIT}", "licence": "CC0 1.0 Universal",
              "sets": {}, "warnings": []}
    made = []
    for sid, sfz, sustained, name, opts in SETS:
        if only and sid not in only:
            continue
        print(f"[{sid}] ...", flush=True)
        process_set(sid, sfz, sustained, name, args.source, args.out, report, opts)
        made.append(sid)
    index_path = os.path.join(args.out, "index.json")
    index = {"version": 1, "instruments": []}
    if os.path.isfile(index_path):
        with open(index_path) as f:
            index = json.load(f)
    index["instruments"] = [i for i in index["instruments"] if i not in made] + made
    with open(index_path, "w") as f:
        json.dump(index, f, indent=1)
    with open(os.path.join(args.out, "report-vsco.json"), "w") as f:
        json.dump(report, f, indent=1)
    total = 0
    for sid in made:
        s = report["sets"][sid]
        t = s["tuningCents"]
        total += s["zones"]
        print(f"{sid}: {s['zones']} notes ({s['keys']}, {s['layers']} velocity layers), {s['seconds']} s of audio, {s['loops']} loops, "
              f"gain {s['gainDb']:+.1f} dB, tuning {t['min']:+.0f}..{t['max']:+.0f} cents (mean |{t['meanAbs']}|), "
              f"pre-roll cut up to {s['preRollMsMax']} ms, channel {s['channel']}")
    fam = {f: sum(report["sets"][m]["zones"] for m in made if report["sets"][m]["family"] == f) for f in ("brass", "woodwinds")}
    print(f"VSCO 2 CE: {total} notes in {len(made)} sets (brass {fam['brass']}, woodwinds {fam['woodwinds']})")
    if report["warnings"]:
        print(f"Warnings ({len(report['warnings'])}):")
        for w in report["warnings"]:
            print(f" - {w}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
