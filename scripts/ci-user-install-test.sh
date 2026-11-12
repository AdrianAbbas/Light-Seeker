#!/usr/bin/env bash
# Reproduce what a user does, then validate like a host.
#   1. unzip the release package and mark every file as a browser download
#      (com.apple.quarantine), which is what broke VEIL in GarageBand
#   2. record whether macOS on this runner refuses the quarantined copy, then leave what a
#      Mac with a build before 1.0 has: copies under the old bundle IDs and company name
#      (other file names) and presets in the old folders
#   3. install with install-mac.sh --ci: the old copies must be gone, the presets copied
#      into ~/Library/Windward Audio/... (the originals kept), and the registry must list
#      each plug-in once, as "Windward Audio: LIGHT SEEKER" and "Windward Audio: LIGHT SEEKER FX"
#   4. auval and the AU host harness (--strict), arm64 and x86_64 (Rosetta), for both
#      plug-ins: LIGHT SEEKER (aumu Lski) and LIGHT SEEKER FX (aufx Lsek): every hosting
#      route, the name and version 1.0.0 hosts see, and an old session of each loading
#      unchanged (Tests/fixtures, state compare)
# Usage: scripts/ci-user-install-test.sh <WindwardAudio-LightSeeker-<version>-macOS-Universal.zip> <harness binary> <log dir>
set -euo pipefail
package="$1"
harness="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
logs="$3"
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
company="Windward Audio"
mkdir -p "$logs"
work="$(mktemp -d)"
status=0

# Run a command with a time limit (a Gatekeeper prompt would otherwise block forever).
limit() { local seconds="$1"; shift; /usr/bin/perl -e 'alarm shift @ARGV; exec @ARGV or die "exec failed: $!\n"' "$seconds" "$@"; }

/usr/bin/ditto -x -k "$package" "$work"
folder="$work/$(basename "$package" .zip)"
flag="0081;$(printf '%x' "$(date +%s)");Chrome;$(/usr/bin/uuidgen)"
/usr/bin/find "$folder" -exec /usr/bin/xattr -w com.apple.quarantine "$flag" {} +
{
    echo "Gatekeeper: $(/usr/sbin/spctl --status 2>&1)"
    /usr/bin/xattr -l "$folder/LightSeeker.component/Contents/MacOS/LightSeeker"
} | tee "$logs/quarantine.txt"

# 2. Quarantined copy installed by hand, without the script (informational).
components="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$components"
rm -rf "$components/LightSeeker.component"
/usr/bin/ditto "$folder/LightSeeker.component" "$components/LightSeeker.component"
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 3
set +e
limit 120 /usr/bin/auval -v aumu Lski Veil > "$logs/auval-quarantined-without-script.txt" 2>&1
quarantined_exit=$?
set -e
echo "auval on the quarantined copy (no install script): exit $quarantined_exit" | tee -a "$logs/quarantine.txt"
rm -rf "$components/LightSeeker.component"

# 2b. A Mac that had a build before 1.0: a copy of each plug-in under its old bundle ID
# and company name, with another file name, and presets in the old folders.
vst3="$HOME/Library/Audio/Plug-Ins/VST3"
mkdir -p "$vst3"
stale_au="$components/LIGHT SEEKER (before 1.0).component"
stale_vst3="$vst3/LIGHT SEEKER FX (before 1.0).vst3"
rm -rf "$stale_au" "$stale_vst3" "$HOME/Library/$company" "$HOME/Library/Audio/Presets/$company"
/usr/bin/ditto "$folder/LightSeeker.component" "$stale_au"
/usr/bin/plutil -replace CFBundleIdentifier -string audio.veil.lightseeker "$stale_au/Contents/Info.plist"
/usr/bin/plutil -replace AudioComponents.0.name -string "Veil Audio: LIGHT SEEKER" "$stale_au/Contents/Info.plist"
/usr/bin/plutil -replace AudioComponents.0.version -integer 256 "$stale_au/Contents/Info.plist"
/usr/bin/ditto "$folder/LightSeekerFX.vst3" "$stale_vst3"
/usr/bin/plutil -replace CFBundleIdentifier -string audio.veil.lightseeker.fx "$stale_vst3/Contents/Info.plist"
for stale in "$stale_au" "$stale_vst3"; do
    /usr/bin/xattr -dr com.apple.quarantine "$stale" 2>/dev/null || true
    /usr/bin/codesign --force --deep --sign - "$stale" > /dev/null 2>&1
