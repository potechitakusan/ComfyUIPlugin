@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 932 >nul

set "SCRIPT_DIR=%~dp0"
set "EASY_INSTALL_URL=https://github.com/Tavris1/ComfyUI-Easy-Install/releases/latest/download/ComfyUI-Easy-Install.zip"
set "EXPORTER_REPOSITORY=https://github.com/potechitakusan/ComfyUI_CCP_API_Exporter.git"

echo.
echo ============================================================
echo ComfyUIPlugin 統合インストーラー
echo ============================================================
echo.
echo ComfyUI の準備方法を選択してください。
echo   1. ComfyUI-Easy-Install を使って ComfyUI をインストールする（未導入の場合に推奨）
echo   2. インストール済みの ComfyUI フォルダーを指定する
echo   3. ComfyUI に関する処理を行わない
echo.

choice /c 123 /n /m "番号を選択してください"
if errorlevel 3 goto :INSTALL_PLUGIN
if errorlevel 2 goto :ASK_COMFYUI_PATH
if errorlevel 1 goto :EASY_INSTALL

:EASY_INSTALL
for %%I in ("%SCRIPT_DIR%..") do set "EASY_INSTALL_PARENT=%%~fI"
if not exist "%EASY_INSTALL_PARENT%\" set "EASY_INSTALL_PARENT=%CD%"
set "EASY_INSTALL_ZIP=%EASY_INSTALL_PARENT%\ComfyUI-Easy-Install.zip"

echo.
echo ComfyUI-Easy-Install をダウンロードします。
echo 保存先: %EASY_INSTALL_ZIP%
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri '%EASY_INSTALL_URL%' -OutFile '%EASY_INSTALL_ZIP%'"
if errorlevel 1 goto :POWERSHELL_ERROR

echo ZIP を展開します。
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -LiteralPath '%EASY_INSTALL_ZIP%' -DestinationPath '%EASY_INSTALL_PARENT%' -Force"
if errorlevel 1 goto :POWERSHELL_ERROR

set "EASY_INSTALLER="
for /r "%EASY_INSTALL_PARENT%" %%F in (ComfyUI-Easy-Install.bat) do if not defined EASY_INSTALLER set "EASY_INSTALLER=%%~fF"
if not defined EASY_INSTALLER (
    echo.
    echo エラー: 展開後の ComfyUI-Easy-Install.bat が見つかりませんでした。
    echo 展開先を確認してください: %EASY_INSTALL_PARENT%
    goto :END
)

echo.
echo ComfyUI-Easy-Install を起動します。
echo 完了後、この画面へ戻ります。
call "%EASY_INSTALLER%"
echo.
echo ComfyUI-Easy-Install の完了後に、インストール先の ComfyUI フォルダーを入力してください。
goto :ASK_COMFYUI_PATH

:ASK_COMFYUI_PATH
set "COMFYUI_DIR="
set /p "COMFYUI_DIR=ComfyUI フォルダーのパス: "
set "COMFYUI_DIR=%COMFYUI_DIR:"=%"
if not defined COMFYUI_DIR (
    echo フォルダーのパスを入力してください。
    goto :ASK_COMFYUI_PATH
)
if not exist "%COMFYUI_DIR%\" (
    echo 指定されたフォルダーが見つかりません: %COMFYUI_DIR%
    goto :ASK_COMFYUI_PATH
)
goto :COPY_COMFYUI_FILES

:COPY_COMFYUI_FILES
echo.
echo ComfyUI へサンプルファイルをコピーします。
set "COMFYUI_INPUT_DIR=%COMFYUI_DIR%\input"
set "COMFYUI_WORKFLOW_DIR=%COMFYUI_DIR%\user\default\workflows"
if not exist "%COMFYUI_INPUT_DIR%\" mkdir "%COMFYUI_INPUT_DIR%"
if not exist "%COMFYUI_WORKFLOW_DIR%\" mkdir "%COMFYUI_WORKFLOW_DIR%"

if exist "%SCRIPT_DIR%input\*.png" (
    copy /Y "%SCRIPT_DIR%input\*.png" "%COMFYUI_INPUT_DIR%\" >nul
    echo input の PNG をコピーしました。
) else (
    echo input 内にコピー対象の PNG はありません。
)
if exist "%SCRIPT_DIR%examples\*.json" (
    copy /Y "%SCRIPT_DIR%examples\*.json" "%COMFYUI_WORKFLOW_DIR%\" >nul
    echo examples の JSON をコピーしました。
) else (
    echo examples 内にコピー対象の JSON はありません。
)

echo.
set "INSTALL_EXPORTER=Y"
set /p "INSTALL_EXPORTER=ComfyUI CCP API Exporter をインストールしますか？ [Y/n]: "
if /i "%INSTALL_EXPORTER%"=="N" goto :INSTALL_PLUGIN

set "EXPORTER_DIR=%COMFYUI_DIR%\custom_nodes\ComfyUI_CCP_API_Exporter"
if exist "%EXPORTER_DIR%\" (
    echo CCP API Exporter は既に配置されています: %EXPORTER_DIR%
    goto :INSTALL_PLUGIN
)
if not exist "%COMFYUI_DIR%\custom_nodes\" mkdir "%COMFYUI_DIR%\custom_nodes"
git --version >nul 2>&1
if errorlevel 1 (
    echo.
    echo 警告: git が見つからないため、CCP API Exporter を導入できませんでした。
    echo Git をインストール後、次を実行してください。
    echo git clone %EXPORTER_REPOSITORY% "%EXPORTER_DIR%"
    goto :INSTALL_PLUGIN
)

echo CCP API Exporter を clone します。
git clone "%EXPORTER_REPOSITORY%" "%EXPORTER_DIR%"
if errorlevel 1 (
    echo 警告: CCP API Exporter の clone に失敗しました。ネットワークと Git の設定を確認してください。
) else (
    echo CCP API Exporter をインストールしました。
)
goto :INSTALL_PLUGIN

:POWERSHELL_ERROR
echo.
echo エラー: PowerShell によるダウンロードまたは ZIP 展開に失敗しました。
echo 実行ポリシーが原因の場合は、PowerShell を開いて次を実行してから再試行してください。
echo Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
goto :END

:INSTALL_PLUGIN
echo.
echo CLIP STUDIO PAINT 用プラグインをインストールします。
if not exist "%SCRIPT_DIR%ComfyUIPlugin_CopyCPM.bat" (
    echo エラー: ComfyUIPlugin_CopyCPM.bat が見つかりません。
    goto :END
)
call "%SCRIPT_DIR%ComfyUIPlugin_CopyCPM.bat"

:END
echo.
echo 統合インストーラーを終了します。
pause
endlocal
