@echo off
setlocal

set MSBUILD="C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
set CONFIG=Debug
set PLATFORM=x64

set ENGINE_DLL=Engine\Bin\%CONFIG%\Engine.dll
set EDITOR_BIN=Editor\Bin\%CONFIG%\
set CLIENT_BIN=Client\Bin\%CONFIG%\

echo [1/2] Engine 빌드 중...
%MSBUILD% Engine\Engine.vcxproj -p:Configuration=%CONFIG% -p:Platform=%PLATFORM% -m -nologo
if %ERRORLEVEL% neq 0 (
    echo [실패] Engine 빌드 실패
    exit /b 1
)
echo [완료] Engine 빌드 성공

echo.
echo [2/2] Engine.dll 복사 중...
if not exist "%EDITOR_BIN%" mkdir "%EDITOR_BIN%"
if not exist "%CLIENT_BIN%" mkdir "%CLIENT_BIN%"

xcopy /Y /D "%ENGINE_DLL%" "%EDITOR_BIN%"
xcopy /Y /D "%ENGINE_DLL%" "%CLIENT_BIN%"

echo.
echo [완료] Engine.dll 복사 완료
echo   - %EDITOR_BIN%Engine.dll
echo   - %CLIENT_BIN%Engine.dll
