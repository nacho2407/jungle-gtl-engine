import os
import re
from pathlib import Path

TYPE_MAP = {
    'bool': 'EPropertyType::Bool',
    'int32': 'EPropertyType::Int',
    'int': 'EPropertyType::Int',
    'float': 'EPropertyType::Float',
    'FVector': 'EPropertyType::Vec3',
    'FVector4': 'EPropertyType::Vec4',
    'FString': 'EPropertyType::String',
    'FName': 'EPropertyType::Name',
}

ROOT = Path(__file__).resolve().parent.parent
ENGINE_SOURCE_DIR = ROOT / 'JSEngine' / 'Source' / 'Engine'

def make_include_path(header_path):
    return header_path.relative_to(ENGINE_SOURCE_DIR).as_posix()

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
    prop_pattern = re.compile(r'UPROPERTY\([^)]*\)\s*([A-Za-z0-9_:]+)\s+([A-Za-z0-9_]+)\s*(?:=.*?)?;')

    for match in prop_pattern.finditer(content):
        cpp_type = match.group(1)
        var_name = match.group(2)
        properties.append({
            'name': var_name,
            'enum': TYPE_MAP.get(cpp_type, 'EPropertyType::Int'),
        })

    # 프로퍼티 문자열들을 조립
    props_str = ',\n                '.join(
        f'{{ "{p["name"]}", {p["enum"]}, offsetof({class_name}, {p["name"]}) }}'
        for p in properties
    )

    include_path = make_include_path(header_path)
    
    # C++ 코드 템플릿의 세로 길이를 압축하고 f-string으로 통일
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