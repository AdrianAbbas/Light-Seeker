# Shared body of LIGHT SEEKER.pkg's preinstall scripts (scripts/build-pkg.sh puts a line
# `mode=prepare|au|vst3` above it). Runs as root, with macOS's bash 3.2, silently: it logs to
# /var/log/install.log only the folder of each copy it removes, never a bundle name. Never
# removes presets, scenes or anything outside the plug-in folders.
#   mode=prepare  copies from the builds before 1.0 (old bundle IDs audio.veil.lightseeker and
#                 audio.veil.lightseeker.fx, whatever their file names; or a component with
#                 these plug-ins' Audio Unit codes and no current bundle ID), in every format,
#                 system-wide and in every user's ~/Library: hosts would list two copies
#   mode=au       other copies of LightSeeker.component and LightSeekerFX.component: per-user
#                 copies (installed from the zip with install-mac.sh) and renamed copies,
#                 which would compete with the system-wide copy this package installs
#   mode=vst3     the same for LightSeeker.vst3 and LightSeekerFX.vst3
# shellcheck disable=SC2154  # mode is set by the line build-pkg.sh puts above this body
set -u
log() { echo "LIGHT SEEKER installer: $*"; }
names="LightSeeker|LightSeekerFX"
new_ids=" com.windwardaudio.lightseeker com.windwardaudio.lightseekerfx "
old_ids=" audio.veil.lightseeker audio.veil.lightseeker.fx "
case "$mode" in
    prepare) exts="component vst3" ;;
    au)      exts="component" ;;
    vst3)    exts="vst3" ;;
    *)       exit 0 ;;
esac
# Whether a component carries these plug-ins' Audio Unit codes (manufacturer Veil, Lski / Lsek).
au_codes_ours() {
    i=0
    while sub=$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:$i:subtype" "$1/Contents/Info.plist" 2>/dev/null); do
        man=$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:$i:manufacturer" "$1/Contents/Info.plist" 2>/dev/null || true)
        if [ "$man" = Veil ] && { [ "$sub" = Lski ] || [ "$sub" = Lsek ]; }; then return 0; fi
        i=$((i + 1))
    done
    return 1
}
for home in /Library /Users/*/Library; do
    [ -d "$home/Audio/Plug-Ins" ] || continue
    for ext in $exts; do
        if [ "$ext" = component ]; then folder="$home/Audio/Plug-Ins/Components"; else folder="$home/Audio/Plug-Ins/VST3"; fi
        [ -d "$folder" ] || continue
        for bundle in "$folder"/*."$ext"; do
            [ -e "$bundle" ] || continue
            base="$(basename "$bundle" ".$ext")"
            id="$(/usr/libexec/PlistBuddy -c "Print :CFBundleIdentifier" "$bundle/Contents/Info.plist" 2>/dev/null || true)"
            by_name=0; current=0; match=0
            case "|$names|" in *"|$base|"*) by_name=1 ;; esac
            if [ -n "$id" ]; then case "$new_ids" in *" $id "*) current=1 ;; esac; fi
            if [ "$mode" = prepare ]; then
                if [ -n "$id" ]; then case "$old_ids" in *" $id "*) match=1 ;; esac; fi
                if [ $match = 0 ] && [ $current = 0 ] && [ "$ext" = component ] && au_codes_ours "$bundle"; then match=1; fi
            else
                if [ $by_name = 1 ] || [ $current = 1 ]; then match=1; fi
                # The copy this package installs (system-wide, its own name) is replaced by the payload.
                if [ "$home" = /Library ] && [ $by_name = 1 ]; then match=0; fi
            fi
            [ $match = 1 ] || continue
            if rm -rf "$bundle"; then log "removed an earlier copy from $folder"; else log "could not remove an earlier copy from $folder"; fi
        done
    done
done
exit 0
