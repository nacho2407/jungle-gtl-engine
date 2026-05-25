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
import hashlib
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


def is_enum_sentinel(entry_name: str) -> bool:
    """Match exact sentinel name or a trailing '_SENTINEL' suffix (e.g. PSA_MAX, ELT_NUM)."""
    if entry_name in ENUM_SENTINELS:
        return True
    if "_" in entry_name and entry_name.rsplit("_", 1)[-1] in ENUM_SENTINELS:
        return True
    return False


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
class ReflectedFunctionParam:
    cpp_type: str
    storage_cpp_type: str
    name: str
    default_value: str | None
    property: ReflectedProperty


@dataclass(frozen=True)
class ReflectedFunction:
    owner: str
    return_type: str
    return_storage_cpp_type: str | None
    return_property: ReflectedProperty | None
    name: str
    signature: str
    identifier: str
    flags: str
    display_name: str
    category: str
    metadata: tuple[tuple[str, str], ...]
    params: tuple[ReflectedFunctionParam, ...]
    is_const: bool
    is_static: bool


@dataclass(frozen=True)
class ReflectedType:
    kind: str
    name: str
    super_name: str | None
    generated_body_line: int | None
    properties: tuple[ReflectedProperty, ...]
    functions: tuple[ReflectedFunction, ...]


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
    "AnimGraphAsset": "UAnimGraphAsset",
    "UAnimGraphAsset": "UAnimGraphAsset",
    "ParticleSystem": "UParticleSystem",
    "UParticleSystem": "UParticleSystem",
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
    "UAnimGraphAsset",
    "UParticleSystem",
    "USkeleton",
}

SUPPORTED_PROPERTY_TYPES = {
    "Bool",
    "ByteBool",
    "Int",
    "Float",
    "Vec3",
    "Vec4",
    "Rotator",
    "String",
    "Name",
    "ObjectRef",
    "Color4",
    "ClassRef",
    "Enum",
    "Struct",
    "SoftObjectRef",
    "Array",
}

FUNCTION_FLAG_MAP = {
    "callable": ("FUNC_Callable",),
    "blueprintcallable": ("FUNC_Callable",),
    "pure": ("FUNC_Pure",),
    "blueprintpure": ("FUNC_Pure",),
    "exec": ("FUNC_Exec",),
    "callineditor": ("FUNC_CallInEditor",),
    "event": ("FUNC_Event",),
    "luaevent": ("FUNC_Event",),
    "implementableevent": ("FUNC_Event", "FUNC_ImplementableEvent"),
    "luaimplementableevent": ("FUNC_Event", "FUNC_ImplementableEvent"),
    "blueprintimplementableevent": ("FUNC_Event", "FUNC_ImplementableEvent"),
    "nativeevent": ("FUNC_Event", "FUNC_NativeEvent"),
    "luanativeevent": ("FUNC_Event", "FUNC_NativeEvent"),
    "blueprintnativeevent": ("FUNC_Event", "FUNC_NativeEvent"),
}