done
old_data="$HOME/Library/Application Support/LightSeeker"
old_host="$HOME/Library/Audio/Presets/Veil Audio/LIGHT SEEKER"
mkdir -p "$old_data/Presets" "$old_data/Instrument Presets" "$old_host"
cp "$repo/Tests/fixtures/old-session-fx.xml" "$old_data/Presets/CI Old Hall.lspreset"
cp "$repo/Tests/fixtures/old-session-instrument.xml" "$old_data/Instrument Presets/CI Old Organ.lsinst"
echo "host preset" > "$old_host/CI Song.aupreset"
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 3
{
    echo "Before install-mac.sh, the registry lists:"
    /usr/bin/auval -a 2>&1 | /usr/bin/grep -E "Lski|Lsek" || echo "  (none)"
} | tee "$logs/registry-before.txt"

# 3. The user's path: run the installer from the quarantined download.
set +e
limit 300 /bin/bash "$folder/install-mac.sh" --ci 2>&1 | tee "$logs/install-mac.txt"
install_exit=${PIPESTATUS[0]}
set -e
echo "install-mac.sh exit $install_exit"
if [[ $install_exit != 0 ]]; then echo "::error::install-mac.sh failed (exit $install_exit)"; status=1; fi
for bundle in LightSeeker LightSeekerFX; do
    if /usr/bin/xattr -lr "$components/$bundle.component" | /usr/bin/grep -q com.apple.quarantine; then
        echo "::error::installed $bundle.component is still quarantined"; status=1
    fi
done
for stale in "$stale_au" "$stale_vst3"; do
    if [[ -e "$stale" ]]; then echo "::error::install-mac.sh left the old copy $stale"; status=1; else echo "old copy removed: $stale"; fi
done
# The presets from before 1.0: copied into the new folders, the originals kept.
for pair in "$old_data/Instrument Presets/CI Old Organ.lsinst=$HOME/Library/$company/LIGHT SEEKER/Presets/CI Old Organ.lsinst" \
            "$old_data/Presets/CI Old Hall.lspreset=$HOME/Library/$company/LIGHT SEEKER FX/Presets/CI Old Hall.lspreset" \
            "$old_host/CI Song.aupreset=$HOME/Library/Audio/Presets/$company/LIGHT SEEKER/CI Song.aupreset"; do
    from="${pair%%=*}"; to="${pair#*=}"
    if [[ -f "$to" && -f "$from" ]] && /usr/bin/cmp -s "$from" "$to"; then
        echo "preset migrated (copy kept): $to"
    else
        echo "::error::preset not migrated: $from -> $to"; status=1
    fi
done | tee "$logs/migration.txt"
[[ ${PIPESTATUS[0]} == 0 ]] || status=1
/usr/bin/grep -q "::error::" "$logs/migration.txt" && status=1

# The registry: each plug-in listed once, under the new company name (both architectures).
registry_check() {
    local arch="$1" type subtype shown entries count
    for plugin in "aumu:Lski:LIGHT SEEKER" "aufx:Lsek:LIGHT SEEKER FX"; do
        IFS=: read -r type subtype shown <<< "$plugin"
        entries=$(/usr/bin/arch "-$arch" /usr/bin/auval -a 2>/dev/null | /usr/bin/grep -E "^[[:space:]]*$type[[:space:]]+$subtype[[:space:]]+Veil" || true)
        count=$(printf '%s' "$entries" | /usr/bin/grep -c . || true)
        echo "$arch: $entries"
        if [[ "$count" != 1 ]]; then echo "::error::the $arch registry lists $type $subtype $count times"; status=1
        elif [[ "$entries" != *"$company: $shown" && "$entries" != *"$company: $shown "* ]]; then echo "::error::the $arch registry lists '$entries', expected '$company: $shown'"; status=1
        else echo "$arch registry: listed once as $company: $shown"; fi
    done
}
registry_check arm64 > "$logs/registry.txt" 2>&1 || true
/usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null && { registry_check x86_64 >> "$logs/registry.txt" 2>&1 || true; }
cat "$logs/registry.txt"
/usr/bin/grep -q "::error::" "$logs/registry.txt" && status=1

