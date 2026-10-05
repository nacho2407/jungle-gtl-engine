@echo off
setlocal EnableExtensions

:: 단독 실행용 게임 클라이언트 패키지 빌드
:: GameClient 구성은 vcxproj에서 TargetName=JSEngineGame 사용
set "SOLUTION_DIR=%~dp0"
set "PROJECT_DIR=%SOLUTION_DIR%JSEngine"
set "SOLUTION_FILE=%SOLUTION_DIR%JSEngine.sln"
set "CONFIGURATION=GameClientRelease"
set "PLATFORM=x64"
set "BUILD_OUTPUT=%PROJECT_DIR%\Bin\%CONFIGURATION%"
set "PACKAGE_DIR=%SOLUTION_DIR%GameBuild"
set "EXE_NAME=JSEngineGame.exe"
set "DEFAULT_SCENE=Asset/Scene/Demonstrate.Scene"    

call :LoadVsDevEnvironment
if errorlevel 1 goto :Fail

echo ============================================
echo  Game Build Script
echo  Configuration: %CONFIGURATION% ^| Platform: %PLATFORM%
echo ============================================

echo.
echo [1/3] Building %CONFIGURATION% %PLATFORM%...
"%MSBUILD_EXE%" "%SOLUTION_FILE%" /p:Configuration=%CONFIGURATION% /p:Platform=%PLATFORM% /m /v:minimal
set "MSBUILD_EXIT_CODE=%ERRORLEVEL%"
:: 로컬 배치 라벨 호출 전에 원래 PATH로 복구
if defined BUILD_PATH_BACKUP set "PATH=%BUILD_PATH_BACKUP%"
set "BUILD_PATH_BACKUP="
if not "%MSBUILD_EXIT_CODE%"=="0" (
    echo BUILD FAILED
    goto :Fail
)

echo.
echo [2/3] Preparing output directory...
if exist "%PACKAGE_DIR%" rmdir /s /q "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%"
if errorlevel 1 goto :Fail

echo.
echo [3/3] Copying files...
:: 실행 파일과 빌드 산출 DLL은 선택된 OutDir에서 가져옴
call :CopyRequiredFile "%BUILD_OUTPUT%\%EXE_NAME%" "%PACKAGE_DIR%\" "%EXE_NAME%"
if errorlevel 1 goto :Fail
call :CopyOptionalFile "%BUILD_OUTPUT%\JSEngineGame.pdb" "%PACKAGE_DIR%\" "JSEngineGame.pdb"
if errorlevel 1 goto :Fail
call :CopyBuildDlls
if errorlevel 1 goto :Fail
call :EnsureRuntimeDll "lua51.dll" "%BUILD_OUTPUT%\lua51.dll" "%PROJECT_DIR%\ThirdParty\luajit\src\lua51.dll"
if errorlevel 1 goto :Fail
call :EnsureRuntimeDll "libfbxsdk.dll" "%BUILD_OUTPUT%\libfbxsdk.dll" "%PROJECT_DIR%\ThirdParty\FBX\lib\release\libfbxsdk.dll"
if errorlevel 1 goto :Fail

:: 런타임 데이터는 게임 런타임이 FPaths::RootDir() 아래에서 기대하는 구조를 따름
call :CopyOptionalDir "%PROJECT_DIR%\Shaders" "%PACKAGE_DIR%\Shaders" "Shaders"
if errorlevel 1 goto :Fail
call :CopyOptionalDir "%PROJECT_DIR%\DerivedData\ShaderCache" "%PACKAGE_DIR%\DerivedData\ShaderCache" "ShaderCache"
if errorlevel 1 goto :Fail
call :CopyOptionalDir "%PROJECT_DIR%\Resources" "%PACKAGE_DIR%\Resources" "Resources"
if errorlevel 1 goto :Fail
call :CopyOptionalDir "%PROJECT_DIR%\Asset" "%PACKAGE_DIR%\Asset" "Asset"
if errorlevel 1 goto :Fail
call :CopyOptionalDir "%PROJECT_DIR%\Settings" "%PACKAGE_DIR%\Settings" "Settings"
if errorlevel 1 goto :Fail

if not exist "%PACKAGE_DIR%\Settings" mkdir "%PACKAGE_DIR%\Settings"
if not exist "%PACKAGE_DIR%\Saves" mkdir "%PACKAGE_DIR%\Saves"
:: 에디터 패키저는 더 풍부한 Game.ini를 작성함. 이 기본값은 에디터에서
:: 패키징 데이터를 만들지 않았을 때도 수동 스크립트 빌드가 실행 가능하도록 함
call :WriteDefaultGameIni
if errorlevel 1 goto :Fail

echo.
echo ============================================
echo  Build complete: %PACKAGE_DIR%
echo ============================================
echo.
pause
exit /b 0