ACCESS_FUNCTION_FLAG_MAP = {
    "public": "FUNC_Public",
    "protected": "FUNC_Protected",
    "private": "FUNC_Private",
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
            if is_enum_sentinel(entry_name):
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


def make_function_metadata(metadata: dict[str, str], flags: set[str], function_name: str, display_name: str) -> tuple[tuple[str, str], ...]:
    values = dict(metadata)
    values.setdefault("displayname", display_name)
    for flag in sorted(flags):
        values.setdefault(flag, "true")
    return tuple(sorted(values.items()))


def canonicalize_cpp_type(cpp_type: str) -> str:
    cpp_type = " ".join(cpp_type.replace("\n", " ").split())
    cpp_type = cpp_type.replace(" *", "*").replace("* ", "*").replace(" &", "&").replace("& ", "&")
    cpp_type = cpp_type.replace(" &&", "&&").replace("&& ", "&&")
    return " ".join(cpp_type.split()).strip()


def make_function_signature(function_name: str, param_types: tuple[str, ...], is_const: bool) -> str:
    params = ",".join(canonicalize_cpp_type(param_type) for param_type in param_types)
    suffix = " const" if is_const else ""
    return f"{function_name}({params}){suffix}"


def make_function_identifier(owner: str, signature: str) -> str:
    digest = hashlib.sha1(signature.encode("utf-8")).hexdigest()[:10]
    return f"{make_cpp_identifier(owner)}_{make_cpp_identifier(signature)}_{digest}"


def make_function_flags_expr(flag_tokens: set[str], is_const: bool, is_static: bool, access: str) -> str:
    values: list[str] = []
    access_flag = ACCESS_FUNCTION_FLAG_MAP.get(access)
    if access_flag:
        values.append(access_flag)
    if is_static:
        values.append("FUNC_Static")
    if is_const:
        values.append("FUNC_Const")

    for token in sorted(flag_tokens):
        mapped_flags = FUNCTION_FLAG_MAP.get(token, tuple())
        for mapped in mapped_flags:
            if mapped and mapped not in values:
                values.append(mapped)

    if "FUNC_Callable" not in values and "FUNC_Event" not in values:
        values.append("FUNC_Callable")

    return " | ".join(values) if values else "FUNC_None"


def split_default_argument(param: str) -> tuple[str, str | None]:
    depth = 0
    in_string: str | None = None
    escape = False
    for index, ch in enumerate(param):
        if in_string:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == in_string:
                in_string = None
            continue
        if ch in {'"', "'"}:
            in_string = ch
        elif ch in "(<[{":
            depth += 1
        elif ch in ")>]}":
            depth -= 1
        elif ch == "=" and depth == 0:
            default_value = param[index + 1:].strip()
            return param[:index].strip(), default_value or None
    return param.strip(), None


def make_parameter_flags_expr(cpp_type: str, is_return: bool = False) -> str:
    if is_return:
        return "PF_ReturnParm"

    canonical = canonicalize_cpp_type(cpp_type)
    is_const = bool(re.search(r"(^|\W)const(\W|$)", canonical))
    is_reference = canonical.endswith("&") and not canonical.endswith("&&")

    values = ["PF_Parm"]
    if is_const:
        values.append("PF_ConstParm")
    if is_reference:
        values.append("PF_ReferenceParm")
        if not is_const:
            values.append("PF_OutParm")
    return " | ".join(values)


def get_access_at(body: str, position: int, default_access: str = "private") -> str:
    access = default_access
    for match in re.finditer(r"(^|[;{}])\s*(public|protected|private)\s*:", body[:position], re.MULTILINE):
        access = match.group(2)
    return access


def get_reflected_default_access_by_name(scan_text: str) -> dict[str, str]:
    defaults: dict[str, str] = {}
    for match in REFLECTED_DECL_RE.finditer(scan_text):
        defaults[match.group("name")] = "public" if match.group("decl") == "struct" else "private"
    return defaults


def normalize_cpp_type(cpp_type: str) -> str:
    cpp_type = " ".join(cpp_type.replace("\n", " ").split())
    cpp_type = cpp_type.replace(" *", "*").replace(" &", "&")
    cpp_type = re.sub(r"\b(const|mutable|volatile)\b", "", cpp_type)

    # Support C++ elaborated type specifiers in reflected declarations:
    #   struct FRawDistributionVector StartVelocity;
    #   class UCameraComponent* ActiveCamera;
    #   enum ESomeEnum Value;
    # Reflection registration needs the actual type name, not the elaborated keyword.
    cpp_type = re.sub(r"^(class|struct|enum)\s+(?=[A-Za-z_])", "", cpp_type)

    return " ".join(cpp_type.split()).strip()


def infer_property_type(cpp_type: str, metadata: dict[str, str]) -> str | None:
    explicit_type = metadata.get("type")
    if explicit_type:
        if explicit_type.startswith("EPropertyType::"):
            explicit_type = explicit_type.split("::", 1)[1]
        elif explicit_type == "Object":
            explicit_type = "ObjectRef"
        return explicit_type if explicit_type in SUPPORTED_PROPERTY_TYPES else None

    enum_type = metadata.get("enumtype") or metadata.get("enum")
    if enum_type:
        return "Enum"

    struct_type = metadata.get("structtype") or metadata.get("struct")
    if struct_type:
        return "Struct"

    normalized = normalize_cpp_type(cpp_type)
    if normalized in {"FString", "FSoftObjectPtr"} and (metadata.get("assettype") or metadata.get("allowedclass")):
        return "SoftObjectRef"
    if normalized.startswith("TArray<"):
        return "Array"
    if normalized == "UClass*" or get_tsubclassof_inner_type(normalized):
        return "ClassRef"
    if get_tobjectptr_inner_type(normalized):
        return "ObjectRef"
    if normalized.endswith("*") and not normalized.endswith("char*"):
        return "ObjectRef"
    return TYPE_MAP.get(normalized)


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


def cpp_float_literal(value: str) -> str:
    literal = value.strip()
    if not literal:
        return "0.0f"

    # Normalize an existing f/F suffix in place so "1f" (invalid C++) becomes "1.0f".
    has_f_suffix = literal.endswith(("f", "F"))
    suffix = literal[-1] if has_f_suffix else "f"
    core = literal[:-1] if has_f_suffix else literal

    # Preserve non-literal C++ expressions such as named constants or function calls.
    if not re.match(r"^[+-]?(?:(?:\d+\.\d*)|(?:\.\d+)|(?:\d+))(?:[eE][+-]?\d+)?$", core):
        return literal

    # Integer literals (no dot, no exponent) need ".0" before the f suffix; "0f" is invalid C++.
    if "." not in core and "e" not in core and "E" not in core:
        core = f"{core}.0"
    return f"{core}{suffix}"


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
        # Primitive types must resolve via TYPE_MAP before the Struct fallback so that
        # `TArray<float>` is not mis-classified when struct_type happens to be set elsewhere.
        mapped = TYPE_MAP.get(element_cpp_type)
        if mapped:
            return mapped
        if (prop.struct_type and prop.struct_type != "nullptr") or (prop.metadata and any(key == "struct" or key == "structtype" for key, _ in prop.metadata)):
            return "Struct"
        return None
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
            f"\tnew FSoftObjectProperty(\n"
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
            "\t)"
        )

    if element_property_type == "String":
        return (
            f"\tnew FStringProperty(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t)"
        )

    if element_property_type == "Name":
        return (
            f"\tnew FNameProperty(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t)"
        )

    if element_property_type == "ClassRef":
        return (
            f"\tnew FClassProperty(\n"
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
            "\t)"
        )

    if element_property_type == "ObjectRef":
        return (
            f"\tnew FObjectProperty(\n"
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
            "\t)"
        )

    if element_property_type == "Struct":
        struct_type = prop.struct_type or f"{element_cpp_type}::StaticStruct()"
        return (
            f"\tnew FStructProperty(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{struct_type},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t)"
        )

    if element_property_type == "Bool":
        return (
            f"\tnew FBoolProperty(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t)"
        )

    if element_property_type in {"Int", "Float"}:
        property_class = "FIntProperty" if element_property_type == "Int" else "FFloatProperty"
        return (
            f"\tnew {property_class}(\n"
            f"\t\t{cpp_string_literal(inner_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\tPF_None,\n"
            f"\t\t0,\n"
            f"\t\tsizeof({element_cpp_type}),\n"
            f"\t\t{cpp_float_literal(prop.min_value)},\n"
            f"\t\t{cpp_float_literal(prop.max_value)},\n"
            f"\t\t{cpp_float_literal(prop.speed_value)},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t)"
        )

    return (
        f"\tnew FGenericProperty(\n"
        f"\t\t{cpp_string_literal(inner_name)},\n"
        f"\t\tEPropertyType::{element_property_type},\n"
        f"\t\t{cpp_string_literal(prop.category)},\n"
        f"\t\tPF_None,\n"
        f"\t\t0,\n"
        f"\t\tsizeof({element_cpp_type}),\n"
        f"\t\t{cpp_float_literal(prop.min_value)},\n"
        f"\t\t{cpp_float_literal(prop.max_value)},\n"
        f"\t\t{cpp_float_literal(prop.speed_value)},\n"
        f"\t\t{cpp_string_literal(prop.display_name)},\n"
        f"\t\t{{{metadata_entries}}},\n"
        f"\t\t{cpp_string_literal(prop.owner)}\n"
        "\t)"
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
        r"(?P<type>"
        r"(?:(?:const|volatile)\s+)*"
        r"(?:(?:class|struct|enum)\s+)?"
        r"[A-Za-z_][A-Za-z0-9_:]*"
        r"(?:\s*<[^;=(){}]+>)?"
        r"(?:\s*[*&])?"
        r")\s+"
        r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)$",
        declaration,
    )
    if not match:
        return None
    return normalize_cpp_type(match.group("type")), match.group("name")



def strip_reference_for_storage(cpp_type: str) -> str:
    normalized = normalize_cpp_type(cpp_type)
    while normalized.endswith("&"):
        normalized = normalized[:-1].strip()
    return normalized


def split_function_params(params: str) -> list[str]:
    params = params.strip()
    if not params or params == "void":
        return []
    return split_metadata_args(params)


def parse_function_parameter(param: str, index: int) -> dict[str, object] | None:
    param, default_value = split_default_argument(param)
    param = re.sub(r"\s+", " ", param).strip()
    param = re.sub(r"\b(register|mutable)\b\s*", "", param).strip()
    if not param or param == "void" or "..." in param:
        return None
    if "&&" in param:
        return None

    match = re.match(r"(?P<type>.+?[\s*&])(?P<name>[A-Za-z_][A-Za-z0-9_]*)$", param)
    if match:
        raw_type = canonicalize_cpp_type(match.group("type"))
        name = match.group("name")
    else:
        raw_type = canonicalize_cpp_type(param)
        name = f"Param{index}"

    return {
        "cpp_type": raw_type,
        "storage_cpp_type": strip_reference_for_storage(raw_type),
        "name": name,
        "default_value": default_value,
        "flags": make_parameter_flags_expr(raw_type),
    }


