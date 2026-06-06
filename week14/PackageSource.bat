@echo off
setlocal EnableExtensions

if "%~1"=="" (
    set "PROJECT_ROOT=%~dp0"
) else (
    set "PROJECT_ROOT=%~f1"
)

for /f "usebackq delims=" %%D in (`powershell -NoProfile -ExecutionPolicy Bypass -Command "[Environment]::GetFolderPath('Desktop')"`) do set "DESKTOP_DIR=%%D"
if not defined DESKTOP_DIR (
    set "DESKTOP_DIR=%USERPROFILE%\Desktop"
)

set "ENGINE_ROOT=%PROJECT_ROOT%\KraftonEngine"
set "SOURCE_DIR=%ENGINE_ROOT%\Source"
set "UI_DIR=%ENGINE_ROOT%\Content\UI"
set "SCRIPT_DIR=%ENGINE_ROOT%\Content\Script"

if not exist "%SOURCE_DIR%\" (
    echo Source directory not found: "%SOURCE_DIR%"
    exit /b 1
)

if not exist "%UI_DIR%\" (
    echo UI directory not found: "%UI_DIR%"
    exit /b 1
)

if not exist "%SCRIPT_DIR%\" (
    echo Script directory not found: "%SCRIPT_DIR%"
    exit /b 1
)

if "%~2"=="" (
    set "OUTPUT_ZIP=%DESKTOP_DIR%\KraftonEngine_Source.zip"
) else (
    set "OUTPUT_ZIP=%~f2"
)

set "STAGING=%TEMP%\KraftonEngine_Source_%RANDOM%%RANDOM%"
set "STAGING_ENGINE=%STAGING%\KraftonEngine"

if exist "%STAGING%" (
    rmdir /s /q "%STAGING%"
)

mkdir "%STAGING_ENGINE%\Source" >nul 2>nul
mkdir "%STAGING_ENGINE%\Content\UI" >nul 2>nul
mkdir "%STAGING_ENGINE%\Content\Script" >nul 2>nul

echo Copying Source files...
robocopy "%SOURCE_DIR%" "%STAGING_ENGINE%\Source" /E /XF *.png *.jpg /NFL /NDL /NJH /NJS /NC /NS /NP >nul
if %ERRORLEVEL% GEQ 8 (
    echo Failed to copy Source files.
    rmdir /s /q "%STAGING%" >nul 2>nul
    exit /b 1
)

echo Copying UI files...
robocopy "%UI_DIR%" "%STAGING_ENGINE%\Content\UI" /E /XF *.png *.jpg /NFL /NDL /NJH /NJS /NC /NS /NP >nul
if %ERRORLEVEL% GEQ 8 (
    echo Failed to copy UI files.
    rmdir /s /q "%STAGING%" >nul 2>nul
    exit /b 1
)

echo Copying Lua script files...
robocopy "%SCRIPT_DIR%" "%STAGING_ENGINE%\Content\Script" *.lua /E /NFL /NDL /NJH /NJS /NC /NS /NP >nul
if %ERRORLEVEL% GEQ 8 (
    echo Failed to copy Lua script files.
    rmdir /s /q "%STAGING%" >nul 2>nul
    exit /b 1
)

if exist "%OUTPUT_ZIP%" (
    del /f /q "%OUTPUT_ZIP%"
)

for %%I in ("%OUTPUT_ZIP%") do (
    if not exist "%%~dpI" (
        mkdir "%%~dpI" >nul 2>nul
    )
)

echo Creating zip: "%OUTPUT_ZIP%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%STAGING%\*' -DestinationPath '%OUTPUT_ZIP%' -Force"
if errorlevel 1 (
    echo Failed to create zip.
    rmdir /s /q "%STAGING%" >nul 2>nul
    exit /b 1
)

rmdir /s /q "%STAGING%" >nul 2>nul

echo Done.
echo "%OUTPUT_ZIP%"
explorer.exe /select,"%OUTPUT_ZIP%"
exit /b 0
