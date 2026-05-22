@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "PY_SCRIPT=%SCRIPT_DIR%Scripts\CheckMissingLFS.py"

if not exist "%PY_SCRIPT%" (
    echo ERROR: Cannot find "%PY_SCRIPT%".
    pause
    exit /b 1
)

set "BUNDLED_PYTHON=%SCRIPT_DIR%Scripts\python\python.exe"
if exist "%BUNDLED_PYTHON%" (
    "%BUNDLED_PYTHON%" "%PY_SCRIPT%"
    set "EXIT_CODE=%ERRORLEVEL%"
    pause
    exit /b %EXIT_CODE%
)

where py >nul 2>nul
if not errorlevel 1 (
    py -3 "%PY_SCRIPT%"
    set "EXIT_CODE=%ERRORLEVEL%"
    pause
    exit /b %EXIT_CODE%
)

where python >nul 2>nul
if not errorlevel 1 (
    python "%PY_SCRIPT%"
    set "EXIT_CODE=%ERRORLEVEL%"
    pause
    exit /b %EXIT_CODE%
)

echo ERROR: Python was not found. Install Python or add it to PATH.
pause
exit /b 1
