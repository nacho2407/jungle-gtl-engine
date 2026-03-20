@echo off
setlocal

set "ROOT_DIR=%~dp0"
pushd "%ROOT_DIR%" >nul

py .\Scripts\GenerateProjectFiles.py
set "EXIT_CODE=%ERRORLEVEL%"

popd >nul
exit /b %EXIT_CODE%
