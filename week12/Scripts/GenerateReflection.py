"""
[ALERT] DO NOT DELETE OR EDIT COMMENT IN THIS FILE ARBITRARILY! 
위 문구는 AI가 임의로 주석을 삭제하지 않도록 하기 위해 작성했고, 얼마든지 수정하셔도 됩니다. 

C++ 헤더 파일을 스캔해서 UE5 스타일의 UCLASS, UPROPERTY, UENUM, UMETA 정보를 읽고, 
각 클래스별로 .gen.cpp 리플렉션 등록 코드를 자동 생성하는 파서 스크립트입니다.

이 스크립트는 다음과 같은 흐름으로 리플렉션 데이터 등록을 수행합니다.
Source/**/*.h 파일 탐색 (Intermediate 폴더 제외)
→ UENUM(...) enum 파싱 및 캐싱
→ UCLASS(...) 및 GENERATED_BODY(...) 클래스 본문 파싱
→ UPROPERTY(...) 멤버 변수 파싱
→ 타입/메타데이터 분석
→ 클래스별 ClassName.gen.cpp 생성
→ StaticClass() 및 FPropertyParams 방식을 통한 리플렉션 등록

주의:
- SRV, CubeSRV, buttons, tag editors, and one-off previews belong to FDebugDetails, not FProperty.
"""

import os
import re
import sys
from pathlib import Path

# Material은 현재 TArray<UMaterialInterface*> 에셋 참조로 등록합니다.
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
    'FGuid': 'EPropertyType::Guid',
    'FQuat': 'EPropertyType::Quat',
    'TArray<FVector>': 'EPropertyType::Vec3Array',
    'TArray<FString>': 'EPropertyType::StringArray',
    'USceneComponent*': 'EPropertyType::SceneComponentRef',
    'TArray<UMaterialInterface*>': 'EPropertyType::Material',
}

# 스크립트 위치를 기준으로 Root와 Source 경로를 계산합니다.
ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIR = ROOT / 'JSEngine' / 'Source'
ENGINE_SOURCE_DIR = SOURCE_DIR / 'Engine'
REFLECTION_OUTPUT_DIR = ROOT / 'JSEngine' / 'Intermediate' / 'Reflection'


# 생성되는 gen.cpp에서 원본 헤더를 include할 때 사용할 상대 경로를 만듭니다.
def make_include_path(header_path):
    if header_path.is_relative_to(ENGINE_SOURCE_DIR):
        return header_path.relative_to(ENGINE_SOURCE_DIR).as_posix()
    return header_path.relative_to(SOURCE_DIR).as_posix()


# 헤더 파일 경로와 클래스 이름을 기반으로 .gen.cpp 출력 경로를 계산합니다.
def make_generated_file_path(header_path, class_name):
    if header_path.is_relative_to(ENGINE_SOURCE_DIR):
        rel_header_path = header_path.relative_to(ENGINE_SOURCE_DIR)
    else:
        rel_header_path = header_path.relative_to(SOURCE_DIR)
    return REFLECTION_OUTPUT_DIR / rel_header_path.with_name(f'{class_name}.gen.cpp')


# C++ 타입 문자열의 공백, const, 포인터/참조 표기를 정규화하여 타입 맵과 비교하기 쉽게 만듭니다.
def normalize_cpp_type(cpp_type):
    normalized = re.sub(r'\bconst\b', '', cpp_type or '')
    normalized = re.sub(r'\s+', ' ', normalized).strip()
    normalized = re.sub(r'\s*<\s*', '<', normalized)
    normalized = re.sub(r'\s*>\s*', '>', normalized)
    normalized = re.sub(r'\s*,\s*', ', ', normalized)
    normalized = re.sub(r'\s*\*\s*', '*', normalized)
    normalized = re.sub(r'\s*&\s*', '&', normalized)
    return normalized


