import os
import re
import sys
from pathlib import Path


# Material, SRV, and CubeSRV stay on the manual descriptor path for now.
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

ENUM_SIZE_MAP = {
    'bool': 1,
    'char': 1,
    'signed char': 1,
    'unsigned char': 1,
    'int8': 1,
    'uint8': 1,
    'int8_t': 1,
    'uint8_t': 1,
    'short': 2,
    'unsigned short': 2,
    'int16': 2,
    'uint16': 2,
    'int16_t': 2,
    'uint16_t': 2,
    'int': 4,
    'unsigned int': 4,
    'int32': 4,
    'uint32': 4,
    'int32_t': 4,
    'uint32_t': 4,
    'long long': 8,
    'unsigned long long': 8,
    'int64': 8,
    'uint64': 8,
    'int64_t': 8,
    'uint64_t': 8,
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


def strip_comments(content):
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    content = re.sub(r'//.*', '', content)
    return content


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


def parse_enum_members(enum_body):
    names = []
    for item in split_metadata_args(enum_body):
        item = re.sub(r'\s*=.*$', '', item).strip()
        item = re.sub(r'\s*UMETA\s*\(.*?\)\s*', '', item).strip()
        item = item.split('::')[-1].strip()
        if re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', item):
            names.append(item)
    return names


def collect_enums():
    enum_map = {}
    enum_pattern = re.compile(
        r'enum\s+(?:class\s+)?(?P<name>[A-Za-z_][A-Za-z0-9_]*)'
        r'(?:\s*:\s*(?P<underlying>[A-Za-z_][A-Za-z0-9_:]*(?:\s+[A-Za-z_][A-Za-z0-9_:]*)?))?'
        r'\s*\{(?P<body>.*?)\};',
        re.DOTALL)

    for header_path in ENGINE_SOURCE_DIR.rglob('*.h'):
        try:
            content = strip_comments(header_path.read_text(encoding='utf-8'))
        except UnicodeDecodeError:
            continue

        for match in enum_pattern.finditer(content):
            enum_name = match.group('name')
            underlying_type = normalize_cpp_type(match.group('underlying') or 'int32')
            underlying_type = underlying_type.replace('::', '')
            enum_size = ENUM_SIZE_MAP.get(underlying_type)
            if enum_size is None:
                print(
                    f"Reflection warning: enum '{enum_name}' has unsupported underlying type '{underlying_type}' in {header_path.relative_to(ROOT)}; skipped.",
                    file=sys.stderr)
                continue

            enum_map[enum_name] = {
                'size': enum_size,
                'names': parse_enum_members(match.group('body')),
            }

    return enum_map


def warn_unknown_type(header_path, cpp_type, var_name):
    print(
        f"Reflection warning: unknown UPROPERTY type '{cpp_type}' for '{var_name}' in {header_path.relative_to(ROOT)}; skipped.",
        file=sys.stderr)


def make_enum_array_name(class_name, property_name):
    return f'Z_{class_name}_{property_name}_EnumNames'


def parse_header_and_generate(header_path, enum_map):
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

        enum_info = enum_map.get(cpp_type)
        property_type = TYPE_MAP.get(cpp_type)
        if enum_info:
            property_type = 'EPropertyType::Enum'

        if not property_type:
            warn_unknown_type(header_path, cpp_type, var_name)
            continue

        enum_array_name = None
        enum_count = 0
        enum_size = 4
        if enum_info:
            enum_array_name = make_enum_array_name(class_name, var_name)
            enum_count = len(enum_info['names'])
            enum_size = enum_info['size']

        properties.append({
            'name': var_name,
            'display_name': get_metadata_value(metadata, 'DisplayName', 'Display'),
            'enum': property_type,
            'min': cpp_float_literal(get_metadata_value(metadata, 'Min', 'ClampMin', 'UIMin'), '0.0f'),
            'max': cpp_float_literal(get_metadata_value(metadata, 'Max', 'ClampMax', 'UIMax'), '0.0f'),
            'speed': cpp_float_literal(get_metadata_value(metadata, 'Speed', 'Step'), '0.1f'),
            'enum_names': enum_info['names'] if enum_info else [],
            'enum_array_name': enum_array_name,
            'enum_count': enum_count,
            'enum_size': enum_size,
        })

    enum_arrays = []
    for prop in properties:
        if not prop['enum_array_name']:
            continue
        names = ', '.join(cpp_string_literal(name) for name in prop['enum_names'])
        enum_arrays.append(
            f'static const char* {prop["enum_array_name"]}[] = {{ {names} }};')
    enum_arrays_str = '\n'.join(enum_arrays)
    if enum_arrays_str:
        enum_arrays_str += '\n\n'

    props_str = ',\n                '.join(
        f'{{ "{p["name"]}", {p["enum"]}, offsetof({class_name}, {p["name"]}), '
        f'EPropertyUsageFlags::Editable, {p["min"]}, {p["max"]}, {p["speed"]}, '
        f'{cpp_string_literal(p["display_name"])}, '
        f'{p["enum_array_name"] or "nullptr"}, {p["enum_count"]}, {p["enum_size"]} }}'
        for p in properties
    )

    include_path = make_include_path(header_path)
    gen_code = f"""// AUTO-GENERATED FILE. DO NOT MODIFY.
#include "{include_path}"
#include "Core/Reflection/ReflectionRegistry.h"

{enum_arrays_str}\
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
    enums = collect_enums()
    for root, _, files in os.walk(ENGINE_SOURCE_DIR):
        for file in files:
            if file.endswith('.h'):
                parse_header_and_generate(Path(root) / file, enums)
