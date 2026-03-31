@echo off
setlocal

set SOLUTION_DIR=%~dp0
set PROJECT_DIR=%SOLUTION_DIR%CraftonEngine
set BUILD_OUTPUT=%PROJECT_DIR%\Bin\Release
set BUILD_OUTPUT_OBJVIEW=%PROJECT_DIR%\Bin\ObjViewDebug
set RELEASE_DIR=%SOLUTION_DIR%ReleaseBuild

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do set VS_PATH=%%i
if not defined VS_PATH (
    echo Cannot find Visual Studio.
    pause
    exit /b 1
)
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo

echo ============================================
echo  Release + ObjViewer Build Script
echo ============================================

echo.
echo [1/4] Building Release x64...
msbuild "%SOLUTION_DIR%CraftonEngine.sln" /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if %ERRORLEVEL% neq 0 (
    echo BUILD FAILED - Release
    pause
    exit /b 1
)

echo.
echo [2/4] Building ObjViewDebug x64...
msbuild "%SOLUTION_DIR%CraftonEngine.sln" /p:Configuration=ObjViewDebug /p:Platform=x64 /m /v:minimal
if %ERRORLEVEL% neq 0 (
    echo BUILD FAILED - ObjViewDebug
    pause
    exit /b 1
)

echo.
echo [3/4] Preparing output directory...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

echo.
echo [4/4] Copying files...

copy "%BUILD_OUTPUT%\CraftonEngine.exe" "%RELEASE_DIR%\" >nul
copy "%BUILD_OUTPUT_OBJVIEW%\CraftonEngine.exe" "%RELEASE_DIR%\ObjViewer.exe" >nul

if exist "%PROJECT_DIR%\imgui.ini" copy "%PROJECT_DIR%\imgui.ini" "%RELEASE_DIR%\" >nul

xcopy "%PROJECT_DIR%\Shaders" "%RELEASE_DIR%\Shaders\" /e /i /q >nul

xcopy "%PROJECT_DIR%\Asset" "%RELEASE_DIR%\Asset\" /e /i /q >nul

if exist "%PROJECT_DIR%\Data" (
    xcopy "%PROJECT_DIR%\Data" "%RELEASE_DIR%\Data\" /e /i /q >nul
)

xcopy "%PROJECT_DIR%\Settings" "%RELEASE_DIR%\Settings\" /e /i /q >nul

if exist "%PROJECT_DIR%\Saves" (
    xcopy "%PROJECT_DIR%\Saves" "%RELEASE_DIR%\Saves\" /e /i /q >nul
)

echo.
echo ============================================
echo  Build complete: %RELEASE_DIR%
echo ============================================
echo.
echo  ReleaseBuild/
echo    CraftonEngine.exe  (Editor)
echo    ObjViewer.exe      (OBJ Viewer)
echo    imgui.ini
echo    Shaders/
echo    Asset/
echo    Data/
echo    Settings/
echo    Saves/
echo.
pause
