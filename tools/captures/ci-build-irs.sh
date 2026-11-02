#!/usr/bin/env bash
# Makes the impulse responses LIGHT SEEKER compiles in (Radiant Shimmer Hall, Cliff Hall
# Echo, Chorus Hall) from the developer's effect captures in the private Samples
# repository. They are derived from private recordings, so they are never committed: CI
# runs this after fetching the recordings (scripts/ci-fetch-samples.sh) and CMake compiles
# whatever it wrote (-DLS_IRS_DIR). Without them the plug-ins use fallback spaces.
#
# The captures are laid out the way run_all.py reads them (<unit>/<patch>/...), as
# symlinks in a scratch folder (the Samples checkout is never changed); each capture is
# found by its file name anywhere in the checkout (the root or a folder).
# Usage: tools/captures/ci-build-irs.sh <Samples checkout> <IR output dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
samples="${1:?Samples checkout}"
out="${2:?output dir}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

find_capture() {   # the first file with this name under the checkout
    find "$samples" -path "$samples/.git" -prune -o -type f -name "$1" -print | head -1
}
link() {   # <file name> <unit/patch/name.wav>
    local src
    src="$(find_capture "$1")"
    if [[ -z "$src" ]]; then echo "Captures: '$1' not found"; return 1; fi
    mkdir -p "$work/effects/$(dirname "$2")"
    ln -s "$src" "$work/effects/$2"
}
kit="$(find_capture '01-pedal-capture-sequence-48k.wav')"
[[ -n "$kit" ]] || { echo "Captures: the dry kit is missing"; exit 1; }
link 'Patch 1- Astral Shimmer 48k.wav' 'shimmer/octave-shimmer/capture-sequence.wav'
link 'Patch 1- Astral Shimmer  48k 0 rate.wav' 'shimmer/octave-shimmer/static/capture-sequence-static.wav'
link 'Patch 2- Abyssal Reverb and Delay 48k.wav' 'dark-hall/reverb-delay/capture-sequence.wav'
link 'Wobbly chorusmodulator +reverb 48k.wav' 'chorus/chorus-reverb/capture-sequence.wav'

python3 -m pip install --quiet numpy scipy
start=$SECONDS
python3 -I "$here/run_all.py" "$work/effects" "$work/analysis" --kit "$kit" > "$work/run_all.log" 2>&1 || { tail -30 "$work/run_all.log"; exit 1; }
mkdir -p "$out"
python3 -I "$here/export_irs.py" "$work/analysis" "$out"
for f in "$out"/*.wav; do echo "Captured space: $(basename "$f") ($(wc -c < "$f" | tr -d ' ') bytes)"; done
echo "Captured spaces made in $((SECONDS - start)) s"
