#!/usr/bin/env bash
# CI gate for the built macOS bundles of both plug-ins: both architecture slices present,
# AU identity (LIGHT SEEKER: an instrument, aumu Lski; LIGHT SEEKER FX: an effect, aufx
# Lsek) and sandboxSafe in Info.plist, minimum macOS version reported, the painted art
# inside every bundle (Contents/Resources/Painted), the recordings in the instrument's
# (Contents/Resources/Samples; a warning when the build had none). 1.0: the company and
# version hosts show (AU name "Windward Audio: LIGHT SEEKER", version 1.0.0 everywhere),
# the new bundle IDs, and the unchanged AU manufacturer code (Veil: old projects load).
# Usage: scripts/ci-check-bundles.sh <build dir, e.g. build>
set -euo pipefail
build="$1"
status=0
version=1.0.0
company="Windward Audio"
check_plugin() {
    local name="$1" type="$2" subtype="$3" shown="$4" id="$5" artefacts="$build/${1}_artefacts/Release"
    for bundle in "$artefacts/AU/$name.component" "$artefacts/VST3/$name.vst3" "$artefacts/Standalone/$name.app"; do
        binary="$bundle/Contents/MacOS/$name"
        if [[ ! -f "$binary" ]]; then
            echo "::error::missing $binary"; status=1; continue
        fi
        for pair in "CFBundleIdentifier=$id" "CFBundleShortVersionString=$version" "CFBundleVersion=$version"; do
            key="${pair%%=*}"; want="${pair#*=}"
            got=$(/usr/libexec/PlistBuddy -c "Print :$key" "$bundle/Contents/Info.plist" 2>/dev/null || echo "(absent)")
            if [[ "$got" == "$want" ]]; then printf '    %-28s %s\n' "$key" "$got"; else echo "::error::$bundle $key expected '$want', got '$got'"; status=1; fi
        done
        copyright=$(/usr/libexec/PlistBuddy -c "Print :NSHumanReadableCopyright" "$bundle/Contents/Info.plist" 2>/dev/null || true)
        printf '    %-28s %s\n' NSHumanReadableCopyright "$copyright"
        if /usr/bin/grep -a -q -i "veil audio\|elsewhere audio" "$bundle/Contents/Info.plist"; then
            echo "::error::$bundle Info.plist still shows an old company name"; status=1
        fi
        archs=$(/usr/bin/lipo -archs "$binary")
        printf '%-70s %s\n' "$binary" "$archs"
        if [[ " $archs " != *" arm64 "* || " $archs " != *" x86_64 "* ]]; then
            echo "::error::$binary must contain arm64 and x86_64 slices, has: $archs"; status=1
        fi
        for arch in x86_64 arm64; do
            minos=$(/usr/bin/otool -arch "$arch" -l "$binary" | /usr/bin/awk '/LC_BUILD_VERSION/{f=1} f&&/minos/{print $2; exit}')
            printf '    %-7s minimum macOS %s\n' "$arch" "${minos:-unknown}"
        done
        painted="$bundle/Contents/Resources/Painted"
        if [[ -f "$painted/map/sky.jpg" ]]; then
            printf '    painted art: %s scenes, %s\n' "$(find "$painted" -name scene.json | wc -l | tr -d ' ')" "$(du -sh "$painted" | cut -f1)"
        elif [[ -f "assets/painted/map/sky.jpg" ]]; then
            echo "::error::missing the painted art in $bundle (Contents/Resources/Painted)"; status=1
        fi
        if [[ "$type" == aumu ]]; then
            if [[ -f "$bundle/Contents/Resources/Samples/index.json" ]]; then
                printf '    recordings: %s notes, %s\n' "$(find "$bundle/Contents/Resources/Samples" -name '*.flac' | wc -l | tr -d ' ')" "$(du -sh "$bundle/Contents/Resources/Samples" | cut -f1)"
            else
                echo "::warning::$name has no recordings (Contents/Resources/Samples): the instruments that use them play their stand-ins"
            fi
        fi
    done
    plist="$artefacts/AU/$name.component/Contents/Info.plist"
    check() {
        local key="$1" expected="$2" actual
        actual=$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:0:$key" "$plist" 2>/dev/null || echo "(absent)")
        if [[ "$actual" == "$expected" ]]; then
            printf '%s Info.plist %-16s %s\n' "$name" "$key" "$actual"
        else
            echo "::error::$name Info.plist $key expected '$expected', got '$actual'"; status=1
        fi
    }
    check type "$type"
    check subtype "$subtype"
    check manufacturer Veil   # unchanged on purpose: hosts find saved projects' plug-ins by it
    check name "$company: $shown"
    check version 65536       # 1.0.0
    check factoryFunction "${name}AUFactory"
    check sandboxSafe true
}
check_plugin LightSeeker aumu Lski "LIGHT SEEKER" com.windwardaudio.lightseeker
check_plugin LightSeekerFX aufx Lsek "LIGHT SEEKER FX" com.windwardaudio.lightseekerfx
exit $status
