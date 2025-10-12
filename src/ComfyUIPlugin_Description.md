# ComfyUIPlugin リリース仕様

## 1. 概要
- Clip Studio Paint のフィルタープラグインとして動作する DLL (`ComfyUIPlugin.dll`)。
- 選択範囲の画像とサブ画像を ComfyUI サーバーへ送信し、生成結果をレイヤーへ描画する。
- `curl` 経由で ComfyUI REST API（`/prompt` / `/history/{id}` / `/view` / `/upload/image`）を呼び出す。
- 設定値は同梱の `ComfyUIPlugin.ini` から読み込み、ユーザー UI（設定コンボ・サブ画像コンボ・プロンプト入力）へ反映する。

## 2. 主なコンポーネント
- `ComfyUIPlugin.cpp`：フィルタープラグイン本体。初期化、設定読み込み、HTTP 通信、画像ファイル変換、一時ファイル管理、描画処理を実装。
- `ComfyUIPlugin.h`：デバッグログ用 API 宣言。
- `FilterPlugIn.cpp / .h`：Clip Studio Plug-in SDK のラッパー実装。オフスクリーンバッファや UI プロパティを扱うユーティリティを提供。
- `bmp_to_png.py` / `png_to_bmp.py`：Pillow を利用した画像形式変換。各 `.bat` から呼び出される。
- `ComfyUIPlugin.ini`：接続先サーバー、API キー、リトライ回数、プリセット設定（テンプレート JSON やプロンプト）を保持。

## 3. 初期化シーケンス
1. `DllMain`（`ComfyUIPlugin.cpp`）で DLL のベースパスを取得し、デバッグログ (`debuglog.txt`) と一時 JSON パスを初期化。
2. `InitializeModule` で Clip Studio 側へモジュール ID を登録し、`FilterInfo` を確保。
3. `InitializeFilter` で `ComfyUIPlugin.ini` から共通設定とプリセットを読み込み。
   - `COMMON` セクションの `server_address`／`api_key`／`getimage_retry_max_count`／`getimage_retry_wait_seconds` をグローバル変数へ格納。
   - `COMMON` 以外のセクション名を設定プリセットとして `g_Settings` に格納。
   - `SubImage` フォルダ直下の `.png` ファイル一覧を `g_SubImages` に格納。
   - プロパティ UI にカテゴリ「ComfyUI API」、フィルター名「Generate」を登録し、プレビュー不可・RGBAlpha 対応を指定。

## 4. ランタイム動作
1. `RunFilter` が呼び出されると、現在の選択範囲を `FilterPlugIn::ImageBuffer` に転送（`Transfer` ユーティリティを使用）。
2. 選択範囲を `temp_img_req.bmp` として保存し、`bmp_to_png.bat` → `bmp_to_png.py` で PNG へ変換。
3. タイムスタンプ付きのファイル名 `temp_img_req_yyyyMMddHHmmss.png`／`temp_subimg_req_yyyyMMddHHmmss.png` を生成し、ComfyUI の `/upload/image` に `curl` で POST。
4. プリセットで指定したテンプレート JSON（例：`template_api_google_gemini_image.json`）を読み込み、以下のマーカーを置換。
   - `###input1###` → プロンプト (`Prompt` 入力欄)
   - `###input2###` → ネガティブプロンプト (`Negative Prompt` 入力欄)
   - `temp_img_req_yyyyMMddhhmmss.png` → アップロード済み入力画像ファイル名
   - `temp_subimg_req_yyyyMMddhhmmss.png` → アップロード済みサブ画像ファイル名
5. 生成した JSON を `temp_post.json` に書き出し、`/prompt` へ POST。レスポンスから `prompt_id` を抽出。
6. `prompt_id` を用いて `/history/{prompt_id}` を最大 `getimage_retry_max_count` 回までリトライしつつポーリング。成功すると `CCPImage_*.png` 形式の出力ファイル名と `type` / `subfolder` を取得。
7. `/view` API で生成画像を `temp_img_res.png` として保存し、`png_to_bmp.bat` → `png_to_bmp.py` で BMP に変換。
8. `FilterPlugIn::Transfer` を使って `temp_img_res.bmp` の内容を選択範囲へ描画。選択範囲がある場合はマスクを尊重して出力。
9. ループは Clip Studio の `Run::Process` が `Restart` を返さない限り終了。

## 5. プロパティ UI
- 項目は `FilterPlugIn::Property` で生成（`ComfyUIPlugin.cpp` 内 `InitProperty`）。
  - `Setting`（コンボ）：`ComfyUIPlugin.ini` の `COMMON` 以外のセクションが列挙される。
  - `SubImage`（コンボ）：`SubImage` フォルダの `.png` リスト。
  - `Prompt`（文字列、800 文字まで）
  - `Negative Prompt`（文字列、800 文字まで）
