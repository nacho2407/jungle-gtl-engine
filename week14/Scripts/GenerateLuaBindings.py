#!/usr/bin/env python3
"""
Generate Lua binding glue from UFUNCTION(Lua) and public UPROPERTY(Lua) declarations.

This is intentionally small and conservative:
- scans headers under <root>/Source
- finds reflected classes/structs
- binds public UFUNCTIONs whose metadata contains Lua
- binds public UPROPERTYs whose metadata contains Lua
- supports declaration-only methods, not inline function bodies
"""

from __future__ import annotations

import argparse
import os
import re
from dataclasses import dataclass
from pathlib import Path


REFLECTED_DECL_RE = re.compile(
    r"\bU(?P<macro_kind>CLASS|STRUCT)\s*\([^)]*\)\s*"
    r"(?P<kind>class|struct)\s+"
    r"(?:(?:[A-Z_][A-Z0-9_]*|final|abstract)\s+)*"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)"
    r"(?:\s*:\s*(?:(?:public|protected|private)\s+)?(?P<super>[A-Za-z_][A-Za-z0-9_:]*))?",
    re.MULTILINE,
)


@dataclass(frozen=True)
class LuaParam:
    cpp_type: str
    name: str


@dataclass(frozen=True)
class LuaFunction:
    class_name: str
    lua_type_name: str
    header: Path
    return_type: str
    name: str
    params: tuple[LuaParam, ...]
    is_const: bool


@dataclass(frozen=True)
class LuaProperty:
    class_name: str
    lua_type_name: str
    header: Path
    cpp_type: str
    member_name: str
    lua_name: str


def strip_comments(text: str) -> str:
    result: list[str] = []
    i = 0
    n = len(text)
    in_string: str | None = None

    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""

        if in_string:
            result.append(ch)
            if ch == "\\" and i + 1 < n:
                result.append(text[i + 1])
                i += 2
                continue
            if ch == in_string:
                in_string = None
            i += 1
            continue

        if ch in ('"', "'"):
            in_string = ch
            result.append(ch)
            i += 1
            continue

        if ch == "/" and nxt == "/":
            while i < n and text[i] != "\n":
                result.append(" ")
                i += 1
            continue

        if ch == "/" and nxt == "*":
            result.append(" ")
            result.append(" ")
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                result.append("\n" if text[i] == "\n" else " ")
                i += 1
            if i + 1 < n:
                result.append(" ")
                result.append(" ")
                i += 2
            continue

        result.append(ch)
        i += 1

    return "".join(result)


def find_matching(text: str, open_index: int, open_ch: str, close_ch: str) -> int:
    depth = 0
    for i in range(open_index, len(text)):
        if text[i] == open_ch:
            depth += 1
        elif text[i] == close_ch:
            depth -= 1
            if depth == 0:
                return i
    return -1


def parse_metadata(raw: str) -> set[str]:
    return {part.strip() for part in raw.split(",") if part.strip()}


def find_statement_end(text: str, start: int) -> int:
    paren = angle = square = 0
    for i in range(start, len(text)):
        ch = text[i]
        if ch == "(":
            paren += 1
        elif ch == ")":
            paren = max(paren - 1, 0)
        elif ch == "<":
            angle += 1
        elif ch == ">":
            angle = max(angle - 1, 0)
        elif ch == "[":
            square += 1
        elif ch == "]":
            square = max(square - 1, 0)
        elif ch == "{" and paren == 0 and angle == 0 and square == 0:
            return -1
        elif ch == ";" and paren == 0 and angle == 0 and square == 0:
            return i
    return -1


def split_params(raw: str) -> list[str]:
    if not raw.strip() or raw.strip() == "void":
        return []

    parts: list[str] = []
    start = 0
    angle = paren = 0
    for i, ch in enumerate(raw):
        if ch == "<":
            angle += 1
        elif ch == ">":
            angle = max(angle - 1, 0)
        elif ch == "(":
            paren += 1
        elif ch == ")":
            paren = max(paren - 1, 0)
        elif ch == "," and angle == 0 and paren == 0:
            parts.append(raw[start:i].strip())
            start = i + 1
    parts.append(raw[start:].strip())
    return [p for p in parts if p]