def make_reflected_property_for_type(
    owner: str,
    cpp_type: str,
    member_name: str,
    display_name: str,
    category: str,
    metadata: dict[str, str],
    flag_tokens: set[str],
    enums: dict[str, ReflectedEnum],
    known_structs: set[str],
    flags_expr: str = "PF_None",
) -> tuple[ReflectedProperty | None, str | None]:
    storage_cpp_type = strip_reference_for_storage(cpp_type)
    property_type = infer_property_type(storage_cpp_type, metadata)
    enum_type = metadata.get("enumtype") or metadata.get("enum")
    struct_type = metadata.get("structtype") or metadata.get("struct")

    if not property_type:
        if storage_cpp_type in enums:
            property_type = "Enum"
            enum_type = storage_cpp_type
        elif storage_cpp_type in known_structs:
            property_type = "Struct"
            struct_type = storage_cpp_type

    if not property_type:
        return None, f"unsupported reflected type '{cpp_type}'"

    if property_type == "Array":
        element_cpp_type = get_array_element_cpp_type(storage_cpp_type)
        if element_cpp_type and not struct_type and element_cpp_type in known_structs:
            struct_type = element_cpp_type
        temp_prop = ReflectedProperty(
            owner=owner,
            cpp_type=storage_cpp_type,
            member_name=member_name,
            display_name=display_name,
            category=category,
            property_type=property_type,
            flags=flags_expr,
            metadata=tuple(sorted(metadata.items())),
            min_value=metadata.get("min") or metadata.get("clampmin") or metadata.get("uimin") or "0.0f",
            max_value=metadata.get("max") or metadata.get("clampmax") or metadata.get("uimax") or "0.0f",
            speed_value=metadata.get("speed", "0.1f"),
            enum_type_name=None,
            struct_type=struct_type or "",
            asset_type=metadata.get("assettype"),
            allowed_class=infer_allowed_class(metadata.get("assettype"), metadata.get("allowedclass")),
        )
        if not get_array_element_cpp_type(storage_cpp_type) or not get_array_element_property_type(temp_prop):
            return None, f"unsupported reflected TArray element type in '{cpp_type}'"

    enum_type_name: str | None = None
    if property_type == "Enum":
        enum_key = enum_type or storage_cpp_type
        if enum_key not in enums:
            return None, f"unknown reflected enum '{enum_key}'"
        enum_type_name = enums[enum_key].name

    if property_type == "Struct" and not struct_type:
        struct_type = storage_cpp_type
    if property_type == "Struct" and struct_type not in known_structs:
        return None, f"unknown reflected struct '{struct_type}'"

    struct_type_expr = f"{struct_type}::StaticStruct()" if struct_type and struct_type != "Struct" else "nullptr"
    asset_type = metadata.get("assettype")
    allowed_class = infer_allowed_class(asset_type, metadata.get("allowedclass"))

    return ReflectedProperty(
        owner=owner,
        cpp_type=storage_cpp_type,
        member_name=member_name,
        display_name=display_name,
        category=category,
        property_type=property_type,
        flags=flags_expr,
        metadata=make_property_metadata(metadata, flag_tokens, member_name, display_name),
        min_value=metadata.get("min") or metadata.get("clampmin") or metadata.get("uimin") or "0.0f",
        max_value=metadata.get("max") or metadata.get("clampmax") or metadata.get("uimax") or "0.0f",
        speed_value=metadata.get("speed", "0.1f"),
        enum_type_name=enum_type_name,
        struct_type=struct_type_expr,
        asset_type=asset_type,
        allowed_class=allowed_class,
    ), None

def find_ufunction_declaration(body: str, statement_start: int) -> tuple[str, int] | None:
    """Return the reflected member declaration and the cursor after it.

    UFUNCTION may annotate either a plain declaration ending with ';' or an
    inline definition ending with a matched function body. The reflection parser
    must consume the declaration prefix only; otherwise the first ';' inside an
    inline body such as `{ return Foo; }` is mistaken for the declaration end.
    """
    cursor = statement_start
    while cursor < len(body) and body[cursor].isspace():
        cursor += 1

    depth_angle = 0
    depth_paren = 0
    depth_bracket = 0
    in_string: str | None = None
    escape = False

    for index in range(cursor, len(body)):
        ch = body[index]
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
            continue

        if ch == '<':
            depth_angle += 1
        elif ch == '>' and depth_angle > 0:
            depth_angle -= 1
        elif ch == '(':
            depth_paren += 1
        elif ch == ')' and depth_paren > 0:
            depth_paren -= 1
        elif ch == '[':
            depth_bracket += 1
        elif ch == ']' and depth_bracket > 0:
            depth_bracket -= 1

        if depth_angle == 0 and depth_paren == 0 and depth_bracket == 0:
            if ch == ';':
                declaration = body[cursor:index].strip()
                return (declaration, index + 1) if declaration else None
            if ch == '{':
                body_end = find_matching(body, index, '{', '}')
                if body_end < 0:
                    return None
                declaration = body[cursor:index].strip()
                return (declaration, body_end + 1) if declaration else None

    return None


def parse_member_function_declaration(declaration: str) -> dict[str, object] | None:
    declaration = declaration.strip()
    declaration = re.sub(r"\s+", " ", declaration)
    declaration = re.sub(r"^(public|protected|private)\s*:\s*", "", declaration)
    declaration = re.sub(r"\s*=\s*(0|default|delete)\s*$", "", declaration).strip()
    declaration = re.sub(r"\b(override|final)\b", "", declaration).strip()

    is_static = bool(re.search(r"\bstatic\b", declaration))
    declaration = re.sub(r"\b(virtual|inline|friend|explicit|static)\b\s*", "", declaration).strip()

    # C++ trailing return syntax: auto Foo(Args...) [const] [noexcept] [ref-qual] -> ReturnType
    trailing = re.match(
        r"auto\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\((?P<params>.*)\)\s*"
        r"(?P<quals>(?:(?:const|noexcept(?:\s*\([^)]*\))?|&&|&)\s*)*)"
        r"->\s*(?P<return>.+)$",
        declaration,
    )
    if trailing:
        quals = trailing.group("quals") or ""
        if "&&" in quals:
            return None
        is_const = bool(re.search(r"\bconst\b", quals))
        return_type = canonicalize_cpp_type(trailing.group("return"))
        name = trailing.group("name")
        params_text = trailing.group("params")
    else:
        # Normal syntax: ReturnType Foo(Args...) [const] [noexcept] [ref-qual]
        match = re.match(
            r"(?P<return>.+?)\s+"
            r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\((?P<params>.*)\)\s*"
            r"(?P<quals>(?:(?:const|noexcept(?:\s*\([^)]*\))?|&&|&)\s*)*)$",
            declaration,
        )
        if not match:
            return None
        quals = match.group("quals") or ""
        if "&&" in quals:
            return None
        is_const = bool(re.search(r"\bconst\b", quals))
        return_type = canonicalize_cpp_type(match.group("return"))
        name = match.group("name")
        params_text = match.group("params")

    params: list[dict[str, object]] = []
    for index, param_text in enumerate(split_function_params(params_text)):
        parsed_param = parse_function_parameter(param_text, index)
        if not parsed_param:
            return None
        params.append(parsed_param)

    return {
        "return_type": return_type,
        "name": name,
        "params": tuple(params),
        "is_const": is_const,
        "is_static": is_static,
    }


