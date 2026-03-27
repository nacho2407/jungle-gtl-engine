@echo off
setlocal

set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set SLN=%~dp0KraftonJungleEngine.sln
set OUTPUT=%~dp0ReleaseBuild

echo ============================================
echo  Krafton Jungle Engine - Release Build
echo ============================================
echo.

:: 1. Release 빌드
echo [1/3] Building Release x64...
%MSBUILD% "%SLN%" /p:Configuration=Release /p:Platform=x64 /v:minimal
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)
echo [OK] Build succeeded.
echo.

:: 2. 출력 폴더 초기화
echo [2/3] Preparing output folder...
if exist "%OUTPUT%" rmdir /s /q "%OUTPUT%"
mkdir "%OUTPUT%"

:: 3. 파일 복사 (FPaths 폴더 구조 유지)
echo [3/3] Copying files...

:: Engine DLL, Client, Editor → 루트에 배치
copy /y "%~dp0Engine\Bin\Release\Engine.dll" "%OUTPUT%\" >nul
copy /y "%~dp0Client\Bin\Release\Client.exe" "%OUTPUT%\" >nul
copy /y "%~dp0Editor\Bin\Release\Editor.exe" "%OUTPUT%\" >nul

:: Shaders
mkdir "%OUTPUT%\Engine\Shaders"
xcopy /y /q "%~dp0Engine\Shaders\*" "%OUTPUT%\Engine\Shaders\" >nul

:: Assets
xcopy /y /q /s /i "%~dp0Assets" "%OUTPUT%\Assets" >nul

echo.
echo ============================================
echo  Build complete: %OUTPUT%
echo ============================================
echo.
echo  Editor: ReleaseBuild\Editor.exe
echo  Client: ReleaseBuild\Client.exe
echo.
pause
