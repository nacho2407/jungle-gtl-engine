#!/usr/bin/env python3
"""
Generate lightweight reflection outputs for KraftonEngine reflected headers.

The current ObjectMacros.h expects GENERATED_BODY() to expand through:

    CURRENT_FILE_ID##_GENERATED_BODY

This script scans headers under Source/, finds reflected declarations that use
GENERATED_BODY(), and writes matching generated headers under Intermediate/Generated
while preserving the source-relative directory layout. It also parses simple
UPROPERTY(...) member declarations and writes one aggregate
Intermediate/Generated/Reflection.generated.cpp file that defines generated
RegisterProperties functions.
"""

from __future__ import annotations

import argparse
import os
import re
from dataclasses import dataclass
from pathlib import Path


REFLECTED_DECL_RE = re.compile(
    r"\bU(?P<kind>CLASS|STRUCT)\s*\([^)]*\)\s*"
    r"(?P<decl>class|struct)\s+"
    r"(?:(?:[A-Z_][A-Z0-9_]*|final|abstract)\s+)*"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)",
    re.MULTILINE,
)

CLASS_DECL_RE = re.compile(
    r"\b(?P<decl>class|struct)\s+"
    r"(?:(?:[A-Z_][A-Z0-9_]*|final|abstract)\s+)*"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)",
    re.MULTILINE,
)

DECLARE_CLASS_RE = re.compile(
    r"\bDECLARE_CLASS\s*\(\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*,",
    re.MULTILINE,
)


@dataclass(frozen=True)
class ReflectedProperty:
    owner: str
    cpp_type: str
    member_name: str
    display_name: str
    category: str
    property_type: str
    flags: str
    metadata: tuple[tuple[str, str], ...]
    min_value: str
    max_value: str
    speed_value: str
    enum_names: str
    enum_count: str
    enum_size: str
    struct_func: str


@dataclass(frozen=True)
class ReflectedType:
    name: str
    properties: tuple[ReflectedProperty, ...]


@dataclass(frozen=True)
class ReflectedHeader:
    header: Path
    generated_header: Path
    file_id: str
    class_names: tuple[str, ...]
    types: tuple[ReflectedType, ...]


TYPE_MAP = {
    "bool": "Bool",
    "uint8": "ByteBool",
    "int": "Int",
    "int32": "Int",
    "float": "Float",
    "FVector": "Vec3",
    "FRotator": "Rotator",
    "FVector4": "Vec4",
    "FColor": "Color4",
    "FString": "String",
    "std::string": "String",
    "FName": "Name",
    "FMaterialSlot": "MaterialSlot",
    "TArray<FVector>": "Vec3Array",
    "TArray<FMaterialSlot>": "MaterialSlotArray",
}


