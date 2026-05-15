import os
import re
import sys
from pathlib import Path


# Material, SRV, CubeSRV, and Enum stay on the manual descriptor path for now.
TYPE_MAP = {
    'bool': 'EPropertyType::Bool',
    'int32': 'EPropertyType::Int',
    'int': 'EPropertyType::Int',
    'float': 'EPropertyType::Float',
    'FVector': 'EPropertyType::Vec3',
    'FVector4': 'EPropertyType::Vec4',
    'FString': 'EPropertyType::String',
    'FName': 'EPropertyType::Name',
    'FColor': 'EPropertyType::Color',
    'TArray<FVector>': 'EPropertyType::Vec3Array',
    'USceneComponent*': 'EPropertyType::SceneComponentRef',
}

ROOT = Path(__file__).resolve().parent.parent
ENGINE_SOURCE_DIR = ROOT / 'JSEngine' / 'Source' / 'Engine'


def make_include_path(header_path):
    return header_path.relative_to(ENGINE_SOURCE_DIR).as_posix()


def normalize_cpp_type(cpp_type):
    normalized = re.sub(r'\s+', ' ', cpp_type).strip()
    normalized = re.sub(r'\s*<\s*', '<', normalized)
    normalized = re.sub(r'\s*>\s*', '>', normalized)
    normalized = re.sub(r'\s*,\s*', ', ', normalized)
    normalized = re.sub(r'\s*\*\s*', '*', normalized)
    return normalized


def parse_property_declaration(declaration):
    declaration = declaration.split('=', 1)[0].strip()
    name_match = re.search(r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*$', declaration)
    if not name_match:
        return None, None

    var_name = name_match.group(1)
    cpp_type = declaration[:name_match.start()].strip()
    return normalize_cpp_type(cpp_type), var_name


def split_metadata_args(metadata):
    args = []
    current = []
    quote = None
    escape = False

    for char in metadata:
        if escape:
            current.append(char)
            escape = False
            continue

        if char == '\\':
            current.append(char)
            escape = True
            continue

        if quote:
            current.append(char)
            if char == quote:
                quote = None
            continue

        if char in ('"', "'"):
            current.append(char)
            quote = char
            continue

        if char == ',':
            arg = ''.join(current).strip()
            if arg:
                args.append(arg)
            current = []
            continue

        current.append(char)

    arg = ''.join(current).strip()
    if arg:
        args.append(arg)
    return args


def unquote_metadata_value(value):
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ('"', "'"):
        return value[1:-1]
    return value


def parse_metadata(metadata):
    result = {}
    for arg in split_metadata_args(metadata):
        if '=' not in arg:
            result[arg.strip()] = True
            continue

        key, value = arg.split('=', 1)
        result[key.strip()] = unquote_metadata_value(value)
    return result


def cpp_string_literal(value):
    if value is None:
        return 'nullptr'
    escaped = value.replace('\\', '\\\\').replace('"', '\\"')
    return f'"{escaped}"'


def cpp_float_literal(value, default_value):
    if value is None:
        return default_value

    raw_value = unquote_metadata_value(str(value)).strip()
    numeric_value = raw_value[:-1] if raw_value.lower().endswith('f') else raw_value
    if not re.fullmatch(r'[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?', numeric_value):
        return default_value

    if '.' not in numeric_value and 'e' not in numeric_value.lower():
        numeric_value += '.0'
    return f'{numeric_value}f'


def get_metadata_value(metadata, *keys):
    for key in keys:
        if key in metadata:
            return metadata[key]
    return None


def warn_unknown_type(header_path, cpp_type, var_name):
    print(
        f"Reflection warning: unknown UPROPERTY type '{cpp_type}' for '{var_name}' in {header_path.relative_to(ROOT)}; skipped.",
        file=sys.stderr)


def parse_header_and_generate(header_path):
    with open(header_path, 'r', encoding='utf-8') as f:
        content = f.read()

    if 'UCLASS(' not in content:
        return

    class_match = re.search(r'class\s+([A-Z]\w+)\s*:', content)
    if not class_match:
        return
    class_name = class_match.group(1)

    properties = []
    prop_pattern = re.compile(r'UPROPERTY\s*\(([^)]*)\)\s*([^;]+);', re.DOTALL)

    for match in prop_pattern.finditer(content):
        metadata = parse_metadata(match.group(1))
        cpp_type, var_name = parse_property_declaration(match.group(2))
        if not cpp_type or not var_name:
            print(
                f"Reflection warning: failed to parse UPROPERTY declaration in {header_path.relative_to(ROOT)}; skipped.",
                file=sys.stderr)
            continue

        property_type = TYPE_MAP.get(cpp_type)
        if not property_type:
            warn_unknown_type(header_path, cpp_type, var_name)
            continue

        properties.append({
            'name': var_name,
            'display_name': get_metadata_value(metadata, 'DisplayName', 'Display'),
            'enum': property_type,
            'min': cpp_float_literal(get_metadata_value(metadata, 'Min', 'ClampMin', 'UIMin'), '0.0f'),
            'max': cpp_float_literal(get_metadata_value(metadata, 'Max', 'ClampMax', 'UIMax'), '0.0f'),
            'speed': cpp_float_literal(get_metadata_value(metadata, 'Speed', 'Step'), '0.1f'),
        })

    props_str = ',\n                '.join(
        f'{{ "{p["name"]}", {p["enum"]}, offsetof({class_name}, {p["name"]}), '
        f'EPropertyUsageFlags::Editable, {p["min"]}, {p["max"]}, {p["speed"]}, '
        f'{cpp_string_literal(p["display_name"])} }}'
        for p in properties
    )

    include_path = make_include_path(header_path)
    gen_code = f"""// AUTO-GENERATED FILE. DO NOT MODIFY.
#include "{include_path}"
#include "Core/Reflection/ReflectionRegistry.h"

struct Z_Construct_UClass_{class_name} {{
    static FClassMetaData GetClassMetaData() {{
        return FClassMetaData {{
            "{class_name}",
            &{class_name}::s_TypeInfo,
            {{
                {props_str}
            }}
        }};
    }}
}};

static FAutoClassRegister Z_Register_{class_name}_Var(Z_Construct_UClass_{class_name}::GetClassMetaData());
"""

    gen_filepath = header_path.with_name(f'{class_name}.gen.cpp')
    with open(gen_filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write(gen_code)
    print(f'Generated: {gen_filepath.relative_to(ROOT)}')


if __name__ == '__main__':
    for root, _, files in os.walk(ENGINE_SOURCE_DIR):
        for file in files:
            if file.endswith('.h'):
                parse_header_and_generate(Path(root) / file)
