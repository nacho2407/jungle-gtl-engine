from __future__ import annotations

import json
import stat
from pathlib import Path


IMAGE_EXTENSIONS = (".dds", ".png", ".jpg", ".jpeg", ".bmp", ".tga")
ATLAS_EXTENSIONS = {
    "Font": ".font",
    "SubUV": ".subuv",
}


def find_same_stem_image(meta_path: Path) -> Path | None:
    target_stem = meta_path.stem.lower()
    for child in meta_path.parent.iterdir():
        if (
            child.is_file()
            and child.stem.lower() == target_stem
            and child.suffix.lower() in IMAGE_EXTENSIONS
        ):
            return child
    return None


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    engine_root = repo_root / "JSEngine"
    asset_root = engine_root / "Asset"

    converted = 0
    removed = 0
    missing_images: list[Path] = []

    for meta_path in sorted(asset_root.rglob("*.meta")):
        try:
            data = json.loads(meta_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            data = {}

        meta_type = data.get("Type")
        atlas_extension = ATLAS_EXTENSIONS.get(meta_type)
        if atlas_extension:
            image_path = find_same_stem_image(meta_path)
            if image_path is None:
                missing_images.append(meta_path)
                continue

            atlas_path = meta_path.with_suffix(atlas_extension)
            atlas_data = {
                "Image": image_path.relative_to(engine_root).as_posix(),
                "Columns": max(1, int(data.get("Columns", 1))),
                "Rows": max(1, int(data.get("Rows", 1))),
            }
            atlas_path.write_text(
                json.dumps(atlas_data, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
            )
            converted += 1

        meta_path.chmod(meta_path.stat().st_mode | stat.S_IWRITE)
        meta_path.unlink()
        removed += 1

    if missing_images:
        print("Skipped atlas metadata without same-stem image:")
        for path in missing_images:
            print(path.relative_to(repo_root).as_posix())
        return 1

    print(f"Converted atlas assets: {converted}")
    print(f"Removed .meta files: {removed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