def normalize_cpp_type(cpp_type: str) -> str:
    cpp_type = re.sub(r"\s+", " ", cpp_type.strip())
    cpp_type = cpp_type.replace(" &", "&").replace("& ", "& ")
    cpp_type = cpp_type.replace(" *", "*").replace("* ", "* ")
    return cpp_type.strip()


def parse_param(raw: str) -> LuaParam | None:
    raw = raw.split("=", 1)[0].strip()
    if not raw:
        return None

    match = re.match(r"(?P<type>.+?)(?:\s+|(?<=[*&]))(?P<name>[A-Za-z_]\w*)$", raw)
    if not match:
        return None

    return LuaParam(
        cpp_type=normalize_cpp_type(match.group("type")),
        name=match.group("name"),
    )


def parse_function_decl(statement: str) -> tuple[str, str, tuple[LuaParam, ...], bool] | None:
    statement = re.sub(r"\s+", " ", statement.strip())
    statement = re.sub(r"^(virtual|static|inline|explicit)\s+", "", statement)

    match = re.match(
        r"(?P<return>.+?)\s+"
        r"(?P<name>[A-Za-z_]\w*)\s*"
        r"\((?P<params>.*)\)\s*"
        r"(?P<const>const)?\s*(?:override|final)?\s*$",
        statement,
    )
    if not match:
        return None

    params: list[LuaParam] = []
    for raw_param in split_params(match.group("params")):
        parsed = parse_param(raw_param)
        if not parsed:
            return None
        params.append(parsed)

    return (
        normalize_cpp_type(match.group("return")),
        match.group("name"),
        tuple(params),
        bool(match.group("const")),
    )


def parse_property_decl(statement: str) -> tuple[str, str] | None:
    statement = statement.split("=", 1)[0].strip()
    statement = statement.rstrip(";").strip()
    statement = re.sub(r"\s+", " ", statement)

    match = re.match(
        r"(?P<type>.+?)(?:\s+|(?<=[*&]))(?P<name>[A-Za-z_]\w*)"
        r"(?:\s*\[[^\]]+\])?$",
        statement,
    )
    if not match:
        return None

    return (
        normalize_cpp_type(match.group("type")),
        match.group("name"),
    )


def lua_type_name_for_class(class_name: str) -> str:
    if len(class_name) > 1 and class_name[0] in ("A", "U"):
        return class_name[1:]
    return class_name


def base_cpp_type(cpp_type: str) -> str:
    cpp_type = normalize_cpp_type(cpp_type)
    cpp_type = cpp_type.replace("const ", "")
    cpp_type = cpp_type.replace("&", "")
    cpp_type = cpp_type.replace("*", "")
    return cpp_type.strip()


def lua_type_for_cpp(cpp_type: str) -> str:
    normalized = normalize_cpp_type(cpp_type)
    pointer = "*" in normalized
    base = base_cpp_type(normalized)

    primitive = {
        "void": "nil",
        "bool": "boolean",
        "int": "integer",
        "int32": "integer",
        "uint32": "integer",
        "float": "number",
        "double": "number",
        "FString": "string",
        "std::string": "string",
        "FName": "string",
        "FVector": "Vector",
        "FVector4": "Vector4",
        "FRotator": "Vector",
        "FTransform": "Transform",
    }
    if base in primitive:
        return primitive[base]

    result = lua_type_name_for_class(base)
    return result + "?" if pointer else result


def is_out_param(cpp_type: str) -> bool:
    normalized = normalize_cpp_type(cpp_type)
    return "&" in normalized and not normalized.startswith("const ")


