@echo off
chcp 932

set APP_NAME=ComfyUIPlugin

:RESTART
echo ------------------------------------------------------------
echo プラグインのインストール先は、CLIP Studioのバージョンによって違います。
echo お使いのバージョンが「1.10.13」以降であれば「1」を、それより古い場合は「2」を入力してください。
echo ------------------------------------------------------------
set /p VER="バージョン確認: "
IF %VER% == 1 (
	set APP_DIR=%USERPROFILE%\AppData\Roaming\CELSYSUserData\CELSYS\CLIPStudioModule\PlugIn\PAINT\%APP_NAME%\
) ELSE IF %VER% == 2 (
	set APP_DIR=%USERPROFILE%\Documents\CELSYS\CLIPStudioModule\PlugIn\PAINT\%APP_NAME%\
) ELSE (
	GOTO RESTART
)

echo ------------------------------------------------------------
echo プラグインをプラグイン用フォルダにインストールします。
echo アンインストールする時はフォルダごと削除してください。
echo インストール先：  %APP_DIR%
echo ------------------------------------------------------------
pause

echo フォルダ作成...
mkdir %APP_DIR%

echo プラグインをコピー...
copy /Y ComfyUIPlugin.cpm %APP_DIR%

echo 設定ファイルをコピー...
copy /Y ComfyUIPlugin.ini %APP_DIR%

echo Pythonスクリプトをコピー...
copy /Y bmp_to_png.bat %APP_DIR%
copy /Y bmp_to_png.py %APP_DIR%
copy /Y png_to_bmp.bat %APP_DIR%
copy /Y png_to_bmp.py %APP_DIR%
mkdir %APP_DIR%\SubImage
copy /Y SubImage %APP_DIR%\SubImage

echo ------------------------------------------------------------
echo インストール完了
echo ------------------------------------------------------------
pause
