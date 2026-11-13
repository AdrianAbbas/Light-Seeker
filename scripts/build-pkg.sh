#!/usr/bin/env bash
# Build the installer package "LIGHT SEEKER.pkg" with productbuild, to the Windward Audio installer
# standard (docs/INSTALLER_STANDARD.md: pages, wording, background and Read Me from installer/,
# rendered by installer/render.py; identifiers com.windwardaudio.pkg.lightseeker[.au|.vst3|.prepare]):
# LIGHT SEEKER (the instrument, LightSeeker.*) and LIGHT SEEKER FX (the effect, LightSeekerFX.*) as Audio Units
# (/Library/Audio/Plug-Ins/Components) and VST3s (/Library/Audio/Plug-Ins/VST3), for all users.
# The sounds (Contents/Resources/Samples and Painted) and the factory presets (compiled into each
# binary) travel inside the bundles. Preinstall scripts silently remove copies from the builds
# before 1.0 and competing copies (packaging/pkg/remove-copies.sh, its comments stripped); the
# AU postinstall resets the registry.
# The bundles must already be signed (scripts/sign-mac.sh, or ad hoc by scripts/package-mac.sh).
# With an installer identity the product is signed (Developer ID Installer, secure timestamp);
# without one it is left unsigned, for the ad-hoc CI build only.
#
# Environment (optional): LIGHTSEEKER_INSTALLER_IDENTITY, LIGHTSEEKER_KEYCHAIN
# The customer download renames it "LIGHT SEEKER Installer.pkg" (scripts/package-mac.sh --customer).
# Usage: scripts/build-pkg.sh <build dir> <output .pkg>
#   (the artefacts are <build dir>/LightSeeker_artefacts/Release and LightSeekerFX_artefacts/Release)
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$1"
out="$2"
instrument="$build/LightSeeker_artefacts/Release"
fx="$build/LightSeekerFX_artefacts/Release"
identity="${LIGHTSEEKER_INSTALLER_IDENTITY:-}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
version=$(/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$instrument/AU/LightSeeker.component/Contents/Info.plist")
echo "LIGHT SEEKER.pkg version $version"

# Payload roots: copies of the signed bundles (ditto keeps the signatures and the stapled tickets).
mkdir -p "$work/root-au" "$work/root-vst3" "$work/packages" "$work/resources"
/usr/bin/ditto "$instrument/AU/LightSeeker.component" "$work/root-au/LightSeeker.component"
/usr/bin/ditto "$fx/AU/LightSeekerFX.component" "$work/root-au/LightSeekerFX.component"
/usr/bin/ditto "$instrument/VST3/LightSeeker.vst3" "$work/root-vst3/LightSeeker.vst3"
/usr/bin/ditto "$fx/VST3/LightSeekerFX.vst3" "$work/root-vst3/LightSeekerFX.vst3"
/usr/bin/find "$work/root-au" "$work/root-vst3" -name .DS_Store -delete
/bin/chmod -R u+rwX,go+rX,go-w "$work/root-au" "$work/root-vst3"
for bundle in "$work"/root-*/*; do
    /usr/bin/codesign --verify --strict "$bundle" || { echo "::error::$(basename "$bundle") is not validly signed"; exit 1; }
done

# Preinstall scripts: one mode line above the shared body, without its comments.
for mode in prepare au vst3; do
    mkdir -p "$work/scripts-$mode"
    { echo '#!/bin/bash'; echo "mode=$mode"; /usr/bin/grep -v -E '^[[:space:]]*#' "$here/packaging/pkg/remove-copies.sh"; } > "$work/scripts-$mode/preinstall"
    chmod 755 "$work/scripts-$mode/preinstall"
    /bin/bash -n "$work/scripts-$mode/preinstall"
done
{ echo '#!/bin/bash'; /usr/bin/grep -v -E '^[[:space:]]*#' "$here/packaging/pkg/postinstall-au"; } > "$work/scripts-au/postinstall"
chmod 755 "$work/scripts-au/postinstall"

# Component packages. The bundles are not relocatable: without this the installer would follow
# a bundle with the same ID elsewhere on the disk (e.g. a copy in ~/Library) and "upgrade" it there.
plist_set() {   # plist_set <plist> <key path> <type> <value>
    /usr/libexec/PlistBuddy -c "Set :$2 $4" "$1" 2> /dev/null || /usr/libexec/PlistBuddy -c "Add :$2 $3 $4" "$1"
}
component_pkg() {   # component_pkg <name> <install location>
    local name="$1" location="$2" plist="$work/$1-components.plist" i=0
    /usr/bin/pkgbuild --analyze --root "$work/root-$name" "$plist" > /dev/null
    while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" > /dev/null 2>&1; do
        plist_set "$plist" "$i:BundleIsRelocatable" bool false
        plist_set "$plist" "$i:BundleOverwriteAction" string upgrade
        i=$((i + 1))
    done
    /usr/bin/pkgbuild --root "$work/root-$name" --component-plist "$plist" --install-location "$location" \
        --scripts "$work/scripts-$name" --identifier "com.windwardaudio.pkg.lightseeker.$name" --version "$version" \
        --ownership recommended "$work/packages/$name.pkg"
}
component_pkg au /Library/Audio/Plug-Ins/Components
component_pkg vst3 /Library/Audio/Plug-Ins/VST3
/usr/bin/pkgbuild --nopayload --scripts "$work/scripts-prepare" --identifier com.windwardaudio.pkg.lightseeker.prepare \
    --version "$version" "$work/packages/prepare.pkg"

# Installer pages, background, licence and the distribution (installer/product.json + templates).
# Disk space: the payload, rounded up to 10 MB.
mkdir -p "$(dirname "$out")"
kb=$(/usr/bin/du -sk "$work/root-au" "$work/root-vst3" | /usr/bin/awk '{ s += $1 } END { print s }')
disk="$(( (kb + 10239) / 10240 * 10 )) MB"
/usr/bin/python3 "$here/installer/render.py" --pkg-version "$version" --disk-space "$disk" \
    --resources "$work/resources" --distribution "$work/distribution.xml" --readme-pdf "$(dirname "$out")/LIGHT SEEKER - Read Me.pdf"
rm -f "$out"
sign_args=()
if [[ -n "$identity" ]]; then
    sign_args=(--sign "$identity" --timestamp)
    [[ -n "${LIGHTSEEKER_KEYCHAIN:-}" ]] && sign_args+=(--keychain "$LIGHTSEEKER_KEYCHAIN")
else
    echo "::notice::LIGHT SEEKER.pkg is built UNSIGNED (no Developer ID Installer identity): for the CI install test only, not for distribution"
fi
/usr/bin/productbuild --distribution "$work/distribution.xml" --package-path "$work/packages" \
    --resources "$work/resources" --identifier com.windwardaudio.pkg.lightseeker --version "$version" \
    ${sign_args[@]+"${sign_args[@]}"} "$out"
echo "Built $out ($(/usr/bin/du -h "$out" | /usr/bin/cut -f1))"
/usr/sbin/pkgutil --check-signature "$out" || [[ -z "$identity" ]]
