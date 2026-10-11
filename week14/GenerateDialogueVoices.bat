@echo off
setlocal

set "PROJECT_ROOT=%~dp0"
if "%PROJECT_ROOT:~-1%"=="\" set "PROJECT_ROOT=%PROJECT_ROOT:~0,-1%"
set "DIALOGUE_FILE=%~1"
set "VOICE_MAP=%~2"
set "EXTRA_ARGS=%~3"
set "PROCESS_ALL_DIALOGUES=0"

if "%DIALOGUE_FILE%"=="" (
    set "PROCESS_ALL_DIALOGUES=1"
    set "DIALOGUE_FILE=KraftonEngine\Content\Script\Dialogue\*.dialogue.lua"
)
if "%VOICE_MAP%"=="" set "VOICE_MAP=Scripts\Tools\DialogueVoiceGenerator\voice_map.json"
if "%EXTRA_ARGS%"=="" set "EXTRA_ARGS="

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
    echo ElevenLabs API key was not found in ELEVENLABS_API_KEY or API_KEY.
    echo Paste it below. Leave blank to only skip existing files.
    set /p "ELEVENLABS_API_KEY=ELEVENLABS_API_KEY: "
)

if "%ELEVENLABS_API_KEY%"=="" (
    echo Existing audio files can still be skipped, but missing ElevenLabs files will fail.
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
if "%EXTRA_ARGS%"=="" (
    echo Mode: skip existing files
) else (
    echo Extra args: %EXTRA_ARGS%
)
echo.

if "%PROCESS_ALL_DIALOGUES%"=="1" (
    for %%F in ("%PROJECT_ROOT%\KraftonEngine\Content\Script\Dialogue\*.dialogue.lua") do (
        echo.
        echo ==== %%~nxF ====
        %PYTHON% "%PROJECT_ROOT%\Scripts\Tools\DialogueVoiceGenerator\generate_dialogue_voices.py" "%%~fF" "%PROJECT_ROOT%\%VOICE_MAP%" --project-root "%PROJECT_ROOT%" %EXTRA_ARGS%
        if errorlevel 1 (
            echo.
            echo Dialogue voice generation failed: %%~fF
            echo.
            pause
            exit /b 1
        )
    )
) else (
    %PYTHON% "%PROJECT_ROOT%\Scripts\Tools\DialogueVoiceGenerator\generate_dialogue_voices.py" "%PROJECT_ROOT%\%DIALOGUE_FILE%" "%PROJECT_ROOT%\%VOICE_MAP%" --project-root "%PROJECT_ROOT%" %EXTRA_ARGS%
    if errorlevel 1 (
        echo.
        echo Dialogue voice generation failed.
        echo.
        pause
        exit /b 1
    )
)

echo.
echo Dialogue voice generation finished.
echo.
pause