def is_supported_cpp_type(cpp_type: str) -> bool:
    base = base_cpp_type(cpp_type)
    supported = {
        "void",
        "bool",
        "int",
        "int32",
        "uint32",
        "float",
        "double",
        "FString",
        "std::string",
        "FName",
        "FVector",
        "FVector4",
        "FRotator",
        "FTransform",
        "UObject",
        "AActor",
        "USceneComponent",
        "UPrimitiveComponent",
        "USkeletalMeshComponent",
        "UCameraComponent",
    }
    return base in supported


def is_supported_lua_param(param: LuaParam) -> bool:
    if is_out_param(param.cpp_type):
        return base_cpp_type(param.cpp_type) in {
            "bool",
            "int",
            "int32",
            "uint32",
            "float",
            "double",
            "FString",
            "std::string",
            "FName",
            "FVector",
            "FVector4",
            "FRotator",
            "FTransform",
        }

    return is_supported_cpp_type(param.cpp_type)


def lua_return_types_for_cpp(cpp_type: str) -> list[str]:
    return [lua_type_for_cpp(cpp_type)]


def render_doc_signature(fn: LuaFunction) -> str:
    lines: list[str] = []
    in_params = [param for param in fn.params if not is_out_param(param.cpp_type)]
    out_params = [param for param in fn.params if is_out_param(param.cpp_type)]

    for param in in_params:
        lines.append(f"---@param {param.name} {lua_type_for_cpp(param.cpp_type)}")
    if base_cpp_type(fn.return_type) != "void":
        for lua_type in lua_return_types_for_cpp(fn.return_type):
            lines.append(f"---@return {lua_type}")
    for param in out_params:
        for lua_type in lua_return_types_for_cpp(param.cpp_type):
            lines.append(f"---@return {lua_type}")

    args = ", ".join(param.name for param in in_params)
    lines.append(f"function {fn.lua_type_name}:{fn.name}({args}) end")
    return "\\n".join(lines)


def binding_cpp_param_type(cpp_type: str) -> str:
    base = base_cpp_type(cpp_type)

    if base == "FName":
        return "const FString&"

    return cpp_type


def binding_call_arg(param: LuaParam) -> str:
    base = base_cpp_type(param.cpp_type)

    if base == "FName":
        return f"FName({param.name})"

    return param.name


def binding_return_expr(return_type: str, call: str) -> str:
    base = base_cpp_type(return_type)

    if base == "FName":
        return f"{call}.ToString()"

    if base == "FRotator":
        return f"{call}.ToVector()"

    return call


def binding_return_exprs(return_type: str, value: str) -> list[str]:
    return [binding_return_expr(return_type, value)]


def binding_out_return_exprs(param: LuaParam) -> list[str]:
    return binding_return_exprs(param.cpp_type, param.name)


def render_lambda_body(fn: LuaFunction) -> str:
    in_params = [param for param in fn.params if not is_out_param(param.cpp_type)]
    out_params = [param for param in fn.params if is_out_param(param.cpp_type)]

    params = ", ".join(f"{binding_cpp_param_type(p.cpp_type)} {p.name}" for p in in_params)
    local_out_params = [f"{base_cpp_type(p.cpp_type)} {p.name}{{}};" for p in out_params]
    args = ", ".join(binding_call_arg(p) if not is_out_param(p.cpp_type) else p.name for p in fn.params)
    const_prefix = "const " if fn.is_const else ""
    call = f"Self.{fn.name}({args})"

    if not out_params:
        if base_cpp_type(fn.return_type) == "void":
            body = f"{call};"
        else:
            body = f"return {binding_return_expr(fn.return_type, call)};"
        return f"[]({const_prefix}{fn.class_name}& Self{', ' if params else ''}{params}) {{ {body} }}"

    statements: list[str] = []
    statements.extend(local_out_params)
    return_values = [expr for p in out_params for expr in binding_out_return_exprs(p)]

    if base_cpp_type(fn.return_type) == "void":
        statements.append(f"{call};")
    else:
        statements.append(f"auto Result = {call};")
        return_values = binding_return_exprs(fn.return_type, "Result") + return_values

    statements.append(f"return std::make_tuple({', '.join(return_values)});")
    body = " ".join(statements)
    return f"[]({const_prefix}{fn.class_name}& Self{', ' if params else ''}{params}) {{ {body} }}"


