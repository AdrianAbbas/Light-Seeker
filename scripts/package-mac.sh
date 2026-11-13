#!/usr/bin/env bash
# Sign the built bundles of both plug-ins ad hoc (whole bundle, --deep), verify them, and
# stage dist/<name>.zip (default WindwardAudio-LightSeeker-<version>-macOS-Universal, the
# version read from the built instrument) with LIGHT SEEKER (the instrument) and
# LIGHT SEEKER FX (the effect), AU and VST3, the install scripts and the changelog.
# That zip is the developer package (CI artifact LIGHT SEEKER-dev-macOS), for testing only.
# Usage: scripts/package-mac.sh <build dir> <dist dir> [name]
#   (the artefacts are <build dir>/LightSeeker_artefacts/Release and LightSeekerFX_artefacts/Release)
#
# Customer package (docs/INSTALLER_STANDARD.md): scripts/package-mac.sh --customer <signed,
# notarised LIGHT SEEKER.pkg> <dist dir> stages <dist dir>/LIGHT SEEKER-<version>-macOS with only
# "LIGHT SEEKER Installer.pkg" and "LIGHT SEEKER - Read Me.pdf" (written by scripts/build-pkg.sh next
# to the pkg), and zips it as <dist dir>/LIGHT SEEKER-<version>-macOS.zip (the release asset).
# <version> is installer/product.json's. It refuses an unsigned pkg.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "${1:-}" == --customer ]]; then
    pkg="${2:?the signed LIGHT SEEKER.pkg}"
    dist="${3:?the dist directory}"
    if ! /usr/sbin/pkgutil --check-signature "$pkg" | /usr/bin/grep -q "Developer ID Installer"; then
        echo "error: $pkg is not signed with Developer ID Installer; no customer package" >&2
        exit 1
    fi
    version=$(/usr/bin/python3 -c 'import json, sys; print(json.load(open(sys.argv[1]))["version"])' "$here/installer/product.json")
    customer="$dist/LIGHT SEEKER-$version-macOS"
    rm -rf "$customer" "$customer.zip"
    mkdir -p "$customer"
    /usr/bin/ditto "$pkg" "$customer/LIGHT SEEKER Installer.pkg"
    install -m 644 "$(dirname "$pkg")/LIGHT SEEKER - Read Me.pdf" "$customer/"
    /usr/bin/ditto -c -k --sequesterRsrc --keepParent "$customer" "$customer.zip"
    echo "Packaged $customer.zip"
    /usr/bin/unzip -l "$customer.zip"
    exit 0
fi
build="$1"
dist="$2"
version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$build/LightSeeker_artefacts/Release/AU/LightSeeker.component/Contents/Info.plist")
package="${3:-WindwardAudio-LightSeeker-$version-macOS-Universal}"
stage="$dist/$package"
rm -rf "$stage" "$dist/$package.zip"
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
/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$stage" "$dist/$package.zip"
echo "Packaged $dist/$package.zip (version $version)"
/usr/bin/unzip -l "$dist/$package.zip" | /usr/bin/grep -E "install|INSTALL|CHANGELOG|MacOS/LightSeeker(FX)?$"
