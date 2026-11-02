"""Turns analysed capture IRs (run_all.py's <out>/<unit>/<patch>/ir.wav) into the compact
impulse responses LIGHT SEEKER compiles in (<out>/<name>.wav, 48 kHz, 16-bit stereo; CI only:
ci-build-irs.sh, never committed: they are derived from private recordings).

    python3 -I tools/captures/export_irs.py <analysis out-dir> <out-dir>

Per IR: the leading silence before the first arrival is removed; the direct sound (the
unit's dry path, 3 ms) is taken out, since the plug-in adds the dry itself; the captured
response is continued past its noise-floor cut by a synthetic tail, octave band by octave
band, at the measured RT60 of each band (times `rt_scale`; the cross-over is 0.25 s long),
so the capture's early reflections, colour and modulation stay and the long tail of the
real unit is restored. An optional tilt brightens or darkens it (a gentle high shelf, dB at
8 kHz); it ends with a 50 ms fade and is normalised to unit energy per channel pair. A mono capture is decorrelated into stereo
with a short all-pass on one side. Product names only: the units are never named.
"""
import json
import os
import sys

import numpy as np
from scipy import signal
from scipy.io import wavfile

SR = 48000

# name in the product -> (analysis folder, tilt dB at 8 kHz, total seconds, RT60 scale, note)
EXPORTS = {
    "radiant-shimmer-hall": ("shimmer/octave-shimmer", 3.0, 9.5, 2.6, "the octave-shimmer patch: long, bright (tail x2.6)"),
    "cliff-hall-echo": ("dark-hall/reverb-delay", 7.0, 6.0, 1.6, "the reverb + delay patch, brightened for daylight"),
    "chorus-hall": ("chorus/chorus-reverb", 3.0, 5.0, 1.1, "the modulated chorus + reverb patch"),
}
# Not exported: the tape-echo take (its IR is 0.35 s with no periodic echo found, the
# RT60s are implausible): the plug-in's tape model (X. Ensemble) is used instead.
BANDS = (125, 250, 500, 1000, 2000, 4000, 8000)


def load(path):
    sr, raw = wavfile.read(path)
    x = raw.astype(np.float64)
    if raw.dtype.kind == "i":
        x /= np.iinfo(raw.dtype).max
    if x.ndim == 1:
        x = x[:, None]
    if sr != SR:
        g = np.gcd(SR, int(sr))
        x = signal.resample_poly(x, SR // g, int(sr) // g, axis=0)
    return x


def stereo(x):
    if x.shape[1] >= 2:
        return x[:, :2]
    # Decorrelate a mono IR: one side through a short chain of all-passes.
    r = x[:, 0].copy()
    for d, g in ((113, 0.6), (271, 0.55), (439, 0.5)):
        b = np.zeros(d + 1); b[0] = -g; b[d] = 1.0
        a = np.zeros(d + 1); a[0] = 1.0; a[d] = -g
        r = signal.lfilter(b, a, r)
    return np.stack([x[:, 0], r], 1)


def tilt(x, db):
    if abs(db) < 0.05:
        return x
    # A first-order high shelf around 3 kHz reaching `db` at 8 kHz.
    w = 2 * np.pi * 3000.0 / SR
    k = 10 ** (db / 20.0)
    a1 = -np.exp(-w)
    hp = signal.lfilter([1 + a1, -(1 + a1)], [2, 2 * a1], x, axis=0) * 2  # one-pole high-pass
    return x + (k - 1.0) * hp


def octave(x, fc):
    lo, hi = fc / np.sqrt(2), min(fc * np.sqrt(2), SR * 0.45)
    sos = signal.butter(3, [lo / (SR / 2), hi / (SR / 2)], btype="band", output="sos")
    return signal.sosfilt(sos, x, axis=0)


def extend(x, rt60, total_s, rt_scale, seed=7):
    """Continues the IR past its last reliable part with band-limited noise per octave."""
    n = int(total_s * SR)
    cut = len(x)
    reliable = cut - int(0.05 * SR)                  # the very end may already be noise
    win = slice(max(0, reliable - int(0.25 * SR)), reliable)
    rng = np.random.default_rng(seed)
    tail = np.zeros((n, x.shape[1]))
    t = (np.arange(n) - (reliable - int(0.125 * SR))) / SR   # 0 at the middle of the window
    for fc in BANDS:
        rt = max(0.2, float(rt60.get(str(fc), rt60.get(fc, 2.0)) or 2.0)) * rt_scale
        level = np.sqrt(np.mean(octave(x, fc)[win] ** 2, axis=0))
        noise = octave(rng.standard_normal((n, x.shape[1])), fc)
        noise /= np.sqrt(np.mean(noise ** 2, axis=0)) + 1e-20
        tail += noise * level * 10 ** (-3.0 * t / rt)[:, None]
    y = np.zeros((n, x.shape[1]))
    y[:cut] = x
    fade = int(0.25 * SR)
    a = reliable - fade
    ramp = np.linspace(0, 1, fade)[:, None]
    y[a:reliable] = x[a:reliable] * np.sqrt(1 - ramp) + tail[a:reliable] * np.sqrt(ramp)
    y[reliable:] = tail[reliable:]
    return y


def process(x, tilt_db, total_s, rt60, rt_scale):
    env = np.abs(x).max(axis=1)
    first = int(np.argmax(env > env.max() * 10 ** (-40 / 20)))
    x = x[max(0, first - int(0.002 * SR)):].copy()
    # Remove the direct sound (the unit's dry path): 3 ms after the first arrival, faded.
    d0, d1 = int(0.0015 * SR), int(0.0045 * SR)
    x[:d0] = 0
    x[d0:d1] *= np.linspace(0, 1, d1 - d0)[:, None] ** 2
    x = extend(x, rt60, total_s, rt_scale)
    x = tilt(x, tilt_db)
    max_s = total_s
    sm = signal.filtfilt(*signal.butter(2, 20.0 / (SR / 2)), np.abs(x).max(axis=1))
    above = np.nonzero(sm > sm.max() * 10 ** (-70 / 20))[0]
    end = min(len(x), int(max_s * SR), (above[-1] + 1) if len(above) else len(x))
    x = x[:end].copy()
    fade = min(int(0.05 * SR), len(x) // 4)
    x[-fade:] *= np.linspace(1, 0, fade)[:, None]
    x /= np.sqrt(np.sum(x ** 2) / 2.0) + 1e-20
    return x


def main(analysis, out):
    os.makedirs(out, exist_ok=True)
    notes = []
    for name, (folder, db, total_s, rt_scale, note) in EXPORTS.items():
        src = os.path.join(analysis, folder, "ir.wav")
        if not os.path.exists(src):
            notes.append(f"- {name}: missing ({src})")
            continue
        rep = json.load(open(os.path.join(analysis, folder, "report.json")))
        rt60 = rep.get("measurements", {}).get("rt60_s") or {}
        x = process(stereo(load(src)), db, total_s, rt60, rt_scale)
        peak = np.abs(x).max()
        pcm = np.clip(x / peak * 0.98, -1, 1)
        wavfile.write(os.path.join(out, name + ".wav"), SR, (pcm * 32767).astype(np.int16))
        info = {"name": name, "seconds": round(len(x) / SR, 3), "scale": float(peak / 0.98), "tilt_db": db,
                "rt60_measured_s": rt60, "rt_scale": rt_scale, "note": note}
        json.dump(info, open(os.path.join(out, name + ".json"), "w"), indent=1)
        notes.append(f"- {name}: {info['seconds']} s, tilt {db:+.1f} dB ({note})")
    print("\n".join(notes))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