- `Setting` を変更すると該当セクションの `template_workflow_filename`／`prompt`／`negative_prompt` がロードされ、UI と内部パラメーターへ反映。
- `SubImage` はユーザー選択を `g_params.input_subimage_filename` に保持し、リクエスト時にサブ画像として送信。

## 6. 設定ファイル仕様 (`ComfyUIPlugin.ini`)
```ini
[COMMON]
server_address = "127.0.0.1:8188"
api_key = "comfyui-xxx"
getimage_retry_max_count = "30"
getimage_retry_wait_seconds = "3"

[PresetName]
template_workflow_filename = "template_xxx.json"
prompt = "..."
negative_prompt = "..."
```
- `server_address`：`host:port` 形式。空の場合はデフォルト `127.0.0.1:8188` を使用。
- `api_key`：任意。指定されると `/prompt` リクエストの `extra_data.api_key_comfy_org` として送信。
- `getimage_retry_max_count`：履歴ポーリングの最大試行回数。整数文字列で指定。
- `getimage_retry_wait_seconds`：履歴ポーリング間隔（秒）。整数文字列で指定。
- 各プリセットセクションは表示名として利用され、最低限 `template_workflow_filename` が必須。プロンプトは空でも可。
- `SubImage` フォルダ内のファイルは `ini` ではなく実際の PNG ファイルから一覧化される。

## 7. ファイル入出力と一時ファイル
- ログ：`debuglog.txt`（毎回アタッチ時に初期化）。
- ワークフロー送信用：`temp_post.json`、レスポンス確認用：`temp_prompt_res.json` / `temp_history_res.json`。
- アップロード画像：`temp_img_req.bmp` → `temp_img_req.png` → `temp_img_req_yyyyMMddHHmmss.png`（POST 後も残る）、サブ画像は `SubImage/<選択ファイル>` を POST。
- 生成結果：`temp_img_res.png` → `temp_img_res.bmp` → Clip Studio へ描画。
- `.bat` は `C:\sd\ComfyUI\ComfyUI_20250927\python_embeded` を前提に `python.exe` を実行。環境に合わせてパスを調整する必要あり。

## 8. 外部依存と前提条件
- Windows 環境（Win32 API、`MultiByteToWideChar`、`fopen_s`、`GetModuleFileNameA` を使用）。
- `curl` コマンドが PATH で利用可能であること。
- `bmp_to_png.py` / `png_to_bmp.py` が要求する Pillow (`PIL`) が組み込まれた Python 実行環境。
- Clip Studio Paint フィルタープラグイン SDK に準拠したホスト。`FilterPlugIn::Server` のサービス群（string/property/offscreen 等）を備えている必要がある。

## 9. リトライ・エラーハンドリング
- `/history` の取得は `CCPImage_` を含むレスポンスが返るまでリトライし、間隔は `getimage_retry_wait_seconds`（秒）。
- HTTP 通信の戻り値チェックは最小限で、`curl` の戻り値をログへ出すのみ。失敗時は空文字を返すので、ワークフロー失敗時には追加の検知が必要。
- 画像変換失敗時はログへ書き出し、`false` を返してフィルター処理を終了。
- 例外処理は主にファイル存在チェックとログ出力に限定されるため、リリース前に周辺スクリプト・バッチの存在確認が必須。

## 10. 制限事項・注意点
- 文字列処理は Shift-JIS（ANSI）→ UTF-16/UTF-8 変換を多用。テンプレート JSON やプロンプトは Shift-JIS で扱われる点に留意。
- `.bat` 内のパスは固定値。配布時はユーザー環境に合わせて修正手順を提供する必要がある。

## 11. リリース前チェック
1. `ComfyUIPlugin.ini` の `COMMON` セクションと各プリセットが最新仕様と一致しているか確認。
2. `SubImage` フォルダに想定の PNG サンプルが配置されているか確認。
3. `curl`、Python、Pillow が配布パッケージ内で動作することを検証。
4. テンプレート JSON 内に定義したマーカー（`temp_img_req_yyyyMMddhhmmss.png` / `temp_subimg_req_yyyyMMddhhmmss.png` / `###input1###` / `###input2###`）が正しいか確認。
5. ComfyUI サーバー（`server_address`）と API キーの既定値がテスト環境に合わせて設定されているか確認。

---

本仕様書は `ComfyUIPlugin.cpp` とその関連ファイル群（`ComfyUIPlugin.h`、`FilterPlugIn.*`、画像変換スクリプト、`ComfyUIPlugin.ini`）を対象とし、`SDPlugin.cpp` 系統は範囲外である。