def parse_ufunctions(
    scan_text: str,
    enums: dict[str, ReflectedEnum],
    known_structs: set[str],
) -> tuple[dict[str, tuple[ReflectedFunction, ...]], list[str]]:
    functions_by_class: dict[str, tuple[ReflectedFunction, ...]] = {}
    warnings: list[str] = []
    default_access_by_name = get_reflected_default_access_by_name(scan_text)

    for class_name, body_start, body_end in find_reflected_type_bodies(scan_text):
        body = scan_text[body_start:body_end]
        found: list[ReflectedFunction] = []
        seen_signatures: set[str] = set()
        cursor = 0

        while True:
            function_index = body.find("UFUNCTION", cursor)
            if function_index < 0:
                break

            # Skip identifier-extended macros like UFUNCTION_DEPRECATED — exact-name only.
            after_name = function_index + len("UFUNCTION")
            if after_name < len(body) and (body[after_name].isalnum() or body[after_name] == "_"):
                cursor = after_name
                continue

            paren_start = body.find("(", function_index)
            if paren_start < 0:
                warnings.append(f"{class_name}: malformed UFUNCTION without metadata list")
                break
            paren_end = find_matching(body, paren_start, "(", ")")
            if paren_end < 0:
                warnings.append(f"{class_name}: malformed UFUNCTION metadata")
                break

            statement_start = paren_end + 1
            declaration_info = find_ufunction_declaration(body, statement_start)
            if not declaration_info:
                warnings.append(f"{class_name}: UFUNCTION without following member declaration")
                break
            declaration_text, declaration_end = declaration_info

            metadata, flag_tokens = parse_metadata(body[paren_start + 1:paren_end])
            parsed = parse_member_function_declaration(declaration_text)
            if not parsed:
                warnings.append(
                    f"error: {class_name}: unsupported UFUNCTION declaration near {declaration_text!r}"
                )
                cursor = declaration_end
                continue

            function_name = str(parsed["name"])
            return_type = str(parsed["return_type"])
            parsed_params = tuple(parsed["params"])  # type: ignore[arg-type]
            param_types = tuple(str(param["cpp_type"]) for param in parsed_params)  # type: ignore[index]
            is_const = bool(parsed["is_const"])
            is_static = bool(parsed["is_static"])
            signature = make_function_signature(function_name, param_types, is_const)
            if signature in seen_signatures:
                warnings.append(f"error: {class_name}.{function_name}: duplicate reflected UFUNCTION signature '{signature}'")
                cursor = declaration_end
                continue
            seen_signatures.add(signature)

            identifier = make_function_identifier(class_name, signature)
            display_name = metadata.get("displayname") or metadata.get("display") or function_name
            category = metadata.get("category") or "Default"
            owner_for_params = f"{identifier}_Params"
            access = get_access_at(body, function_index, default_access_by_name.get(class_name, "private"))
            function_flags = make_function_flags_expr(flag_tokens, is_const, is_static, access)

            function_metadata = dict(metadata)
            function_metadata.setdefault("signature", signature)
            function_metadata.setdefault("access", access)
            function_metadata.setdefault("static", "true" if is_static else "false")
            function_metadata.setdefault("const", "true" if is_const else "false")

            params: list[ReflectedFunctionParam] = []
            for param in parsed_params:  # type: ignore[assignment]
                param_cpp_type = str(param["cpp_type"])
                param_storage_type = str(param["storage_cpp_type"])
                param_name = str(param["name"])
                default_value = param["default_value"]  # type: ignore[index]
                param_metadata: dict[str, str] = {}
                if default_value:
                    param_metadata["defaultvalue"] = str(default_value)
                param_property, error = make_reflected_property_for_type(
                    owner_for_params,
                    param_storage_type,
                    param_name,
                    param_name,
                    category,
                    param_metadata,
                    set(),
                    enums,
                    known_structs,
                    str(param["flags"]),
                )
                if error or not param_property:
                    warnings.append(f"error: {class_name}.{function_name} parameter '{param_name}': {error}")
                    break
                params.append(
                    ReflectedFunctionParam(
                        cpp_type=param_cpp_type,
                        storage_cpp_type=param_storage_type,
                        name=param_name,
                        default_value=str(default_value) if default_value else None,
                        property=param_property,
                    )
                )
            else:
                return_property: ReflectedProperty | None = None
                return_storage_type: str | None = None
                if return_type != "void":
                    if return_type.endswith("&"):
                        warnings.append(
                            f"error: {class_name}.{function_name}: reflected return references are not supported; return by value or pointer"
                        )
                        cursor = declaration_end
                        continue
                    return_storage_type = strip_reference_for_storage(return_type)
                    return_property, error = make_reflected_property_for_type(
                        owner_for_params,
                        return_storage_type,
                        "ReturnValue",
                        "Return Value",
                        category,
                        {},
                        set(),
                        enums,
                        known_structs,
                        make_parameter_flags_expr(return_type, is_return=True),
                    )
                    if error or not return_property:
                        warnings.append(f"error: {class_name}.{function_name} return type: {error}")
                        cursor = declaration_end
                        continue

                found.append(
                    ReflectedFunction(
                        owner=class_name,
                        return_type=return_type,
                        return_storage_cpp_type=return_storage_type,
                        return_property=return_property,
                        name=function_name,
                        signature=signature,
                        identifier=identifier,
                        flags=function_flags,
                        display_name=display_name,
                        category=category,
                        metadata=make_function_metadata(function_metadata, flag_tokens, function_name, display_name),
                        params=tuple(params),
                        is_const=is_const,
                        is_static=is_static,
                    )
                )
            cursor = declaration_end

        if found:
            functions_by_class[class_name] = tuple(found)

    return functions_by_class, warnings


def parse_uproperties(scan_text: str, enums: dict[str, ReflectedEnum], known_structs: set[str]) -> tuple[dict[str, tuple[ReflectedProperty, ...]], list[str]]:
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

            # Skip identifier-extended macros like UPROPERTY_DEPRECATED — exact-name only.
            after_name = prop_index + len("UPROPERTY")
            if after_name < len(body) and (body[after_name].isalnum() or body[after_name] == "_"):
                cursor = after_name
                continue

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
            normalized_cpp_type = strip_reference_for_storage(cpp_type)
            if not property_type:
                if normalized_cpp_type in enums:
                    property_type = "Enum"
                elif normalized_cpp_type in known_structs:
                    property_type = "Struct"
            if not property_type:
                warnings.append(
                    f"error: {class_name}.{member_name}: unsupported reflected property type '{cpp_type}'. "
                    "Add an explicit supported Type=..., Struct=..., Enum=..., or extend TYPE_MAP."
                )
                cursor = semicolon + 1
                continue

            if property_type == "Array":
                element_cpp_type = get_array_element_cpp_type(cpp_type)
                inferred_array_struct_type = metadata.get("structtype") or metadata.get("struct")
                if element_cpp_type and not inferred_array_struct_type and element_cpp_type in known_structs:
                    inferred_array_struct_type = element_cpp_type
                element_property_type = get_array_element_property_type(
                    ReflectedProperty(
                        owner=class_name,
                        cpp_type=cpp_type,
                        member_name=member_name,
                        display_name=member_name,
                        category="Default",
                        property_type=property_type,
                        flags="PF_None",
                        metadata=tuple(sorted(metadata.items())),
                        min_value="0.0f",
                        max_value="0.0f",
                        speed_value="0.1f",
                        enum_type_name=None,
                        struct_type=inferred_array_struct_type or "",
                        asset_type=metadata.get("assettype"),
                        allowed_class=infer_allowed_class(metadata.get("assettype"), metadata.get("allowedclass")),
                    )
                )
                if not element_cpp_type or not element_property_type:
                    warnings.append(
                        f"error: {class_name}.{member_name}: unsupported reflected TArray element type in '{cpp_type}'. "
                        "Add an explicit supported element type or extend TYPE_MAP."
                    )
                    cursor = semicolon + 1
                    continue

            display_name = metadata.get("displayname") or metadata.get("display") or member_name
            category = metadata.get("category") or "Default"
            min_value = metadata.get("min") or metadata.get("clampmin") or metadata.get("uimin") or "0.0f"
            max_value = metadata.get("max") or metadata.get("clampmax") or metadata.get("uimax") or "0.0f"
            speed_value = metadata.get("speed", "0.1f")
            enum_type = metadata.get("enumtype") or metadata.get("enum")
            enum_type_name: str | None = None
            if property_type == "Enum":
                enum_key = enum_type or cpp_type
                if enum_key not in enums:
                    warnings.append(f"error: {class_name}.{member_name}: unknown reflected enum '{enum_key}'")
                    cursor = semicolon + 1
                    continue
                enum_info = enums[enum_key]
                enum_type_name = enum_info.name
            struct_type = metadata.get("structtype") or metadata.get("struct")
            if property_type == "Array":
                element_cpp_type = get_array_element_cpp_type(cpp_type)
                if element_cpp_type and not struct_type and element_cpp_type in known_structs:
                    struct_type = element_cpp_type
            if property_type == "Struct" and not struct_type:
                struct_type = normalized_cpp_type
            if property_type == "Struct" and struct_type not in known_structs:
                warnings.append(f"error: {class_name}.{member_name}: unknown reflected struct '{struct_type}'")
                cursor = semicolon + 1
                continue
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