# 4. Validators on the installed copy.
run_check() {
    local name="$1"; shift
    set +e
    limit 600 "$@" > "$logs/$name.txt" 2>&1
    local code=$?
    set -e
    echo "$name: exit $code"
    if [[ $code != 0 ]]; then echo "::error::$name failed (exit $code)"; status=1; fi
}
strict=""  # becomes -strict when this auval supports it
/usr/bin/auval -h 2>&1 | /usr/bin/grep -qE -- '(^|[[:space:]])-strict([[:space:]]|$)' && strict="-strict"
# One process per hosting route, so a crash in one cannot hide the others. Every
# route is a gate: AUv2 API out-of-process and in-process, and the AUAudioUnit API
# out-of-process (rendered through AVAudioEngine since Milestone 1, see docs/DEVLOG.md).
# The instrument plays notes over MIDI on each route; the effect processes a sine.
# old-session: a session saved before 1.0 loads unchanged (Tests/fixtures, state compare).
suites="v2-oop v2-inproc auaudiounit old-session"
rosetta=0
/usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null && rosetta=1
for plugin in "instrument:aumu:Lski:LIGHT SEEKER" "effect:aufx:Lsek:LIGHT SEEKER FX"; do
    IFS=: read -r kind type subtype shown <<< "$plugin"
    identity=(--expect-name "$company: $shown" --expect-version 0x10000 --old-session "$repo/Tests/fixtures/old-session-${kind/effect/fx}.xml")
    for arch in arm64 x86_64; do
        [[ $arch == arm64 || $rosetta == 1 ]] || continue
        # shellcheck disable=SC2086  # $strict is empty or one flag
        run_check "auval-$kind-$arch" /usr/bin/arch "-$arch" /usr/bin/auval $strict -v "$type" "$subtype" Veil
        # The version and company hosts read: auval prints both as it opens the component.
        if /usr/bin/grep -q -E "Version: *1\.0\.0|0x10000" "$logs/auval-$kind-$arch.txt"; then
            echo "auval-$kind-$arch: version 1.0.0 reported"
        else
            echo "::error::auval-$kind-$arch did not report version 1.0.0"; /usr/bin/grep -i version "$logs/auval-$kind-$arch.txt" | head -5; status=1
        fi
        if /usr/bin/grep -q "Manufacturer String: $company" "$logs/auval-$kind-$arch.txt"; then
            echo "auval-$kind-$arch: manufacturer $company"
        else
            echo "::error::auval-$kind-$arch did not report the manufacturer $company"; /usr/bin/grep -i manufacturer "$logs/auval-$kind-$arch.txt" | head -5; status=1
        fi
        for suite in $suites; do run_check "harness-$arch-$kind-$suite" /usr/bin/arch "-$arch" "$harness" --strict --only "$suite" --component "$type:$subtype:Veil" "${identity[@]}"; done
    done
done
if [[ $rosetta == 0 ]]; then echo "::error::Rosetta unavailable: the x86_64 slices were not validated"; status=1; fi
# Control (also a gate): Apple's own AULowpass through the same AUAudioUnit route, so a
# failure there tells a harness problem from a LIGHT SEEKER problem.
run_check control-apple-lowpass-auaudiounit "$harness" --strict --only auaudiounit --no-window --component "aufx:lpas:appl"
grep -h -E "^(FAIL|SUMMARY)" "$logs"/harness-*.txt || true
python3 "$(dirname "${BASH_SOURCE[0]}")/ci-crash-summary.py" 60 LightSeekerAUHarness AUHostingService auval LightSeeker LightSeekerFX > "$logs/crashes.txt" 2>&1 || true
cat "$logs/crashes.txt"
exit $status
