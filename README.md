
# ComfyUIPlugin - クリスタ用 画像生成プラグイン

クリスタのフィルターとして動作する画像生成プラグインです。ComfyUI経由です。


## 注意

このプラグインは開発中のものです。<br>

(fork元の開発者様が)問い合わせたところオープンソースでの開発が「SDKの使用許諾条件を満たさない」との回答を頂きましたので、公式のSDKを使用せずに開発しています。<br>
その為、プラグインとしての動作に対して保証やサポートをすることはできません。<br>
あくまで自己責任でのご使用をお願いします。<br>
<br>
また、本プラグインはComfyUIのAPIと連携が必須です。<br>
ComfyUIにアカウント登録してNanoBanana（有料）等を利用するか、GPUが必要なローカル生成を実行するかのいずれかが前提となります。<br>
ComfyUIでできない画像生成は、本プラグインを導入しても実行できないため、ご注意ください。<br>
あくまでもComfyUIと（多少）便利に連携するためのプラグインのため、 **本プラグイン自体に生成機能はない** ことはご認識ください。<br>
<br>

* ComfyUIはオープンソースソフトウェアです。
* 「Gemini」は、Google LLCの登録商標です。「Nano-Banana」もGoogleにて商標登録されている可能性があります。
* 「Seedream」は、中国のByteDanceによって開発された生成AIモデルです。商標登録されている可能性があります。
* 「Qwen」は、中国のアリババ社によって開発された生成AIモデルです。商標登録されている可能性があります。
* 「FLUX（※FLUX.1）」は、Black Forest Labsによって開発された生成AIモデルです。商標登録されている可能性があります。
* 生成された画像の利用については、それぞれの生成AIのモデルの規約に従ってください。
* 「CLIP STUDIO」は株式会社セルシスの商標または登録商標です。

## 使用方法

