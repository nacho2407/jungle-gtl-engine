@echo off
setlocal

set SOLUTION_DIR=%~dp0
set PROJECT_DIR=%SOLUTION_DIR%KraftonEngine
set BUILD_OUTPUT=%PROJECT_DIR%\Bin\Game
set GAME_DIR=%SOLUTION_DIR%GameBuild
set GAME_BIN=%GAME_DIR%\Bin

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
echo  Game Build Script
echo ============================================

:: 1. MSBuild로 Game x64 빌드
echo.
echo [1/3] Building Game x64...
msbuild "%SOLUTION_DIR%KraftonEngine.sln" /p:Configuration=Game /p:Platform=x64 /m /v:minimal
if %ERRORLEVEL% neq 0 (
    echo BUILD FAILED
    pause
    exit /b 1
)

:: 2. 기존 GameBuild 폴더 정리
echo.
echo [2/3] Preparing output directory...
if exist "%GAME_DIR%" rmdir /s /q "%GAME_DIR%"
mkdir "%GAME_DIR%"
mkdir "%GAME_BIN%"

:: 3. 파일 복사
echo.
echo [3/3] Copying files...

:: exe + 동봉 DLL 은 Bin\ 서브폴더로 (PhysX / RmlUi / fmod 등 — vcxproj PostBuildEvent
:: 가 Bin\Game 로 복사해둔 것들).
copy "%BUILD_OUTPUT%\KraftonEngine.exe" "%GAME_BIN%\" >nul
xcopy "%BUILD_OUTPUT%\*.dll" "%GAME_BIN%\" /y /q >nul

:: 리소스는 루트에 (engine 의 FPaths 가 CWD 기준으로 Asset/Shaders/Data/Settings 를 찾음).
xcopy "%PROJECT_DIR%\Shaders" "%GAME_DIR%\Shaders\" /e /i /q >nul
xcopy "%PROJECT_DIR%\Asset" "%GAME_DIR%\Asset\" /e /i /q >nul
xcopy "%PROJECT_DIR%\Settings" "%GAME_DIR%\Settings\" /e /i /q >nul
xcopy "%PROJECT_DIR%\Data" "%GAME_DIR%\Data\" /e /i /q >nul

:: 런처 — 더블클릭으로 게임 실행. CWD 를 GameBuild 루트로 맞춰서 FPaths 가 리소스
:: 폴더를 정확히 찾게 하고, exe 는 Bin\ 서브폴더에서 (옆의 DLL 들과 함께) 실행.
(
echo @echo off
echo cd /d "%%~dp0"
echo start "" "%%~dp0Bin\KraftonEngine.exe"
) > "%GAME_DIR%\Play.bat"

echo.
echo ============================================
echo  Build complete: %GAME_DIR%
echo ============================================
echo.
echo  GameBuild/
echo    Play.bat        (실행)
echo    Bin/
echo      KraftonEngine.exe + *.dll
echo    Shaders/
echo    Asset/
echo    Data/
echo    Settings/
echo.
pause
