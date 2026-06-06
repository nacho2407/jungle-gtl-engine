@echo off
setlocal

set "PROJECT_ROOT=%~dp0"
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"
set "DIALOGUE_FILE=%~1"
set "VOICE_MAP=%~2"
set "EXTRA_ARGS=%~3"

if "%DIALOGUE_FILE%"=="" set "DIALOGUE_FILE=KraftonEngine\Content\Script\Dialogue\Prologue.dialogue.lua"
if "%VOICE_MAP%"=="" set "VOICE_MAP=Scripts\Tools\DialogueVoiceGenerator\voice_map.json"
if "%EXTRA_ARGS%"=="" set "EXTRA_ARGS=--overwrite"

set "PYTHON=py -3"
py -3 --version >nul 2>nul
if errorlevel 1 (
    set "PYTHON=python"
    python --version >nul 2>nul
    if errorlevel 1 (
        echo Python was not found. Install Python or add it to PATH.
        echo.
        pause
        exit /b 1
    )
)

if "%ELEVENLABS_API_KEY%"=="" (
    if not "%API_KEY%"=="" set "ELEVENLABS_API_KEY=%API_KEY%"
)

if "%ELEVENLABS_API_KEY%"=="" (
    echo ElevenLabs API key was not found in ELEVENLABS_API_KEY.
    echo Paste it below. It will only be used for this run.
    set /p "ELEVENLABS_API_KEY=ELEVENLABS_API_KEY: "
)

if "%ELEVENLABS_API_KEY%"=="" (
    echo.
    echo No API key was provided. Cannot generate ElevenLabs voices.
    echo.
    pause
    exit /b 1
)

if not exist "%PROJECT_ROOT%\%VOICE_MAP%" (
    if exist "%PROJECT_ROOT%\Scripts\Tools\DialogueVoiceGenerator\voice_map.example.json" (
        copy "%PROJECT_ROOT%\Scripts\Tools\DialogueVoiceGenerator\voice_map.example.json" "%PROJECT_ROOT%\Scripts\Tools\DialogueVoiceGenerator\voice_map.json" >nul
        echo Created Scripts\Tools\DialogueVoiceGenerator\voice_map.json from example.
        echo Fill voice_id values before real generation.
    )
)

%PYTHON% -c "import imageio_ffmpeg" >nul 2>nul
if errorlevel 1 (
    echo Installing imageio-ffmpeg for audio postprocess...
    %PYTHON% -m pip install imageio-ffmpeg
    if errorlevel 1 (
        echo Failed to install imageio-ffmpeg.
        echo.
        pause
        exit /b 1
    )
)

echo.
echo Generating dialogue voices...
echo Dialogue: %DIALOGUE_FILE%
echo Voice map: %VOICE_MAP%
echo.

%PYTHON% "%PROJECT_ROOT%\Scripts\Tools\DialogueVoiceGenerator\generate_dialogue_voices.py" "%PROJECT_ROOT%\%DIALOGUE_FILE%" "%PROJECT_ROOT%\%VOICE_MAP%" --project-root "%PROJECT_ROOT%" %EXTRA_ARGS%
if errorlevel 1 (
    echo.
    echo Dialogue voice generation failed.
    echo.
    pause
    exit /b 1
)

echo.
echo Dialogue voice generation finished.
echo.
pause
