@echo off
setlocal

set SOLUTION_DIR=%~dp0
set PROJECT_DIR=%SOLUTION_DIR%JSEngine
set BUILD_OUTPUT=%PROJECT_DIR%\Bin\Release
set RELEASE_DIR=%SOLUTION_DIR%ReleaseBuild

:: Load Visual Studio Developer environment for msbuild.
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do set VS_PATH=%%i
if not defined VS_PATH (
    echo Visual Studio was not found.
    pause
    exit /b 1
)
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo

echo ============================================
echo  Release Build Script
echo ============================================

:: 1. Build Release x64 with MSBuild.
echo.
echo [1/3] Building Release x64...
msbuild "%SOLUTION_DIR%JSEngine.sln" /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if %ERRORLEVEL% neq 0 (
    echo BUILD FAILED
    pause
    exit /b 1
)

:: 2. Recreate ReleaseBuild directory.
echo.
echo [2/3] Preparing output directory...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

:: 3. Copy files.
echo.
echo [3/3] Copying files...

:: Executable and debug symbols.
copy "%BUILD_OUTPUT%\JSEngine.exe" "%RELEASE_DIR%\" >nul
if exist "%BUILD_OUTPUT%\*.pdb" copy "%BUILD_OUTPUT%\*.pdb" "%RELEASE_DIR%\" >nul

:: Runtime DLLs.
copy "%BUILD_OUTPUT%\libfbxsdk.dll" "%RELEASE_DIR%\" >nul
copy "%BUILD_OUTPUT%\lua51.dll" "%RELEASE_DIR%\" >nul

:: ImGui layout.
if exist "%PROJECT_DIR%\imgui.ini" copy "%PROJECT_DIR%\imgui.ini" "%RELEASE_DIR%\" >nul

:: Shaders.
xcopy "%PROJECT_DIR%\Shaders" "%RELEASE_DIR%\Shaders\" /e /i /q >nul

:: Local shader cache (generated .cso files).
if exist "%PROJECT_DIR%\DerivedData\ShaderCache" (
    xcopy "%PROJECT_DIR%\DerivedData\ShaderCache" "%RELEASE_DIR%\DerivedData\ShaderCache\" /e /i /q >nul
)

:: Asset.
xcopy "%PROJECT_DIR%\Asset" "%RELEASE_DIR%\Asset\" /e /i /q >nul

:: Settings.
xcopy "%PROJECT_DIR%\Settings" "%RELEASE_DIR%\Settings\" /e /i /q >nul

:: Resources.
xcopy "%PROJECT_DIR%\Resources" "%RELEASE_DIR%\Resources\" /e /i /q >nul

:: Saves, excluding crash dump output.
if exist "%PROJECT_DIR%\Saves" (
    robocopy "%PROJECT_DIR%\Saves" "%RELEASE_DIR%\Saves" /e /xd "%PROJECT_DIR%\Saves\Dump" /xf *.dmp >nul
    if ERRORLEVEL 8 (
        echo SAVES COPY FAILED
        pause
        exit /b 1
    )
)

echo.
echo ============================================
echo  Build complete: %RELEASE_DIR%
echo ============================================
echo.
echo  ReleaseBuild/
echo    JSEngine.exe
echo    *.pdb
echo    libfbxsdk.dll
echo    lua51.dll
echo    imgui.ini
echo    Shaders/
echo    DerivedData/ShaderCache/
echo    Asset/
echo    Settings/
echo    Resources/
echo    Saves/ (without dumps)
echo.
pause
