#!/usr/bin/env python3
import argparse
import ast
import json
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")

ENTRY_RE = re.compile(r"\{(?P<body>.*?)\}\s*,?", re.DOTALL)
FIELD_RE = re.compile(r"(?P<key>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?P<value>\[\[.*?\]\]|\"(?:\\.|[^\"])*\"|'(?:\\.|[^'])*'|[-+]?\d+(?:\.\d+)?)", re.DOTALL)


def find_project_root(start: Path) -> Path:
    for path in [start, *start.parents]:
        if (path / "KraftonEngine").exists():
            return path
    raise RuntimeError("Could not find project root containing KraftonEngine.")


def lua_string_to_python(value: str) -> str:
    value = value.strip()
    if value.startswith("[[") and value.endswith("]]"):
        return value[2:-2]
    if len(value) >= 2 and value[0] in ("\"", "'") and value[-1] == value[0]:
        try:
            return ast.literal_eval(value)
        except (SyntaxError, ValueError):
            return value[1:-1]
    return value


def parse_dialogue_lua(path: Path) -> dict:
    text = path.read_text(encoding="utf-8")
    dialogue_id = "Dialogue"
    scene = ""

    entries_start_match = re.search(r"entries\s*=", text)
    header_text = text[:entries_start_match.start()] if entries_start_match else text
    for match in FIELD_RE.finditer(header_text):
        key = match.group("key")
        value = lua_string_to_python(match.group("value"))
        if key == "id":
            dialogue_id = value
        elif key == "scene":
            scene = value

    entries_block_match = re.search(r"entries\s*=\s*\{(?P<entries>.*)\}\s*}\s*$", text, re.DOTALL)
    if entries_block_match is None:
        raise RuntimeError(f"No entries table found: {path}")

    entries = []
    for entry_match in ENTRY_RE.finditer(entries_block_match.group("entries")):
        body = entry_match.group("body")
        fields = {}
        for field_match in FIELD_RE.finditer(body):
            key = field_match.group("key")
            raw_value = field_match.group("value")
            value = lua_string_to_python(raw_value)
            if re.fullmatch(r"[-+]?\d+(?:\.\d+)?", raw_value.strip()):
                value = float(raw_value)
            fields[key] = value

        if "text" in fields:
            fields["index"] = len(entries) + 1
            if not fields.get("id"):
                fields["id"] = f"{dialogue_id}_{fields['index']:04d}"
            entries.append(fields)

    return {
        "id": dialogue_id,
        "scene": scene,
        "entries": entries,
    }


def sanitize_filename(value: str) -> str:
    value = re.sub(r"[^A-Za-z0-9가-힣_-]+", "_", value.strip())
    value = value.strip("_")
    return value or "Unknown"


def write_lua_manifest(path: Path, dialogue_id: str, generated_entries: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "return {",
        f"    id = \"{dialogue_id}\",",
        "    by_id = {",
    ]
    for entry in generated_entries:
        lines.extend([
            f"        [\"{entry['id']}\"] = {{",
            f"            index = {entry['index']},",
            f"            speaker = \"{entry['speaker']}\",",
            f"            key = \"{entry['key']}\",",
            f"            path = \"{entry['path']}\",",
            f"            volume = {entry['volume']:.3f},",
            f"            duration = {entry.get('duration', 0.0):.3f}",
            "        },",
        ])
    lines.extend([
        "    },",
        "    entries = {",
    ])
    for entry in generated_entries:
        lines.extend([
            "        {",
            f"            id = \"{entry['id']}\",",
            f"            index = {entry['index']},",
            f"            speaker = \"{entry['speaker']}\",",
            f"            key = \"{entry['key']}\",",
            f"            path = \"{entry['path']}\",",
            f"            volume = {entry['volume']:.3f},",
            f"            duration = {entry.get('duration', 0.0):.3f}",
            "        },",
        ])
    lines.extend([
        "    }",
        "}",
        "",
    ])
    path.write_text("\n".join(lines), encoding="utf-8")


def elevenlabs_tts(api_key: str, voice_id: str, model_id: str, output_format: str, text: str, settings: dict) -> bytes:
    url = f"https://api.elevenlabs.io/v1/text-to-speech/{voice_id}?output_format={output_format}"
    payload = {
        "text": text,
        "model_id": model_id,
        "voice_settings": {
            "stability": float(settings.get("stability", 0.5)),
            "similarity_boost": float(settings.get("similarity_boost", 0.75)),
            "style": float(settings.get("style", 0.0)),
            "speed": float(settings.get("speed", 1.0)),
            "use_speaker_boost": bool(settings.get("use_speaker_boost", True)),
        },
    }
    request = urllib.request.Request(
        url,
        data=json.dumps(payload, ensure_ascii=False).encode("utf-8"),
        headers={
            "xi-api-key": api_key,
            "Content-Type": "application/json",
            "Accept": "audio/mpeg",
        },
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=90) as response:
        return response.read()


