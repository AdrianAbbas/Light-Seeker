#!/usr/bin/env bash
# Remove LIGHT SEEKER and LIGHT SEEKER FX (Windward Audio) for the current user.
#   bash uninstall-mac.sh           remove the plug-ins, keep your saved presets
#   bash uninstall-mac.sh --purge   also remove your presets and drop-in scenes
#                                   (~/Library/Windward Audio/LIGHT SEEKER and LIGHT SEEKER FX,
#                                   and the copies kept from builds before 1.0)
set -euo pipefail
purge=0
[[ "${1:-}" == --purge ]] && purge=1

# Every copy of the two plug-ins, by bundle ID (this release's and the earlier builds'),
# whatever its file name, plus the standard file names.
ids="com.windwardaudio.lightseeker com.windwardaudio.lightseekerfx audio.veil.lightseeker audio.veil.lightseeker.fx"
ours() {
    local id
    case "$(basename "$1")" in LightSeeker.component|LightSeeker.vst3|LightSeekerFX.component|LightSeekerFX.vst3) return 0 ;; esac
    id=$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$1/Contents/Info.plist" 2>/dev/null || true)
    [[ -n "$id" && " $ids " == *" $id "* ]]
}

removed=0
for dir in "$HOME/Library/Audio/Plug-Ins/Components" "$HOME/Library/Audio/Plug-Ins/VST3"; do
    [[ -d "$dir" ]] || continue
    while IFS= read -r -d '' bundle; do
        if ours "$bundle"; then
            rm -rf "$bundle"
            echo "Removed $bundle"
            removed=1
        fi
    done < <(/usr/bin/find "$dir" -maxdepth 1 \( -name '*.component' -o -name '*.vst3' \) -print0 2>/dev/null)
done
[[ $removed == 1 ]] || echo "No LIGHT SEEKER plug-ins found in ~/Library/Audio/Plug-Ins."

for data in "$HOME/Library/Windward Audio/LIGHT SEEKER" "$HOME/Library/Windward Audio/LIGHT SEEKER FX" \
            "$HOME/Library/Audio/Presets/Windward Audio/LIGHT SEEKER" "$HOME/Library/Audio/Presets/Windward Audio/LIGHT SEEKER FX" \
            "$HOME/Library/Application Support/LightSeeker" \
            "$HOME/Library/Audio/Presets/Veil Audio/LIGHT SEEKER" "$HOME/Library/Audio/Presets/Veil Audio/LIGHT SEEKER FX" \
            "$HOME/Library/Audio/Presets/Veil Audio/LightSeeker" "$HOME/Library/Audio/Presets/Veil Audio/LightSeekerFX"; do
    if [[ $purge == 1 && -d "$data" ]]; then
        rm -rf "$data"
        echo "Removed $data"
    elif [[ -d "$data" ]]; then
        echo "Kept your presets and scenes in $data (use --purge to remove them)."
    fi
done
# The company folder, if nothing else is left in it.
[[ $purge == 1 ]] && rmdir "$HOME/Library/Windward Audio" 2>/dev/null || true

for dir in /Library/Audio/Plug-Ins/Components /Library/Audio/Plug-Ins/VST3; do
    [[ -d "$dir" ]] || continue
    while IFS= read -r -d '' system_copy; do
        if ours "$system_copy"; then echo "A system-wide copy remains: sudo rm -rf \"$system_copy\""; fi
    done < <(/usr/bin/find "$dir" -maxdepth 1 \( -name '*.component' -o -name '*.vst3' \) -print0 2>/dev/null)
done

/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "Audio Unit registry reset. Restart GarageBand or Logic."