def detect_newline_style(path: Path, default: str = "\n") -> str:
    try:
        data = path.read_bytes()
    except OSError:
        return default

    crlf = data.count(b"\r\n")
    lf = data.count(b"\n") - crlf
    cr = data.count(b"\r") - crlf

    if crlf >= lf and crlf >= cr and crlf > 0:
        return "\r\n"
    if lf >= cr and lf > 0:
        return "\n"
    if cr > 0:
        return "\r"
    return default


def normalize_newlines_for_write(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def write_text_preserving_newline(path: Path, content: str, default_newline: str = "\n") -> None:
    newline = detect_newline_style(path, default_newline) if path.exists() else default_newline
    path.write_text(normalize_newlines_for_write(content), encoding="utf-8", newline=newline)


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


def ensure_generated_header_include(root: Path, generated_root: Path, header: Path, text: str, scan_text: str) -> tuple[str, bool]:
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
            fixed_text, include_changed = ensure_generated_header_include(root, generated_root, header, text, scan_text)
            if include_changed:
                rel_header = header.relative_to(root)
                if dry_run:
                    print(f"would update generated include in {rel_header}")
                else:
                    write_text_preserving_newline(header, fixed_text)
                    print(f"updated generated include in {rel_header}")
                text = fixed_text
                scan_text = strip_comments(text)
        header_texts.append((header, text, scan_text))
        enums.update(parse_uenums(header, scan_text))

    known_structs: set[str] = set()
    for _, _, scan_text in header_texts:
        for match in REFLECTED_DECL_RE.finditer(scan_text):
            if match.group("kind") == "STRUCT":
                known_structs.add(match.group("name"))

    for header, text, scan_text in header_texts:
        reflected_decls: list[tuple[str, str, str | None, int | None]] = []
        for match in REFLECTED_DECL_RE.finditer(scan_text):
            class_name = match.group("name")
            super_name = match.group("super")
            brace_start = scan_text.find("{", match.end())
            if brace_start < 0:
                reflected_decls.append((match.group("kind"), class_name, super_name, None))
                continue
            brace_end = find_matching(scan_text, brace_start, "{", "}")
            line = find_generated_body_line(scan_text, brace_start + 1, brace_end) if brace_end >= 0 else None
            reflected_decls.append((match.group("kind"), class_name, super_name, line))

        for kind, class_name, super_name, _ in reflected_decls:
            if kind == "STRUCT" and super_name:
                super_short_name = super_name.rsplit("::", 1)[-1]
                if super_name not in known_structs and super_short_name not in known_structs:
                    warnings.append(
                        f"{header.relative_to(root)}: error: {class_name}: USTRUCT super '{super_name}' is not reflected"
                    )

        declarations = [name for _, name, _, _ in reflected_decls]
        properties_by_class, property_warnings = parse_uproperties(scan_text, enums, known_structs)
        functions_by_class, function_warnings = parse_ufunctions(scan_text, enums, known_structs)

        property_types = tuple(
            ReflectedType(
                kind="CLASS",
                name=class_name,
                super_name=None,
                generated_body_line=None,
                properties=properties_by_class.get(class_name, tuple()),
                functions=functions_by_class.get(class_name, tuple()),
            )
            for class_name in sorted(set(properties_by_class) | set(functions_by_class))
        )

        if not declarations:
            for warning in property_warnings + function_warnings:
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

        for warning in property_warnings + function_warnings:
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
                        functions=functions_by_class.get(class_name, tuple()),
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
                    "    static void RegisterProperties(UStruct* Struct); \\",
                    "    static void RegisterFunctions(UStruct* Struct);",
                    "",
                ]
            )
        elif reflected_type.kind == "STRUCT" and reflected_type.super_name:
            lines.extend(
                [
                    f"#define {macro_name} \\",
                    f"    using Super = {reflected_type.super_name}; \\",
                    "    static UStruct StaticStructInstance; \\",
                    "    static FStructRegistrar s_StructRegistrar; \\",
                    "    static UStruct* StaticStruct() { return &StaticStructInstance; } \\",
                    "    static void RegisterProperties(UStruct* Struct); \\",
                    "    static void RegisterFunctions(UStruct* Struct);",
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
                    "    static void RegisterProperties(UStruct* Struct); \\",
                    "    static void RegisterFunctions(UStruct* Struct);",
                    "",
                ]
            )
        else:
            lines.extend(
                [
                    f"#define {macro_name} \\",
                    "    static void RegisterProperties(UStruct* Struct); \\",
                    "    static void RegisterFunctions(UStruct* Struct);",
                    "",
                ]
            )

    return "\n".join(lines)