def strip_comments(text: str) -> str:
    """Remove C/C++ comments while preserving line structure for simple scans."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.DOTALL)
    text = re.sub(r"//.*", "", text)
    return text


def find_matching(text: str, start: int, open_char: str, close_char: str) -> int:
    depth = 0
    in_string: str | None = None
    escape = False

    for index in range(start, len(text)):
        ch = text[index]
        if in_string:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == in_string:
                in_string = None
            continue

        if ch == '"' or ch == "'":
            in_string = ch
        elif ch == open_char:
            depth += 1
        elif ch == close_char:
            depth -= 1
            if depth == 0:
                return index

    return -1


def split_metadata_args(args: str) -> list[str]:
    parts: list[str] = []
    current: list[str] = []
    depth = 0
    in_string: str | None = None
    escape = False

    for ch in args:
        if in_string:
            current.append(ch)
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == in_string:
                in_string = None
            continue

        if ch == '"' or ch == "'":
            in_string = ch
            current.append(ch)
        elif ch in "([{":
            depth += 1
            current.append(ch)
        elif ch in ")]}":
            depth -= 1
            current.append(ch)
        elif ch == "," and depth == 0:
            part = "".join(current).strip()
            if part:
                parts.append(part)
            current = []
        else:
            current.append(ch)

    part = "".join(current).strip()
    if part:
        parts.append(part)
    return parts


def parse_metadata(args: str) -> tuple[dict[str, str], set[str]]:
    values: dict[str, str] = {}
    flags: set[str] = set()

    for part in split_metadata_args(args):
        if "=" in part:
            key, value = part.split("=", 1)
            key = key.strip().lower()
            value = value.strip()
            if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
                value = value[1:-1]
            values[key] = value
        else:
            flags.add(part.strip().lower())

    return values, flags


def make_property_metadata(metadata: dict[str, str], flags: set[str], member_name: str, display_name: str) -> tuple[tuple[str, str], ...]:
    values = dict(metadata)
    values.setdefault("member", member_name)
    values.setdefault("displayname", display_name)
    for flag in sorted(flags):
        values.setdefault(flag, "true")
    return tuple(sorted(values.items()))


def normalize_cpp_type(cpp_type: str) -> str:
    cpp_type = " ".join(cpp_type.replace("\n", " ").split())
    cpp_type = cpp_type.replace(" *", "*").replace(" &", "&")
    cpp_type = re.sub(r"\b(const|mutable|volatile)\b", "", cpp_type)
    return " ".join(cpp_type.split()).strip()


def infer_property_type(cpp_type: str, metadata: dict[str, str]) -> str:
    explicit_type = metadata.get("type")
    if explicit_type:
        if explicit_type.startswith("EPropertyType::"):
            return explicit_type.split("::", 1)[1]
        return explicit_type

    enum_type = metadata.get("enumtype") or metadata.get("enum")
    if enum_type:
        return "Enum"

    struct_func = metadata.get("structfunc") or metadata.get("struct")
    if struct_func:
        return "Struct"

    normalized = normalize_cpp_type(cpp_type)
    return TYPE_MAP.get(normalized, "String")


def make_flags_expr(flags: set[str]) -> str:
    values: list[str] = []
    if {"edit", "editanywhere", "visibleanywhere", "editdefaultsonly", "editinstanceonly"} & flags:
        values.append("PF_Edit")
    if {"save", "savegame"} & flags:
        values.append("PF_Save")
    if {"readonly", "visibleanywhere"} & flags:
        values.append("PF_ReadOnly")
    if "transient" in flags:
        values.append("PF_Transient")
        values = [value for value in values if value != "PF_Save"]

    return " | ".join(values) if values else "PF_None"


def cpp_string_literal(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def find_reflected_type_bodies(scan_text: str) -> list[tuple[str, int, int]]:
    bodies: list[tuple[str, int, int]] = []
    seen: set[str] = set()

    for match in REFLECTED_DECL_RE.finditer(scan_text):
        class_name = match.group("name")
        brace_start = scan_text.find("{", match.end())
        if brace_start < 0:
            continue
        brace_end = find_matching(scan_text, brace_start, "{", "}")
        if brace_end < 0:
            continue
        seen.add(class_name)
        bodies.append((class_name, brace_start + 1, brace_end))

    for match in CLASS_DECL_RE.finditer(scan_text):
        class_name = match.group("name")
        if class_name in seen:
            continue

        brace_start = scan_text.find("{", match.end())
        if brace_start < 0:
            continue
        brace_end = find_matching(scan_text, brace_start, "{", "}")
        if brace_end < 0:
            continue

        body = scan_text[brace_start + 1:brace_end]
        declare_match = DECLARE_CLASS_RE.search(body)
        if not declare_match:
            continue

        declared_name = declare_match.group("name")
        if declared_name != class_name:
            continue

        seen.add(class_name)
        bodies.append((class_name, brace_start + 1, brace_end))

    return bodies


def parse_member_declaration(declaration: str) -> tuple[str, str] | None:
    declaration = declaration.strip()
    declaration = re.sub(r"\s+", " ", declaration)
    declaration = re.sub(r"^(public|protected|private)\s*:\s*", "", declaration)
    declaration = re.sub(r"\b(static|mutable|constexpr|inline)\b\s*", "", declaration)
    declaration = declaration.split("=", 1)[0].strip()
    declaration = declaration.split(":", 1)[0].strip()

    match = re.match(
        r"(?P<type>[A-Za-z_][A-Za-z0-9_:]*(?:\s*<[^;=(){}]+>)?(?:\s*[*&])?)\s+"
        r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)$",
        declaration,
    )
    if not match:
        return None
    return normalize_cpp_type(match.group("type")), match.group("name")


def parse_uproperties(scan_text: str) -> tuple[dict[str, tuple[ReflectedProperty, ...]], list[str]]:
    properties_by_class: dict[str, tuple[ReflectedProperty, ...]] = {}
    warnings: list[str] = []

    for class_name, body_start, body_end in find_reflected_type_bodies(scan_text):
        body = scan_text[body_start:body_end]
        found: list[ReflectedProperty] = []
        cursor = 0

        while True:
            prop_index = body.find("UPROPERTY", cursor)
            if prop_index < 0:
                break

            paren_start = body.find("(", prop_index)
            if paren_start < 0:
                warnings.append(f"{class_name}: malformed UPROPERTY without metadata list")
                break
            paren_end = find_matching(body, paren_start, "(", ")")
            if paren_end < 0:
                warnings.append(f"{class_name}: malformed UPROPERTY metadata")
                break

            statement_start = paren_end + 1
            semicolon = body.find(";", statement_start)
            if semicolon < 0:
                warnings.append(f"{class_name}: UPROPERTY without following member declaration")
                break

            metadata, flag_tokens = parse_metadata(body[paren_start + 1:paren_end])
            explicit_member = metadata.get("member")
            if explicit_member:
                member_name = explicit_member
                cpp_type = metadata.get("cpptype") or metadata.get("ctype") or metadata.get("type") or "FString"
            else:
                member = parse_member_declaration(body[statement_start:semicolon])
                if not member:
                    warnings.append(f"{class_name}: unsupported UPROPERTY declaration near {body[statement_start:semicolon].strip()!r}")
                    cursor = semicolon + 1
                    continue

                cpp_type, member_name = member

            property_type = infer_property_type(cpp_type, metadata)

            display_name = metadata.get("displayname") or metadata.get("display") or member_name
            category = metadata.get("category") or "Default"
            min_value = metadata.get("min") or metadata.get("clampmin") or metadata.get("uimin") or "0.0f"
            max_value = metadata.get("max") or metadata.get("clampmax") or metadata.get("uimax") or "0.0f"
            speed_value = metadata.get("speed", "0.1f")
            enum_names = metadata.get("enumnames") or metadata.get("names") or "nullptr"
            enum_count = metadata.get("enumcount") or metadata.get("count") or "0"
            enum_type = metadata.get("enumtype") or metadata.get("enum")
            enum_size = f"sizeof({enum_type})" if enum_type else metadata.get("enumsize", "sizeof(int32)")
            struct_func = metadata.get("structfunc") or metadata.get("struct") or "nullptr"

            found.append(
                ReflectedProperty(
                    owner=class_name,
                    cpp_type=cpp_type,
                    member_name=member_name,
                    display_name=display_name,
                    category=category,
                    property_type=property_type,
                    flags=make_flags_expr(flag_tokens),
                    metadata=make_property_metadata(metadata, flag_tokens, member_name, display_name),
                    min_value=min_value,
                    max_value=max_value,
                    speed_value=speed_value,
                    enum_names=enum_names,
                    enum_count=enum_count,
                    enum_size=enum_size,
                    struct_func=struct_func,
                )
            )
            cursor = semicolon + 1

        if found:
            properties_by_class[class_name] = tuple(found)

    return properties_by_class, warnings


def make_file_id(root: Path, header: Path) -> str:
    rel = header.relative_to(root).as_posix()
    file_id = re.sub(r"[^A-Za-z0-9_]", "_", rel)
    if not file_id or file_id[0].isdigit():
        file_id = f"KR_{file_id}"
    return file_id


def make_generated_header_path(root: Path, generated_root: Path, header: Path) -> Path:
    rel = header.relative_to(root)
    return generated_root / rel.with_name(f"{header.stem}.generated.h")


def find_reflected_headers(root: Path, source_dir: Path, generated_root: Path) -> tuple[list[ReflectedHeader], list[str]]:
    reflected: list[ReflectedHeader] = []
    warnings: list[str] = []

    for header in sorted(source_dir.rglob("*.h")):
        if header.name.endswith(".generated.h"):
            continue

        text = header.read_text(encoding="utf-8-sig")
        scan_text = strip_comments(text)
        declarations = [m.group("name") for m in REFLECTED_DECL_RE.finditer(scan_text)]
        properties_by_class, property_warnings = parse_uproperties(scan_text)

        property_types = tuple(
            ReflectedType(name=class_name, properties=properties_by_class[class_name])
            for class_name in sorted(properties_by_class)
        )

        if not declarations:
            for warning in property_warnings:
                warnings.append(f"{header.relative_to(root)}: {warning}")
            if property_types:
                reflected.append(
                    ReflectedHeader(
                        header=header,
                        generated_header=make_generated_header_path(root, generated_root, header),
                        file_id=make_file_id(root, header),
                        class_names=tuple(),
                        types=property_types,
                    )
                )
            continue

        if not re.search(r"\bGENERATED_BODY\s*\(", scan_text):
            warnings.append(f"{header.relative_to(root)}: reflected declaration without GENERATED_BODY()")
            continue

        for warning in property_warnings:
            warnings.append(f"{header.relative_to(root)}: {warning}")

        reflected.append(
            ReflectedHeader(
                header=header,
                generated_header=make_generated_header_path(root, generated_root, header),
                file_id=make_file_id(root, header),
                class_names=tuple(declarations),
                types=tuple(
                    ReflectedType(name=class_name, properties=properties_by_class.get(class_name, tuple()))
                    for class_name in declarations
                ) + tuple(
                    reflected_type
                    for reflected_type in property_types
                    if reflected_type.name not in declarations
                ),
            )
        )

    return reflected, warnings


def render_generated_header(item: ReflectedHeader, root: Path) -> str:
    source_rel = item.header.relative_to(root).as_posix()
    class_list = ", ".join(item.class_names)

    return (
        "// This file is generated by Tools/GenerateHeaders.py. Do not edit manually.\n"
        f"// Source: {source_rel}\n"
        f"// Reflected types: {class_list}\n"
        "#pragma once\n"
        "\n"
        "#undef CURRENT_FILE_ID\n"
        f"#define CURRENT_FILE_ID {item.file_id}\n"
        "\n"
        f"#define {item.file_id}_GENERATED_BODY \\\n"
        "    static void RegisterProperties(UClass* Class);\n"
    )


def render_property(prop: ReflectedProperty) -> str:
    metadata_entries = ", ".join(
        f"{{{cpp_string_literal(key)}, {cpp_string_literal(value)}}}"
        for key, value in prop.metadata
    )

    return (
        "\tClass->AddProperty({\n"
        f"\t\t{cpp_string_literal(prop.member_name)},\n"
        f"\t\tEPropertyType::{prop.property_type},\n"
        f"\t\t{cpp_string_literal(prop.category)},\n"
        f"\t\t{prop.flags},\n"
        f"\t\t[](UObject* Object)->void* {{ return &static_cast<{prop.owner}*>(Object)->{prop.member_name}; }},\n"
        f"\t\t{prop.min_value},\n"
        f"\t\t{prop.max_value},\n"
        f"\t\t{prop.speed_value},\n"
        f"\t\t{prop.enum_names},\n"
        f"\t\t{prop.enum_count},\n"
        f"\t\t{prop.enum_size},\n"
        f"\t\t{prop.struct_func},\n"
        f"\t\t{cpp_string_literal(prop.display_name)},\n"
        f"\t\t{{{metadata_entries}}},\n"
        f"\t\t{cpp_string_literal(prop.owner)}\n"
        "\t});\n"
    )


def render_generated_cpp(reflected: list[ReflectedHeader], root: Path, generated_cpp: Path) -> str:
    generated_types: list[tuple[ReflectedHeader, ReflectedType]] = []
    for item in reflected:
        for reflected_type in item.types:
            if reflected_type.properties:
                generated_types.append((item, reflected_type))

    lines: list[str] = [
        "// This file is generated by Tools/GenerateHeaders.py. Do not edit manually.",
        "#include \"Object/Object.h\"",
    ]

    for item, _ in generated_types:
        include_path = Path(os.path.relpath(item.header, generated_cpp.parent)).as_posix()
        lines.append(f"#include \"{include_path}\"")

    lines.append("")

    if not generated_types:
        lines.append("// No generated UPROPERTY registrations.")
        lines.append("")
        return "\n".join(lines)

    for _, reflected_type in generated_types:
        lines.append(f"void {reflected_type.name}::RegisterProperties(UClass* Class)")
        lines.append("{")
        if reflected_type.properties:
            for prop in reflected_type.properties:
                lines.append(render_property(prop).rstrip())
        else:
            lines.append("\t(void)Class;")
        lines.append("}")
        lines.append("")

    return "\n".join(lines)


def write_if_changed(path: Path, content: str) -> bool:
    if path.exists():
        old = path.read_text(encoding="utf-8")
        if old == content:
            return False

    path.write_text(content, encoding="utf-8", newline="\n")
    return True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate KraftonEngine *.generated.h headers.")
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Project root. Defaults to the parent directory of Tools/.",
    )
    parser.add_argument(
        "--source-dir",
        type=Path,
        default=None,
        help="Header scan directory. Defaults to <root>/Source.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print what would be generated without writing files.",
    )
    parser.add_argument(
        "--generated-cpp",
        type=Path,
        default=None,
        help="Aggregate generated cpp output. Defaults to <root>/Intermediate/Generated/Reflection.generated.cpp.",
    )
    parser.add_argument(
        "--generated-root",
        type=Path,
        default=None,
        help="Generated include root. Defaults to <root>/Intermediate/Generated.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    source_dir = (args.source_dir or root / "Source").resolve()
    generated_root = (args.generated_root or root / "Intermediate" / "Generated").resolve()
    generated_cpp = (args.generated_cpp or generated_root / "Reflection.generated.cpp").resolve()

    if not source_dir.exists():
        print(f"error: source directory does not exist: {source_dir}")
        return 1

    reflected, warnings = find_reflected_headers(root, source_dir, generated_root)

    changed = 0
    for item in reflected:
        if not item.class_names:
            continue
        content = render_generated_header(item, root)
        rel_generated = item.generated_header.relative_to(root)
        if args.dry_run:
            print(f"would generate {rel_generated} for {', '.join(item.class_names)}")
            continue

        item.generated_header.parent.mkdir(parents=True, exist_ok=True)
        if write_if_changed(item.generated_header, content):
            changed += 1
            print(f"generated {rel_generated}")
        else:
            print(f"unchanged {rel_generated}")

    generated_cpp_content = render_generated_cpp(reflected, root, generated_cpp)
    rel_generated_cpp = generated_cpp.relative_to(root)
    if args.dry_run:
        print(f"would generate {rel_generated_cpp}")
    else:
        generated_cpp.parent.mkdir(parents=True, exist_ok=True)
        if write_if_changed(generated_cpp, generated_cpp_content):
            changed += 1
            print(f"generated {rel_generated_cpp}")
        else:
            print(f"unchanged {rel_generated_cpp}")

    for warning in warnings:
        print(f"warning: {warning}")

    action = "would process" if args.dry_run else "processed"
    print(f"{action} {len(reflected)} reflected header(s), {changed} changed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
