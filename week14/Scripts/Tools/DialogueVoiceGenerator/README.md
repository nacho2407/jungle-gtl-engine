# Dialogue Voice Generator

`.dialogue.lua` files can be converted into pre-generated voice assets with
Edge TTS or ElevenLabs.

## Setup

Install the free Edge TTS generator:

```bat
python -m pip install edge-tts
```

For ElevenLabs only, set your API key:

```bat
set ELEVENLABS_API_KEY=your_api_key_here
```

## Generate

```bat
GenerateDialogueVoices.bat KraftonEngine\Content\Script\Dialogue\Prologue.dialogue.lua Scripts\Tools\DialogueVoiceGenerator\voice_map.json
```

`voice_map.json` can enable only selected speakers. The default setup uses the
free `edge` provider, so no ElevenLabs API key is needed.

Edge provider example:

```json
{
  "provider": "edge",
  "voice": "en-US-SteffanNeural",
  "rate": "+0%",
  "pitch": "-5Hz"
}
```

Optional post-processing uses ffmpeg. If `ffmpeg` is not installed globally, run:

```bat
python -m pip install imageio-ffmpeg
```

Then add either `postprocess_filter` or `postprocess_filter_complex` to a speaker
entry. The default `voice_map.json` uses this for lower-pitched KAIN, noisy Drake,
and a mechanical SYSTEM voice.

ElevenLabs provider settings map directly to API payload values:

```json
{
  "voice_id": "6F5Zhi321D3Oq7v1oNT4",
  "model_id": "eleven_multilingual_v2",
  "stability": 0.50,
  "similarity_boost": 0.75,
  "style": 0.20,
  "use_speaker_boost": true
}
```

Each dialogue line gets a stable id. If an entry has no explicit `id`, the tool
generates `<dialogue_id>_<index>`, such as `Prologue_0001`. The generated voice
manifest exposes both `by_id` and `entries`, so game code can look up audio by
dialogue id instead of relying only on order.

Generated audio is written under:

```text
KraftonEngine/Content/Audio/Dialogues/<dialogue_id>/
```

The generated Lua manifest is written under:

```text
KraftonEngine/Content/Script/Dialogue/Generated/<dialogue_id>.voices.lua
```

`Prologue.lua` loads that manifest automatically when it exists.
