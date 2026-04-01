@echo off
setlocal

set SOLUTION_DIR=%~dp0
set PROJECT_DIR=%SOLUTION_DIR%KraftonEngine
set BUILD_OUTPUT=%PROJECT_DIR%\Bin\Release
set RELEASE_DIR=%SOLUTION_DIR%ReleaseBuild

:: VS Developer 환경 로드 (msbuild PATH 등록)
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do set VS_PATH=%%i
if not defined VS_PATH (
    echo Visual Studio를 찾을 수 없습니다.
    pause
    exit /b 1
)
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo

echo ============================================
echo  Release Build Script
echo ============================================

:: 1. MSBuild로 Release x64 빌드
echo.
echo [1/3] Building Release x64...
msbuild "%SOLUTION_DIR%KraftonEngine.sln" /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if %ERRORLEVEL% neq 0 (
    echo BUILD FAILED
    pause
    exit /b 1
)

:: 2. 기존 ReleaseBuild 폴더 정리
echo.
echo [2/3] Preparing output directory...
if exist "%RELEASE_DIR%" rmdir /s /q "%RELEASE_DIR%"
mkdir "%RELEASE_DIR%"

:: 3. 파일 복사
echo.
echo [3/3] Copying files...

:: 실행 파일 (루트에)
copy "%BUILD_OUTPUT%\KraftonEngine.exe" "%RELEASE_DIR%\" >nul

:: ImGui 레이아웃 (도킹 설정 포함)
if exist "%PROJECT_DIR%\imgui.ini" copy "%PROJECT_DIR%\imgui.ini" "%RELEASE_DIR%\" >nul

:: Shaders
xcopy "%PROJECT_DIR%\Shaders" "%RELEASE_DIR%\Shaders\" /e /i /q >nul

:: Asset (Scene 등)
xcopy "%PROJECT_DIR%\Asset" "%RELEASE_DIR%\Asset\" /e /i /q >nul

:: Settings
xcopy "%PROJECT_DIR%\Settings" "%RELEASE_DIR%\Settings\" /e /i /q >nul

:: Saves (있으면 복사)
if exist "%PROJECT_DIR%\Saves" (
    xcopy "%PROJECT_DIR%\Saves" "%RELEASE_DIR%\Saves\" /e /i /q >nul
)

echo.
echo ============================================
echo  Build complete: %RELEASE_DIR%
echo ============================================
echo.
echo  ReleaseBuild/
echo    KraftonEngine.exe
echo    imgui.ini
echo    Shaders/
echo    Asset/Scene/
echo    Settings/
echo    Saves/
echo.
pause