def edge_tts_to_file(text: str, output_path: Path, settings: dict) -> None:
    voice = str(settings.get("voice", "")).strip()
    if not voice:
        raise RuntimeError("edge provider requires a voice value, for example en-US-GuyNeural.")

    command = [
        sys.executable,
        "-m",
        "edge_tts",
        "--voice",
        voice,
        "--text",
        text,
        "--write-media",
        str(output_path),
    ]

    rate = str(settings.get("rate", "")).strip()
    pitch = str(settings.get("pitch", "")).strip()
    edge_volume = str(settings.get("edge_volume", "")).strip()
    if rate:
        command.extend(["--rate", rate])
    if pitch:
        command.extend(["--pitch", pitch])
    if edge_volume:
        command.extend(["--volume", edge_volume])

    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
    if result.returncode == 0:
        return

    detail = (result.stderr or result.stdout or "").strip()
    if "No module named edge_tts" in detail:
        raise RuntimeError("edge-tts is not installed. Run: python -m pip install edge-tts")
    raise RuntimeError(f"edge-tts failed: {detail}")


def get_ffmpeg_exe() -> str | None:
    configured = os.environ.get("FFMPEG_PATH", "").strip()
    if configured:
        return configured

    found = shutil.which("ffmpeg")
    if found:
        return found

    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except Exception:
        return None


def run_audio_postprocess(audio_path: Path, settings: dict) -> None:
    audio_filter = str(settings.get("postprocess_filter", "")).strip()
    filter_complex = str(settings.get("postprocess_filter_complex", "")).strip()
    if not audio_filter and not filter_complex:
        return

    ffmpeg = get_ffmpeg_exe()
    if not ffmpeg:
        raise RuntimeError("ffmpeg is required for postprocess filters. Run: python -m pip install imageio-ffmpeg")

    temp_path = audio_path.with_name(audio_path.stem + ".postprocess" + audio_path.suffix)
    command = [
        ffmpeg,
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        str(audio_path),
    ]

    if filter_complex:
        command.extend(["-filter_complex", filter_complex])
    else:
        command.extend(["-af", audio_filter])

    command.extend([str(temp_path)])

    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
    if result.returncode != 0:
        detail = (result.stderr or result.stdout or "").strip()
        raise RuntimeError(f"ffmpeg postprocess failed: {detail}")

    temp_path.replace(audio_path)


def get_mp3_duration_seconds(audio_path: Path) -> float | None:
    data = audio_path.read_bytes()
    offset = 0
    if len(data) >= 10 and data[:3] == b"ID3":
        size = 0
        for value in data[6:10]:
            size = (size << 7) | (value & 0x7F)
        offset = 10 + size

    bitrates = {
        ("1", 3): [0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448],
        ("1", 2): [0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384],
        ("1", 1): [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320],
        ("2", 3): [0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256],
        ("2", 2): [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160],
        ("2", 1): [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160],
    }
    sample_rates = {
        "1": [44100, 48000, 32000],
        "2": [22050, 24000, 16000],
        "2.5": [11025, 12000, 8000],
    }

    duration = 0.0
    frames = 0
    i = offset
    while i + 4 <= len(data):
        if data[i] != 0xFF or (data[i + 1] & 0xE0) != 0xE0:
            i += 1
            continue

        header = int.from_bytes(data[i:i + 4], byteorder="big")
        version_bits = (header >> 19) & 0x03
        layer_bits = (header >> 17) & 0x03
        bitrate_index = (header >> 12) & 0x0F
        sample_rate_index = (header >> 10) & 0x03
        padding = (header >> 9) & 0x01

        if version_bits == 1 or layer_bits == 0 or bitrate_index in (0, 15) or sample_rate_index == 3:
            i += 1
            continue

        version = {3: "1", 2: "2", 0: "2.5"}[version_bits]
        layer = layer_bits
        bitrate_version = "1" if version == "1" else "2"
        bitrate = bitrates[(bitrate_version, layer)][bitrate_index] * 1000
        sample_rate = sample_rates[version][sample_rate_index]

        if layer == 3:
            frame_size = int(((12 * bitrate) / sample_rate + padding) * 4)
            samples_per_frame = 384
        elif layer == 2:
            frame_size = int((144 * bitrate) / sample_rate + padding)
            samples_per_frame = 1152
        else:
            coefficient = 144 if version == "1" else 72
            frame_size = int((coefficient * bitrate) / sample_rate + padding)
            samples_per_frame = 1152 if version == "1" else 576

        if frame_size <= 0:
            i += 1
            continue

        duration += samples_per_frame / sample_rate
        frames += 1
        i += frame_size

    if frames == 0:
        return None
    return duration


