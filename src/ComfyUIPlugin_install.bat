@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "EASY_INSTALL_URL=https://github.com/Tavris1/ComfyUI-Easy-Install/releases/latest/download/ComfyUI-Easy-Install.zip"
set "EXPORTER_REPOSITORY=https://github.com/potechitakusan/ComfyUI_CCP_API_Exporter.git"

echo.
echo ============================================================
echo ComfyUIPlugin installer
echo ============================================================
echo.
echo Select how to prepare ComfyUI:
echo   1. Install ComfyUI with ComfyUI-Easy-Install ^(recommended if not installed^)
echo   2. Specify an existing ComfyUI folder
echo   3. Skip ComfyUI setup

echo.
choice /c 123 /n /m "Select 1, 2, or 3"
if errorlevel 3 goto :INSTALL_PLUGIN
if errorlevel 2 goto :ASK_COMFYUI_PATH
if errorlevel 1 goto :EASY_INSTALL

:EASY_INSTALL
for %%I in ("%SCRIPT_DIR%..") do set "EASY_INSTALL_PARENT=%%~fI"
if not exist "%EASY_INSTALL_PARENT%\" set "EASY_INSTALL_PARENT=%CD%"
set "EASY_INSTALL_ZIP=%EASY_INSTALL_PARENT%\ComfyUI-Easy-Install.zip"

echo.
echo Downloading ComfyUI-Easy-Install...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri '%EASY_INSTALL_URL%' -OutFile '%EASY_INSTALL_ZIP%'"
if errorlevel 1 goto :POWERSHELL_ERROR

echo Extracting ComfyUI-Easy-Install...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -LiteralPath '%EASY_INSTALL_ZIP%' -DestinationPath '%EASY_INSTALL_PARENT%' -Force"
if errorlevel 1 goto :POWERSHELL_ERROR

set "EASY_INSTALLER="
for /r "%EASY_INSTALL_PARENT%" %%F in (ComfyUI-Easy-Install.bat) do if not defined EASY_INSTALLER set "EASY_INSTALLER=%%~fF"
if not defined EASY_INSTALLER (
    echo Error: ComfyUI-Easy-Install.bat was not found after extraction.
    goto :END
)

echo Starting ComfyUI-Easy-Install. Return to this window when it finishes.
call "%EASY_INSTALLER%"
goto :ASK_COMFYUI_PATH

:ASK_COMFYUI_PATH
set "COMFYUI_DIR="
set /p "COMFYUI_DIR=ComfyUI folder path: "
set "COMFYUI_DIR=%COMFYUI_DIR:"=%"
if not defined COMFYUI_DIR (
    echo Enter a folder path.
    goto :ASK_COMFYUI_PATH
)
if not exist "%COMFYUI_DIR%\" (
    echo Folder not found: %COMFYUI_DIR%
    goto :ASK_COMFYUI_PATH
)
goto :COPY_COMFYUI_FILES

:COPY_COMFYUI_FILES
set "COMFYUI_INPUT_DIR=%COMFYUI_DIR%\input"
set "COMFYUI_WORKFLOW_DIR=%COMFYUI_DIR%\user\default\workflows"
if not exist "%COMFYUI_INPUT_DIR%\" mkdir "%COMFYUI_INPUT_DIR%"
if not exist "%COMFYUI_WORKFLOW_DIR%\" mkdir "%COMFYUI_WORKFLOW_DIR%"

if exist "%SCRIPT_DIR%input\*.png" (
    copy /Y "%SCRIPT_DIR%input\*.png" "%COMFYUI_INPUT_DIR%\" >nul
    echo Copied input PNG files.
) else (
    echo No input PNG files were found.
)
if exist "%SCRIPT_DIR%examples\*.json" (
    copy /Y "%SCRIPT_DIR%examples\*.json" "%COMFYUI_WORKFLOW_DIR%\" >nul
    echo Copied example workflow JSON files.
) else (
    echo No example workflow JSON files were found.
)

set "INSTALL_EXPORTER=Y"
set /p "INSTALL_EXPORTER=Install ComfyUI CCP API Exporter? [Y/n]: "
if /i "%INSTALL_EXPORTER%"=="N" goto :INSTALL_PLUGIN

set "EXPORTER_DIR=%COMFYUI_DIR%\custom_nodes\ComfyUI_CCP_API_Exporter"
if exist "%EXPORTER_DIR%\" (
    echo CCP API Exporter already exists: %EXPORTER_DIR%
    git --version >nul 2>&1
    if errorlevel 1 (
        echo Warning: Git was not found, so CCP API Exporter was not updated.
    ) else (
        echo Updating CCP API Exporter...
        git -C "%EXPORTER_DIR%" pull --ff-only
        if errorlevel 1 echo Warning: CCP API Exporter update failed. Check that the folder is a Git repository.
    )
    goto :INSTALL_PLUGIN
)
if not exist "%COMFYUI_DIR%\custom_nodes\" mkdir "%COMFYUI_DIR%\custom_nodes"
git --version >nul 2>&1
if errorlevel 1 (
    echo Git was not found. Install Git and then run:
    echo git clone %EXPORTER_REPOSITORY% "%EXPORTER_DIR%"
    goto :INSTALL_PLUGIN
)

echo Cloning CCP API Exporter...
git clone "%EXPORTER_REPOSITORY%" "%EXPORTER_DIR%"
if errorlevel 1 (
    echo Warning: CCP API Exporter clone failed.
) else (
    echo CCP API Exporter installed.
)
goto :INSTALL_PLUGIN

:POWERSHELL_ERROR
echo.
echo PowerShell download or extraction failed.
echo If execution policy is the cause, run this in PowerShell and retry:
echo Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
goto :END

:INSTALL_PLUGIN
echo.
echo Installing the CLIP STUDIO PAINT plugin...
if not exist "%SCRIPT_DIR%ComfyUIPlugin_CopyCPM.bat" (
    echo Error: ComfyUIPlugin_CopyCPM.bat was not found.
    goto :END
)
call "%SCRIPT_DIR%ComfyUIPlugin_CopyCPM.bat"

:END
echo.
echo Installer finished.
pause
endlocal