# 문자열 리터럴 안의 내용은 보존하면서 // 및 /* */ 주석을 제거합니다.
def strip_comments(content):
    result = []
    i = 0
    quote = None
    escape = False

    while i < len(content):
        ch = content[i]
        nxt = content[i + 1] if i + 1 < len(content) else ''

        if quote:
            result.append(ch)
            if escape:
                escape = False
            elif ch == '\\':
                escape = True
            elif ch == quote:
                quote = None
            i += 1
            continue

        if ch in ('"', "'"):
            quote = ch
            result.append(ch)
            i += 1
            continue

        if ch == '/' and nxt == '/':
            # 개행은 유지해서 line 구조가 크게 망가지지 않게 합니다.
            while i < len(content) and content[i] not in '\r\n':
                i += 1
            continue

        if ch == '/' and nxt == '*':
            i += 2
            while i + 1 < len(content) and not (content[i] == '*' and content[i + 1] == '/'):
                if content[i] in '\r\n':
                    result.append(content[i])
                i += 1
            i += 2
            continue

        result.append(ch)
        i += 1

    return ''.join(result)


# 현재 위치부터 공백 문자를 건너뛰고 다음 의미 있는 문자 위치를 반환합니다.
def skip_ws(content, index):
    while index < len(content) and content[index].isspace():
        index += 1
    return index


# 여는 괄호/중괄호 위치에서 시작해 대응되는 닫히는 위치를 찾습니다.
def find_matching_delimiter(content, open_index, open_ch='(', close_ch=')'):
    if open_index < 0 or open_index >= len(content) or content[open_index] != open_ch:
        return -1

    depth = 0
    quote = None
    escape = False
    i = open_index

    while i < len(content):
        ch = content[i]

        if quote:
            if escape:
                escape = False
            elif ch == '\\':
                escape = True
            elif ch == quote:
                quote = None
            i += 1
            continue

        if ch in ('"', "'"):
            quote = ch
            i += 1
            continue

        if ch == open_ch:
            depth += 1
        elif ch == close_ch:
            depth -= 1
            if depth == 0:
                return i

        i += 1

    return -1


# UCLASS(...), UPROPERTY(...) 등 매크로 호출 하나를 괄호 짝에 맞춰 읽어냅니다.
def read_balanced_macro(content, keyword, start_index):
    match = re.search(rf'\b{re.escape(keyword)}\s*\(', content[start_index:])
    if not match:
        return None

    macro_start = start_index + match.start()
    open_paren = start_index + match.end() - 1
    close_paren = find_matching_delimiter(content, open_paren, '(', ')')
    if close_paren == -1:
        return None

    return {
        'start': macro_start,
        'open': open_paren,
        'close': close_paren,
        'end': close_paren + 1,
        'metadata': content[open_paren + 1:close_paren],
    }


# 지정한 범위 내에서 특정 매크로 호출을 순차적으로 찾아 반환하는 제너레이터입니다.
def iter_macro_invocations(content, keyword, start=0, end=None):
    if end is None:
        end = len(content)

    index = start
    while index < end:
        macro = read_balanced_macro(content, keyword, index)
        if not macro or macro['start'] >= end:
            break
        yield macro
        index = macro['end']


# 매크로 뒤에 오는 변수나 함수 선언문을 세미콜론(;)까지 읽어냅니다.
def read_statement_until_semicolon(content, start, end):
    quote = None
    escape = False
    paren_depth = 0
    angle_depth = 0
    brace_depth = 0
    bracket_depth = 0
    i = start

    while i < end:
        ch = content[i]

        if quote:
            if escape:
                escape = False
            elif ch == '\\':
                escape = True
            elif ch == quote:
                quote = None
            i += 1
            continue

        if ch in ('"', "'"):
            quote = ch
            i += 1
            continue

        if ch == '(':
            paren_depth += 1
        elif ch == ')' and paren_depth > 0:
            paren_depth -= 1
        elif ch == '<':
            angle_depth += 1
        elif ch == '>' and angle_depth > 0:
            angle_depth -= 1
        elif ch == '{':
            brace_depth += 1
        elif ch == '}' and brace_depth > 0:
            brace_depth -= 1
        elif ch == '[':
            bracket_depth += 1
        elif ch == ']' and bracket_depth > 0:
            bracket_depth -= 1
        elif ch == ';' and paren_depth == 0 and angle_depth == 0 and brace_depth == 0 and bracket_depth == 0:
            return content[start:i].strip(), i + 1

        i += 1

    return None, start


