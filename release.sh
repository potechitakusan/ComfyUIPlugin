#!/bin/sh
set -eu

# Usage: ./release.sh [vX.Y.Z] [destination]
# Creates the self-contained macOS release folder. Run forMac/build.sh first.
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE="$ROOT/forMac/dist/ComfyUIPlugin"
VERSION=${1:-v0.5.0}
DESTINATION=${2:-"$ROOT/../github/release/ComfyUIPlugin_${VERSION}_macOS"}

case "$VERSION" in
    v[0-9]*.[0-9]*.[0-9]*) ;;
    *) echo "Specify a version such as v0.5.0." >&2; exit 1 ;;
esac

if [ ! -d "$SOURCE" ]; then
    echo "macOS build output was not found: $SOURCE" >&2
    echo "Run ./forMac/build.sh first." >&2
    exit 1
fi

for required in ComfyUIPlugin.cpm ComfyUINanoBananaPlugin.cpm ComfyUIPlugin.ini UserSetting.ini; do
    if [ ! -e "$SOURCE/$required" ]; then
        echo "Required macOS release file is missing: $SOURCE/$required" >&2
        exit 1
    fi
done

if [ -e "$DESTINATION" ]; then
    printf 'Existing release folder will be replaced: %s\nContinue? [y/N]: ' "$DESTINATION"
    IFS= read -r answer
    case "$answer" in
        y|Y|yes|YES) rm -rf "$DESTINATION" ;;
        *) echo "Cancelled."; exit 1 ;;
    esac
fi

PACKAGE_SOURCE="$DESTINATION/dist/ComfyUIPlugin"
mkdir -p "$PACKAGE_SOURCE/SubImage" "$DESTINATION/input" "$DESTINATION/examples"
ditto "$SOURCE/ComfyUIPlugin.cpm" "$PACKAGE_SOURCE/ComfyUIPlugin.cpm"
ditto "$SOURCE/ComfyUINanoBananaPlugin.cpm" "$PACKAGE_SOURCE/ComfyUINanoBananaPlugin.cpm"
cp "$SOURCE/ComfyUIPlugin.ini" "$SOURCE/UserSetting.ini" "$PACKAGE_SOURCE/"
cp "$ROOT/forMac/install.sh" "$DESTINATION/install.sh"
chmod +x "$DESTINATION/install.sh"

for input_file in "$ROOT"/input/*.png; do
    [ -f "$input_file" ] || continue
    cp "$input_file" "$DESTINATION/input/"
done
for workflow_file in "$ROOT"/examples/*.json; do
    [ -f "$workflow_file" ] || continue
    cp "$workflow_file" "$DESTINATION/examples/"
done

echo "Release files created: $DESTINATION"