def render_property_allocation(
    prop: ReflectedProperty,
    index: int,
    container_type: str | None = None,
    member_name: str | None = None,
    reflected_name: str | None = None,
) -> str:
    metadata_entries = ", ".join(
        f"{{{cpp_string_literal(key)}, {cpp_string_literal(value)}}}"
        for key, value in prop.metadata
    )
    enum_type_expr = f"FEnum::FindEnumByName({cpp_string_literal(prop.enum_type_name)})" if prop.enum_type_name else "nullptr"
    property_class = get_property_class(prop)
    owner_type = container_type or prop.owner
    member_expr = member_name or prop.member_name
    property_name = reflected_name or prop.member_name
    offset_expr = f"offsetof({owner_type}, {member_expr})"
    size_expr = f"sizeof(static_cast<{owner_type}*>(nullptr)->{member_expr})"

    if property_class == "FEnumProperty":
        return (
            f"new FEnumProperty(\n"
            f"\t\t{cpp_string_literal(property_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\t{offset_expr},\n"
            f"\t\t{size_expr},\n"
            f"\t\t{enum_type_expr},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t)"
        )

    if property_class == "FObjectProperty":
        return (
            f"new FObjectProperty(\n"
            f"\t\t{cpp_string_literal(property_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\t{offset_expr},\n"
            f"\t\t{size_expr},\n"
            f"\t\t{get_object_property_ops(prop)},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)},\n"
            f"\t\t{cpp_optional_string_literal(get_object_property_class(prop))}\n"
            "\t)"
        )

    if property_class == "FClassProperty":
        return (
            f"new FClassProperty(\n"
            f"\t\t{cpp_string_literal(property_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\t{offset_expr},\n"
            f"\t\t{size_expr},\n"
            f"\t\t{get_class_property_ops(prop)},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)},\n"
            f"\t\t{cpp_optional_string_literal(get_class_property_class(prop))}\n"
            "\t)"
        )

    if property_class == "FSoftObjectProperty":
        return (
            f"new FSoftObjectProperty(\n"
            f"\t\t{cpp_string_literal(property_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\t{offset_expr},\n"
            f"\t\t{size_expr},\n"
            f"\t\t{get_soft_object_property_ops(prop)},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)},\n"
            f"\t\t{cpp_optional_string_literal(prop.asset_type)},\n"
            f"\t\t{cpp_optional_string_literal(prop.allowed_class)}\n"
            "\t)"
        )

    if property_class == "FStructProperty":
        return (
            f"new FStructProperty(\n"
            f"\t\t{cpp_string_literal(property_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\t{offset_expr},\n"
            f"\t\t{size_expr},\n"
            f"\t\t{prop.struct_type},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t)"
        )

    if property_class in {"FIntProperty", "FFloatProperty"}:
        return (
            f"new {property_class}(\n"
            f"\t\t{cpp_string_literal(property_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\t{offset_expr},\n"
            f"\t\t{size_expr},\n"
            f"\t\t{cpp_float_literal(prop.min_value)},\n"
            f"\t\t{cpp_float_literal(prop.max_value)},\n"
            f"\t\t{cpp_float_literal(prop.speed_value)},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t)"
        )

    if property_class in {"FBoolProperty", "FStringProperty", "FNameProperty"}:
        return (
            f"new {property_class}(\n"
            f"\t\t{cpp_string_literal(property_name)},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\t{offset_expr},\n"
            f"\t\t{size_expr},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t)"
        )

    if property_class == "FArrayProperty":
        element_cpp_type = get_array_element_cpp_type(prop.cpp_type)
        element_property_type = get_array_element_property_type(prop)
        if not element_cpp_type or not element_property_type:
            raise RuntimeError(f"unsupported generated array property {prop.owner}.{prop.member_name}: {prop.cpp_type}")
        inner_property_source = build_array_inner_property(
            prop,
            f"G{make_cpp_identifier(prop.owner)}_{make_cpp_identifier(prop.member_name)}_{index}_Property_Inner",
            element_cpp_type,
            element_property_type,
            metadata_entries,
        )
        return (
            f"new FArrayProperty(\n"
            f"\t\t{cpp_string_literal(property_name)},\n"
            f"\t\tEPropertyType::{prop.property_type},\n"
            f"\t\tEPropertyType::{element_property_type},\n"
            f"\t\tFArrayProperty::GetArrayOps<{element_cpp_type}>(),\n"
            f"\t\t{inner_property_source},\n"
            f"\t\t{cpp_string_literal(prop.category)},\n"
            f"\t\t{prop.flags},\n"
            f"\t\t{offset_expr},\n"
            f"\t\t{size_expr},\n"
            f"\t\t{cpp_string_literal(prop.display_name)},\n"
            f"\t\t{{{metadata_entries}}},\n"
            f"\t\t{cpp_string_literal(prop.owner)}\n"
            "\t)"
        )

    return (
        f"new FGenericProperty(\n"
        f"\t\t{cpp_string_literal(property_name)},\n"
        f"\t\tEPropertyType::{prop.property_type},\n"
        f"\t\t{cpp_string_literal(prop.category)},\n"
        f"\t\t{prop.flags},\n"
        f"\t\t{offset_expr},\n"
        f"\t\t{size_expr},\n"
        f"\t\t{cpp_float_literal(prop.min_value)},\n"
        f"\t\t{cpp_float_literal(prop.max_value)},\n"
        f"\t\t{cpp_float_literal(prop.speed_value)},\n"
        f"\t\t{cpp_string_literal(prop.display_name)},\n"
        f"\t\t{{{metadata_entries}}},\n"
        f"\t\t{cpp_string_literal(prop.owner)}\n"
        "\t)"
    )


def render_property(prop: ReflectedProperty, index: int) -> str:
    return f"\tStruct->AddProperty({render_property_allocation(prop, index)});\n"


