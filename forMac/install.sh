#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -d "$ROOT/dist/ComfyUIPlugin" ]; then
    # Development repository layout.
    PROJECT_ROOT=$(CDPATH= cd -- "$ROOT/.." && pwd)
    SOURCE="$ROOT/dist/ComfyUIPlugin"
elif [ -f "$ROOT/ComfyUIPlugin.cpm" ]; then
    # Self-contained release layout created by release.sh.
    PROJECT_ROOT="$ROOT"
    SOURCE="$ROOT"
else
    echo "プラグインのビルド済みファイルが見つかりません。先に ./build.sh を実行してください。" >&2
    exit 1
fi
DESTINATION=${1:-"$HOME/Library/CELSYS/CLIPStudioModule/PlugIn/PAINT/ComfyUIPlugin"}
EASY_INSTALL_REPOSITORY="https://github.com/Tavris1/ComfyUI-Easy-Install.git"
EXPORTER_REPOSITORY="https://github.com/potechitakusan/ComfyUI_CCP_API_Exporter.git"


ask_comfyui_path() {
    while :; do
        printf 'ComfyUI フォルダのパス: '
        IFS= read -r COMFYUI_DIR
        if [ -z "$COMFYUI_DIR" ]; then
            echo "フォルダーパスを入力してください。" >&2
        elif [ ! -d "$COMFYUI_DIR" ]; then
            echo "指定されたフォルダーが見つかりません: $COMFYUI_DIR" >&2
        else
            return 0
        fi
    done
}

install_easy_comfyui() {
    EASY_INSTALL_DIR="$PROJECT_ROOT/ComfyUI-Easy-Install"
    if [ ! -d "$EASY_INSTALL_DIR/.git" ]; then
        echo "ComfyUI-Easy-Install を取得しています..."
        git clone --single-branch --branch MAC-Linux "$EASY_INSTALL_REPOSITORY" "$EASY_INSTALL_DIR"
    fi
    if [ ! -f "$EASY_INSTALL_DIR/ComfyUI-Easy-Install.sh" ]; then
        echo "ComfyUI-Easy-Install.sh が見つかりません: $EASY_INSTALL_DIR" >&2
        exit 1
    fi
    chmod +x "$EASY_INSTALL_DIR/ComfyUI-Easy-Install.sh"
    (
        cd "$EASY_INSTALL_DIR"
        ./ComfyUI-Easy-Install.sh
    )
    ask_comfyui_path
}

copy_comfyui_files() {
    COMFYUI_INPUT_DIR="$COMFYUI_DIR/input"
    COMFYUI_WORKFLOW_DIR="$COMFYUI_DIR/user/default/workflows"
    mkdir -p "$COMFYUI_INPUT_DIR" "$COMFYUI_WORKFLOW_DIR"

    input_count=0
    for input_file in "$PROJECT_ROOT"/input/*.png; do
        [ -f "$input_file" ] || continue
        cp -f "$input_file" "$COMFYUI_INPUT_DIR/"
        input_count=$((input_count + 1))
    done
    echo "input の PNG を $COMFYUI_INPUT_DIR へ $input_count 件コピーしました。"

    workflow_count=0
    for workflow_file in "$PROJECT_ROOT"/examples/*.json; do
        [ -f "$workflow_file" ] || continue
        cp -f "$workflow_file" "$COMFYUI_WORKFLOW_DIR/"
        workflow_count=$((workflow_count + 1))
    done
    echo "examples の JSON を $COMFYUI_WORKFLOW_DIR へ $workflow_count 件コピーしました。"

    printf 'ComfyUI CCP API Exporter をインストールしますか？ [Y/n]: '
    IFS= read -r INSTALL_EXPORTER
    case "$INSTALL_EXPORTER" in
        n|N) return 0 ;;
    esac

    EXPORTER_DIR="$COMFYUI_DIR/custom_nodes/ComfyUI_CCP_API_Exporter"
    if [ -d "$EXPORTER_DIR" ]; then
        echo "CCP API Exporter は既に配置されています: $EXPORTER_DIR"
    elif ! command -v git >/dev/null 2>&1; then
        echo "git が見つからないため、CCP API Exporter を導入できませんでした。" >&2
    else
        mkdir -p "$COMFYUI_DIR/custom_nodes"
        git clone "$EXPORTER_REPOSITORY" "$EXPORTER_DIR" || echo "CCP API Exporter の clone に失敗しました。" >&2
    fi
}

echo "ComfyUI のセットアップを選択してください。"
echo "  1. ComfyUI-Easy-Install を使用して導入する"
echo "  2. インストール済みの ComfyUI フォルダを指定する"
echo "  3. ComfyUI 関連の処理を行わない"
printf '番号を選択してください [3]: '
IFS= read -r SETUP_CHOICE
case "${SETUP_CHOICE:-3}" in
    1) install_easy_comfyui; copy_comfyui_files ;;
    2) ask_comfyui_path; copy_comfyui_files ;;
    3) ;;
    *) echo "1、2、3 のいずれかを指定してください。" >&2; exit 1 ;;
esac

mkdir -p "$DESTINATION" "$DESTINATION/SubImage"

echo "プラグインを上書きしています..."
# ditto は既存バンドルをマージするため、Windows の copy /Y と同じく
# 常にビルド済み CPM に置き換える。
rm -rf "$DESTINATION/ComfyUIPlugin.cpm" "$DESTINATION/ComfyUINanoBananaPlugin.cpm"
ditto "$SOURCE/ComfyUIPlugin.cpm" "$DESTINATION/ComfyUIPlugin.cpm"
ditto "$SOURCE/ComfyUINanoBananaPlugin.cpm" "$DESTINATION/ComfyUINanoBananaPlugin.cpm"

echo "設定を上書きしています..."
cp -f "$SOURCE/ComfyUIPlugin.ini" "$DESTINATION/ComfyUIPlugin.ini"
# (no image) 選択時に必要なプレースホルダー。SubImage は既存の内容を保持する。
cp -f "$SOURCE/empty.png" "$DESTINATION/empty.png"

if [ -f "$DESTINATION/UserSetting.ini" ]; then
    echo "UserSetting.ini は既存のファイルを保持します。"
else
    cp "$SOURCE/UserSetting.ini" "$DESTINATION/UserSetting.ini"
    echo "UserSetting.ini を新規作成しました。"
fi

echo "Installed: $DESTINATION"
