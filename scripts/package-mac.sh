#!/usr/bin/env bash
# Sign the built bundles of both plug-ins ad hoc (whole bundle, --deep), verify them, and
# stage dist/<name>.zip (default WindwardAudio-LightSeeker-<version>-macOS-Universal, the
# version read from the built instrument) with LIGHT SEEKER (the instrument) and
# LIGHT SEEKER FX (the effect), AU and VST3, the install scripts and the changelog.
# Usage: scripts/package-mac.sh <build dir> <dist dir> [name]
#   (the artefacts are <build dir>/LightSeeker_artefacts/Release and LightSeekerFX_artefacts/Release)
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$1"
dist="$2"
version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$build/LightSeeker_artefacts/Release/AU/LightSeeker.component/Contents/Info.plist")
name="${3:-WindwardAudio-LightSeeker-$version-macOS-Universal}"
stage="$dist/$name"
rm -rf "$stage" "$dist/$name.zip"
mkdir -p "$stage"

for name in LightSeeker LightSeekerFX; do
    artefacts="$build/${name}_artefacts/Release"
    for bundle in "$artefacts/AU/$name.component" "$artefacts/VST3/$name.vst3" "$artefacts/Standalone/$name.app"; do
        # Developer ID signing (optional CI path) runs before this script; keep it.
        if ! /usr/bin/codesign -dv --verbose=2 "$bundle" 2>&1 | /usr/bin/grep -q "Authority=Developer ID Application"; then
            /usr/bin/codesign --force --deep --sign - --timestamp=none "$bundle"
        fi
        /usr/bin/codesign --verify --strict --verbose=4 "$bundle"
        /usr/bin/codesign -dv --verbose=2 "$bundle" 2>&1 | /usr/bin/grep -E "^(Identifier|Format|Signature|Authority|Sealed Resources)"
    done
    /usr/bin/ditto "$artefacts/AU/$name.component" "$stage/$name.component"
    /usr/bin/ditto "$artefacts/VST3/$name.vst3" "$stage/$name.vst3"
done
install -m 755 "$here/scripts/install-mac.sh" "$here/scripts/uninstall-mac.sh" "$here/scripts/diagnose-mac.sh" "$stage/"
install -m 644 "$here/packaging/INSTALL.txt" "$here/CHANGELOG.md" "$stage/"
# ditto keeps permissions and bundle metadata, unlike a plain zip of the artifact.
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$stage" "$dist/$name.zip"
echo "Packaged $dist/$name.zip (version $version)"
/usr/bin/unzip -l "$dist/$name.zip" | /usr/bin/grep -E "install|INSTALL|CHANGELOG|MacOS/LightSeeker(FX)?$"
