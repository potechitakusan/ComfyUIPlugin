#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE="$ROOT/dist/ComfyUIPlugin"
DESTINATION=${1:-"$HOME/Library/CELSYS/CLIPStudioModule/PlugIn/PAINT/ComfyUIPlugin"}

if [ ! -d "$SOURCE" ]; then
    echo "dist がありません。先に ./build.sh を実行してください。" >&2
    exit 1
fi

mkdir -p "$DESTINATION/SubImage"
ditto "$SOURCE/ComfyUIPlugin.cpm" "$DESTINATION/ComfyUIPlugin.cpm"
ditto "$SOURCE/ComfyUINanoBananaPlugin.cpm" "$DESTINATION/ComfyUINanoBananaPlugin.cpm"
cp -f "$SOURCE/ComfyUIPlugin.ini" "$SOURCE/empty.png" "$DESTINATION/"

for template in "$SOURCE"/template_*.json; do
    target="$DESTINATION/$(basename "$template")"
    [ -f "$target" ] || cp "$template" "$target"
done

if [ -f "$DESTINATION/UserSetting.ini" ]; then
    echo "UserSetting.ini は既存のファイルを保持します。"
else
    cp "$SOURCE/UserSetting.ini" "$DESTINATION/UserSetting.ini"
fi

echo "Installed: $DESTINATION"
