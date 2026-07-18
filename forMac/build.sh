#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SHARED_SRC=$(CDPATH= cd -- "$ROOT/../src" && pwd)
PROJECT_ROOT=$(CDPATH= cd -- "$ROOT/.." && pwd)
BUILD_DIR="$ROOT/build"
DIST_DIR="$ROOT/dist/ComfyUIPlugin"
SDK=$(xcrun --sdk macosx --show-sdk-path)
ARCHS=${ARCHS:-"x86_64 arm64"}

if [ ! -f "$SHARED_SRC/ComfyUIPlugin.cpp" ] || [ ! -f "$SHARED_SRC/ComvertImage_mac.mm" ]; then
    echo "共通 src が見つかりません。forMac は ComfyUIPlugin リポジトリ直下に配置してください。" >&2
    exit 1
fi

build_bundle() {
    product=$1
    mode=$2
    bundle="$DIST_DIR/$product.cpm"
    slices=""

    mkdir -p "$bundle/Contents/MacOS" "$bundle/Contents/Resources" "$BUILD_DIR/$product"
    for arch in $ARCHS; do
        output="$BUILD_DIR/$product/$product-$arch"
        extra=""
        sources="$SHARED_SRC/ComfyUIPlugin.cpp $SHARED_SRC/ComvertImage_mac.mm $SHARED_SRC/FilterPlugIn.cpp"
        if [ "$mode" = "banana" ]; then
            extra="-DCOMFYUI_INCLUDE_DEFAULT_ENTRYPOINT=0"
            sources="$sources $SHARED_SRC/ComfyUINanoBananaPlugin.cpp"
        fi
        # shellcheck disable=SC2086
        xcrun clang++ -std=c++20 -O2 -arch "$arch" -isysroot "$SDK" \
            -mmacosx-version-min=11.0 -fvisibility=hidden -Wno-deprecated-declarations -I"$SHARED_SRC" $extra \
            -bundle -undefined dynamic_lookup -framework ImageIO -framework CoreGraphics $sources -o "$output"
        slices="$slices $output"
    done
    # shellcheck disable=SC2086
    xcrun lipo -create $slices -output "$bundle/Contents/MacOS/$product"
    cp "$ROOT/Info.plist" "$bundle/Contents/Info.plist"
    /usr/libexec/PlistBuddy -c "Set :CFBundleExecutable $product" "$bundle/Contents/Info.plist"
    /usr/libexec/PlistBuddy -c "Set :CFBundleName $product" "$bundle/Contents/Info.plist"
    /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier local.comfyuiplugin.$product" "$bundle/Contents/Info.plist"
    dot_clean -m "$bundle"
    codesign --force --sign - "$bundle"
}

convert_ini_to_utf8() {
    input=$1
    output=$2
    if ! iconv -f CP932 -t UTF-8 "$input" > "$output"; then
        echo "INI ファイルの UTF-8 変換に失敗しました: $input" >&2
        exit 1
    fi
}

mkdir -p "$DIST_DIR/SubImage"
build_bundle ComfyUIPlugin standard
build_bundle ComfyUINanoBananaPlugin banana

convert_ini_to_utf8 "$SHARED_SRC/ComfyUIPlugin.ini" "$DIST_DIR/ComfyUIPlugin.ini"
convert_ini_to_utf8 "$SHARED_SRC/UserSetting.ini" "$DIST_DIR/UserSetting.ini"
for template in "$PROJECT_ROOT"/template_*.json; do
    [ -f "$template" ] || continue
    cp "$template" "$DIST_DIR/"
done
dot_clean -m "$DIST_DIR"

echo "Built: $DIST_DIR"
