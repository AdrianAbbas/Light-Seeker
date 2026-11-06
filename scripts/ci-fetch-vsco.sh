#!/usr/bin/env bash
# Fetches the brass and woodwinds of VSCO 2 Community Edition (CC0 1.0; docs/LICENSES/VSCO-2-CE.txt) from
# its official source, prepares it for the instrument's sampler (tools/sample-prep/vsco.py)
# and adds it to the prepared samples folder, beside the developer's recordings (run it
# after scripts/ci-fetch-samples.sh, which empties that folder first). The samples are
# never committed to this repository. Only the folders used (the brass, and since the real
# woodwinds update the flute, oboe, piccolo, clarinet and bassoon articulations played), the
# SFZ maps and the licence are fetched (a blob-filtered sparse clone of the pinned tag: about
# 470 MB), and the
# licence is checked against the one logged before anything is prepared.
#
# The prepared notes are kept in <cache dir> (CI caches it under a key of the pinned commit
# and the prep scripts): with a filled cache nothing is downloaded. Without the library
# (the fetch failing) the build goes on and the brass and woodwinds play their synthesised
# stand-ins.
# Usage: scripts/ci-fetch-vsco.sh <cache dir> <prepared samples dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cache="${1:?cache directory}"
out="${2:?prepared samples directory}"
repo="https://github.com/sgossner/VSCO-2-CE.git"
tag="1.1.0"
commit="28092772094b2d9f1148d84cea97f4545b8c687d"
licence_sha="36ffd9dc085d529a7e60e1276d73ae5a030b020313e6c5408593a6ae2af39673"
stamp="$commit $(cat "$here/tools/sample-prep/vsco.py" "$here/tools/sample-prep/prepare.py" | shasum -a 256 | cut -c1-16)"
mkdir -p "$out"

if [[ -f "$cache/stamp" && "$(cat "$cache/stamp")" == "$stamp" && -f "$cache/prepared/index.json" ]]; then
    echo "VSCO 2 CE: the prepared brass from the cache ($stamp)"
else
    src="$(mktemp -d)/vsco"
    rm -rf "$cache"
    mkdir -p "$cache"
    fetched=false
    for attempt in 1 2 3; do
        rm -rf "$src"
        if git clone -q --filter=blob:none --no-checkout --depth 1 --branch "$tag" "$repo" "$src" \
            && [[ "$(git -C "$src" rev-parse HEAD)" == "$commit" ]] \
            && git -C "$src" sparse-checkout set --no-cone '/Brass/F Horn/sus/' '/Brass/F Horn/stac/' '/Brass/Trumpet/sus/' '/Brass/Trumpet/stac/' \
                '/Brass/Tenor Trombone/sus/' '/Brass/Tenor Trombone/stac/' '/Brass/Tuba/sus/' '/Brass/Tuba/stac/' \
                '/Woodwinds/Flute/susNV/' '/Woodwinds/Flute/susvib/' '/Woodwinds/Flute/expvib/' '/Woodwinds/Flute/stac/' \
                '/Woodwinds/Oboe/Vib/' '/Woodwinds/Oboe/Stacc/' '/Woodwinds/Piccolo/Sus/' '/Woodwinds/Piccolo/Stac/' \
                '/Woodwinds/Clarinet/susLong/' '/Woodwinds/Bassoon/sus/' '/*.sfz' '/LICENSE' '/How To Install.txt' \
            && git -C "$src" read-tree -mu HEAD; then
            fetched=true
            break
        fi
        sleep $((attempt * 5))
    done
    if [[ "$fetched" != true ]]; then
        echo "::warning title=Library brass not fetched::Could not fetch VSCO 2 CE $tag ($commit) from $repo: this build's brass and woodwinds play their synthesised stand-ins."
        exit 0
    fi
    got_sha="$(shasum -a 256 "$src/LICENSE" | cut -d' ' -f1)"
    if [[ "$got_sha" != "$licence_sha" ]] || ! head -1 "$src/LICENSE" | grep -q "CC0 1.0 Universal"; then
        echo "::warning title=Library licence changed::VSCO 2 CE's LICENSE is not the CC0 1.0 text logged in docs/LICENSES/VSCO-2-CE.txt (sha256 $got_sha): not used, the brass and woodwinds play their stand-ins."
        exit 0
    fi
    echo "VSCO 2 CE: $tag $(git -C "$src" log -1 --format='%h %ci') (CC0 1.0 Universal, licence sha256 checked), $(find "$src/Brass" -iname '*.wav' | wc -l | tr -d ' ') brass WAVs, $(find "$src/Woodwinds" -iname '*.wav' | wc -l | tr -d ' ') woodwind WAVs"
    python3 -m pip install --quiet -r "$here/tools/sample-prep/requirements.txt"
    if ! python3 "$here/tools/sample-prep/vsco.py" --source "$src" --out "$cache/prepared" | tee "$cache/prepare.log"; then
        echo "::warning title=Library brass preparation failed::vsco.py failed (see the log): this build's brass and woodwinds play their synthesised stand-ins."
        rm -rf "$cache" "$src"
        exit 0
    fi
    cp "$src/LICENSE" "$cache/prepared/VSCO-2-CE-LICENSE.txt"
    echo "$stamp" > "$cache/stamp"
    rm -rf "$src"
fi

# Into the prepared samples folder: the sets, and their ids added to index.json.
python3 - "$cache/prepared" "$out" <<'PY'
import json, os, shutil, sys
src, out = sys.argv[1], sys.argv[2]
ids = json.load(open(os.path.join(src, "index.json")))["instruments"]
index_path = os.path.join(out, "index.json")
index = json.load(open(index_path)) if os.path.isfile(index_path) else {"version": 1, "instruments": []}
for sid in ids:
    shutil.rmtree(os.path.join(out, sid), ignore_errors=True)
    shutil.copytree(os.path.join(src, sid), os.path.join(out, sid))
    shutil.copy(os.path.join(src, sid + ".json"), out)
for extra in ("report-vsco.json", "VSCO-2-CE-LICENSE.txt"):
    if os.path.isfile(os.path.join(src, extra)):
        shutil.copy(os.path.join(src, extra), out)
index["instruments"] = [i for i in index["instruments"] if i not in ids] + ids
json.dump(index, open(index_path, "w"), indent=1)
total = 0
for sid in ids:
    zones = json.load(open(os.path.join(out, sid + ".json")))["zones"]
    total += len(zones)
    print(f"Prepared: {len(zones)} notes ({sid}, VSCO 2 CE)")
wood = sum(len(json.load(open(os.path.join(out, sid + ".json")))["zones"]) for sid in ids if sid.split("-")[1] in ("flute", "oboe", "piccolo", "clarinet", "bassoon"))
print(f"Prepared: {total} library notes ({total - wood} brass, {wood} woodwinds); {len(index['instruments'])} sets in all")
PY
