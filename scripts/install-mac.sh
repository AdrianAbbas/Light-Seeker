#!/usr/bin/env bash
# Install LIGHT SEEKER (the instrument) and LIGHT SEEKER FX (the effect) for the current
# user and make sure macOS will load them.
#
# Run it from the unzipped LightSeeker-macOS-Universal folder:
#     bash install-mac.sh
# (Tip: type "bash " in Terminal, then drag install-mac.sh into the window.)
#
# What it does:
#   1. copies LightSeeker.component and LightSeekerFX.component to
#      ~/Library/Audio/Plug-Ins/Components and the .vst3 bundles to ~/Library/Audio/Plug-Ins/VST3
#   2. removes the com.apple.quarantine flag your browser added. Without this,
#      macOS refuses to load the plug-in and GarageBand shows a silent slot
#   3. re-signs every bundle (ad hoc) and verifies the signatures
#   4. restarts the Audio Unit registrar so hosts see the new copy
#   5. runs Apple's Audio Unit validator (auval) on both and prints PASS or FAIL
#   6. prints the next steps for GarageBand and Logic
#
# Options:
#   --ci              non-interactive: no prompts, no check for running hosts
#   --out-of-process  remove the sandboxSafe flag from the installed copy, so
#                     GarageBand hosts the plug-ins in a separate process (fallback only)
# (No `set -u`: macOS bash 3.2 treats an empty "$@" as unbound.)
set -eo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ci=0
out_of_process=0
for arg in "$@"; do
    case "$arg" in
        --ci) ci=1 ;;
        --out-of-process) out_of_process=1 ;;
        -h|--help) /usr/bin/sed -n '2,24p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 64 ;;
    esac
done

bold() { printf '\033[1m%s\033[0m\n' "$*"; }
ok()   { printf '  \033[32mOK\033[0m   %s\n' "$*"; }
warn() { printf '  \033[33mNOTE\033[0m %s\n' "$*"; }
fail() { printf '  \033[31mFAIL\033[0m %s\n' "$*"; }

if [[ "$(uname -s)" != Darwin ]]; then
    echo "install-mac.sh runs on macOS only." >&2
    exit 1
fi

# The two plug-ins: bundle name, Audio Unit type and subtype, name in hosts.
# (DUNGEON CRAWLER's plug-ins have their own names and codes: both can be installed.)
plugins="LightSeeker:aumu:Lski:LIGHT SEEKER (instrument)
LightSeekerFX:aufx:Lsek:LIGHT SEEKER FX (effect)"
missing=0
while IFS=: read -r bundle type subtype label; do
    if [[ ! -d "$here/$bundle.component" ]]; then
        echo "$bundle.component was not found next to this script ($here)." >&2
        missing=1
    fi
done <<< "$plugins"
if [[ $missing == 1 ]]; then
    echo "Unzip LightSeeker-macOS-Universal.zip first and run the script from that folder." >&2
    exit 1
fi

components="$HOME/Library/Audio/Plug-Ins/Components"
vst3="$HOME/Library/Audio/Plug-Ins/VST3"
logs="$HOME/Library/Logs/LightSeeker"
mkdir -p "$components" "$vst3" "$logs"

bold "LIGHT SEEKER and LIGHT SEEKER FX installer"

# Hosts keep their own view of plug-ins while running.
if [[ $ci == 0 ]]; then
    running=$(/usr/bin/pgrep -l -x GarageBand 2>/dev/null; /usr/bin/pgrep -l -f 'Logic Pro' 2>/dev/null || true)
    if [[ -n "$running" ]]; then
        warn "GarageBand or Logic is running. Quit it (Cmd-Q), then press Return to continue."
        read -r _
    fi
fi

# A second, system-wide copy would compete with this one.
for system_copy in /Library/Audio/Plug-Ins/Components/LightSeeker.component /Library/Audio/Plug-Ins/VST3/LightSeeker.vst3 \
                   /Library/Audio/Plug-Ins/Components/LightSeekerFX.component /Library/Audio/Plug-Ins/VST3/LightSeekerFX.vst3; do
    if [[ -e "$system_copy" ]]; then
        warn "An older system-wide copy exists: $system_copy"
        warn "Remove it so hosts load the new one:  sudo rm -rf \"$system_copy\""
    fi
done