def get_audio_duration_seconds(audio_path: Path) -> float | None:
    if audio_path.suffix.lower() == ".mp3":
        return get_mp3_duration_seconds(audio_path)
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate voice files from .dialogue.lua.")
    parser.add_argument("dialogue_lua", type=Path)
    parser.add_argument("voice_map", type=Path)
    parser.add_argument("--project-root", type=Path, default=None)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--overwrite", action="store_true")
    parser.add_argument("--require-all-speakers", action="store_true")
    parser.add_argument("--sleep", type=float, default=0.25)
    args = parser.parse_args()

    dialogue_path = args.dialogue_lua.resolve()
    voice_map_path = args.voice_map.resolve()
    project_root = args.project_root.resolve() if args.project_root else find_project_root(Path.cwd().resolve())

    if not dialogue_path.exists():
        raise FileNotFoundError(dialogue_path)
    if not voice_map_path.exists():
        raise FileNotFoundError(voice_map_path)

    dialogue = parse_dialogue_lua(dialogue_path)
    voice_map = json.loads(voice_map_path.read_text(encoding="utf-8"))
    speakers = voice_map.get("speakers", {})
    default_provider = voice_map.get("default_provider", "edge")
    model_id = voice_map.get("default_model_id", "eleven_multilingual_v2")
    output_format = voice_map.get("output_format", "mp3_44100_128")
    tts_text_field = voice_map.get("tts_text_field", "text_en")
    ext = "mp3" if output_format.startswith("mp3") else "wav"
    api_key = os.environ.get("ELEVENLABS_API_KEY", "").strip()

    dry_run = args.dry_run
    audio_dir = project_root / "KraftonEngine" / "Content" / "Audio" / "Dialogues" / dialogue["id"]
    manifest_path = project_root / "KraftonEngine" / "Content" / "Script" / "Dialogue" / "Generated" / f"{dialogue['id']}.voices.lua"

    generated = []
    missing_speakers = set()

    print(f"Dialogue: {dialogue['id']} ({len(dialogue['entries'])} entries)")
    if dry_run:
        print("Mode: dry-run")

    for entry in dialogue["entries"]:
        speaker = str(entry.get("speaker", "")).strip() or "UNKNOWN"
        display_text = str(entry.get("text", "")).strip()
        tts_text = str(entry.get(tts_text_field, "")).strip()
        if not tts_text:
            continue

        entry_id = str(entry.get("id", f"{dialogue['id']}_{entry['index']:04d}")).strip()
        speaker_config = speakers.get(speaker)
        voice_id = ""
        if speaker_config is not None:
            if speaker_config.get("enabled", True) is False:
                continue
            voice_id = str(speaker_config.get("voice_id", "")).strip()
        if speaker_config is None:
            missing_speakers.add(speaker)
            continue

        provider = str(speaker_config.get("provider", default_provider)).strip().lower()
        if provider == "elevenlabs" and (not voice_id or voice_id.startswith("replace_with_")):
            missing_speakers.add(speaker)
            continue

        model_id_for_entry = speaker_config.get("model_id", model_id)
        filename = f"{entry['index']:04d}_{sanitize_filename(entry_id)}_{sanitize_filename(speaker)}.{ext}"
        audio_path = audio_dir / filename
        relative_audio_path = f"Dialogues/{dialogue['id']}/{filename}"
        key = f"Dialogue:{dialogue['id']}:{entry_id}"
        generated_entry = {
            "id": entry_id,
            "index": entry["index"],
            "speaker": speaker,
            "key": key,
            "path": relative_audio_path,
            "volume": float(speaker_config.get("volume", 1.0)),
        }

        print(f"[{entry['index']:04d}] {entry_id} / {speaker}: {display_text}")
        print(f"       provider: {provider}")
        print(f"       TTS ({tts_text_field}): {tts_text}")
        print(f"       -> {relative_audio_path}")

        if dry_run:
            generated.append(generated_entry)
            continue
        if audio_path.exists() and not args.overwrite:
            duration = get_audio_duration_seconds(audio_path)
            if duration is not None:
                generated_entry["duration"] = duration
                print(f"       duration: {duration:.3f}s")
            generated.append(generated_entry)
            print("       skipped: exists")
            continue

        audio_dir.mkdir(parents=True, exist_ok=True)
        if provider == "elevenlabs":
            if not api_key:
                raise RuntimeError("ELEVENLABS_API_KEY is required for ElevenLabs provider.")
            try:
                audio = elevenlabs_tts(api_key, voice_id, model_id_for_entry, output_format, tts_text, {
                    **voice_map,
                    **speaker_config,
                })
            except urllib.error.HTTPError as exc:
                detail = exc.read().decode("utf-8", errors="replace")
                raise RuntimeError(f"ElevenLabs failed for entry {entry['index']}: HTTP {exc.code}\n{detail}") from exc
            audio_path.write_bytes(audio)
        elif provider == "edge":
            edge_tts_to_file(tts_text, audio_path, speaker_config)
        else:
            raise RuntimeError(f"Unsupported TTS provider '{provider}' for speaker {speaker}.")

        run_audio_postprocess(audio_path, speaker_config)
        duration = get_audio_duration_seconds(audio_path)
        if duration is not None:
            generated_entry["duration"] = duration
            print(f"       duration: {duration:.3f}s")
        generated.append(generated_entry)
        time.sleep(args.sleep)

    if dry_run:
        print(f"Manifest preview: {manifest_path}")
    else:
        write_lua_manifest(manifest_path, dialogue["id"], generated)
        print(f"Manifest: {manifest_path}")

    if missing_speakers:
        print("Missing speaker voice_id values:")
        for speaker in sorted(missing_speakers):
            print(f"  - {speaker}")
        if args.require_all_speakers:
            return 2

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
