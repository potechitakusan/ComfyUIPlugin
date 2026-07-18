@echo off
setlocal EnableExtensions DisableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "APP_NAME=ComfyUIPlugin"

echo.
echo CLIP STUDIO PAINT version:
echo   1. Version 1.10.13 or later
echo   2. Older version
choice /c 12 /n /m "Select 1 or 2"
if errorlevel 2 (
    set "APP_DIR=%USERPROFILE%\Documents\CELSYS\CLIPStudioModule\PlugIn\PAINT\%APP_NAME%"
) else (
    set "APP_DIR=%APPDATA%\CELSYSUserData\CELSYS\CLIPStudioModule\PlugIn\PAINT\%APP_NAME%"
)

echo.
echo Installing to: %APP_DIR%
if not exist "%APP_DIR%\" mkdir "%APP_DIR%"
if not exist "%APP_DIR%\SubImage\" mkdir "%APP_DIR%\SubImage"

for %%F in (
    "ComfyUIPlugin.cpm"
    "ComfyUINanoBananaPlugin.cpm"
    "ComfyUIPlugin.ini"
    "bmp_to_png.bat"
    "bmp_to_png.py"
    "png_to_bmp.bat"
    "png_to_bmp.py"
) do (
    if not exist "%SCRIPT_DIR%%%~F" (
        echo Error: Required file was not found: %%~F
        goto :END
    )
    copy /Y "%SCRIPT_DIR%%%~F" "%APP_DIR%\" >nul
)

if exist "%SCRIPT_DIR%SubImage\*" xcopy /E /I /Y "%SCRIPT_DIR%SubImage" "%APP_DIR%\SubImage" >nul
if exist "%APP_DIR%\UserSetting.ini" (
    echo Existing UserSetting.ini was kept.
) else (
    copy /Y "%SCRIPT_DIR%UserSetting.ini" "%APP_DIR%\UserSetting.ini" >nul
)

echo Plugin installation completed.

:END
pause
endlocal