def property_setter_param_type(cpp_type: str) -> str:
    base = base_cpp_type(cpp_type)

    if base == "FName":
        return "const FString&"

    if base == "FRotator":
        return "const FVector&"

    return cpp_type


def property_get_expr(prop: LuaProperty) -> str:
    base = base_cpp_type(prop.cpp_type)
    member = f"Self.{prop.member_name}"

    if base == "FName":
        return f"{member}.ToString()"

    if base == "FRotator":
        return f"{member}.ToVector()"

    return member


def property_set_expr(prop: LuaProperty) -> str:
    base = base_cpp_type(prop.cpp_type)
    member = f"Self.{prop.member_name}"

    if base == "FName":
        return f"{member} = FName(Value);"

    if base == "FRotator":
        return f"{member} = FRotator(Value);"

    return f"{member} = Value;"


def render_property_binding(prop: LuaProperty) -> list[str]:
    return [
        f"\tType.Property(\"{prop.lua_name}\", \"{lua_type_for_cpp(prop.cpp_type)}\",",
        f"\t\t[](const {prop.class_name}& Self) {{ return {property_get_expr(prop)}; }},",
        f"\t\t[]({prop.class_name}& Self, {property_setter_param_type(prop.cpp_type)} Value) {{ {property_set_expr(prop)} }});",
    ]


def parse_lua_functions(header: Path, root: Path, scan_text: str) -> list[LuaFunction]:
    functions: list[LuaFunction] = []

    for decl in REFLECTED_DECL_RE.finditer(scan_text):
        class_name = decl.group("name")
        brace_start = scan_text.find("{", decl.end())
        if brace_start < 0:
            continue
        brace_end = find_matching(scan_text, brace_start, "{", "}")
        if brace_end < 0:
            continue

        body = scan_text[brace_start + 1:brace_end]
        access = "private" if decl.group("kind") == "class" else "public"
        cursor = 0
        while cursor < len(body):
            access_match = re.search(r"\b(public|protected|private)\s*:", body[cursor:])
            ufunc_match = re.search(r"\bUFUNCTION\s*\((?P<meta>[^)]*)\)", body[cursor:])
            candidates = [
                (cursor + access_match.start(), "access", access_match) if access_match else None,
                (cursor + ufunc_match.start(), "ufunction", ufunc_match) if ufunc_match else None,
            ]
            candidates = [c for c in candidates if c is not None]
            if not candidates:
                break
            pos, kind, match = min(candidates, key=lambda item: item[0])

            if kind == "access":
                access = match.group(1)
                cursor = pos + len(match.group(0))
                continue

            metadata = parse_metadata(match.group("meta"))
            cursor = pos + len(match.group(0))
            if "Lua" not in metadata or access != "public":
                continue

            statement_start = cursor
            statement_end = find_statement_end(body, statement_start)
            if statement_end < 0:
                continue

            statement = body[statement_start:statement_end]
            parsed = parse_function_decl(statement)
            cursor = statement_end + 1
            if not parsed:
                continue

            return_type, fn_name, params, is_const = parsed
            if not is_supported_cpp_type(return_type):
                print(
                    f"[LuaBindings] skip {class_name}::{fn_name}: "
                    f"unsupported return type {return_type}"
                )
                continue

            unsupported_param = next((p for p in params if not is_supported_lua_param(p)), None)
            if unsupported_param:
                print(
                    f"[LuaBindings] skip {class_name}::{fn_name}: "
                    f"unsupported parameter type {unsupported_param.cpp_type} {unsupported_param.name}"
                )
                continue

            functions.append(
                LuaFunction(
                    class_name=class_name,
                    lua_type_name=lua_type_name_for_class(class_name),
                    header=header,
                    return_type=return_type,
                    name=fn_name,
                    params=params,
                    is_const=is_const,
                )
            )

    return functions