# 메타데이터(UMETA 등) 내부 인자를 쉼표(,) 기준으로 분리합니다. (문자열/괄호 내부 쉼표 무시)
def split_metadata_args(metadata):
    args = []
    current = []
    quote = None
    escape = False
    paren_depth = 0
    angle_depth = 0
    brace_depth = 0

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

        if char == '(':
            paren_depth += 1
        elif char == ')' and paren_depth > 0:
            paren_depth -= 1
        elif char == '<':
            angle_depth += 1
        elif char == '>' and angle_depth > 0:
            angle_depth -= 1
        elif char == '{':
            brace_depth += 1
        elif char == '}' and brace_depth > 0:
            brace_depth -= 1

        if char == ',' and paren_depth == 0 and angle_depth == 0 and brace_depth == 0:
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


# 메타데이터 값의 양끝 따옴표를 제거합니다.
def unquote_metadata_value(value):
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in ('"', "'"):
        return value[1:-1]
    return value


# 메타데이터 인자 문자열을 파싱하여 key-value 딕셔너리로 변환합니다.
def parse_metadata(metadata):
    result = {}
    for arg in split_metadata_args(metadata or ''):
        if '=' not in arg:
            result[arg.strip()] = True
            continue

        key, value = arg.split('=', 1)
        result[key.strip()] = unquote_metadata_value(value)
    return result


# Python 문자열을 C++ 코드에 안전하게 넣을 수 있는 문자열 리터럴로 변환합니다.
def cpp_string_literal(value):
    if value is None:
        return 'nullptr'
    escaped = value.replace('\\', '\\\\').replace('"', '\\"')
    return f'"{escaped}"'


# 메타데이터의 숫자 문자열을 C++ float 리터럴 형태로 변환합니다.
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


# 여러 후보 키 중에서 메타데이터에 존재하는 첫 번째 값을 반환합니다.
def get_metadata_value(metadata, *keys):
    for key in keys:
        if key in metadata:
            return metadata[key]
    return None


# UPROPERTY 메타데이터를 기반으로 런타임 프로퍼티 플래그(EPropertyFlags)를 C++ 코드로 생성합니다.
def make_runtime_property_flags(metadata):
    flags = [
        'EPropertyFlags::Read',
        'EPropertyFlags::Write',
        'EPropertyFlags::Edit',
    ]

    if get_metadata_value(metadata, 'Transient') is not None:
        flags.append('EPropertyFlags::Transient')
    if get_metadata_value(metadata, 'SaveGame') is not None:
        flags.append('EPropertyFlags::SaveGame')
    if get_metadata_value(metadata, 'Animatable') is not None:
        flags.append('EPropertyFlags::Animatable')
    if get_metadata_value(metadata, 'LuaRead') is not None:
        flags.append('EPropertyFlags::LuaRead')
    if get_metadata_value(metadata, 'LuaWrite') is not None:
        flags.append('EPropertyFlags::LuaWrite')

    return ' | '.join(flags)


# UCLASS 메타데이터와 이름 규칙을 기반으로 클래스 플래그(ClassFlags)를 생성합니다.
def make_class_flags(metadata, class_name, parent_name, is_abstract=False):
    flags = []
    if is_abstract or get_metadata_value(metadata, 'Abstract') is not None:
        flags.append('CF_Abstract')
    if class_name.startswith('A') or parent_name.startswith('A') or get_metadata_value(metadata, 'Actor') is not None:
        flags.append('CF_Actor')
    if 'Component' in class_name or 'Component' in parent_name or get_metadata_value(metadata, 'Component') is not None:
        flags.append('CF_Component')
    if 'Camera' in class_name or 'Camera' in parent_name or get_metadata_value(metadata, 'Camera') is not None:
        flags.append('CF_Camera')

    return ' | '.join(flags) if flags else 'CF_None'