install_bundle() {
    local src="$1" dest="$2" binary
    rm -rf "$dest"
    /usr/bin/ditto "$src" "$dest"
    binary="$dest/Contents/MacOS/$(basename "${dest%.*}")"
    chmod +x "$binary"
    /usr/bin/xattr -dr com.apple.quarantine "$dest" 2>/dev/null || true
    if /usr/bin/xattr -lr "$dest" 2>/dev/null | /usr/bin/grep -q com.apple.quarantine; then
        fail "could not remove the quarantine flag from $dest"
        return 1
    fi
    if [[ "$dest" == *.component && $out_of_process == 1 ]]; then
        /usr/libexec/PlistBuddy -c "Delete :AudioComponents:0:sandboxSafe" "$dest/Contents/Info.plist" 2>/dev/null || true
        warn "sandboxSafe removed: GarageBand will host $(basename "$dest") out-of-process"
    fi
    # A Developer ID signed and notarised release keeps its signature; anything else
    # is re-signed ad hoc for this Mac.
    if /usr/bin/codesign -dv --verbose=2 "$dest" 2>&1 | /usr/bin/grep -q "Authority=Developer ID Application" \
        && [[ $out_of_process == 0 ]]; then
        :
    elif ! /usr/bin/codesign --force --deep --sign - "$dest" > "$logs/codesign-sign-$(basename "$dest").txt" 2>&1; then
        fail "signing failed for $dest (see $logs/codesign-sign-$(basename "$dest").txt)"
        return 1
    fi
    if ! /usr/bin/codesign --verify --strict --verbose=2 "$dest" > "$logs/codesign-$(basename "$dest").txt" 2>&1; then
        fail "signature check failed for $dest (see $logs/codesign-$(basename "$dest").txt)"
        return 1
    fi
    ok "$(basename "$dest") installed, quarantine removed, signature valid ($(/usr/bin/file -b "$binary" | /usr/bin/sed 's/Mach-O universal binary with //'))"
}

bold "1-3. Copying, removing quarantine, signing"
while IFS=: read -r bundle type subtype label; do
    install_bundle "$here/$bundle.component" "$components/$bundle.component"
    if [[ -d "$here/$bundle.vst3" ]]; then
        install_bundle "$here/$bundle.vst3" "$vst3/$bundle.vst3"
    else
        warn "$bundle.vst3 not in this folder; skipped"
    fi
done <<< "$plugins"

bold "4. Resetting the Audio Unit registry"
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
while IFS=: read -r bundle type subtype label; do
    registered=0
    for _ in $(seq 1 30); do
        if /usr/bin/auval -a 2>/dev/null | /usr/bin/grep -Eq "^[[:space:]]*$type[[:space:]]+$subtype[[:space:]]+Veil"; then
            registered=1
            break
        fi
        sleep 1
    done
    if [[ $registered == 1 ]]; then ok "macOS lists $type $subtype Veil ($label)"; else fail "$label is not listed by the Audio Unit registry after 30 s"; fi
done <<< "$plugins"

bold "5. Validating with Apple's auval"
result=0
failed_logs=""
while IFS=: read -r bundle type subtype label; do
    auval_log="$logs/install-auval-$bundle.txt"
    set +e
    /usr/bin/auval -v "$type" "$subtype" Veil > "$auval_log" 2>&1
    auval_exit=$?
    set -e
    if [[ $auval_exit == 0 ]] && /usr/bin/grep -q "AU VALIDATION SUCCEEDED" "$auval_log"; then
        ok "auval $label: PASS ($(uname -m))"
    else
        fail "auval $label: FAIL ($(uname -m), exit $auval_exit). Full log: $auval_log"
        /usr/bin/grep -E "FATAL|ERROR|Cannot open" "$auval_log" | /usr/bin/head -5 | /usr/bin/sed 's/^/       /'
        result=1
        failed_logs="$failed_logs $auval_log"
    fi
    if [[ "$(uname -m)" == arm64 ]] && /usr/bin/arch -x86_64 /usr/bin/true 2>/dev/null; then
        set +e
        /usr/bin/arch -x86_64 /usr/bin/auval -v "$type" "$subtype" Veil > "$logs/install-auval-$bundle-x86_64.txt" 2>&1
        x86_exit=$?
        set -e
        if [[ $x86_exit == 0 ]]; then ok "auval $label: PASS (x86_64 under Rosetta)"; else warn "auval $label under Rosetta exited $x86_exit (only matters for Intel-mode hosts)"; fi
    fi
done <<< "$plugins"

bold "6. Next steps"
cat <<'NEXT'
  GarageBand: the instrument
    1. Open GarageBand > Settings > Audio/MIDI and make sure "Audio Units" is ticked.
    2. Quit and reopen GarageBand.
    3. Add a Software Instrument track. In the Library or Smart Controls (B) > Plug-ins,
       click the instrument slot: Audio Units > Veil Audio > LIGHT SEEKER.
    4. Play your keyboard or the piano roll. Click a place on the map: its instrument
       plays inside its space. "Lock instrument" keeps the instrument while you travel.
  GarageBand: the effect
    On any track click an empty Audio Effect slot: Audio Units > Veil Audio > LIGHT SEEKER FX.
    If the window shows sliders instead of the interface, use the view menu at the top of
    the plug-in window to switch from "Controls" to "Editor".
  Logic Pro
    1. Logic Pro > Settings > Plug-in Manager, find Veil Audio > LIGHT SEEKER and LIGHT SEEKER FX.
       If one shows as failed, select it and click "Reset & Rescan Selection".
    2. Instrument: on a Software Instrument track, the Instrument slot > AU Instruments >
       Veil Audio > LIGHT SEEKER. Effect: any Audio FX slot > Audio Units > Veil Audio > LIGHT SEEKER FX.
NEXT
if [[ $result == 0 ]]; then
    bold "LIGHT SEEKER and LIGHT SEEKER FX are installed and validated."
else
    bold "Installed, but validation failed. Send$failed_logs when reporting this."
fi
exit $result
