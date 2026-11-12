#!/usr/bin/env bash
# Install LIGHT SEEKER (the instrument) and LIGHT SEEKER FX (the effect) by Windward Audio,
# version 1.0.0, for the current user and make sure macOS will load them.
#
# Run it from the unzipped WindwardAudio-LightSeeker-1.0.0-macOS-Universal folder:
#     bash install-mac.sh
# (Tip: type "bash " in Terminal, then drag install-mac.sh into the window.)
#
# What it does:
#   0. removes copies left by builds before 1.0 under their old bundle IDs (whatever their
#      file names), so hosts list each plug-in once, and copies your presets and drop-in
#      scenes from the old folders into ~/Library/Windward Audio/LIGHT SEEKER and
#      ~/Library/Windward Audio/LIGHT SEEKER FX (the originals stay where they are)
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
        -h|--help) /usr/bin/sed -n '2,28p' "${BASH_SOURCE[0]}"; exit 0 ;;
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
# The Audio Unit manufacturer code is unchanged since the first builds, so projects saved
# with them still find both plug-ins; only the company name hosts show changed (1.0).
company="Windward Audio"
manufacturer=Veil
plugins="LightSeeker:aumu:Lski:LIGHT SEEKER
LightSeekerFX:aufx:Lsek:LIGHT SEEKER FX"
# Bundle IDs of the builds before 1.0 (the same plug-ins): their copies are removed.
old_ids="audio.veil.lightseeker audio.veil.lightseeker.fx"
new_ids="com.windwardaudio.lightseeker com.windwardaudio.lightseekerfx"
missing=0
while IFS=: read -r bundle type subtype label; do
    if [[ ! -d "$here/$bundle.component" ]]; then
        echo "$bundle.component was not found next to this script ($here)." >&2
        missing=1
    fi
done <<< "$plugins"
if [[ $missing == 1 ]]; then
    echo "Unzip the LIGHT SEEKER download first and run the script from the unzipped folder." >&2
    exit 1
fi

components="$HOME/Library/Audio/Plug-Ins/Components"
vst3="$HOME/Library/Audio/Plug-Ins/VST3"
logs="$HOME/Library/Logs/LightSeeker"
mkdir -p "$components" "$vst3" "$logs"

bold "LIGHT SEEKER and LIGHT SEEKER FX 1.0.0 installer ($company)"

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

# The bundle ID of a plug-in bundle (empty when it has none).
bundle_id() { /usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$1/Contents/Info.plist" 2>/dev/null || true; }
# Whether a component is one of these plug-ins by its Audio Unit codes (aumu Lski / aufx Lsek).
au_codes_ours() {
    local plist="$1/Contents/Info.plist" i=0 sub man
    while sub=$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:$i:subtype" "$plist" 2>/dev/null); do
        man=$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:$i:manufacturer" "$plist" 2>/dev/null || true)
        if [[ "$man" == "$manufacturer" && ( "$sub" == Lski || "$sub" == Lsek ) ]]; then return 0; fi
        i=$((i + 1))
    done
    return 1
}
# Removes every earlier copy: a bundle with an old bundle ID (any file name), or a component
# with these Audio Unit codes and no current bundle ID. A copy in /Library needs sudo: it is
# removed if this user may, otherwise the command is printed.
remove_old_copies() {
    local dir bundle id old=0
    for dir in "$components" "$vst3" /Library/Audio/Plug-Ins/Components /Library/Audio/Plug-Ins/VST3; do
        [[ -d "$dir" ]] || continue
        while IFS= read -r -d '' bundle; do
            id=$(bundle_id "$bundle")
            case " $new_ids " in *" $id "*) continue ;; esac
            if [[ " $old_ids " == *" $id "* ]] || { [[ "$bundle" == *.component ]] && au_codes_ours "$bundle"; }; then
                old=1
                if rm -rf "$bundle" 2>/dev/null && [[ ! -e "$bundle" ]]; then
                    ok "removed an earlier copy: $bundle"
                else
                    warn "an earlier copy needs an administrator to remove it: sudo rm -rf \"$bundle\""
                fi
            fi
        done < <(/usr/bin/find "$dir" -maxdepth 1 \( -name '*.component' -o -name '*.vst3' \) -print0 2>/dev/null)
    done
    [[ $old == 1 ]] || ok "no earlier copies under the old bundle IDs"
}

