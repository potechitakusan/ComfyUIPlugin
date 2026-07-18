@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 932 >nul

rem Usage: release.bat [vX.Y.Z] [destination]
rem Creates the Windows release folder containing only files required to install
rem and use the plugin. Run src\build.bat beforehand.
set "ROOT=%~dp0"
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "VERSION=%~1"
if not defined VERSION set "VERSION=v0.5.0"
set "DEFAULT_DESTINATION=%ROOT%\..\github\release\ComfyUIPlugin_%VERSION%"
set "DESTINATION=%~2"
if not defined DESTINATION set "DESTINATION=%DEFAULT_DESTINATION%"

set "CPM_SOURCE_DIR=%CPM_SOURCE_DIR%"
if not defined CPM_SOURCE_DIR set "CPM_SOURCE_DIR=%APPDATA%\CELSYSUserData\CELSYS\CLIPStudioModule\PlugIn\PAINT\ComfyUIPlugin"

if not exist "%CPM_SOURCE_DIR%\ComfyUIPlugin.cpm" (
    echo Error: ComfyUIPlugin.cpm was not found.
    echo Expected: %CPM_SOURCE_DIR%\ComfyUIPlugin.cpm
    echo Run src\build.bat first, or set CPM_SOURCE_DIR to the folder containing the CPM files.
    exit /b 1
)
if not exist "%CPM_SOURCE_DIR%\ComfyUINanoBananaPlugin.cpm" (
    echo Error: ComfyUINanoBananaPlugin.cpm was not found.
    echo Expected: %CPM_SOURCE_DIR%\ComfyUINanoBananaPlugin.cpm
    exit /b 1
)

if exist "%DESTINATION%\" (
    echo.
    echo The existing release folder will be replaced:
    echo   %DESTINATION%
    choice /c YN /n /m "Continue"
    if errorlevel 2 exit /b 1
    rmdir /s /q "%DESTINATION%"
    if exist "%DESTINATION%\" (
        echo Error: Could not remove the existing release folder.
        exit /b 1
    )
)

mkdir "%DESTINATION%\SubImage" || goto :ERROR
mkdir "%DESTINATION%\input" || goto :ERROR
mkdir "%DESTINATION%\examples" || goto :ERROR

for %%F in (
    "ComfyUIPlugin.ini"
    "UserSetting.ini"
    "bmp_to_png.bat"
    "bmp_to_png.py"
    "png_to_bmp.bat"
    "png_to_bmp.py"
    "ComfyUIPlugin_install.bat"
    "ComfyUIPlugin_CopyCPM.bat"
) do (
    if not exist "%ROOT%\src\%%~F" (
        echo Error: Required source file is missing: src\%%~F
        goto :ERROR
    )
    copy /y "%ROOT%\src\%%~F" "%DESTINATION%\" >nul || goto :ERROR
)

copy /y "%CPM_SOURCE_DIR%\ComfyUIPlugin.cpm" "%DESTINATION%\" >nul || goto :ERROR
copy /y "%CPM_SOURCE_DIR%\ComfyUINanoBananaPlugin.cpm" "%DESTINATION%\" >nul || goto :ERROR

robocopy "%ROOT%\input" "%DESTINATION%\input" *.png /e /r:1 /w:1 /nfl /ndl /njh /njs >nul
if errorlevel 8 goto :ERROR
robocopy "%ROOT%\examples" "%DESTINATION%\examples" *.json /e /r:1 /w:1 /nfl /ndl /njh /njs >nul
if errorlevel 8 goto :ERROR

echo.
echo Release files created:
echo   %DESTINATION%
exit /b 0

:ERROR
echo.
echo Error: Failed to create the release folder.
exit /b 1