# C++ 식별자로 사용할 수 없는 문자를 '_'로 치환합니다.
def sanitize_cpp_identifier(name):
    sanitized = re.sub(r'[^A-Za-z0-9_]', '_', name)
    if sanitized and sanitized[0].isdigit():
        sanitized = '_' + sanitized
    return sanitized


# C++ 정수 리터럴의 접미사(u, l 등)를 제거하여 Python eval()이 처리할 수 있게 합니다.
def strip_numeric_suffixes(expr):
    return re.sub(r'\b(0[xX][0-9A-Fa-f]+|\d+)(?:[uUlL]+)\b', lambda match: match.group(1), expr)


# enum 값에 할당된 상수 표현식을 파싱 및 계산합니다.
def try_eval_enum_expr(expr, known_values):
    expr = strip_numeric_suffixes(expr.strip())
    if not expr:
        return None

    tokens = set(re.findall(r'\b[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)?\b', expr))
    for token in sorted(tokens, key=len, reverse=True):
        short_token = token.split('::')[-1]
        if token in known_values:
            replacement = known_values[token]
        elif short_token in known_values:
            replacement = known_values[short_token]
        else:
            return None
        expr = re.sub(rf'\b{re.escape(token)}\b', str(replacement), expr)

    if not re.fullmatch(r'[0-9xXa-fA-F\s\+\-\*\/\%\|\&\^\~\<\>\(\)]+', expr):
        return None

    try:
        return int(eval(expr, {'__builtins__': {}}, {}))
    except Exception:
        return None


# enum 멤버 뒤에 붙은 UMETA(...) 구문을 분리하여 반환합니다.
def parse_trailing_umeta(item):
    match = re.search(r'\bUMETA\s*\(', item)
    if not match:
        return item.strip(), {}

    open_paren = match.end() - 1
    close_paren = find_matching_delimiter(item, open_paren, '(', ')')
    if close_paren == -1:
        return item.strip(), {}

    tail = item[close_paren + 1:].strip()
    if tail:
        return item.strip(), {}

    metadata = parse_metadata(item[open_paren + 1:close_paren])
    return item[:match.start()].strip(), metadata


# enum 본문 내의 각 멤버를 파싱하여 이름, 표시 이름, 정수값을 추출합니다.
def parse_enum_members(enum_body):
    values = []
    known_values = {}
    next_value = 0

    for item in split_metadata_args(enum_body):
        item = item.strip()
        if not item:
            continue

        item, umeta_metadata = parse_trailing_umeta(item)
        if '=' in item:
            name_part, value_expr = item.split('=', 1)
            name = name_part.strip().split('::')[-1].strip()
            parsed_value = try_eval_enum_expr(value_expr, known_values)
            value = parsed_value if parsed_value is not None else next_value
        else:
            name = item.strip().split('::')[-1].strip()
            value = next_value

        if not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', name):
            continue

        if get_metadata_value(umeta_metadata, 'Hidden') is None:
            display_name = get_metadata_value(umeta_metadata, 'DisplayName', 'Display') or name
            values.append({
                'name': name,
                'display_name': display_name,
                'value': value,
            })

        known_values[name] = value
        next_value = value + 1

    return values


# 특정 위치를 감싸고 있는 C++ namespace들을 찾아 목록으로 반환합니다.
def find_enclosing_namespaces(content, position):
    namespaces = []
    ns_pattern = re.compile(r'\bnamespace\s+([A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)*)\s*\{')

    for match in ns_pattern.finditer(content):
        open_brace = content.find('{', match.start())
        if open_brace == -1 or open_brace > position:
            continue
        close_brace = find_matching_delimiter(content, open_brace, '{', '}')
        if close_brace != -1 and open_brace < position < close_brace:
            namespaces.append((open_brace, match.group(1)))

    namespaces.sort(key=lambda item: item[0])
    result = []
    for _, name in namespaces:
        result.extend(name.split('::'))
    return result