def render_function(function: ReflectedFunction, index: int) -> str:
    metadata_entries = ", ".join(
        f"{{{cpp_string_literal(key)}, {cpp_string_literal(value)}}}"
        for key, value in function.metadata
    )
    param_struct = f"{function.identifier}_Params"
    fields: list[str] = []
    for param in function.params:
        if param.default_value:
            fields.append(f"\t\t{param.storage_cpp_type} {param.name} = {param.default_value};")
        else:
            fields.append(f"\t\t{param.storage_cpp_type} {param.name}{{}};")
    if function.return_storage_cpp_type:
        fields.append(f"\t\t{function.return_storage_cpp_type} ReturnValue{{}};")
    if not fields:
        fields.append("\t\tuint8 __Dummy = 0;")

    param_property_entries = []
    for param_index, param in enumerate(function.params):
        param_property_entries.append(
            render_property_allocation(
                param.property,
                param_index,
                param_struct,
                param.name,
                param.name,
            )
        )
    param_properties_expr = "TArray<FProperty*>{"
    if param_property_entries:
        param_properties_expr += "\n\t\t\t" + ",\n\t\t\t".join(param_property_entries) + "\n\t\t"
    param_properties_expr += "}"

    return_property_expr = "nullptr"
    if function.return_property:
        return_property_expr = render_property_allocation(
            function.return_property,
            len(function.params),
            param_struct,
            "ReturnValue",
            "ReturnValue",
        )

    call_args = ", ".join(f"Params->{param.name}" for param in function.params)
    if function.is_static:
        call_target = f"{function.owner}::{function.name}({call_args})"
        instance_setup = ""
    else:
        instance_type = f"const {function.owner}" if function.is_const else function.owner
        instance_setup = (
            f"\t\t\t{instance_type}* TypedInstance = static_cast<{instance_type}*>(Instance);\n"
            "\t\t\tif (!TypedInstance)\n"
            "\t\t\t{\n"
            "\t\t\t\treturn false;\n"
            "\t\t\t}\n"
        )
        call_target = f"TypedInstance->{function.name}({call_args})"

    is_implementable_event = "FUNC_ImplementableEvent" in function.flags and "FUNC_NativeEvent" not in function.flags
    if is_implementable_event:
        if function.return_storage_cpp_type:
            invoke_body = (
                f"{instance_setup}"
                "\t\t\tif (Frame.ReturnValue)\n"
                "\t\t\t{\n"
                f"\t\t\t\t*static_cast<{function.return_storage_cpp_type}*>(Frame.ReturnValue) = Params->ReturnValue;\n"
                "\t\t\t}\n"
            )
        else:
            invoke_body = f"{instance_setup}"
    elif function.return_storage_cpp_type:
        invoke_body = (
            f"{instance_setup}"
            f"\t\t\t{function.return_storage_cpp_type} Result = {call_target};\n"
            "\t\t\tif (Frame.ReturnValue)\n"
            "\t\t\t{\n"
            f"\t\t\t\t*static_cast<{function.return_storage_cpp_type}*>(Frame.ReturnValue) = Result;\n"
            "\t\t\t}\n"
            "\t\t\telse\n"
            "\t\t\t{\n"
            "\t\t\t\tParams->ReturnValue = Result;\n"
            "\t\t\t}\n"
        )
    else:
        invoke_body = f"{instance_setup}\t\t\t{call_target};\n"

    return (
        f"\tstruct {param_struct}\n"
        "\t{\n"
        + "\n".join(fields) + "\n"
        "\t};\n"
        f"\tStruct->AddFunction(new FFunction(\n"
        f"\t\t{cpp_string_literal(function.name)},\n"
        f"\t\t{cpp_string_literal(function.signature)},\n"
        f"\t\t{cpp_string_literal(function.category)},\n"
        f"\t\t{cpp_string_literal(function.display_name)},\n"
        f"\t\t{function.flags},\n"
        f"\t\t{{{metadata_entries}}},\n"
        f"\t\t{cpp_string_literal(function.owner)},\n"
        f"\t\t{param_properties_expr},\n"
        f"\t\t{return_property_expr},\n"
        f"\t\tsizeof({param_struct}),\n"
        f"\t\t[](void* Instance, FFunctionFrame& Frame)->bool\n"
        "\t\t{\n"
        f"\t\t\t{param_struct} LocalParams{{}};\n"
        f"\t\t\t{param_struct}* Params = Frame.Parameters ? static_cast<{param_struct}*>(Frame.Parameters) : &LocalParams;\n"
        f"{invoke_body}"
        "\t\t\treturn true;\n"
        "\t\t},\n"
        f"\t\t[]()->void* {{ return new {param_struct}(); }},\n"
        f"\t\t[](void* Storage) {{ delete static_cast<{param_struct}*>(Storage); }}\n"
        "\t));\n"
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

    def add_property_include(prop: ReflectedProperty) -> None:
        property_class = get_property_class(prop)
        include_path = PROPERTY_CLASS_INCLUDES.get(property_class)
        if include_path:
            includes.add(include_path)

        if property_class == "FArrayProperty":
            inner_property_class = get_array_inner_property_class(prop)
            inner_include_path = PROPERTY_CLASS_INCLUDES.get(inner_property_class or "")
            if inner_include_path:
                includes.add(inner_include_path)

    for prop in reflected_type.properties:
        add_property_include(prop)

    for function in reflected_type.functions:
        for param in function.params:
            add_property_include(param.property)
        if function.return_property:
            add_property_include(function.return_property)

    return sorted(includes)



def get_class_flags_expr(reflected_type: ReflectedType) -> str:
    flags: list[str] = []
    class_name = reflected_type.name
    super_name = reflected_type.super_name or ""

    if class_name == "AActor" or super_name.startswith("A"):
        flags.append("CF_Actor")
    if class_name.endswith("Component") or super_name.endswith("Component"):
        flags.append("CF_Component")
    if "Camera" in class_name:
        flags.append("CF_Camera")

    return " | ".join(flags) if flags else "CF_None"

def render_type_registration(reflected_type: ReflectedType) -> str:
    if reflected_type.kind == "STRUCT":
        struct_name = reflected_type.name
        super_expr = f"&{reflected_type.super_name}::StaticStructInstance" if reflected_type.super_name else "nullptr"
        return (
            f"UStruct {struct_name}::StaticStructInstance(\n"
            f"\t\"{struct_name}\",\n"
            f"\t{super_expr},\n"
            f"\tsizeof({struct_name})\n"
            ");\n"
            f"FStructRegistrar {struct_name}::s_StructRegistrar(&{struct_name}::StaticStructInstance);\n"
            "\n"
            "namespace {\n"
            f"\tstruct {struct_name}_ReflectionRegistrar {{\n"
            f"\t\t{struct_name}_ReflectionRegistrar() {{\n"
            f"\t\t\t{struct_name}::RegisterProperties({struct_name}::StaticStruct());\n"
            f"\t\t\t{struct_name}::RegisterFunctions({struct_name}::StaticStruct());\n"
            "\t\t}\n"
            "\t};\n"
            f"\t{struct_name}_ReflectionRegistrar G{struct_name}_ReflectionRegistrar;\n"
            "}\n"
        )

    if not reflected_type.super_name:
        return ""

    class_name = reflected_type.name
    super_name = reflected_type.super_name
    class_flags_expr = get_class_flags_expr(reflected_type)
    return (
        f"UClass {class_name}::StaticClassInstance(\n"
        f"\t\"{class_name}\",\n"
        f"\t&{super_name}::StaticClassInstance,\n"
        f"\tsizeof({class_name}),\n"
        f"\t{class_flags_expr}\n"
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
        f"\tstruct {class_name}_ReflectionRegistrar {{\n"
        f"\t\t{class_name}_ReflectionRegistrar() {{\n"
        f"\t\t\t{class_name}::RegisterProperties({class_name}::StaticClass());\n"
        f"\t\t\t{class_name}::RegisterFunctions({class_name}::StaticClass());\n"
        "\t\t}\n"
        "\t};\n"
        f"\t{class_name}_ReflectionRegistrar G{class_name}_ReflectionRegistrar;\n"
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
            entries = ", ".join(
                f"{{{cpp_string_literal(entry)}, static_cast<int64>({enum.name}::{entry})}}"
                for entry in enum.entries
            )
            lines.append(f"static const FEnumEntry {symbol}_Entries[] = {{{entries}}};")
            lines.append(
                f"static const FEnum {symbol}_Enum = "
                f"{{{cpp_string_literal(enum.name)}, {symbol}_Entries, "
                f"static_cast<uint32>(sizeof({symbol}_Entries) / sizeof({symbol}_Entries[0])), "
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

    lines.extend(
        [
            f"void {reflected_type.name}::RegisterFunctions(UStruct* Struct)",
            "{",
        ]
    )

    for index, function in enumerate(reflected_type.functions):
        lines.append(render_function(function, index).rstrip())

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


def write_if_changed(path: Path, content: str, default_newline: str = "\r\n") -> bool:
    normalized_content = normalize_newlines_for_write(content)
    if path.exists():
        old = path.read_text(encoding="utf-8")
        if normalize_newlines_for_write(old) == normalized_content:
            return False

    write_text_preserving_newline(path, normalized_content, default_newline=default_newline)
    return True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate KraftonEngine *.generated.h headers.")
    parser.add_argument(
        "--root",
        type=Path,
        default=None,
        help="Project root. Defaults to the parent directory of Source/.",
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
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="Run internal regression tests for the codegen primitives and exit.",
    )
    return parser.parse_args()


def run_self_tests() -> int:
    """Regression tests for codegen primitives that have historically broken builds.

    Add a case here whenever a codegen bug escapes to the C++ compiler. The cost
    of one assertion line is much lower than the cost of a build-time discovery.
    """
    failures: list[str] = []

    def check(label: str, got: object, expected: object) -> None:
        if got != expected:
            failures.append(f"{label}: expected {expected!r}, got {got!r}")

    # cpp_float_literal — int metadata like Min=0 must not become invalid "0f".
    check("cpp_float_literal('0')", cpp_float_literal("0"), "0.0f")
    check("cpp_float_literal('1')", cpp_float_literal("1"), "1.0f")
    check("cpp_float_literal('-1')", cpp_float_literal("-1"), "-1.0f")
    check("cpp_float_literal('+2')", cpp_float_literal("+2"), "+2.0f")
    check("cpp_float_literal('0.5')", cpp_float_literal("0.5"), "0.5f")
    check("cpp_float_literal('.5')", cpp_float_literal(".5"), ".5f")
    check("cpp_float_literal('5.')", cpp_float_literal("5."), "5.f")
    check("cpp_float_literal('1e3')", cpp_float_literal("1e3"), "1e3f")
    check("cpp_float_literal('1.5E-3')", cpp_float_literal("1.5E-3"), "1.5E-3f")
    check("cpp_float_literal('0.0f')", cpp_float_literal("0.0f"), "0.0f")
    check("cpp_float_literal('1.0F')", cpp_float_literal("1.0F"), "1.0F")
    # Invalid integer+f suffix (e.g. user writes Min=1f) must normalize to "1.0f".
    check("cpp_float_literal('1f')", cpp_float_literal("1f"), "1.0f")
    check("cpp_float_literal('0F')", cpp_float_literal("0F"), "0.0F")
    check("cpp_float_literal('-2f')", cpp_float_literal("-2f"), "-2.0f")
    check("cpp_float_literal('')", cpp_float_literal(""), "0.0f")
    check("cpp_float_literal('M_PI')", cpp_float_literal("M_PI"), "M_PI")
    check("cpp_float_literal('FMath::PI')", cpp_float_literal("FMath::PI"), "FMath::PI")
    check("cpp_float_literal(' 2 ')", cpp_float_literal(" 2 "), "2.0f")


    # parse_member_declaration / normalize_cpp_type — elaborated type specifiers must not break UPROPERTY.
    check(
        "parse_member_declaration('struct FRawDistributionVector StartVelocity')",
        parse_member_declaration("struct FRawDistributionVector StartVelocity"),
        ("FRawDistributionVector", "StartVelocity"),
    )
    check(
        "parse_member_declaration('struct FRawDistributionFloat StartVelocityRadial')",
        parse_member_declaration("struct FRawDistributionFloat StartVelocityRadial"),
        ("FRawDistributionFloat", "StartVelocityRadial"),
    )
    check(
        "parse_member_declaration('class UCameraComponent* ActiveCamera')",
        parse_member_declaration("class UCameraComponent* ActiveCamera"),
        ("UCameraComponent*", "ActiveCamera"),
    )
    check(
        "normalize_cpp_type('struct FRawDistributionFloat')",
        normalize_cpp_type("struct FRawDistributionFloat"),
        "FRawDistributionFloat",
    )
    check(
        "get_array_element_cpp_type('TArray<struct FRawDistributionFloat>')",
        get_array_element_cpp_type("TArray<struct FRawDistributionFloat>"),
        "FRawDistributionFloat",
    )

    # get_array_element_property_type — TArray<float> must report Float, not Struct.
    def array_prop(cpp_type: str, struct_type: str = "nullptr", metadata: tuple = ()) -> ReflectedProperty:
        return ReflectedProperty(
            owner="UTest", cpp_type=cpp_type,
            member_name="X", display_name="X", category="Test",
            property_type="Array", flags="PF_None",
            metadata=metadata,
            min_value="0.0f", max_value="0.0f", speed_value="0.1f",
            enum_type_name=None, struct_type=struct_type,
            asset_type=None, allowed_class=None,
        )

    check("TArray<float>", get_array_element_property_type(array_prop("TArray<float>")), "Float")
    check("TArray<int32>", get_array_element_property_type(array_prop("TArray<int32>")), "Int")
    check("TArray<FString>", get_array_element_property_type(array_prop("TArray<FString>")), "String")
    check(
        "TArray<FMyStruct> with struct_type",
        get_array_element_property_type(array_prop("TArray<FMyStruct>", struct_type="FMyStruct::StaticStruct()")),
        "Struct",
    )
    # Unknown primitive (no TYPE_MAP entry, no struct_type) must return None — not Struct.
    check("TArray<int64> (unmapped)", get_array_element_property_type(array_prop("TArray<int64>")), None)

    # find_ufunction_declaration — inline body must consume past the matching brace.
    inline_body = " bool IsVisible() const { return bIsVisible; }\n void Next();"
    result = find_ufunction_declaration(inline_body, 0)
    check("inline body decl text", result and result[0], "bool IsVisible() const")
    check("inline body cursor lands after '}'", result and inline_body[result[1] - 1], "}")

    plain_decl = " void Foo(int x);\n void Bar();"
    result = find_ufunction_declaration(plain_decl, 0)
    check("plain decl text", result and result[0], "void Foo(int x)")
    check("plain decl cursor at ';'", result and plain_decl[result[1] - 1], ";")

    template_param = " TArray<int> GetItems() const;\n"
    result = find_ufunction_declaration(template_param, 0)
    check("template return parses", result and result[0], "TArray<int> GetItems() const")

    # USTRUCT inheritance must be preserved in generated metadata; otherwise
    # FStructProperty traversal cannot include inherited reflected fields.
    fake_struct = ReflectedType(
        kind="STRUCT",
        name="FChildStruct",
        super_name="FBaseStruct",
        generated_body_line=42,
        properties=tuple(),
        functions=tuple(),
    )
    fake_header = ReflectedHeader(
        header=Path("/Project/Source/Test/FChildStruct.h"),
        generated_header=Path("/Project/Intermediate/Generated/Source/Test/FChildStruct.generated.h"),
        file_id="Source_Test_FChildStruct_h",
        class_names=("FChildStruct",),
        types=(fake_struct,),
    )
    check(
        "USTRUCT generated header emits Super alias",
        "using Super = FBaseStruct" in render_generated_header(fake_header, Path("/Project")),
        True,
    )
    check(
        "USTRUCT registration keeps reflected super",
        "&FBaseStruct::StaticStructInstance" in render_type_registration(fake_struct),
        True,
    )

    # is_enum_sentinel — exact and suffix matches.
    check("is_enum_sentinel('MAX')", is_enum_sentinel("MAX"), True)
    check("is_enum_sentinel('PSA_MAX')", is_enum_sentinel("PSA_MAX"), True)
    check("is_enum_sentinel('ELT_NUM')", is_enum_sentinel("ELT_NUM"), True)
    check("is_enum_sentinel('MaxValue')", is_enum_sentinel("MaxValue"), False)
    check("is_enum_sentinel('PSA_FacingCameraPosition')", is_enum_sentinel("PSA_FacingCameraPosition"), False)
    check("is_enum_sentinel('ActiveCount')", is_enum_sentinel("ActiveCount"), True)
    check("is_enum_sentinel('PSA_ActiveCount')", is_enum_sentinel("PSA_ActiveCount"), True)

    if failures:
        print("SELF-TEST FAILURES:")
        for failure in failures:
            print(f"  - {failure}")
        return 1
    print("self-test: all checks passed")
    return 0


def main() -> int:
    args = parse_args()
    if args.self_test:
        return run_self_tests()
    root = args.root.resolve() if args.root else Path(__file__).resolve().parents[1]
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
