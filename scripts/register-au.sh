#!/usr/bin/env bash
# Install a component into ~/Library/Audio/Plug-Ins/Components, restart the AU registrar
# and wait until it is listed. Used by CI experiments.
# Usage: scripts/register-au.sh <path/to/X.component> [type subtype]
#   (defaults: aumu Lski, LIGHT SEEKER; LIGHT SEEKER FX is aufx Lsek)
set -euo pipefail
source_bundle="$1"
type="${2:-aumu}"
subtype="${3:-Lski}"
components="$HOME/Library/Audio/Plug-Ins/Components"
installed="$components/$(basename "$source_bundle")"
mkdir -p "$components"
rm -rf "$installed"
/usr/bin/ditto "$source_bundle" "$installed"
/usr/bin/killall -9 AudioComponentRegistrar 2>/dev/null || true
for attempt in $(seq 1 30); do
    if /usr/bin/auval -a 2>/dev/null | /usr/bin/grep -Eq "^[[:space:]]*$type[[:space:]]+$subtype[[:space:]]+Veil"; then
        echo "registered after $attempt attempt(s)"
        exit 0
    fi
    sleep 1
done
echo "$type/$subtype/Veil not registered after 30 s" >&2
exit 2