# Copies one folder's files into another, never overwriting a file already there.
copy_missing() {
    local from="$1" to="$2" file rel count=0
    [[ -d "$from" ]] || { echo 0; return; }
    while IFS= read -r -d '' file; do
        rel="${file#"$from"/}"
        [[ "$(basename "$rel")" == .* ]] && continue
        if [[ ! -e "$to/$rel" ]]; then
            mkdir -p "$(dirname "$to/$rel")" && /bin/cp -p "$file" "$to/$rel" && count=$((count + 1))
        fi
    done < <(/usr/bin/find "$from" -type f -print0 2>/dev/null)
    echo "$count"
}
# The user's data from builds before 1.0 (the plug-ins make the same copy on their first
# launch; see Source/UserData.h). Nothing is moved or deleted.
migrate_data() {
    local old_shared="$HOME/Library/Application Support/LightSeeker" new_root="$HOME/Library/$company"
    local old_host="$HOME/Library/Audio/Presets/Veil Audio" new_host="$HOME/Library/Audio/Presets/$company"
    local name presets folder n total note
    for name in "LIGHT SEEKER" "LIGHT SEEKER FX"; do
        note="$new_root/$name/MIGRATED.txt"
        if [[ -f "$note" ]]; then ok "$name: your earlier data was already copied (see $note)"; continue; fi
        if [[ "$name" == "LIGHT SEEKER" ]]; then presets="Instrument Presets"; folder=LightSeeker; else presets="Presets"; folder=LightSeekerFX; fi
        if [[ ! -d "$old_shared/$presets" && ! -d "$old_shared/scenes" && ! -d "$old_host/$name" && ! -d "$old_host/$folder" ]]; then continue; fi
        total=0
        n=$(copy_missing "$old_shared/$presets" "$new_root/$name/Presets"); total=$((total + n))
        n=$(copy_missing "$old_shared/scenes" "$new_root/$name/Scenes"); total=$((total + n))
        n=$(copy_missing "$old_host/$name" "$new_host/$name"); total=$((total + n))
        n=$(copy_missing "$old_host/$folder" "$new_host/$folder"); total=$((total + n))
        mkdir -p "$new_root/$name"
        printf '%s: your data from earlier builds was copied here by install-mac.sh on %s.\nThe originals were left where they were (%s, %s).\n' \
            "$name" "$(date)" "$old_shared" "$old_host" > "$note"
        ok "$name: $total files from earlier builds copied into $new_root/$name (originals kept)"
    done
}

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

bold "0. Earlier copies and your data"
remove_old_copies
migrate_data

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
        entries=$(/usr/bin/auval -a 2>/dev/null | /usr/bin/grep -E "^[[:space:]]*$type[[:space:]]+$subtype[[:space:]]+$manufacturer" || true)
        if [[ -n "$entries" ]]; then
            registered=1
            break
        fi
        sleep 1
    done
    if [[ $registered == 0 ]]; then
        fail "$label is not listed by the Audio Unit registry after 30 s"
    elif [[ $(printf '%s\n' "$entries" | /usr/bin/wc -l | /usr/bin/tr -d ' ') != 1 ]]; then
        fail "$label is listed more than once: remove the other copies (see above)"
    elif [[ "$entries" != *"$company: $label"* ]]; then
        warn "$label is listed, but not yet under its new name: restart the Mac if hosts show the old one"
    else
        ok "macOS lists $company: $label"
    fi
done <<< "$plugins"

bold "5. Validating with Apple's auval"
result=0
failed_logs=""
while IFS=: read -r bundle type subtype label; do
    auval_log="$logs/install-auval-$bundle.txt"
    set +e
    /usr/bin/auval -v "$type" "$subtype" "$manufacturer" > "$auval_log" 2>&1
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
        /usr/bin/arch -x86_64 /usr/bin/auval -v "$type" "$subtype" "$manufacturer" > "$logs/install-auval-$bundle-x86_64.txt" 2>&1
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
       click the instrument slot: Audio Units > Windward Audio > LIGHT SEEKER.
    4. Play your keyboard or the piano roll. Click a place on the map: its instrument
       plays inside its space. "Lock instrument" keeps the instrument while you travel.
  GarageBand: the effect
    On any track click an empty Audio Effect slot: Audio Units > Windward Audio > LIGHT SEEKER FX.
    If the window shows sliders instead of the interface, use the view menu at the top of
    the plug-in window to switch from "Controls" to "Editor".
  Logic Pro
    1. Logic Pro > Settings > Plug-in Manager, find Windward Audio > LIGHT SEEKER and LIGHT SEEKER FX.
       If one shows as failed, select it and click "Reset & Rescan Selection".
    2. Instrument: on a Software Instrument track, the Instrument slot > AU Instruments >
       Windward Audio > LIGHT SEEKER. Effect: any Audio FX slot > Audio Units > Windward Audio > LIGHT SEEKER FX.
  Projects and presets made with earlier builds open as before. Your presets and drop-in
  scenes now live in ~/Library/Windward Audio/LIGHT SEEKER and .../LIGHT SEEKER FX.
NEXT
if [[ $result == 0 ]]; then
    bold "LIGHT SEEKER and LIGHT SEEKER FX are installed and validated."
else
    bold "Installed, but validation failed. Send$failed_logs when reporting this."
fi
exit $result