def parse_lua_properties(header: Path, root: Path, scan_text: str) -> list[LuaProperty]:
    properties: list[LuaProperty] = []

    for decl in REFLECTED_DECL_RE.finditer(scan_text):
        class_name = decl.group("name")
        brace_start = scan_text.find("{", decl.end())
        if brace_start < 0:
            continue
        brace_end = find_matching(scan_text, brace_start, "{", "}")
        if brace_end < 0:
            continue

        body = scan_text[brace_start + 1:brace_end]
        access = "private" if decl.group("kind") == "class" else "public"
        cursor = 0
        while cursor < len(body):
            access_match = re.search(r"\b(public|protected|private)\s*:", body[cursor:])
            uprop_match = re.search(r"\bUPROPERTY\s*\((?P<meta>[^)]*)\)", body[cursor:])
            candidates = [
                (cursor + access_match.start(), "access", access_match) if access_match else None,
                (cursor + uprop_match.start(), "uproperty", uprop_match) if uprop_match else None,
            ]
            candidates = [c for c in candidates if c is not None]
            if not candidates:
                break
            pos, kind, match = min(candidates, key=lambda item: item[0])

            if kind == "access":
                access = match.group(1)
                cursor = pos + len(match.group(0))
                continue

            metadata = parse_metadata(match.group("meta"))
            cursor = pos + len(match.group(0))
            if "Lua" not in metadata:
                continue

            statement_start = cursor
            statement_end = find_statement_end(body, statement_start)
            if statement_end < 0:
                continue

            statement = body[statement_start:statement_end]
            parsed = parse_property_decl(statement)
            cursor = statement_end + 1
            if not parsed:
                continue

            cpp_type, member_name = parsed
            if access != "public":
                print(
                    f"[LuaBindings] skip {class_name}::{member_name}: "
                    "UPROPERTY(Lua) direct binding only supports public members"
                )
                continue

            if not is_supported_cpp_type(cpp_type):
                print(
                    f"[LuaBindings] skip {class_name}::{member_name}: "
                    f"unsupported property type {cpp_type}"
                )
                continue

            properties.append(
                LuaProperty(
                    class_name=class_name,
                    lua_type_name=lua_type_name_for_class(class_name),
                    header=header,
                    cpp_type=cpp_type,
                    member_name=member_name,
                    lua_name=member_name,
                )
            )

    return properties


def resolve_root(root: Path) -> Path:
    root = root.resolve()
    if (root / "Source").exists():
        return root
    if (root / "KraftonEngine" / "Source").exists():
        return root / "KraftonEngine"
    return root


def collect_lua_functions(root: Path, source_dir: Path) -> list[LuaFunction]:
    functions: list[LuaFunction] = []
    for header in sorted(source_dir.rglob("*.h")):
        if header.name.endswith(".generated.h"):
            continue
        text = header.read_text(encoding="utf-8-sig")
        scan_text = strip_comments(text)
        if "UFUNCTION" not in scan_text or "Lua" not in scan_text:
            continue
        functions.extend(parse_lua_functions(header, root, scan_text))
    return functions


def collect_lua_properties(root: Path, source_dir: Path) -> list[LuaProperty]:
    properties: list[LuaProperty] = []
    for header in sorted(source_dir.rglob("*.h")):
        if header.name.endswith(".generated.h"):
            continue
        text = header.read_text(encoding="utf-8-sig")
        scan_text = strip_comments(text)
        if "UPROPERTY" not in scan_text or "Lua" not in scan_text:
            continue
        properties.extend(parse_lua_properties(header, root, scan_text))
    return properties


def render_generated_header() -> str:
    return "\n".join(
        [
            "#pragma once",
            "// This file is generated by Scripts/GenerateLuaBindings.py. Do not edit manually.",
            "",
            "#include <sol/sol.hpp>",
            "",
            "void RegisterGeneratedLuaBindings(sol::state& Lua);",
            "",
        ]
    )


