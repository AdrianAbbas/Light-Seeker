#!/usr/bin/env bash
# Fetches the developer's recordings from the private AdrianAbbas/Samples repository and
# prepares them for the sampler (tools/sample-prep/prepare.py). The token comes from the
# repository secret SAMPLES_TOKEN (a fine-grained token with read access to that one
# repository); it is passed as an HTTP header, never in a URL or on disk.
# (Copied from WORLDBUILDER. LIGHT SEEKER's change: without the token, or when the fetch
# fails, it warns and leaves the output empty: the build goes on and the instruments that
# use the recordings play their synthesised stand-ins.)
# Usage: scripts/ci-fetch-samples.sh <checkout dir> <prepared output dir>
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
checkout="${1:?checkout directory}"
out="${2:?output directory}"
repo="${SAMPLES_REPOSITORY:-AdrianAbbas/Samples}"

rm -rf "$checkout" "$out"
mkdir -p "$out"
if [[ -z "${SAMPLES_TOKEN:-}" ]]; then
    echo "::warning title=Samples token missing::The repository secret SAMPLES_TOKEN is not set, so the private recordings ($repo) cannot be fetched: this build's instruments use their synthesised stand-ins. Add a fine-grained token with Contents read access to $repo under Settings > Secrets and variables > Actions (see docs/DEVLOG.md, Direction Update 1), then re-run."
    exit 0
fi
auth=$(printf 'x-access-token:%s' "$SAMPLES_TOKEN" | base64 | tr -d '\n')
echo "::add-mask::$auth"
if ! git -c http.extraheader="AUTHORIZATION: basic $auth" clone -q --depth 1 "https://github.com/$repo.git" "$checkout"; then
    echo "::warning title=Samples fetch failed::Could not clone $repo with SAMPLES_TOKEN (expired, or no Contents read access?): this build's instruments use their synthesised stand-ins."
    exit 0
fi
echo "Samples: $(git -C "$checkout" log -1 --format='%h %s') ($(find "$checkout" -maxdepth 1 -iname '*.mp3' | wc -l | tr -d ' ') MP3s, $(find "$checkout" -maxdepth 1 -iname '*.wav' | wc -l | tr -d ' ') WAVs)"

python3 -m pip install --quiet -r "$here/tools/sample-prep/requirements.txt"
if ! python3 "$here/tools/sample-prep/prepare.py" --source "$checkout" --out "$out" | tee "$out.log"; then
    echo "::warning title=Samples preparation failed::prepare.py failed (see the log): this build's instruments use their synthesised stand-ins."
    rm -rf "$out" && mkdir -p "$out"
fi
# The recordings themselves are not needed after preparation.
rm -rf "$checkout"
if [[ -f "$out/index.json" ]]; then echo "Prepared: $(find "$out" -name '*.flac' | wc -l | tr -d ' ') notes"; fi
