#!/usr/bin/env python3
"""
Generate lightweight reflection outputs for KraftonEngine reflected headers.

The current ObjectMacros.h expects GENERATED_BODY() to expand through:

    CURRENT_FILE_ID##_##__LINE__##_GENERATED_BODY

This script scans headers under Source/, finds reflected declarations that use
GENERATED_BODY(), and writes matching generated headers under Intermediate/Generated
while preserving the source-relative directory layout. It also parses simple
UPROPERTY(...) member declarations and writes per-type generated cpp files for
RegisterProperties definitions. Reflection.generated.cpp remains as a small
translation-unit hub that includes those generated cpp files for the project.
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
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)"
    r"(?:\s*:\s*(?:(?:public|protected|private)\s+)?(?P<super>[A-Za-z_][A-Za-z0-9_:]*))?",
    re.MULTILINE,
)

UENUM_RE = re.compile(
    r"\bUENUM\s*\([^)]*\)\s*"
    r"enum\s+(?:class\s+)?"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_:]*)"
    r"(?:\s*:\s*(?P<underlying>[A-Za-z_][A-Za-z0-9_:]*))?\s*{",
    re.MULTILINE,
)

ENUM_SENTINELS = {"COUNT", "MAX", "ActiveCount", "Num", "NUM", "Count"}


@dataclass(frozen=True)
class ReflectedEnum:
    name: str
    header: Path
    underlying_type: str
    entries: tuple[str, ...]


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
    enum_type_name: str | None
    struct_type: str
    asset_type: str | None
    allowed_class: str | None


@dataclass(frozen=True)
class ReflectedType:
    kind: str
    name: str
    super_name: str | None
    generated_body_line: int | None
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
}

ASSET_ALLOWED_CLASS_MAP = {
    "StaticMesh": "UStaticMesh",
    "SkeletalMesh": "USkeletalMesh",
    "Material": "UMaterial",
    "Texture": "UTexture2D",
    "AnimSequence": "UAnimSequence",
    "UAnimSequence": "UAnimSequence",
    "Skeleton": "USkeleton",
    "USkeleton": "USkeleton",
}

ASSET_OBJECT_CLASSES = {
    "UStaticMesh",
    "USkeletalMesh",
    "UMaterial",
    "UTexture2D",
    "UAnimSequence",
    "UAnimSequenceBase",
    "USkeleton",
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


def strip_enum_value(entry: str) -> str:
    entry = entry.strip()
    if not entry:
        return ""
    entry = entry.split("=", 1)[0].strip()
    entry = re.sub(r"\s+UMETA\s*\([^)]*\)\s*$", "", entry).strip()
    return entry


def parse_uenums(header: Path, scan_text: str) -> dict[str, ReflectedEnum]:
    enums: dict[str, ReflectedEnum] = {}

    for match in UENUM_RE.finditer(scan_text):
        enum_name = match.group("name")
        brace_start = scan_text.find("{", match.end() - 1)
        if brace_start < 0:
            continue
        brace_end = find_matching(scan_text, brace_start, "{", "}")
        if brace_end < 0:
            continue

        raw_entries = split_metadata_args(scan_text[brace_start + 1:brace_end])
        entries: list[str] = []
        for raw_entry in raw_entries:
            entry_name = strip_enum_value(raw_entry)
            if not entry_name:
                continue
            if entry_name in ENUM_SENTINELS:
                break
            entries.append(entry_name)

        short_name = enum_name.rsplit("::", 1)[-1]
        info = ReflectedEnum(
            name=enum_name,
            header=header,
            underlying_type=match.group("underlying") or "int32",
            entries=tuple(entries),
        )
        enums[enum_name] = info
        enums[short_name] = info

    return enums


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
        if explicit_type == "Object":
            return "ObjectRef"
        return explicit_type

    enum_type = metadata.get("enumtype") or metadata.get("enum")
    if enum_type:
        return "Enum"

    struct_type = metadata.get("structtype") or metadata.get("struct")
    if struct_type:
        return "Struct"

    normalized = normalize_cpp_type(cpp_type)
    if normalized.startswith("TArray<"):
        return "Array"
    if get_tsubclassof_inner_type(normalized):
        return "ClassRef"
    if get_tobjectptr_inner_type(normalized):
        return "ObjectRef"
    if normalized.endswith("*") and not normalized.endswith("char*"):
        return "ObjectRef"
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
    if {"instanced", "instancedreference"} & flags:
        values.append("PF_InstancedReference")

    return " | ".join(values) if values else "PF_None"


def make_cpp_identifier(value: str) -> str:
    value = re.sub(r"[^A-Za-z0-9_]", "_", value)
    if not value or value[0].isdigit():
        value = f"KR_{value}"
    return value


def cpp_string_literal(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def cpp_optional_string_literal(value: str | None) -> str:
    return cpp_string_literal(value) if value else "nullptr"


def infer_allowed_class(asset_type: str | None, explicit_allowed_class: str | None) -> str | None:
    if explicit_allowed_class:
        return explicit_allowed_class
    if asset_type:
        return ASSET_ALLOWED_CLASS_MAP.get(asset_type)
    return None


def is_soft_object_property(prop: ReflectedProperty) -> bool:
    if prop.property_type == "SoftObjectRef":
        return True
    normalized = normalize_cpp_type(prop.cpp_type)
    return normalized in {"FString", "FSoftObjectPtr"} and bool(prop.asset_type or prop.allowed_class)


def get_soft_object_property_ops(prop: ReflectedProperty) -> str:
    if normalize_cpp_type(prop.cpp_type) == "FSoftObjectPtr":
        return "FSoftObjectProperty::GetSoftObjectPtrOps()"
    return "FSoftObjectProperty::GetStringOps()"


def is_object_property(prop: ReflectedProperty) -> bool:
    return prop.property_type == "ObjectRef"


def get_tobjectptr_inner_type(cpp_type: str) -> str | None:
    normalized = normalize_cpp_type(cpp_type)
    match = re.match(r"TObjectPtr\s*<\s*(?P<inner>.+)\s*>$", normalized)
    return normalize_cpp_type(match.group("inner")) if match else None


def get_tsubclassof_inner_type(cpp_type: str) -> str | None:
    normalized = normalize_cpp_type(cpp_type)
    match = re.match(r"TSubclassOf\s*<\s*(?P<inner>.+)\s*>$", normalized)
    return normalize_cpp_type(match.group("inner")) if match else None


def get_object_property_class(prop: ReflectedProperty) -> str | None:
    if prop.allowed_class:
        return prop.allowed_class

    normalized = normalize_cpp_type(prop.cpp_type)
    object_ptr_inner = get_tobjectptr_inner_type(normalized)
    if object_ptr_inner:
        return object_ptr_inner

    if normalized.endswith("*"):
        return normalized[:-1].strip()
    return None


def get_object_reference_class(cpp_type: str, allowed_class: str | None = None) -> str | None:
    if allowed_class:
        return allowed_class

    normalized = normalize_cpp_type(cpp_type)
    object_ptr_inner = get_tobjectptr_inner_type(normalized)
    if object_ptr_inner:
        return object_ptr_inner

    if normalized.endswith("*"):
        return normalized[:-1].strip()
    return None


def is_asset_object_reference(cpp_type: str, allowed_class: str | None = None) -> bool:
    object_class = get_object_reference_class(cpp_type, allowed_class)
    return object_class in ASSET_OBJECT_CLASSES


def get_object_property_ops(prop: ReflectedProperty) -> str:
    object_class = get_object_property_class(prop) or "UObject"
    if get_tobjectptr_inner_type(prop.cpp_type):
        return f"FObjectProperty::GetObjectPtrOps<{object_class}>()"
    return f"FObjectProperty::GetRawPointerOps<{object_class}>()"


def get_class_property_class(prop: ReflectedProperty) -> str | None:
    if prop.allowed_class:
        return prop.allowed_class

    subclass_inner = get_class_property_class_for_type(prop.cpp_type)
    if subclass_inner:
        return subclass_inner

    return None


def get_class_property_class_for_type(cpp_type: str) -> str | None:
    subclass_inner = get_tsubclassof_inner_type(cpp_type)
    if subclass_inner:
        return subclass_inner

    return None


def get_class_property_ops(prop: ReflectedProperty) -> str:
    return get_class_property_ops_for_type(prop.cpp_type)


def get_class_property_ops_for_type(cpp_type: str) -> str:
    subclass_inner = get_tsubclassof_inner_type(cpp_type)
    if subclass_inner:
        return f"FClassProperty::GetSubclassOfOps<{subclass_inner}>()"
    return "FClassProperty::GetRawClassOps()"


def get_array_element_cpp_type(cpp_type: str) -> str | None:
    normalized = normalize_cpp_type(cpp_type)
    match = re.match(r"TArray\s*<\s*(?P<inner>.+)\s*>$", normalized)
    return normalize_cpp_type(match.group("inner")) if match else None


def get_array_element_property_type(prop: ReflectedProperty) -> str | None:
    element_cpp_type = get_array_element_cpp_type(prop.cpp_type)
    if element_cpp_type:
        if element_cpp_type in {"FString", "FSoftObjectPtr"} and bool(prop.asset_type or prop.allowed_class):
            return "SoftObjectRef"
        if get_tsubclassof_inner_type(element_cpp_type) or element_cpp_type == "UClass*":
            return "ClassRef"
        if get_tobjectptr_inner_type(element_cpp_type) or (element_cpp_type.endswith("*") and not element_cpp_type.endswith("char*")):
            return "ObjectRef"
        if prop.struct_type or prop.metadata and any(key == "struct" or key == "structtype" for key, _ in prop.metadata):
            return "Struct"
        return TYPE_MAP.get(element_cpp_type, "String")
    return None


def get_object_property_class_for_type(cpp_type: str, allowed_class: str | None = None) -> str | None:
    if allowed_class:
        return allowed_class

    normalized = normalize_cpp_type(cpp_type)
    object_ptr_inner = get_tobjectptr_inner_type(normalized)
    if object_ptr_inner:
        return object_ptr_inner

    if normalized.endswith("*"):
        return normalized[:-1].strip()
    return None


def get_object_property_ops_for_type(cpp_type: str) -> str:
    object_class = get_object_property_class_for_type(cpp_type) or "UObject"
    if get_tobjectptr_inner_type(cpp_type):
        return f"FObjectProperty::GetObjectPtrOps<{object_class}>()"
    return f"FObjectProperty::GetRawPointerOps<{object_class}>()"


def build_array_inner_property(
    prop: ReflectedProperty,
    inner_symbol: str,
    element_cpp_type: str,
    element_property_type: str,
    metadata_entries: str,
) -> str:
    inner_name = f"{prop.member_name}_Inner"
    if element_property_type == "SoftObjectRef":
        soft_ops = (
            "FSoftObjectProperty::GetSoftObjectPtrOps()"
            if element_cpp_type == "FSoftObjectPtr"
            else "FSoftObjectProperty::GetStringOps()"
        )
        return (
            f"\tstatic const FSoftObjectProperty {inner_symbol}(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{soft_ops},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)},\n"
            f"\t\t{cpp_optional_string_literal(prop.asset_type)},\n"
            f"\t\t{cpp_optional_string_literal(prop.allowed_class)}\n"
            "\t);\n"
        )

    if element_property_type == "String":
        return (
            f"\tstatic const FStringProperty {inner_symbol}(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t);\n"
        )

    if element_property_type == "Name":
        return (
            f"\tstatic const FNameProperty {inner_symbol}(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t);\n"
        )

    if element_property_type == "ClassRef":
        return (
            f"\tstatic const FClassProperty {inner_symbol}(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{get_class_property_ops_for_type(element_cpp_type)},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)},\n"
            f"\t\t{cpp_optional_string_literal(prop.allowed_class or get_class_property_class_for_type(element_cpp_type))}\n"
            "\t);\n"
        )

    if element_property_type == "ObjectRef":
        return (
            f"\tstatic const FObjectProperty {inner_symbol}(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{get_object_property_ops_for_type(element_cpp_type)},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)},\n"
            f"\t\t{cpp_optional_string_literal(prop.allowed_class or get_object_property_class_for_type(element_cpp_type))}\n"
            "\t);\n"
        )

    if element_property_type == "Struct":
        struct_type = prop.struct_type or f"{element_cpp_type}::StaticStruct()"
        return (
            f"\tstatic const FStructProperty {inner_symbol}(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{struct_type},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t);\n"
        )

    if element_property_type == "Bool":
        return (
            f"\tstatic const FBoolProperty {inner_symbol}(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t);\n"
        )

    if element_property_type in {"Int", "Float"}:
        property_class = "FIntProperty" if element_property_type == "Int" else "FFloatProperty"
        return (
            f"\tstatic const {property_class} {inner_symbol}(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{prop.min_value},\n"
            f"\t\t{prop.max_value},\n"
            f"\t\t{prop.speed_value},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t);\n"
        )

    return (
        f"\tstatic const FGenericProperty {inner_symbol}(\n"
        f"\t\t{cpp_string_literal(inner_name)},\n"
        f"\t\tEPropertyType::{element_property_type},\n"
        f"\t\t{cpp_string_literal(prop.category)},\n"
        f"\t\tPF_None,\n"
        f"\t\t0,\n"
        f"\t\tsizeof({element_cpp_type}),\n"
        f"\t\t{prop.min_value},\n"
        f"\t\t{prop.max_value},\n"
        f"\t\t{prop.speed_value},\n"
        f"\t\t{cpp_string_literal(prop.display_name)},\n"
        f"\t\t{{{metadata_entries}}},\n"
        f"\t\t{cpp_string_literal(prop.owner)}\n"
        "\t);\n"
    )


def find_reflected_type_bodies(scan_text: str) -> list[tuple[str, int, int]]:
    bodies: list[tuple[str, int, int]] = []

    for match in REFLECTED_DECL_RE.finditer(scan_text):
        class_name = match.group("name")
        brace_start = scan_text.find("{", match.end())
        if brace_start < 0:
            continue
        brace_end = find_matching(scan_text, brace_start, "{", "}")
        if brace_end < 0:
            continue
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


def parse_uproperties(scan_text: str, enums: dict[str, ReflectedEnum]) -> tuple[dict[str, tuple[ReflectedProperty, ...]], list[str]]:
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
            enum_type = metadata.get("enumtype") or metadata.get("enum")
            enum_type_name: str | None = None
            if property_type == "Enum" and enum_type and enum_type in enums:
                enum_info = enums[enum_type]
                enum_type_name = enum_info.name
            struct_type = metadata.get("structtype") or metadata.get("struct")
            if property_type == "Struct" and not struct_type:
                struct_type = cpp_type
            struct_type_expr = f"{struct_type}::StaticStruct()" if struct_type and struct_type != "Struct" else "nullptr"
            asset_type = metadata.get("assettype")
            allowed_class = infer_allowed_class(asset_type, metadata.get("allowedclass"))

            if property_type == "ObjectRef" and is_asset_object_reference(cpp_type, allowed_class):
                asset_class = get_object_reference_class(cpp_type, allowed_class) or cpp_type
                warnings.append(
                    f"error: {class_name}.{member_name}: asset UObject reference '{asset_class}' "
                    "must not be reflected as FObjectProperty; use FSoftObjectPtr/FString with AssetType instead"
                )
                cursor = semicolon + 1
                continue

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
                    enum_type_name=enum_type_name,
                    struct_type=struct_type_expr,
                    asset_type=asset_type,
                    allowed_class=allowed_class,
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


def make_generated_header_include(root: Path, header: Path) -> str:
    rel = header.relative_to(root).with_name(f"{header.stem}.generated.h").as_posix()
    return f'#include "{rel}"'


GENERATED_INCLUDE_RE = re.compile(
    r'^[ \t]*#include[ \t]+"(?P<path>[^"]+\.generated\.h)"[ \t]*(?://.*)?$',
    re.MULTILINE,
)


def get_line_start_index(text: str, line_number: int) -> int:
    if line_number <= 1:
        return 0
    index = 0
    for _ in range(line_number - 1):
        next_index = text.find("\n", index)
        if next_index < 0:
            return len(text)
        index = next_index + 1
    return index


def first_reflected_decl_line(scan_text: str) -> int | None:
    first_line: int | None = None
    for match in REFLECTED_DECL_RE.finditer(scan_text):
        line = get_line_number(scan_text, match.start())
        first_line = line if first_line is None else min(first_line, line)
    return first_line


def ensure_generated_header_include(root: Path, header: Path, text: str, scan_text: str) -> tuple[str, bool]:
    if not REFLECTED_DECL_RE.search(scan_text):
        return text, False

    expected_include = make_generated_header_include(root, header)
    matches = list(GENERATED_INCLUDE_RE.finditer(text))
    if matches:
        kept_expected = False
        pieces: list[str] = []
        cursor = 0
        changed = False
        for match in matches:
            pieces.append(text[cursor:match.start()])
            current_line = match.group(0)
            current_include = f'#include "{match.group("path")}"'
            if not kept_expected:
                pieces.append(expected_include)
                kept_expected = True
                changed = changed or current_include != expected_include
            else:
                changed = True
            cursor = match.end()
        pieces.append(text[cursor:])
        return "".join(pieces), changed

    reflected_line = first_reflected_decl_line(scan_text)
    if reflected_line is None:
        return text, False

    insert_at = get_line_start_index(text, reflected_line)
    prefix = text[:insert_at].rstrip()
    suffix = text[insert_at:].lstrip("\r\n")
    separator = "\n\n" if prefix else ""
    return f"{prefix}{separator}{expected_include}\n\n{suffix}", True


def get_line_number(text: str, index: int) -> int:
    return text.count("\n", 0, index) + 1


def find_generated_body_line(scan_text: str, body_start: int, body_end: int) -> int | None:
    match = re.search(r"\bGENERATED_BODY\s*\(", scan_text[body_start:body_end])
    if not match:
        return None
    return get_line_number(scan_text, body_start + match.start())


def find_reflected_headers(
    root: Path,
    source_dir: Path,
    generated_root: Path,
    fix_generated_includes: bool,
    dry_run: bool,
) -> tuple[list[ReflectedHeader], dict[str, ReflectedEnum], list[str]]:
    reflected: list[ReflectedHeader] = []
    warnings: list[str] = []
    header_texts: list[tuple[Path, str, str]] = []
    enums: dict[str, ReflectedEnum] = {}

    for header in sorted(source_dir.rglob("*.h")):
        if header.name.endswith(".generated.h"):
            continue

        text = header.read_text(encoding="utf-8-sig")
        scan_text = strip_comments(text)
        if fix_generated_includes:
            fixed_text, include_changed = ensure_generated_header_include(root, header, text, scan_text)
            if include_changed:
                rel_header = header.relative_to(root)
                if dry_run:
                    print(f"would update generated include in {rel_header}")
                else:
                    header.write_text(fixed_text, encoding="utf-8", newline="\n")
                    print(f"updated generated include in {rel_header}")
                text = fixed_text
                scan_text = strip_comments(text)
        header_texts.append((header, text, scan_text))
        enums.update(parse_uenums(header, scan_text))

    for header, text, scan_text in header_texts:
        reflected_decls: list[tuple[str, str, str | None, int | None]] = []
        for match in REFLECTED_DECL_RE.finditer(scan_text):
            class_name = match.group("name")
            brace_start = scan_text.find("{", match.end())
            if brace_start < 0:
                reflected_decls.append((match.group("kind"), class_name, match.group("super"), None))
                continue
            brace_end = find_matching(scan_text, brace_start, "{", "}")
            line = find_generated_body_line(scan_text, brace_start + 1, brace_end) if brace_end >= 0 else None
            reflected_decls.append((match.group("kind"), class_name, match.group("super"), line))

        declarations = [name for _, name, _, _ in reflected_decls]
        properties_by_class, property_warnings = parse_uproperties(scan_text, enums)

        property_types = tuple(
            ReflectedType(
                kind="CLASS",
                name=class_name,
                super_name=None,
                generated_body_line=None,
                properties=properties_by_class[class_name],
            )
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
                    ReflectedType(
                        kind=kind,
                        name=class_name,
                        super_name=super_name,
                        generated_body_line=generated_body_line,
                        properties=properties_by_class.get(class_name, tuple()),
                    )
                    for kind, class_name, super_name, generated_body_line in reflected_decls
                ) + tuple(
                    reflected_type
                    for reflected_type in property_types
                    if reflected_type.name not in declarations
                ),
            )
        )

    unique_enums = {enum.name: enum for enum in enums.values()}
    return reflected, unique_enums, warnings


def render_generated_header(item: ReflectedHeader, root: Path) -> str:
    source_rel = item.header.relative_to(root).as_posix()
    class_list = ", ".join(item.class_names)

    lines: list[str] = [
        "// This file is generated by Tools/GenerateHeaders.py. Do not edit manually.",
        f"// Source: {source_rel}",
        f"// Reflected types: {class_list}",
        "#pragma once",
        "",
        "#undef CURRENT_FILE_ID",
        f"#define CURRENT_FILE_ID {item.file_id}",
        "",
    ]

    for reflected_type in item.types:
        if reflected_type.name not in item.class_names or reflected_type.generated_body_line is None:
            continue

        macro_name = f"{item.file_id}_{reflected_type.generated_body_line}_GENERATED_BODY"
        if reflected_type.kind == "CLASS" and reflected_type.super_name:
            lines.extend(
                [
                    f"#define {macro_name} \\",
                    "public: \\",
                    f"    using Super = {reflected_type.super_name}; \\",
                    "    static UClass StaticClassInstance; \\",
                    "    static FClassRegistrar s_Registrar; \\",
                    "    static UClass* StaticClass() { return &StaticClassInstance; } \\",
                    "    UClass* GetClass() const override { return StaticClass(); } \\",
                    "    static void RegisterProperties(UStruct* Struct);",
                    "",
                ]
            )
        elif reflected_type.kind == "STRUCT":
            lines.extend(
                [
                    f"#define {macro_name} \\",
                    "    static UStruct StaticStructInstance; \\",
                    "    static FStructRegistrar s_StructRegistrar; \\",
                    "    static UStruct* StaticStruct() { return &StaticStructInstance; } \\",
                    "    static void RegisterProperties(UStruct* Struct);",
                    "",
                ]
            )
        else:
            lines.extend(
                [
                    f"#define {macro_name} \\",
                    "    static void RegisterProperties(UStruct* Struct);",
                    "",
                ]
            )

    return "\n".join(lines)


def render_property(prop: ReflectedProperty, index: int) -> str:
    metadata_entries = ", ".join(
        f"{{{cpp_string_literal(key)}, {cpp_string_literal(value)}}}"
        for key, value in prop.metadata
    )
    enum_type_expr = f"FEnum::FindEnumByName({cpp_string_literal(prop.enum_type_name)})" if prop.enum_type_name else "nullptr"
    property_symbol = f"G{make_cpp_identifier(prop.owner)}_{make_cpp_identifier(prop.member_name)}_{index}_Property"
    property_class = get_property_class(prop)

    if property_class == "FEnumProperty":
        return (
            f"\tstatic const FEnumProperty {property_symbol}(\n"
            f"\t\t{cpp_string_literal(prop.member_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\toffsetof({prop.owner}, {prop.member_name}),\n"
            f"\t\tsizeof(static_cast<{prop.owner}*>(nullptr)->{prop.member_name}),\n"
            f"\t\t{enum_type_expr},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t);\n"
            f"\tStruct->AddProperty(&{property_symbol});\n"
        )

    if property_class == "FObjectProperty":
        return (
            f"\tstatic const FObjectProperty {property_symbol}(\n"
            f"\t\t{cpp_string_literal(prop.member_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\toffsetof({prop.owner}, {prop.member_name}),\n"
            f"\t\tsizeof(static_cast<{prop.owner}*>(nullptr)->{prop.member_name}),\n"
            f"\t\t{get_object_property_ops(prop)},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)},\n"
            f"\t\t{cpp_optional_string_literal(get_object_property_class(prop))}\n"
            "\t);\n"
            f"\tStruct->AddProperty(&{property_symbol});\n"
        )

    if property_class == "FClassProperty":
        return (
            f"\tstatic const FClassProperty {property_symbol}(\n"
            f"\t\t{cpp_string_literal(prop.member_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\toffsetof({prop.owner}, {prop.member_name}),\n"
            f"\t\tsizeof(static_cast<{prop.owner}*>(nullptr)->{prop.member_name}),\n"
            f"\t\t{get_class_property_ops(prop)},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)},\n"
            f"\t\t{cpp_optional_string_literal(get_class_property_class(prop))}\n"
            "\t);\n"
            f"\tStruct->AddProperty(&{property_symbol});\n"
        )

    if property_class == "FSoftObjectProperty":
        return (
            f"\tstatic const FSoftObjectProperty {property_symbol}(\n"
            f"\t\t{cpp_string_literal(prop.member_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\toffsetof({prop.owner}, {prop.member_name}),\n"
            f"\t\tsizeof(static_cast<{prop.owner}*>(nullptr)->{prop.member_name}),\n"
            f"\t\t{get_soft_object_property_ops(prop)},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)},\n"
            f"\t\t{cpp_optional_string_literal(prop.asset_type)},\n"
            f"\t\t{cpp_optional_string_literal(prop.allowed_class)}\n"
            "\t);\n"
            f"\tStruct->AddProperty(&{property_symbol});\n"
        )

    if property_class == "FStructProperty":
        return (
            f"\tstatic const FStructProperty {property_symbol}(\n"
            f"\t\t{cpp_string_literal(prop.member_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\toffsetof({prop.owner}, {prop.member_name}),\n"
            f"\t\tsizeof(static_cast<{prop.owner}*>(nullptr)->{prop.member_name}),\n"
            f"\t\t{prop.struct_type},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t);\n"
            f"\tStruct->AddProperty(&{property_symbol});\n"
        )

    if property_class in {"FIntProperty", "FFloatProperty"}:
        return (
            f"\tstatic const {property_class} {property_symbol}(\n"
            f"\t\t{cpp_string_literal(prop.member_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\toffsetof({prop.owner}, {prop.member_name}),\n"
            f"\t\tsizeof(static_cast<{prop.owner}*>(nullptr)->{prop.member_name}),\n"
            f"\t\t{prop.min_value},\n"
            f"\t\t{prop.max_value},\n"
            f"\t\t{prop.speed_value},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t);\n"
            f"\tStruct->AddProperty(&{property_symbol});\n"
        )

    if property_class in {"FBoolProperty", "FStringProperty", "FNameProperty"}:
        return (
            f"\tstatic const {property_class} {property_symbol}(\n"
            f"\t\t{cpp_string_literal(prop.member_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\toffsetof({prop.owner}, {prop.member_name}),\n"
            f"\t\tsizeof(static_cast<{prop.owner}*>(nullptr)->{prop.member_name}),\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t);\n"
            f"\tStruct->AddProperty(&{property_symbol});\n"
        )

    if property_class == "FArrayProperty":
        element_cpp_type = get_array_element_cpp_type(prop.cpp_type)
        element_property_type = get_array_element_property_type(prop)
        if not element_cpp_type or not element_property_type:
            element_cpp_type = "FVector"
            element_property_type = "Vec3"
        inner_property_symbol = f"{property_symbol}_Inner"
        inner_property_source = build_array_inner_property(
            prop,
            inner_property_symbol,
            element_cpp_type,
            element_property_type,
            metadata_entries,
        )
        return (
            inner_property_source +
            f"\tstatic const FArrayProperty {property_symbol}(\n"
            f"\t\t{cpp_string_literal(prop.member_name)},\n"
            f"\t\tEPropertyType::{prop.property_type},\n"
            f"\t\tEPropertyType::{element_property_type},\n"
            f"\t\tFArrayProperty::GetArrayOps<{element_cpp_type}>(),\n"
            f"\t\t&{inner_property_symbol},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\toffsetof({prop.owner}, {prop.member_name}),\n"
            f"\t\tsizeof(static_cast<{prop.owner}*>(nullptr)->{prop.member_name}),\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t);\n"
            f"\tStruct->AddProperty(&{property_symbol});\n"
        )

    return (
        f"\tstatic const FGenericProperty {property_symbol}(\n"
        f"\t\t{cpp_string_literal(prop.member_name)},\n"
        f"\t\tEPropertyType::{prop.property_type},\n"
        f"\t\t{cpp_string_literal(prop.category)},\n"
        f"\t\t{prop.flags},\n"
        f"\t\toffsetof({prop.owner}, {prop.member_name}),\n"
        f"\t\tsizeof(static_cast<{prop.owner}*>(nullptr)->{prop.member_name}),\n"
        f"\t\t{prop.min_value},\n"
        f"\t\t{prop.max_value},\n"
        f"\t\t{prop.speed_value},\n"
        f"\t\t{cpp_string_literal(prop.display_name)},\n"
        f"\t\t{{{metadata_entries}}},\n"
        f"\t\t{cpp_string_literal(prop.owner)}\n"
        "\t);\n"
        f"\tStruct->AddProperty(&{property_symbol});\n"
    )


def get_property_class(prop: ReflectedProperty) -> str:
    if prop.property_type == "Bool":
        return "FBoolProperty"
    if prop.property_type == "String" and not is_soft_object_property(prop):
        return "FStringProperty"
    if prop.property_type == "Name":
        return "FNameProperty"
    if prop.property_type == "Int":
        return "FIntProperty"
    if prop.property_type == "Float":
        return "FFloatProperty"
    if prop.property_type == "Enum":
        return "FEnumProperty"
    if prop.property_type == "Struct":
        return "FStructProperty"
    if prop.property_type == "ClassRef":
        return "FClassProperty"
    if is_object_property(prop):
        return "FObjectProperty"
    if is_soft_object_property(prop):
        return "FSoftObjectProperty"
    if prop.property_type == "Array":
        return "FArrayProperty"
    return "FGenericProperty"


PROPERTY_CLASS_INCLUDES = {
    "FArrayProperty": "Core/Property/ArrayProperty.h",
    "FBoolProperty": "Core/Property/BoolProperty.h",
    "FClassProperty": "Core/Property/ClassProperty.h",
    "FEnumProperty": "Core/Property/EnumProperty.h",
    "FFloatProperty": "Core/Property/NumericProperty.h",
    "FGenericProperty": "Core/Property/GenericProperty.h",
    "FIntProperty": "Core/Property/NumericProperty.h",
    "FNameProperty": "Core/Property/NameProperty.h",
    "FObjectProperty": "Core/Property/ObjectProperty.h",
    "FSoftObjectProperty": "Core/Property/SoftObjectProperty.h",
    "FStringProperty": "Core/Property/StringProperty.h",
    "FStructProperty": "Core/Property/StructProperty.h",
}


def get_array_inner_property_class(prop: ReflectedProperty) -> str | None:
    element_property_type = get_array_element_property_type(prop)
    if not element_property_type:
        return None

    if element_property_type == "SoftObjectRef":
        return "FSoftObjectProperty"
    if element_property_type == "String":
        return "FStringProperty"
    if element_property_type == "Name":
        return "FNameProperty"
    if element_property_type == "Bool":
        return "FBoolProperty"
    if element_property_type == "Int":
        return "FIntProperty"
    if element_property_type == "Float":
        return "FFloatProperty"
    if element_property_type == "ClassRef":
        return "FClassProperty"
    if element_property_type == "ObjectRef":
        return "FObjectProperty"
    if element_property_type == "Struct":
        return "FStructProperty"
    return "FGenericProperty"


def get_property_includes(reflected_type: ReflectedType) -> list[str]:
    includes: set[str] = set()
    for prop in reflected_type.properties:
        property_class = get_property_class(prop)
        include_path = PROPERTY_CLASS_INCLUDES.get(property_class)
        if include_path:
            includes.add(include_path)

        if property_class == "FArrayProperty":
            inner_property_class = get_array_inner_property_class(prop)
            inner_include_path = PROPERTY_CLASS_INCLUDES.get(inner_property_class or "")
            if inner_include_path:
                includes.add(inner_include_path)

    return sorted(includes)


def render_type_registration(reflected_type: ReflectedType) -> str:
    if reflected_type.kind == "STRUCT":
        struct_name = reflected_type.name
        return (
            f"UStruct {struct_name}::StaticStructInstance(\n"
            f"\t\"{struct_name}\",\n"
            "\tnullptr,\n"
            f"\tsizeof({struct_name})\n"
            ");\n"
            f"FStructRegistrar {struct_name}::s_StructRegistrar(&{struct_name}::StaticStructInstance);\n"
            "\n"
            "namespace {\n"
            f"\tstruct {struct_name}_PropertyRegistrar {{\n"
            f"\t\t{struct_name}_PropertyRegistrar() {{\n"
            f"\t\t\t{struct_name}::RegisterProperties({struct_name}::StaticStruct());\n"
            "\t\t}\n"
            "\t};\n"
            f"\t{struct_name}_PropertyRegistrar G{struct_name}_PropertyRegistrar;\n"
            "}\n"
        )

    if not reflected_type.super_name:
        return ""

    class_name = reflected_type.name
    super_name = reflected_type.super_name
    return (
        f"UClass {class_name}::StaticClassInstance(\n"
        f"\t\"{class_name}\",\n"
        f"\t&{super_name}::StaticClassInstance,\n"
        f"\tsizeof({class_name}),\n"
        "\tCF_None\n"
        ");\n"
        f"FClassRegistrar {class_name}::s_Registrar(&{class_name}::StaticClassInstance);\n"
        "\n"
        "namespace {\n"
        f"\tstruct {class_name}_RegisterFactory {{\n"
        f"\t\t{class_name}_RegisterFactory() {{\n"
        "\t\t\tFObjectFactory::Get().Register(\n"
        f"\t\t\t\t\"{class_name}\",\n"
        f"\t\t\t\t[](UObject* InOuter)->UObject* {{ return UObjectManager::Get().CreateObject<{class_name}>(InOuter); }}\n"
        "\t\t\t);\n"
        "\t\t}\n"
        "\t};\n"
        f"\t{class_name}_RegisterFactory G{class_name}_RegisterFactory;\n"
        "}\n"
        "\n"
        "namespace {\n"
        f"\tstruct {class_name}_PropertyRegistrar {{\n"
        f"\t\t{class_name}_PropertyRegistrar() {{\n"
        f"\t\t\t{class_name}::RegisterProperties({class_name}::StaticClass());\n"
        "\t\t}\n"
        "\t};\n"
        f"\t{class_name}_PropertyRegistrar G{class_name}_PropertyRegistrar;\n"
        "}\n"
    )


def collect_generated_types(reflected: list[ReflectedHeader]) -> list[tuple[ReflectedHeader, ReflectedType]]:
    generated_types: list[tuple[ReflectedHeader, ReflectedType]] = []
    for item in reflected:
        for reflected_type in item.types:
            if reflected_type.name in item.class_names and (reflected_type.kind == "STRUCT" or reflected_type.super_name):
                generated_types.append((item, reflected_type))
    return generated_types


def get_generated_type_cpp_path(item: ReflectedHeader, reflected_type: ReflectedType) -> Path:
    return item.generated_header.with_name(f"{reflected_type.name}.generated.cpp")


def get_generated_enum_cpp_path(generated_root: Path) -> Path:
    return generated_root / "EnumRegistry.generated.cpp"


def get_enum_symbol(enum_name: str) -> str:
    return f"G{make_cpp_identifier(enum_name)}"


def render_enum_registry_cpp(enums: dict[str, ReflectedEnum], root: Path, enum_cpp: Path) -> str:
    lines: list[str] = [
        "// This file is generated by Tools/GenerateHeaders.py. Do not edit manually.",
        "// UENUM metadata registration.",
        "#include \"Core/Types/PropertyTypes.h\"",
        "",
    ]

    headers = sorted({enum.header for enum in enums.values()})
    for header in headers:
        include_path = Path(os.path.relpath(header, enum_cpp.parent)).as_posix()
        lines.append(f"#include \"{include_path}\"")

    lines.extend(["", "namespace {"])

    for enum in sorted(enums.values(), key=lambda item: item.name):
        symbol = get_enum_symbol(enum.name)
        if enum.entries:
            entries = ", ".join(cpp_string_literal(entry) for entry in enum.entries)
            lines.append(f"static const char* {symbol}_Names[] = {{{entries}}};")
            lines.append(
                f"static const FEnum {symbol}_Enum = "
                f"{{{cpp_string_literal(enum.name)}, {symbol}_Names, "
                f"static_cast<uint32>(sizeof({symbol}_Names) / sizeof({symbol}_Names[0])), "
                f"sizeof({enum.name})}};"
            )
        else:
            lines.append(
                f"static const FEnum {symbol}_Enum = "
                f"{{{cpp_string_literal(enum.name)}, nullptr, 0, sizeof({enum.name})}};"
            )
        lines.append(f"static FEnumRegistrar {symbol}_Registrar(&{symbol}_Enum);")
        lines.append("")

    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def render_generated_type_cpp(item: ReflectedHeader, reflected_type: ReflectedType, root: Path) -> str:
    generated_cpp = get_generated_type_cpp_path(item, reflected_type)
    source_rel = item.header.relative_to(root).as_posix()
    include_path = Path(os.path.relpath(item.header, generated_cpp.parent)).as_posix()

    lines: list[str] = [
        "// This file is generated by Tools/GenerateHeaders.py. Do not edit manually.",
        f"// Source: {source_rel}",
        "#include \"Object/Reflection/ObjectFactory.h\"",
        "#include <cstddef>",
    ]

    for property_include in get_property_includes(reflected_type):
        lines.append(f"#include \"{property_include}\"")

    lines.extend([
        f"#include \"{include_path}\"",
        "",
    ])

    type_registration = render_type_registration(reflected_type)
    if type_registration:
        lines.append(type_registration.rstrip())
        lines.append("")

    lines.extend(
        [
            f"void {reflected_type.name}::RegisterProperties(UStruct* Struct)",
            "{",
        ]
    )

    for index, prop in enumerate(reflected_type.properties):
        lines.append(render_property(prop, index).rstrip())

    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def render_generated_cpp(reflected: list[ReflectedHeader], enums: dict[str, ReflectedEnum], root: Path, generated_cpp: Path, enum_cpp: Path) -> str:
    generated_types = collect_generated_types(reflected)

    lines: list[str] = [
        "// This file is generated by Tools/GenerateHeaders.py. Do not edit manually.",
        "// Per-type registration definitions live next to their generated headers.",
    ]

    if enums:
        include_path = Path(os.path.relpath(enum_cpp, generated_cpp.parent)).as_posix()
        lines.append(f"#include \"{include_path}\"")

    for item, reflected_type in generated_types:
        type_cpp = get_generated_type_cpp_path(item, reflected_type)
        include_path = Path(os.path.relpath(type_cpp, generated_cpp.parent)).as_posix()
        lines.append(f"#include \"{include_path}\"")

    lines.append("")

    if not generated_types:
        lines.append("// No generated UPROPERTY registrations.")
        lines.append("")
        return "\n".join(lines)

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
        "--no-fix-generated-includes",
        action="store_true",
        help="Do not insert or repair source generated-header includes before generation.",
    )
    parser.add_argument(
        "--generated-cpp",
        type=Path,
        default=None,
        help="Generated cpp hub output. Defaults to <root>/Intermediate/Generated/Reflection.generated.cpp.",
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

    reflected, enums, warnings = find_reflected_headers(
        root,
        source_dir,
        generated_root,
        fix_generated_includes=not args.no_fix_generated_includes,
        dry_run=args.dry_run,
    )

    fatal_errors = [warning for warning in warnings if warning.startswith("error: ") or ": error: " in warning]
    if fatal_errors:
        for warning in warnings:
            prefix = "error" if warning in fatal_errors else "warning"
            text = warning[7:] if warning.startswith("error: ") else warning
            print(f"{prefix}: {text}")
        return 1

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

    for item, reflected_type in collect_generated_types(reflected):
        type_cpp = get_generated_type_cpp_path(item, reflected_type)
        type_cpp_content = render_generated_type_cpp(item, reflected_type, root)
        rel_type_cpp = type_cpp.relative_to(root)
        if args.dry_run:
            print(f"would generate {rel_type_cpp}")
            continue

        type_cpp.parent.mkdir(parents=True, exist_ok=True)
        if write_if_changed(type_cpp, type_cpp_content):
            changed += 1
            print(f"generated {rel_type_cpp}")
        else:
            print(f"unchanged {rel_type_cpp}")

    enum_cpp = get_generated_enum_cpp_path(generated_root)
    enum_cpp_content = render_enum_registry_cpp(enums, root, enum_cpp)
    rel_enum_cpp = enum_cpp.relative_to(root)
    if args.dry_run:
        print(f"would generate {rel_enum_cpp}")
    else:
        enum_cpp.parent.mkdir(parents=True, exist_ok=True)
        if write_if_changed(enum_cpp, enum_cpp_content):
            changed += 1
            print(f"generated {rel_enum_cpp}")
        else:
            print(f"unchanged {rel_enum_cpp}")

    generated_cpp_content = render_generated_cpp(reflected, enums, root, generated_cpp, enum_cpp)
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