def render_generated_cpp(
    root: Path,
    generated_cpp: Path,
    functions: list[LuaFunction],
    properties: list[LuaProperty],
) -> str:
    headers = sorted({fn.header for fn in functions} | {prop.header for prop in properties})
    by_class: dict[str, list[LuaFunction]] = {}
    for fn in functions:
        by_class.setdefault(fn.class_name, []).append(fn)
    properties_by_class: dict[str, list[LuaProperty]] = {}
    for prop in properties:
        properties_by_class.setdefault(prop.class_name, []).append(prop)
        by_class.setdefault(prop.class_name, [])

    lines: list[str] = [
        "// This file is generated by Scripts/GenerateLuaBindings.py. Do not edit manually.",
        "",
        "#include \"Lua/LuaDocRegistry.h\"",
        "",
        "#include <tuple>",
    ]

    for header in headers:
        include_path = Path(os.path.relpath(header, generated_cpp.parent)).as_posix()
        lines.append(f"#include \"{include_path}\"")

    lines.append("")

    for class_name in sorted(by_class):
        lines.append(f"static void RegisterGeneratedLuaBindings_{class_name}(sol::state& Lua);")

    if by_class:
        lines.append("")

    lines.extend(
        [
            "void RegisterGeneratedLuaBindings(sol::state& Lua)",
            "{",
        ]
    )
    for class_name in sorted(by_class):
        lines.append(f"\tRegisterGeneratedLuaBindings_{class_name}(Lua);")
    lines.extend(["}", ""])

    for class_name in sorted(by_class):
        lua_type = lua_type_name_for_class(class_name)
        lines.extend(
            [
                f"static void RegisterGeneratedLuaBindings_{class_name}(sol::state& Lua)",
                "{",
                f"\tauto Type = FLuaDocRegistry::Get().ExtendType<{class_name}>(Lua, \"{lua_type}\");",
            ]
        )
        for fn in by_class[class_name]:
            lines.extend(
                [
                    f"\tType.Method(\"{fn.name}\",",
                    f"\t\t\"{render_doc_signature(fn)}\",",
                    f"\t\t{render_lambda_body(fn)});",
                ]
            )
        for prop in properties_by_class.get(class_name, []):
            lines.extend(render_property_binding(prop))
        lines.extend(["}", ""])

    if not functions and not properties:
        lines.append("// No UFUNCTION(Lua) or public UPROPERTY(Lua) declarations found.")
        lines.append("")

    return "\n".join(lines)


def write_if_changed(path: Path, content: str) -> bool:
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    return True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate KraftonEngine Lua binding glue.")
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="KraftonEngine project root, or repository root containing KraftonEngine/.",
    )
    parser.add_argument("--source-dir", type=Path, default=None)
    parser.add_argument("--generated-root", type=Path, default=None)
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = resolve_root(args.root)
    source_dir = (args.source_dir or root / "Source").resolve()
    generated_root = (args.generated_root or root / "Intermediate" / "Generated").resolve()
    generated_h = generated_root / "LuaBindings.generated.h"
    generated_cpp = generated_root / "LuaBindings.generated.cpp"

    functions = collect_lua_functions(root, source_dir)
    properties = collect_lua_properties(root, source_dir)
    header_content = render_generated_header()
    cpp_content = render_generated_cpp(root, generated_cpp, functions, properties)

    if args.dry_run:
        print(f"would generate {generated_h.relative_to(root)}")
        print(f"would generate {generated_cpp.relative_to(root)}")
        print(f"found {len(functions)} UFUNCTION(Lua) declaration(s)")
        print(f"found {len(properties)} public UPROPERTY(Lua) declaration(s)")
        return 0

    h_changed = write_if_changed(generated_h, header_content)
    cpp_changed = write_if_changed(generated_cpp, cpp_content)
    print(("generated" if h_changed else "unchanged") + f" {generated_h.relative_to(root)}")
    print(("generated" if cpp_changed else "unchanged") + f" {generated_cpp.relative_to(root)}")
    print(f"found {len(functions)} UFUNCTION(Lua) declaration(s)")
    print(f"found {len(properties)} public UPROPERTY(Lua) declaration(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