# namespace 목록과 식별자 이름을 조합하여 완전한 이름(Qualified Name)을 만듭니다.
def qualify_name(namespace_parts, name):
    return '::'.join(namespace_parts + [name]) if namespace_parts else name


# 소스 디렉토리 내에서 Intermediate 폴더를 제외한 모든 헤더 파일 경로를 순회합니다.
def iter_header_paths():
    for header_path in SOURCE_DIR.rglob('*.h'):
        if 'Intermediate' in header_path.parts:
            continue
        yield header_path


# 전체 헤더 파일을 스캔하여 UENUM 정보를 수집하고 맵 형태로 반환합니다.
def collect_enums():
    enum_infos = []

    for header_path in iter_header_paths():
        try:
            content = strip_comments(header_path.read_text(encoding='utf-8'))
        except UnicodeDecodeError:
            continue

        for macro in iter_macro_invocations(content, 'UENUM'):
            enum_match = re.search(r'\benum\s+(?:class\s+)?([A-Za-z_][A-Za-z0-9_]*)\b', content[macro['end']:])
            if not enum_match:
                continue

            enum_start = macro['end'] + enum_match.start()
            enum_name = enum_match.group(1)
            open_brace = content.find('{', enum_start)
            if open_brace == -1:
                continue

            enum_header = content[enum_start:open_brace]
            header_match = re.search(
                r'\benum\s+(?:class\s+)?(?P<name>[A-Za-z_][A-Za-z0-9_]*)'
                r'(?:\s*:\s*(?P<underlying>.*?))?\s*$',
                enum_header,
                re.DOTALL,
            )
            if not header_match:
                continue

            close_brace = find_matching_delimiter(content, open_brace, '{', '}')
            if close_brace == -1:
                continue

            semi = skip_ws(content, close_brace + 1)
            if semi >= len(content) or content[semi] != ';':
                continue

            namespace_parts = find_enclosing_namespaces(content, enum_start)
            qualified_name = qualify_name(namespace_parts, enum_name)
            enum_infos.append({
                'name': enum_name,
                'qualified_name': qualified_name,
                'values': parse_enum_members(content[open_brace + 1:close_brace]),
            })

    enum_map = {}
    short_name_counts = {}
    for enum_info in enum_infos:
        short_name_counts[enum_info['name']] = short_name_counts.get(enum_info['name'], 0) + 1
        enum_map[enum_info['qualified_name']] = enum_info

    for enum_info in enum_infos:
        if short_name_counts[enum_info['name']] == 1:
            enum_map[enum_info['name']] = enum_info
        else:
            print(
                f"Reflection warning: UENUM short name '{enum_info['name']}' is ambiguous; use qualified name.",
                file=sys.stderr,
            )

    return enum_map


