# ComfyUIPlugin for macOS

`forMac` は macOS 向けのビルド・配布用フォルダーです。C++ の共通実装はリポジトリ直下の `src` を使用するため、`forMac` 単体ではなく ComfyUIPlugin リポジトリ全体を Mac へコピーしてください。

## 必要環境

- macOS 11 以降
- Xcode Command Line Tools
- CLIP STUDIO PAINT
- 別途起動した ComfyUI

画像変換には macOS 標準の ImageIO/CoreGraphics を使用します。Python と Pillow は不要です。

## ビルド

```sh
cd forMac
chmod +x build.sh install.sh
./build.sh
```

既定では Intel (`x86_64`) と Apple Silicon (`arm64`) の Universal Binary を生成します。Apple Silicon のみの場合は次のように実行します。

```sh
ARCHS=arm64 ./build.sh
```

成果物は `forMac/dist/ComfyUIPlugin` に生成されます。通常版と Nano Banana 版の `.cpm`、設定ファイルが含まれます。サンプルワークフローはリポジトリ直下の `examples` にあります。

## インストール

```sh
./install.sh
```

既定のコピー先は以下です。

```text
~/Library/CELSYS/CLIPStudioModule/PlugIn/PAINT/ComfyUIPlugin
```

`UserSetting.ini`、既存のテンプレート JSON、`SubImage` は保持します。初回のみテンプレート JSON と `UserSetting.ini` をコピーします。コピー後は CLIP STUDIO PAINT を再起動してください。

## 実機確認

macOS では未検証です。通常生成、選択範囲マスク、非矩形選択の外接矩形処理、透明部分アウトペイント、Intel/Apple Silicon の両アーキテクチャを確認してください。