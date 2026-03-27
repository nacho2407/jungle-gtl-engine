@echo off
setlocal

echo [1/3] Engine 빌드 산출물 삭제 중...
if exist "Engine\Bin" rmdir /s /q "Engine\Bin"
if exist "Engine\Build" rmdir /s /q "Engine\Build"
echo   완료

echo [2/3] Editor 빌드 산출물 삭제 중...
if exist "Editor\Bin" rmdir /s /q "Editor\Bin"
if exist "Editor\Build" rmdir /s /q "Editor\Build"
echo   완료

echo [3/3] Client 빌드 산출물 삭제 중...
if exist "Client\Bin" rmdir /s /q "Client\Bin"
if exist "Client\Build" rmdir /s /q "Client\Build"
echo   완료

echo.
echo [완료] 모든 빌드 산출물 삭제 완료
