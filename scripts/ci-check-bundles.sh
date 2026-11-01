#!/usr/bin/env bash
# CI gate for the built macOS bundles: both architecture slices present, AU identity
# (an effect, aufx) and sandboxSafe in Info.plist, minimum macOS version reported, the
# painted art inside every bundle (Contents/Resources/Painted).
# Usage: scripts/ci-check-bundles.sh <artefacts dir, e.g. build/LightSeeker_artefacts/Release>
set -euo pipefail
artefacts="$1"
status=0
for bundle in "$artefacts/AU/LightSeeker.component" "$artefacts/VST3/LightSeeker.vst3" "$artefacts/Standalone/LightSeeker.app"; do
    binary="$bundle/Contents/MacOS/LightSeeker"
    if [[ ! -f "$binary" ]]; then
        echo "::error::missing $binary"; status=1; continue
    fi
    archs=$(/usr/bin/lipo -archs "$binary")
    printf '%-60s %s\n' "$binary" "$archs"
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
    else
        echo "::warning::no painted art yet (assets/painted has no map): the bundles use the fallback scenes"
    fi
done


plist="$artefacts/AU/LightSeeker.component/Contents/Info.plist"
check() {
    local key="$1" expected="$2" actual
    actual=$(/usr/libexec/PlistBuddy -c "Print :AudioComponents:0:$key" "$plist" 2>/dev/null || echo "(absent)")
    if [[ "$actual" == "$expected" ]]; then
        printf 'Info.plist %-16s %s\n' "$key" "$actual"
    else
        echo "::error::Info.plist $key expected '$expected', got '$actual'"; status=1
    fi
}
check type aufx
check subtype Lsek
check manufacturer Veil
check factoryFunction LightSeekerAUFactory
check sandboxSafe true
exit $status