:LoadVsDevEnvironment
:: Developer Command Prompt에 의존하지 않고 Visual Studio 설치 경로 탐색
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo vswhere.exe not found.
    exit /b 1
)

for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
    echo Visual Studio installation not found.
    exit /b 1
)

set "MSBUILD_EXE=%VS_PATH%\MSBuild\Current\Bin\amd64\MSBuild.exe"
if not exist "%MSBUILD_EXE%" set "MSBUILD_EXE=%VS_PATH%\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD_EXE%" (
    echo MSBuild.exe not found.
    exit /b 1
)

call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -no_logo
if errorlevel 1 exit /b %ERRORLEVEL%
call :NormalizePathEnvironment
exit /b %ERRORLEVEL%

:NormalizePathEnvironment
:: 일부 셸은 PATH와 Path를 동시에 넘김. MSBuild의 .NET Task 환경은 이를
:: 중복 키로 취급하므로, MSBuild 실행 중에는 오래된 혼합 대소문자 항목 제거
set "BUILD_PATH_BACKUP=%PATH%"
set "HAS_UPPER_PATH="
set "HAS_TITLE_PATH="
for /f "tokens=1 delims==" %%v in ('set p') do (
    if "%%v"=="PATH" set "HAS_UPPER_PATH=1"
    if "%%v"=="Path" set "HAS_TITLE_PATH=1"
)
if defined HAS_UPPER_PATH if defined HAS_TITLE_PATH set "Path="
exit /b 0

:CopyRequiredFile
if not exist "%~1" (
    echo Missing required file: %~1
    exit /b 1
)
copy /Y "%~1" "%~2" >nul
if errorlevel 1 (
    echo Failed to copy %~3
    exit /b 1
)
echo Copied %~3
exit /b 0

:CopyOptionalFile
if not exist "%~1" (
    echo Skipped %~3
    exit /b 0
)
copy /Y "%~1" "%~2" >nul
if errorlevel 1 (
    echo Failed to copy %~3
    exit /b 1
)
echo Copied %~3
exit /b 0

:CopyBuildDlls
:: Post-build 이벤트로 복사된 런타임 DLL을 포함해 MSBuild가 출력한 모든 DLL 복사
if not exist "%BUILD_OUTPUT%\*.dll" (
    echo Skipped build output DLLs
    exit /b 0
)
copy /Y "%BUILD_OUTPUT%\*.dll" "%PACKAGE_DIR%\" >nul
if errorlevel 1 (
    echo Failed to copy build output DLLs
    exit /b 1
)
echo Copied build output DLLs
exit /b 0

:EnsureRuntimeDll
:: MSBuild가 런타임 DLL을 복사하지 않았다면 원본 위치에서 직접 가져옴
if exist "%PACKAGE_DIR%\%~1" (
    echo Found %~1
    exit /b 0
)
if exist "%~2" (
    copy /Y "%~2" "%PACKAGE_DIR%\%~1" >nul
    if errorlevel 1 exit /b 1
    echo Copied %~1
    exit /b 0
)
if exist "%~3" (
    copy /Y "%~3" "%PACKAGE_DIR%\%~1" >nul
    if errorlevel 1 exit /b 1
    echo Copied %~1
    exit /b 0
)
echo Missing runtime DLL: %~1
exit /b 1

:CopyOptionalDir
if not exist "%~1\" (
    echo Skipped %~3
    exit /b 0
)
xcopy "%~1" "%~2\" /e /i /y /q >nul
if errorlevel 2 (
    echo Failed to copy %~3
    exit /b 1
)
echo Copied %~3
exit /b 0

:WriteDefaultGameIni
:: UGameEngine::LoadGameSettings()가 읽는 최소 시작 설정
(
    echo [Game]
    echo GameName=CyberPsycho
    echo ExecutableName=%EXE_NAME%
    echo StartupScene=%DEFAULT_SCENE%
    echo GameModeClass=AGameModeBase
    echo PlayerControllerClass=APlayerController
    echo DefaultPawnClass=ADefaultPawn
    echo DefaultPawnPrefabPath=
    echo.
    echo [Scenes]
    echo Count=1
    echo Scene0=%DEFAULT_SCENE%
    echo.
    echo [Branding]
    echo Icon=
    echo Splash=
    echo SplashMinSeconds=3
    echo.
    echo [Render]
    echo bShadow=true
    echo bSkeletalMesh=true
) > "%PACKAGE_DIR%\Settings\Game.ini"
if errorlevel 1 (
    echo Failed to write Settings\Game.ini
    exit /b 1
)
echo Wrote Settings\Game.ini
exit /b 0

:Fail
echo.
echo Build script failed.
pause
exit /b 1
