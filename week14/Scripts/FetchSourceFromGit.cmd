@echo off
setlocal

set "GIT_EXE=%~1"
set "GIT_REPO=%~2"
set "COMMIT=%~3"
set "SOURCE_PATH=%~4"
set "TARGET=%~5"
set "CACHE_DIR=%~6"

if "%CACHE_DIR%"=="" (
    for %%I in ("%TARGET%") do set "CACHE_DIR=%%~dpI"
)

if not exist "%CACHE_DIR%" mkdir "%CACHE_DIR%" 2>nul

set "LOG_FILE=%CACHE_DIR%\srcsrv_fetch.log"
set "ERR_FILE=%CACHE_DIR%\srcsrv_git_error.txt"

>>"%LOG_FILE%" echo ===== Source Server fetch =====
>>"%LOG_FILE%" echo User=%USERDOMAIN%\%USERNAME%
>>"%LOG_FILE%" echo Computer=%COMPUTERNAME%
>>"%LOG_FILE%" echo GitExe=%GIT_EXE%
>>"%LOG_FILE%" echo GitRepo=%GIT_REPO%
>>"%LOG_FILE%" echo Commit=%COMMIT%
>>"%LOG_FILE%" echo SourcePath=%SOURCE_PATH%
>>"%LOG_FILE%" echo Target=%TARGET%

if not exist "%GIT_EXE%" (
    >>"%LOG_FILE%" echo Configured GitExe is missing. Trying PATH and default install locations.
    for /f "delims=" %%G in ('where git.exe 2^>nul') do (
        if exist "%%G" (
            set "GIT_EXE=%%G"
            goto :GitResolved
        )
    )

    if exist "%ProgramFiles%\Git\cmd\git.exe" (
        set "GIT_EXE=%ProgramFiles%\Git\cmd\git.exe"
        goto :GitResolved
    )

    if exist "%ProgramFiles%\Git\bin\git.exe" (
        set "GIT_EXE=%ProgramFiles%\Git\bin\git.exe"
        goto :GitResolved
    )

    if exist "%LocalAppData%\Programs\Git\cmd\git.exe" (
        set "GIT_EXE=%LocalAppData%\Programs\Git\cmd\git.exe"
        goto :GitResolved
    )

    if exist "%LocalAppData%\Programs\Git\bin\git.exe" (
        set "GIT_EXE=%LocalAppData%\Programs\Git\bin\git.exe"
        goto :GitResolved
    )

    >>"%LOG_FILE%" echo Missing git executable.
    echo Missing git executable. Tried configured path and default Git for Windows locations.>"%ERR_FILE%"
    exit /b 10
)

:GitResolved
>>"%LOG_FILE%" echo ResolvedGitExe=%GIT_EXE%

"%GIT_EXE%" -c "safe.directory=%GIT_REPO%" --git-dir="%GIT_REPO%" show "%COMMIT%:%SOURCE_PATH%" > "%TARGET%" 2> "%ERR_FILE%"
set "EXIT_CODE=%ERRORLEVEL%"
>>"%LOG_FILE%" echo GitExitCode=%EXIT_CODE%

if not "%EXIT_CODE%"=="0" exit /b %EXIT_CODE%
if not exist "%TARGET%" (
    >>"%LOG_FILE%" echo Target was not created.
    exit /b 11
)

for %%I in ("%TARGET%") do set "TARGET_SIZE=%%~zI"
>>"%LOG_FILE%" echo TargetSize=%TARGET_SIZE%

if "%TARGET_SIZE%"=="0" exit /b 12

exit /b 0