# C++ 멤버 변수 선언문에서 타입과 변수명을 분리하여 추출합니다.
def parse_property_declaration(declaration):
    declaration = declaration.split('=', 1)[0].strip()
    declaration = re.sub(r'\s*\{.*\}\s*$', '', declaration, flags=re.DOTALL).strip()
    declaration = re.sub(r'\bUPROPERTY\s*\(.*\)', '', declaration).strip()

    name_match = re.search(r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*$', declaration)
    if not name_match:
        return None, None

    var_name = name_match.group(1)
    cpp_type = declaration[:name_match.start()].strip()
    return normalize_cpp_type(cpp_type), var_name


# 지원하지 않는 UPROPERTY 타입을 발견했을 때 경고를 출력합니다.
def warn_unknown_type(header_path, cpp_type, var_name):
    print(
        f"Reflection warning: unknown UPROPERTY type '{cpp_type}' for '{var_name}' in {header_path.relative_to(ROOT)}; skipped.",
        file=sys.stderr,
    )


# FEnumValue 배열에 사용할 정적 변수 이름을 생성합니다.
def make_enum_values_array_name(enum_name):
    return f'Z_Enum_{sanitize_cpp_identifier(enum_name)}_Values'


# UEnum(또는 FEnumMetaData)에 사용할 정적 변수 이름을 생성합니다.
def make_enum_meta_name(enum_name):
    return f'Z_Enum_{sanitize_cpp_identifier(enum_name)}_Meta'


# 수집된 enum 정보를 기반으로 .gen.cpp에 들어갈 메타데이터 등록 코드를 생성합니다.
def generate_enum_metadata(enum_infos):
    blocks = []
    for enum_key in sorted(enum_infos):
        enum_info = enum_infos[enum_key]
        enum_name = enum_info['qualified_name']
        values_array_name = make_enum_values_array_name(enum_name)
        enum_meta_name = make_enum_meta_name(enum_name)
        values = enum_info['values']
        values_body = ',\n    '.join(
            f'{{ {cpp_string_literal(value["name"])}, {cpp_string_literal(value["display_name"])}, {value["value"]} }}'
            for value in values
        )
        if values_body:
            values_body = '    ' + values_body + '\n'
        blocks.append(
            f'static const FEnumValue {values_array_name}[] = {{\n{values_body}}};\n'
            f'static const UEnum {enum_meta_name} = {{ '
            f'{cpp_string_literal(enum_name)}, static_cast<uint8>(sizeof({enum_name})), {values_array_name}, {len(values)} }};'
        )
    return '\n\n'.join(blocks)


# 클래스 선언부에서 부모 클래스 이름을 추출합니다.
def parse_parent_from_class_header(class_header):
    if ':' not in class_header:
        return None

    inheritance = class_header.split(':', 1)[1]
    parent_match = re.search(r'\bpublic\s+([A-Za-z_][A-Za-z0-9_:]*)\b', inheritance)
    if parent_match:
        return parent_match.group(1).split('::')[-1]
    return None


# 클래스 본문에 직접 선언된 pure virtual 함수가 있는지 확인합니다.
# 일반 멤버 초기화자 `= 0`과 구분하기 위해 virtual이 포함된 선언만 대상으로 삼습니다.
def class_has_direct_pure_virtual(class_body):
    return re.search(
        r'\bvirtual\b[^;{}]*=\s*0\s*;',
        class_body or '',
        re.DOTALL,
    ) is not None


# GENERATED_BODY 매크로 내부의 인자(클래스 이름, 부모 클래스 이름)를 파싱합니다.
def parse_generated_body(content, class_info):
    for macro in iter_macro_invocations(content, 'GENERATED_BODY', class_info['body_start'], class_info['body_end']):
        args = split_metadata_args(macro['metadata'])
        if len(args) >= 2:
            return args[0].strip(), args[1].strip()
        return None, None
    return None, None


# 파일 내에서 UCLASS 매크로와 클래스 본문을 찾아 정보를 수집합니다.
def find_uclass_declarations(content):
    classes = []

    for macro in iter_macro_invocations(content, 'UCLASS'):
        class_match = re.search(r'\bclass\b', content[macro['end']:])
        if not class_match:
            continue

        class_keyword = macro['end'] + class_match.start()
        open_brace = content.find('{', class_keyword)
        if open_brace == -1:
            continue

        class_header = content[class_keyword:open_brace]
        before_inheritance = class_header.split(':', 1)[0]
        identifiers = re.findall(r'\b[A-Za-z_][A-Za-z0-9_]*\b', before_inheritance)
        if len(identifiers) < 2 or identifiers[0] != 'class':
            continue

        class_name = identifiers[-1]
        if not re.match(r'[A-Z]\w*$', class_name):
            continue

        close_brace = find_matching_delimiter(content, open_brace, '{', '}')
        if close_brace == -1:
            continue

        parsed_parent_name = parse_parent_from_class_header(class_header) or 'UObject'
        class_body = content[open_brace + 1:close_brace]
        class_info = {
            'name': class_name,
            'parent_name': parsed_parent_name,
            'metadata': parse_metadata(macro['metadata']),
            'class_start': class_keyword,
            'body_start': open_brace + 1,
            'body_end': close_brace,
            'class_end': close_brace + 1,
            'is_abstract': class_has_direct_pure_virtual(class_body) or get_metadata_value(parse_metadata(macro['metadata']), 'Abstract') is not None,
        }

        generated_class_name, generated_parent_name = parse_generated_body(content, class_info)
        if generated_class_name and generated_class_name != class_name:
            raise RuntimeError(
                f"GENERATED_BODY class mismatch: parsed '{class_name}', macro '{generated_class_name}'")
        if generated_parent_name and generated_parent_name != parsed_parent_name:
            raise RuntimeError(
                f"GENERATED_BODY parent mismatch for '{class_name}': parsed '{parsed_parent_name}', macro '{generated_parent_name}'")

        classes.append(class_info)

    return classes


# 특정 클래스 본문 내부에 선언된 UPROPERTY와 변수 선언을 수집합니다.
def find_uproperties_in_class(content, class_info):
    properties = []
    body_start = class_info['body_start']
    body_end = class_info['body_end']

    for macro in iter_macro_invocations(content, 'UPROPERTY', body_start, body_end):
        metadata = parse_metadata(macro['metadata'])
        decl_start = skip_ws(content, macro['end'])
        declaration, _ = read_statement_until_semicolon(content, decl_start, body_end)
        if not declaration:
            continue
        properties.append({
            'metadata': metadata,
            'declaration': declaration,
            'macro_start': macro['start'],
        })

    return properties


# C++ 타입을 분석하여 EPropertyType과 연관된 enum 정보를 반환합니다.
def resolve_property_type(cpp_type, enum_map):
    enum_info = enum_map.get(cpp_type)
    if not enum_info and '::' in cpp_type:
        enum_info = enum_map.get(cpp_type.split('::')[-1])

    if enum_info:
        return 'EPropertyType::Enum', enum_info

    property_type = TYPE_MAP.get(cpp_type)
    if property_type:
        return property_type, None

    return None, None


# 파싱된 클래스 정보를 바탕으로 ClassName.gen.cpp 파일을 생성합니다.
def generate_class_file(header_path, class_info, properties, used_enums):
    class_name = class_info['name']
    parent_name = class_info['parent_name']
    enum_metadata_str = generate_enum_metadata(used_enums)
    if enum_metadata_str:
        enum_metadata_str += '\n\n'

    enum_registration = '\n'.join(
        f'        FReflectionRegistry::Get().RegisterEnum(&{make_enum_meta_name(enum_info["qualified_name"])});'
        for enum_name, enum_info in sorted(used_enums.items())
    )
    if enum_registration:
        enum_registration += '\n'

    runtime_props_str = '\n'.join(
        '        Class->AddProperty(FProperty(FPropertyParams{\n'
        f'            {cpp_string_literal(p["name"])},\n'
        f'            {cpp_string_literal(p["display_name"])},\n'
        f'            {cpp_string_literal(p["category"])},\n'
        f'            {p["property_type"]},\n'
        f'            {p["property_flags"]},\n'
        f'            offsetof({class_name}, {p["name"]}),\n'
        f'            sizeof((({class_name}*)nullptr)->{p["name"]}),\n'
        f'            {p["min"]},\n'
        f'            {p["max"]},\n'
        f'            {p["speed"]},\n'
        f'            {("&" + make_enum_meta_name(p["enum_info"]["qualified_name"])) if p["enum_info"] else "nullptr"},\n'
        '            nullptr,\n'
        '            nullptr\n'
        '        }));'
        for p in properties
    )

    create_func = 'nullptr'
    if not class_info.get('is_abstract', False):
        create_func = f'[]() -> UObject* {{ return CreateReflectedObject<{class_name}>(); }}'

    include_path = make_include_path(header_path)
    class_flags = make_class_flags(class_info['metadata'], class_name, parent_name, class_info.get('is_abstract', False))
    gen_code = f"""// AUTO-GENERATED FILE. DO NOT MODIFY.
#include \"{include_path}\"
#include \"Core/Reflection/ReflectionRegistry.h\"
#include \"Object/Class.h\"
#include \"Object/Object.h\"
#include \"Object/Property.h\"

{enum_metadata_str}\
struct Z_Construct_UClass_{class_name} {{
    static void RegisterRuntimeEnums() {{
{enum_registration}    }}

    static void RegisterRuntimeProperties(UClass* Class) {{
        if (!Class) {{
            return;
        }}
{runtime_props_str}
    }}
}};

UClass* {class_name}::StaticClass()
{{
    static UClass Class(
        \"{class_name}\",
        {parent_name}::StaticClass(),
        sizeof({class_name}),
        {class_flags},
        {create_func});

    static bool bRegistered = false;
    if (!bRegistered)
    {{
        bRegistered = true;
        Z_Construct_UClass_{class_name}::RegisterRuntimeEnums();
        FReflectionRegistry::Get().RegisterUClass(&Class);
        Z_Construct_UClass_{class_name}::RegisterRuntimeProperties(&Class);
    }}
    return &Class;
}}

struct Z_AutoRegister_UClass_{class_name} {{
    Z_AutoRegister_UClass_{class_name}() {{
        {class_name}::StaticClass();
    }}
}};

static Z_AutoRegister_UClass_{class_name} Z_AutoRegister_UClass_{class_name}_Var;
"""

    gen_filepath = make_generated_file_path(header_path, class_name)
    gen_filepath.parent.mkdir(parents=True, exist_ok=True)
    with open(gen_filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write(gen_code)
    print(f'Generated: {gen_filepath.relative_to(ROOT)}')


# 헤더 파일 하나를 분석하여 UCLASS, UPROPERTY 등을 추출하고 .gen.cpp 코드를 생성합니다.
def parse_header_and_generate(header_path, enum_map):
    try:
        raw_content = header_path.read_text(encoding='utf-8')
    except UnicodeDecodeError:
        return

    content = strip_comments(raw_content)
    if 'UCLASS' not in content:
        return

    class_infos = find_uclass_declarations(content)
    if not class_infos:
        return

    for class_info in class_infos:
        class_name = class_info['name']
        properties = []
        used_enums = {}

        for prop in find_uproperties_in_class(content, class_info):
            cpp_type, var_name = parse_property_declaration(prop['declaration'])
            if not cpp_type or not var_name:
                print(
                    f"Reflection warning: failed to parse UPROPERTY declaration in {header_path.relative_to(ROOT)}; skipped.",
                    file=sys.stderr,
                )
                continue

            property_type, enum_info = resolve_property_type(cpp_type, enum_map)
            if not property_type:
                warn_unknown_type(header_path, cpp_type, var_name)
                continue

            if enum_info:
                used_enums[enum_info['qualified_name']] = enum_info

            metadata = prop['metadata']
            properties.append({
                'name': var_name,
                'display_name': get_metadata_value(metadata, 'DisplayName', 'Display'),
                'property_type': property_type,
                'category': get_metadata_value(metadata, 'Category'),
                'property_flags': make_runtime_property_flags(metadata),
                'min': cpp_float_literal(get_metadata_value(metadata, 'Min', 'ClampMin', 'UIMin'), '0.0f'),
                'max': cpp_float_literal(get_metadata_value(metadata, 'Max', 'ClampMax', 'UIMax'), '0.0f'),
                'speed': cpp_float_literal(get_metadata_value(metadata, 'Speed', 'Step'), '0.1f'),
                'enum_info': enum_info,
            })

        generate_class_file(header_path, class_info, properties, used_enums)


if __name__ == '__main__':
    enums = collect_enums()
    for header in iter_header_paths():
        parse_header_and_generate(header, enums)