使用イメージは[Xのポスト](https://x.com/potechi_takusan/status/1976281630278029393)を参照してください。
<br>
[リリースページ](https://github.com/potechitakusan/ComfyUIPlugin/releases) から最新バージョンをダウンロードしてください。<br>
解凍すると中に「ComfyUIPlugin_install.bat」というバッチファイルがあるのでそれを実行してください。<br>
プラグイン用のフォルダにファイルがコピーされます。<br>
<br>
また、ComfyUI自体のインストール、および、特定のフォルダにテンプレートのjsonファイルの格納が必要です。<br>
後述のComfyUI関連セットアップも実施してください。<br>
<br>
フィルタ自体は、クリスタを起動するとメニューの「フィルター(I)」の所に「ComfyUI API(X)」という項目が増えてるので、そこからご使用ください。<br>

## ComfyUI関連セットアップ

### ComfyUIインストール

本プラグインは、CLIP STUDIO PAINT EX で動作するプラグインのプログラムから、ComfyUIのAPIを呼び出して画像生成し、生成結果をレイヤー画像から置き換えるフィルタです。<br>
このため **ComfyUIインストールは必須** です。<br>
また、ComfyUIを多少でも使い慣れていないと、フィルタで何をできるか理解が困難であると想定されます。<br>
このため、まずはComfyUIをある程度使ってみることを推奨します。<br>

インストールは以下のサイト（外部リンク）などを参照ください。<br>

[【Stable Diffusion】ComfyUIのインストールと使い方！Windows編](https://note.com/aiaicreate/n/n0bb6c5b8bfa2)<br>

起動したら、ブラウザのURL（通常は http://127.0.0.1:8188/ ）をメモしてください。<br>

また、インストール後のComfyUIの「input」フォルダに、以下の名前の画像（内容は任意）を入れてください。動作確認で利用します。<br>

temp_img_req_yyyyMMddhhmmss.png  ※ポーズ指定用の画像。3Dのポーズ画像など。<br>
temp_subimg_req_yyyyMMddhhmmss.png  ※キャラクター指定用の画像。自分で描いたキャラクターのAポーズの画像など。<br>
empty.png  ※サブ画像を送信しないテンプレートのプレースホルダー。リリースに同梱されている `input/empty.png` をコピーしてください。<br>
<br>

### ComfyUIの動作確認

examplesフォルダにある以下のファイルをComfyUIの画面にドラッグ＆ドロップして開き、動作することを確認してください。

NanoBananaサンプル：ccp_api_google_gemini_image.json<br>
NanoBanana 4inputsサンプル：ccp_api_google_gemini_image_4inputs.json<br>
SeeDream4サンプル：ccp_api_bytedance_seedream4.json<br>
QwenImageサンプル：ccp_qwen_image_edit_2509.json<br>
FluxKontextDevサンプル：ccp_flux_kontext_dev.json<br>

NanoBananaは有料のAPIを呼び出すため、設定 >> ユーザー の画面にてユーザー登録してください。
クレジットも登録しておきましょう。5$くらいあればしばらく遊べます。
また、APIキーが必要になりますので、[こちら](https://docs.comfy.org/interface/user#no-api-key%2C-apply-for-an-api-key-first)を参照してAPIキーを取得してください。（パスワードと同等のため、扱いには注意してください）

NanoBananaのプロンプトの例は以下です。ワークフロー内の「###input1###」を置き換えます。

    2枚目の画像のキャラクターを1枚目のポーズに変更（画像の体の模様は無視）してください。アングルや画角は1枚目を保ってください。

QwenImage、FluxKontextDevはそれぞれモデルのダウンロードが必要です。必要に応じてカスタムノードもインストールしてください。

QwenImageのプロンプトの例は以下です。ワークフロー内の「###input1###」を置き換えます。

    Keep Picture 2 character the same and change pose that Picture 1. White background.

FluxKontextDevのプロンプトの例は以下です。ワークフロー内の「###input1###」を置き換えます。

    line art.

それぞれ動作確認ができたら、画像のファイル名とプロンプトを元に戻してください。

ccp_api_google_gemini_image.json の戻し方
* 左の入力画像は「temp_img_req_yyyyMMddhhmmss.png」を指定する。
* 右の入力画像は「temp_subimg_req_yyyyMMddhhmmss.png」を指定する。
* プロンプトは「###input1###」を指定する。

ccp_api_google_gemini_image_4inputs.json の戻し方
* Picture 1 の入力画像は「temp_img_req_yyyyMMddhhmmss.png」を指定する。
* Picture 2 の入力画像は「temp_subimg_req_yyyyMMddhhmmss.png」を指定する。
* Picture 3 の入力画像は「temp_subimg2_req_yyyyMMddhhmmss.png」を指定する。
* Picture 4 の入力画像は「temp_subimg3_req_yyyyMMddhhmmss.png」を指定する。
* プロンプトは「###input1###」を指定する。

ccp_api_bytedance_seedream4.json の戻し方
* Picture 1 の入力画像は「temp_img_req_yyyyMMddhhmmss.png」を指定する。
* Picture 2 の入力画像は「temp_subimg_req_yyyyMMddhhmmss.png」を指定する。
* Picture 3 の入力画像は「temp_subimg2_req_yyyyMMddhhmmss.png」を指定する。
* Picture 4 の入力画像は「temp_subimg3_req_yyyyMMddhhmmss.png」を指定する。
* プロンプトは「###input1###」を指定する。

ccp_qwen_image_edit_2509.json の戻し方
* 左の入力画像は「temp_img_req_yyyyMMddhhmmss.png」を指定する。
* 右の入力画像は「temp_subimg_req_yyyyMMddhhmmss.png」を指定する。
* プロンプトは「###input1###」を指定する。
* ネガティブプロンプトは「###input2###」を指定する。

ccp_flux_kontext_dev.json の戻し方
* 左の入力画像は「temp_img_req_yyyyMMddhhmmss.png」を指定する。
* プロンプトは「###input1###」を指定する。

修正が終わったら、左上のメニュー の 設定 >> 開発者モードをON にしたのち、
左上のメニュー の 設定 >> File >> エクスポート（API） でjsonファイルをダウンロードします。
ダウンロード後、ファイル名の先頭の「ccp_」を「template_」に変更してから、以下のComfyUIPluginフォルダに格納します。
バージョンが「1.10.13」以降の場合のフォルダは以下です。

%USERPROFILE%\AppData\Roaming\CELSYSUserData\CELSYS\CLIPStudioModule\PlugIn\PAINT\ComfyUIPlugin

バージョンが「1.10.13」以前の古い場合のフォルダは以下です。

%USERPROFILE%\Documents\CELSYS\CLIPStudioModule\PlugIn\PAINT\ComfyUIPlugin

ファイルエクスプローラーのアドレスバーに入力すると移動できます。
以降、「ComfyUIPluginのフォルダ」と記載した場合はこのパスを開いてください。

### ComfyUIPluginの設定ファイル（UserSetting.ini）の修正

ComfyUIPluginのフォルダの `UserSetting.ini` を開き、`[COMMON]` セクションのコメントを外して以下を設定します。（行頭の `;` を削除してください）

    server_address = "http://127.0.0.1:8188"
    api_key = "comfyui-xxxx"

http://127.0.0.1:8188 の部分は、ComfyUIを起動して開いたブラウザのURLを入力してください。
comfyui-xxxx の部分は、先ほど入手したAPIキーを入力してください。

`ComfyUIPlugin.ini` は配布時の既定値が記載されており、アップデート時に上書きされます。個別の調整やプリセット追加は `UserSetting.ini` に記載してください。

### Pythonパスの設定

Pythonが環境変数のPATHに設定されていない場合、ComfyUIPluginのフォルダの png_to_bmp.bat と bmp_to_png.bat を開き、
先頭のPATHを設定してください。

    @rem set path=C:\ComfyUI\python_embeded;%path%

ComfyUIをポータブル版でインストールした場合は、@rem を削除して以下のように書き換えます。
python_embeded フォルダに python.exe があることを確認してください。

    set path=C:\【ComfyUIのインストールフォルダ】\python_embeded;%path%

ComfyUIをStabilityMatrixでインストールした場合は、@rem を削除して以下のように書き換えます。
Scripts フォルダに python.exe があることを確認してください。

    set path=C:\【StabilityMatrixのインストールフォルダ】\Packages\ComfyUI\venv\Scripts;%path%

Pythonは普通にインストールしていたらpathはたぶん不要です。
PIL（pillow）をプラグインからpythonで利用しています。

### キャラクター参照用の画像の保存

ComfyUIPluginのフォルダの下に SubImage というフォルダを作成します。
その中に、キャラクターの特徴がわかるpngファイルを保存してください。
※NanoBananaに渡すことになるため、ご自身で作成した画像であることを推奨します。

## 利用手順

CLIP STUDIO PAINT EX を開きます。
キャンバスサイズは長辺1280px程度を推奨します。

### Nano-Banana呼び出し

3D人形などを利用し、キャラクターのポーズを決めます。
そのレイヤーが表示されている状態から、メニュー >> レイヤー >> 表示レイヤーのコピーを統合 を指定してラスターレイヤーを作成します。
フィルタ >> ComfyUI API >> Generate にてフィルタ画面を開き、Setting: が「Google Gemini Image(Nano-Banana)」であることを確認してOKを押下します。
画像生成が成功した場合、レイヤーが生成後の画像に置き換わります。

### Qwen Image Edit 2509呼び出し

NanoBananaの場合と同じ操作で、Setting: にて「Qwen Image Edit 2509」を指定してOKを押下します。
画像生成が成功した場合、レイヤーが生成後の画像に置き換わります。
3D人形は、顔や体がのっぺりした人形だとポーズとして認識されない可能性があるため、他のキャラクターや実写画像でも試してください。
ComfyUIのinputフォルダに真っ黒な画像が出力されている場合、ポーズ認識に失敗しています。成功の場合はカラフルな線の画像が出力されます。

### FLUX Kontext Dev呼び出し

NanoBananaの場合と同じ操作で、Setting: にて「FLUX Kontext Dev」を指定してOKを押下します。
画像生成が成功した場合、レイヤーが生成後の画像に置き換わります。
このワークフローは線画変換です。

### 生成AIへのINPUT／OUTPUTについて

本プラグインは、ComfyUIのワークフローに、画像を1～2個と、生成のためのプロンプトを設定して生成を実行します。
生成された画像をComfyUIから取り出し、フィルタ実行時のレイヤーに上書きします。

画像の１個目は、CLIP STUDIO PAINT EXのキャンバスの選択中のレイヤーです。
キャンバスを矩形選択した場合、選択した範囲のみ生成AIに渡します。
キャンバスが大きい場合、全体を渡すと画像生成に大きく時間がかかったり、クレジットの消費が増えるため、適宜選択して利用してください。

画像の２～４個目は、ComfyUIPluginフォルダの SubImage フォルダに格納したPNGファイルです。
フィルタ画面の SubImage(Picture 2/3/4) で切り替えます。`(no image)` を選ぶと該当のサブ画像を送信せずにワークフローを実行します（テンプレート内では `empty.png` を参照します）。
`Google Gemini Image(Nano-Banana)` や `Qwen Image Edit 2509` では主に Picture 2 を使用します。
`Google Gemini Image(Nano-Banana) 4inputs`・`SeeDream4 4inputs` では Picture 2～4 を活用してください。
`FLUX Kontext Dev` のワークフローでは、画像の１個目のみ渡します。

プロンプトは、フィルタのPromptの文章を渡します。
尚、Setting: を切り替えた場合、画面のプロンプト表示は変わりませんが、内部では `ComfyUIPlugin.ini` の既定値に `UserSetting.ini` の上書きを適用した内容が送信されます。
（CLIP STUDIO PAINT EXのフィルタの制限で、プログラム側から表示の更新ができません）
プロンプトを修正する場合は、画面上で文章を修正するか、`UserSetting.ini` の該当セクションで `prompt`／`negative_prompt` を設定してください。
（UserSetting.iniの修正後は、CLIP STUDIO PAINT EXを再起動してください）

## FAQ

### 【初級編】
・まだComfyUIを使ったことがありません。どれが一番手っ取り早いですか？

インストールはポータブル版が楽です。
GPUのメモリが少ない場合は run_cpu.bat で起動し、NanoBanana（Googleのオンラインサービス）を試してみてください。
ローカル環境で生成AIを動作させる場合（QwenImageやFLUX.1の場合）、NVIDIAのGPUが実質的に必須になります。

・ComfyUIのユーザー登録や、クレジットの追加は必須ですか？

NanoBananaなど有料のAPIを利用する場合のみ必須です。ローカル環境での生成では不要です。

・ComfyUIのユーザー登録せずに使える範囲を教えてください。

本プラグインのサンプルでは、QwenImageEdit、FLUX Kontext Devがローカル環境での生成です。

・GPUのメモリの目安は？ どのメーカーのものがあればよいですか？

ローカル環境で生成AIを動作させる場合（QwenImageやFLUX.1の場合）、NVIDIAのGPUが実質的に必須になります。
本プラグインの開発者は、16GBのGPU（GeForce RTX 4070 Ti SUPER）を利用しています。

・ComfyUIのユーザー登録もせず、GPUもなしで使える方法はありますか？

生成AIは、なんだかんだお金がかるんですよね。

・MacOSには対応していますか？

Windowsでのみ動作確認を行っております。Macは現状動作しません。（2025年10月時点）

・フィルタ実行したらなんか画面がチカチカします。あと動作中に画面クリックすると「応答を待ちますか？」みたいな画面が出ます。

手抜き実装のため、今動いているかを出すためにサブ画面が出たり消えたりします。
初期設定では待ち時間を最大90秒で設定しています。Ｘでも見ながら気長に待ちましょう。

・CLIP STUDIOで画像編集する際のキャンバスはどれくらいの大きさに対応していますか？

目安は長辺1280pxぐらいだと思います。
生成AIに渡す画像が大きいと、クレジットの消費が増えたり生成時間が長くなったりします。
キャンバス自体が大きい場合は、ラスターレイヤーを矩形選択してからフィルタ実行すれば、生成AIには選択した範囲のみ渡せます。

・SubImageフォルダの画像は必須ですか？

本プラグインのサンプルでは、NanoBanana、QwenImageEdit2509ではPicture 2が必須です。`Google Gemini Image(Nano-Banana) 4inputs` や `SeeDream4 4inputs` では Picture 2～4 を使います。
一部のワークフローでは `(no image)` を選んでスキップすることもできます。

・SubImageフォルダに画像を置いてもフィルタの画面に表示されません。

画像はPNG形式（拡張子が.png）のみ対応しています。
格納後は、CLIP STUDIO PAINT EX を一度閉じてからもう一度開いてください。

・画像生成は成功した気がするのですが、思ったような生成結果になりません。

画像生成の内容は生成AIのモデル任せのため、同じ入力内容でも結果が違うことがよくあったりします。
いろいろ試してみましょう。

・画像生成は成功した気がするのですが、CLIP STUDIOの画面に表示されません。あと、さっき生成した結果に戻したいです。

生成結果は、ラスターレイヤーの不透明な部分を上書きします。
透明レイヤーの場合は生成されても上書きされないため、白色でもよいので全体に色を塗ってください。

画像の生成結果は、ComfyUIのoutputフォルダの「CLIPSTUDIO_ComfyUI_PLUGIN」に格納されているのでそちらを探してみてください。
生成に90秒以上かかった場合、CLIP STUDIOの画面に出力されません。
どうしても生成に90秒以上かかる場合は、UserSetting.ini の getimage_retry_max_count、getimage_retry_wait_seconds を増やしてみてください。
UserSetting.ini の修正後は、CLIP STUDIO PAINT EX を一度閉じてからもう一度開いてください。

・プロンプトの動きがいまいちおかしい気がします。

CLIP STUDIO PAINT EX の仕様で、Setting: を変えても画面表示はPromptが更新されません。
内部では `ComfyUIPlugin.ini` の既定値、または、 `UserSetting.ini` の上書きを適用した prompt の値が設定されています。
どのような内容がComfyUIに渡されているかは、ComfyUIPluginのフォルダの temp_xxx.xxx ファイルをのぞいてみてください。

・ComfyUIのinputフォルダの画像増えまくってない？

生成の都度で、inputフォルダにファイルが増えます。適宜消してください。

### 【エラー編】
・動きません。何を調べればよいですか？

ComfyUIPluginのフォルダの「debuglog.txt」「debuglog_py.txt」を見てみてください。何かヒントがあるかもしれません。

・「debuglog_py.txt」に「python.exe が存在しません」のようなエラーが出ます。または、「debuglog_py.txt」に変換完了とかが出ません。

「png_to_bmp.bat」及び「bmp_to_png.bat」にて、環境変数pathにpython.exeが格納されているフォルダが指定されているか確認してください。


### 【中級編】
・ワークフローは自分で修正しても大丈夫？

問題ありません。
以下のルールは守ってください。

- キャンバスの画像をINPUTにする場合は、ファイル名は「temp_img_req_yyyyMMddhhmmss.png」にする。
- SubImageの画像をINPUTにする場合は、Picture 2 を「temp_subimg_req_yyyyMMddhhmmss.png」、Picture 3 を「temp_subimg2_req_yyyyMMddhhmmss.png」、Picture 4 を「temp_subimg3_req_yyyyMMddhhmmss.png」にする。
- Promptの文章を渡す場合、###input1###にする。
- Negative Promptの文章を渡す場合、###input2###にする。
- 出力画像のプレフィックスは「CLIPSTUDIO_ComfyUI_PLUGIN\CCPImage」にする。生成されるファイル名が「CCPImage_xxxx.png」になるようにする。
- 他の画像をインプットとする場合、名前が「CCPImage」にならないようにする。
- 上記を満たしたjsonファイルを「エクスポート（API）」で保存し、ComfyUIPluginのフォルダに格納する。    

・自分で作ったワークフローは使える？

利用できます。
ワークフローは前述のルールを守ってください。
ComfyUIPluginのフォルダの UserSetting.ini に以下のようにセクションを追加してください。
「エクスポート（API）」したファイルはComfyUIPluginのフォルダに格納してください。

    [Flux Kontext dev]
        template_workflow_filename = "エクスポート（API）したjsonのファイル名"
        prompt = "プロンプトの初期設定"
        negative_prompt = ""

・inpaint的な使い方できる？

生成AIには、画面全体、もしくは、選択した範囲のみ渡されます。
マスクを渡す方法はありませんが、マスクしたい個所を赤く塗ってプロンプトで指示すればもしかしたら動くかもしれません。

### 【上級編】
・実装が気に入りません。ビルドしなおすには？

ごめんて。<br>
ビルドは「build.bat」を参照してください。ComfyUIPlugin.sln がビルドできればcpmファイルが更新されるので、ComfyUIPluginのフォルダに上書きしてください。

・なんでPython実行してるの？ C++だけで作った方が良くない？

PNGとBMPの相互変換が必要なのですが、C++でちょうどよいものが見つからなかったんですよね。

・Macで動かすように直せる？

bmpとpngの変換にpythonを使っており、その実行に.batを経由しています。
このため、シェルで動かせるようにC++ソース直す必要があります。
あとはファイルパス関連の実装を直したり、Windowsのみのライブラリを直せれば動くような気がします。

## 補足情報

### ComfyUIPlugin.ini / UserSetting.ini の設定値について

- `ComfyUIPlugin.ini` は配布時の既定値が記載されています。アップデート時に上書きされるため、直接修正しないでください。
- `UserSetting.ini` に同じキーを記載すると、既定値を上書きできます。セクションの追加もこちらで行ってください。
- server_address ： ComfyUIのAPIのcurl実行で利用
- api_key ： NanoBananaなど有料のAPIを呼び出す場合に必要なログイン用
- getimage_retry_max_count ： 画像が生成されるまでポーリングする際のリトライ回数
- getimage_retry_wait_seconds ： 画像が生成されるまでポーリングする際のリトライ間隔（秒）

### テンプレートのマーカーについて

ComfyUIのAPI呼び出し時、テンプレート内の文字列を以下のように置き換えてPOSTします。

- temp_img_req_yyyyMMddhhmmss.png ： キャンバスの画像のファイル名
- temp_subimg_req_yyyyMMddhhmmss.png ： SubImageの画像のファイル名
- ###input1### ： プロンプト
- ###input2### ： ネガティブプロンプト

また、生成結果のhistoryの取得結果から、「CCPImage」という文字を探してファイルダウンロードするため、生成結果以外に「CCPImage」という文字を含めると生成結果をレイヤーに反映できません。
