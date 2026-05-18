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

TYPE_MAP = {
    'bool': 'EPropertyType::Bool',
    'int32': 'EPropertyType::Int',
    'int': 'EPropertyType::Int',
    'float': 'EPropertyType::Float',
    'FString': 'EPropertyType::String',
    'FName': 'EPropertyType::Name',
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
    if class_name.startswith('A') or parent_name.startswith('A'):
        flags.append('CF_Actor')
    if 'Component' in class_name or 'Component' in parent_name:
        flags.append('CF_Component')
    if 'Camera' in class_name or 'Camera' in parent_name:
        flags.append('CF_Camera')
    if get_metadata_value(metadata, 'Placeable') is not None:
        flags.append('CF_Placeable')
    if get_metadata_value(metadata, 'SpawnableComponent') is not None:
        flags.append('CF_SpawnableComponent')

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


ASSET_OBJECT_TYPES = {
    'UMaterialInterface',
    'UMaterial',
    'UMaterialInstance',
    'UStaticMesh',
    'USkeletalMesh',
    'UAnimationAsset',
    'UAnimSequence',
    'UAnimSequenceBase',
    'UTexture',
}


def parse_single_template_arg(cpp_type, template_name):
    prefix = f'{template_name}<'
    if not cpp_type.startswith(prefix) or not cpp_type.endswith('>'):
        return None
    inner = cpp_type[len(prefix):-1].strip()
    if not inner:
        return None
    return inner


def is_uobject_type_name(type_name):
    short_name = type_name.split('::')[-1]
    return len(short_name) > 1 and short_name[0] in ('U', 'A')


def object_class_expr(type_name):
    return f'{type_name}::StaticClass()' if is_uobject_type_name(type_name) else 'nullptr'


def reference_kind_for_object_type(type_name, metadata=None, b_soft=False):
    metadata = metadata or {}
    explicit = get_metadata_value(metadata, 'ReferenceKind')
    if explicit:
        value = str(explicit).strip()
        if value.startswith('EObjectReferenceKind::'):
            return value
        if value in ('RuntimeObject', 'ActorComponent', 'Asset', 'None'):
            return f'EObjectReferenceKind::{value}'

    if b_soft:
        return 'EObjectReferenceKind::Asset'

    short_name = type_name.split('::')[-1]
    if short_name in ASSET_OBJECT_TYPES:
        return 'EObjectReferenceKind::Asset'
    if short_name.endswith('Component'):
        return 'EObjectReferenceKind::ActorComponent'
    return 'EObjectReferenceKind::RuntimeObject'




def make_type_info(
    cpp_type,
    property_type,
    enum_info=None,
    object_class='nullptr',
    reference_kind='EObjectReferenceKind::None',
    inner=None,
    array_ops='nullptr',
    soft_ops='nullptr',
    object_ops='nullptr',
    script_struct='nullptr',
    editor_hint='nullptr',
    struct_info=None,
):
    return {
        'cpp_type': cpp_type,
        'property_type': property_type,
        'enum_info': enum_info,
        'object_class': object_class,
        'reference_kind': reference_kind,
        'inner': inner,
        'array_ops': array_ops,
        'soft_ops': soft_ops,
        'object_ops': object_ops,
        'script_struct': script_struct,
        'editor_hint': editor_hint,
        'struct_info': struct_info,
    }


def find_struct_info(struct_map, cpp_type):
    struct_info = struct_map.get(cpp_type) if struct_map else None
    if not struct_info and '::' in cpp_type:
        struct_info = struct_map.get(cpp_type.split('::')[-1])
    return struct_info


def struct_static_expr(struct_info):
    return f'{struct_info["qualified_name"]}::StaticStruct()'


def default_editor_hint_for_struct(struct_info, metadata=None):
    metadata = metadata or {}
    explicit = get_metadata_value(metadata, 'EditorHint')
    if explicit:
        return explicit
    explicit = get_metadata_value(struct_info.get('metadata', {}), 'EditorHint')
    if explicit:
        return explicit
    return struct_info['name']

def make_object_ops_expr(value_cpp_type):
    if value_cpp_type.endswith('*'):
        pointed_type = value_cpp_type[:-1]
        return f'GetRawObjectPtrOps<{pointed_type}>()'

    inner = parse_single_template_arg(value_cpp_type, 'TObjectPtr')
    if inner:
        return f'GetTObjectPtrOps<{inner}>()'

    return 'nullptr'


def make_soft_ops_expr(value_cpp_type):
    inner = parse_single_template_arg(value_cpp_type, 'TSoftObjectPtr')
    if inner:
        return f'GetSoftObjectPtrOps<{inner}>()'
    return 'nullptr'


def make_array_ops_expr(value_cpp_type):
    inner = parse_single_template_arg(value_cpp_type, 'TArray')
    if inner:
        return f'GetArrayPropertyOps<{inner}>()'
    return 'nullptr'


def make_property_type_info(cpp_type, enum_map, struct_map=None, metadata=None):
    enum_info = enum_map.get(cpp_type)
    if not enum_info and '::' in cpp_type:
        enum_info = enum_map.get(cpp_type.split('::')[-1])

    if enum_info:
        return make_type_info(
            cpp_type=cpp_type,
            property_type='EPropertyType::Enum',
            enum_info=enum_info,
        )

    struct_info = find_struct_info(struct_map or {}, cpp_type)
    if struct_info:
        editor_hint = default_editor_hint_for_struct(struct_info, metadata)
        return make_type_info(
            cpp_type=cpp_type,
            property_type='EPropertyType::Struct',
            script_struct=struct_static_expr(struct_info),
            editor_hint=cpp_string_literal(editor_hint),
            struct_info=struct_info,
        )

    inner_array_type = parse_single_template_arg(cpp_type, 'TArray')
    if inner_array_type:
        inner_info = make_property_type_info(inner_array_type, enum_map, struct_map, metadata=None)
        if not inner_info:
            return None
        return make_type_info(
            cpp_type=cpp_type,
            property_type='EPropertyType::Array',
            inner=inner_info,
            array_ops=make_array_ops_expr(cpp_type),
        )

    soft_type = parse_single_template_arg(cpp_type, 'TSoftObjectPtr')
    if soft_type:
        return make_type_info(
            cpp_type=cpp_type,
            property_type='EPropertyType::SoftObjectPtr',
            object_class=object_class_expr(soft_type),
            reference_kind=reference_kind_for_object_type(soft_type, metadata, b_soft=True),
            soft_ops=make_soft_ops_expr(cpp_type),
        )

    object_ptr_type = parse_single_template_arg(cpp_type, 'TObjectPtr')
    if object_ptr_type:
        return make_type_info(
            cpp_type=cpp_type,
            property_type='EPropertyType::ObjectPtr',
            object_class=object_class_expr(object_ptr_type),
            reference_kind=reference_kind_for_object_type(object_ptr_type, metadata),
            object_ops=make_object_ops_expr(cpp_type),
        )

    if cpp_type.endswith('*'):
        pointed_type = cpp_type[:-1]
        if is_uobject_type_name(pointed_type):
            return make_type_info(
                cpp_type=cpp_type,
                property_type='EPropertyType::ObjectPtr',
                object_class=object_class_expr(pointed_type),
                reference_kind=reference_kind_for_object_type(pointed_type, metadata),
                object_ops=make_object_ops_expr(cpp_type),
            )

    property_type = TYPE_MAP.get(cpp_type)
    if property_type:
        return make_type_info(cpp_type=cpp_type, property_type=property_type)

    return None

def iter_type_infos(type_info):
    if not type_info:
        return
    yield type_info
    if type_info.get('inner'):
        yield from iter_type_infos(type_info['inner'])


def make_property_params_block(prop, name_expr, offset_expr, size_expr, inner_expr='nullptr'):
    type_info = prop['type_info']
    enum_info = type_info['enum_info']
    enum_expr = ("&" + make_enum_meta_name(enum_info["qualified_name"])) if enum_info else "nullptr"
    return (
        'FPropertyParams{\n'
        f'            {name_expr},\n'
        f'            {cpp_string_literal(prop["display_name"])},\n'
        f'            {cpp_string_literal(prop["category"])},\n'
        f'            {type_info["property_type"]},\n'
        f'            {prop["property_flags"]},\n'
        f'            {offset_expr},\n'
        f'            {size_expr},\n'
        f'            {prop["min"]},\n'
        f'            {prop["max"]},\n'
        f'            {prop["speed"]},\n'
        f'            {enum_expr},\n'
        f'            {type_info["object_class"]},\n'
        f'            {type_info["reference_kind"]},\n'
        f'            {inner_expr},\n'
        f'            {type_info["array_ops"]},\n'
        f'            {type_info["soft_ops"]},\n'
        f'            {type_info["object_ops"]},\n'
        f'            {type_info["script_struct"]},\n'
        f'            {type_info["editor_hint"]}\n'
        '        }'
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
            f'static const UEnum {enum_meta_name}(\n'
            f'    {cpp_string_literal(enum_name)}, static_cast<uint8>(sizeof({enum_name})), {values_array_name}, {len(values)});'
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



def parse_generated_struct_body(content, struct_info):
    for macro in iter_macro_invocations(content, 'GENERATED_STRUCT_BODY', struct_info['body_start'], struct_info['body_end']):
        args = split_metadata_args(macro['metadata'])
        if len(args) >= 1:
            return args[0].strip()
        return None
    return None


# 파일 내에서 USTRUCT 매크로와 구조체 본문을 찾아 정보를 수집합니다.
def find_ustruct_declarations(content):
    structs = []

    for macro in iter_macro_invocations(content, 'USTRUCT'):
        struct_match = re.search(r'\bstruct\b', content[macro['end']:])
        if not struct_match:
            continue

        struct_keyword = macro['end'] + struct_match.start()
        open_brace = content.find('{', struct_keyword)
        if open_brace == -1:
            continue

        struct_header = content[struct_keyword:open_brace]
        before_inheritance = struct_header.split(':', 1)[0]
        identifiers = re.findall(r'\b[A-Za-z_][A-Za-z0-9_]*\b', before_inheritance)
        if len(identifiers) < 2 or identifiers[0] != 'struct':
            continue

        struct_name = identifiers[-1]
        close_brace = find_matching_delimiter(content, open_brace, '{', '}')
        if close_brace == -1:
            continue

        namespace_parts = find_enclosing_namespaces(content, struct_keyword)
        struct_info = {
            'name': struct_name,
            'qualified_name': qualify_name(namespace_parts, struct_name),
            'metadata': parse_metadata(macro['metadata']),
            'struct_start': struct_keyword,
            'body_start': open_brace + 1,
            'body_end': close_brace,
            'struct_end': close_brace + 1,
        }

        generated_struct_name = parse_generated_struct_body(content, struct_info)
        if generated_struct_name and generated_struct_name != struct_name:
            raise RuntimeError(
                f"GENERATED_STRUCT_BODY struct mismatch: parsed '{struct_name}', macro '{generated_struct_name}'")
        if not generated_struct_name:
            raise RuntimeError(
                f"USTRUCT '{struct_name}' is missing GENERATED_STRUCT_BODY({struct_name})")

        structs.append(struct_info)

    return structs


# 전체 헤더 파일을 스캔하여 USTRUCT 정보를 수집하고 맵 형태로 반환합니다.
def collect_structs():
    struct_infos = []

    for header_path in iter_header_paths():
        try:
            content = strip_comments(header_path.read_text(encoding='utf-8'))
        except UnicodeDecodeError:
            continue
        if 'USTRUCT' not in content:
            continue
        for struct_info in find_ustruct_declarations(content):
            struct_info['header_path'] = header_path
            struct_infos.append(struct_info)

    struct_map = {}
    short_name_counts = {}
    for struct_info in struct_infos:
        short_name_counts[struct_info['name']] = short_name_counts.get(struct_info['name'], 0) + 1
        struct_map[struct_info['qualified_name']] = struct_info

    for struct_info in struct_infos:
        if short_name_counts[struct_info['name']] == 1:
            struct_map[struct_info['name']] = struct_info
        else:
            print(
                f"Reflection warning: USTRUCT short name '{struct_info['name']}' is ambiguous; use qualified name.",
                file=sys.stderr,
            )

    return struct_map


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


# 특정 본문 범위 내부에 선언된 UPROPERTY와 변수 선언을 수집합니다.
def find_uproperties_in_range(content, body_start, body_end):
    properties = []
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


# 특정 클래스 본문 내부에 선언된 UPROPERTY와 변수 선언을 수집합니다.
def find_uproperties_in_class(content, class_info):
    return find_uproperties_in_range(content, class_info['body_start'], class_info['body_end'])


# 특정 구조체 본문 내부에 선언된 UPROPERTY와 변수 선언을 수집합니다.
def find_uproperties_in_struct(content, struct_info):
    return find_uproperties_in_range(content, struct_info['body_start'], struct_info['body_end'])


# C++ 타입을 분석하여 EPropertyType과 연관된 enum/struct 정보를 반환합니다.
def resolve_property_type(cpp_type, enum_map, struct_map=None, metadata=None):
    type_info = make_property_type_info(cpp_type, enum_map, struct_map, metadata)
    if not type_info:
        return None
    return type_info


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

    static_property_defs = []
    static_name_counter = 0

    def make_inner_property(type_info, owner_prop_name):
        nonlocal static_name_counter
        inner_expr = 'nullptr'
        if type_info.get('inner'):
            inner_expr = make_inner_property(type_info['inner'], owner_prop_name)

        static_name_counter += 1
        static_name = f'Z_Property_{class_name}_{sanitize_cpp_identifier(owner_prop_name)}_Inner_{static_name_counter}'
        fake_prop = {
            'display_name': None,
            'category': None,
            'property_flags': 'EPropertyFlags::Read | EPropertyFlags::Write | EPropertyFlags::Edit',
            'min': '0.0f',
            'max': '0.0f',
            'speed': '0.1f',
            'type_info': type_info,
        }
        size_expr = f'sizeof({type_info["cpp_type"]})'
        params = make_property_params_block(fake_prop, "nullptr", "0", size_expr, inner_expr)
        static_property_defs.append(f'static const FProperty {static_name}({params});')
        return f'&{static_name}'

    runtime_prop_lines = []
    for p in properties:
        inner_expr = 'nullptr'
        if p['type_info'].get('inner'):
            inner_expr = make_inner_property(p['type_info']['inner'], p['name'])
        params = make_property_params_block(
            p,
            cpp_string_literal(p["name"]),
            f'offsetof({class_name}, {p["name"]})',
            f'sizeof((({class_name}*)nullptr)->{p["name"]})',
            inner_expr)
        runtime_prop_lines.append(f'        Class->AddProperty(FProperty({params}));')

    static_property_defs_str = '\n'.join(static_property_defs)
    if static_property_defs_str:
        static_property_defs_str += '\n\n'

    runtime_props_str = '\n'.join(runtime_prop_lines)

    create_func = 'nullptr'
    if not class_info.get('is_abstract', False):
        create_func = f'[]() -> UObject* {{ return CreateReflectedObject<{class_name}>(); }}'

    include_path = make_include_path(header_path)
    class_flags = make_class_flags(class_info['metadata'], class_name, parent_name, class_info.get('is_abstract', False))
    class_display_name = get_metadata_value(class_info['metadata'], 'DisplayName', 'Display')
    class_category = get_metadata_value(class_info['metadata'], 'Category')
    gen_code = f"""// AUTO-GENERATED FILE. DO NOT MODIFY.
#include \"{include_path}\"
#include \"Core/Reflection/ReflectionRegistry.h\"
#include \"Object/Class.h\"
#include \"Object/Object.h\"
#include \"Object/Property.h\"

{enum_metadata_str}{static_property_defs_str}\
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
        {create_func},
        {cpp_string_literal(class_display_name)},
        {cpp_string_literal(class_category)});

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




def generate_struct_file(header_path, struct_info, properties, used_enums):
    struct_name = struct_info['name']
    qualified_name = struct_info['qualified_name']
    enum_metadata_str = generate_enum_metadata(used_enums)
    if enum_metadata_str:
        enum_metadata_str += '\n\n'

    enum_registration = '\n'.join(
        f'        FReflectionRegistry::Get().RegisterEnum(&{make_enum_meta_name(enum_info["qualified_name"])});'
        for enum_name, enum_info in sorted(used_enums.items())
    )
    if enum_registration:
        enum_registration += '\n'

    static_property_defs = []
    static_name_counter = 0

    def make_inner_property(type_info, owner_prop_name):
        nonlocal static_name_counter
        inner_expr = 'nullptr'
        if type_info.get('inner'):
            inner_expr = make_inner_property(type_info['inner'], owner_prop_name)

        static_name_counter += 1
        static_name = f'Z_Property_{struct_name}_{sanitize_cpp_identifier(owner_prop_name)}_Inner_{static_name_counter}'
        fake_prop = {
            'display_name': None,
            'category': None,
            'property_flags': 'EPropertyFlags::Read | EPropertyFlags::Write | EPropertyFlags::Edit',
            'min': '0.0f',
            'max': '0.0f',
            'speed': '0.1f',
            'type_info': type_info,
        }
        size_expr = f'sizeof({type_info["cpp_type"]})'
        params = make_property_params_block(fake_prop, "nullptr", "0", size_expr, inner_expr)
        static_property_defs.append(f'static const FProperty {static_name}({params});')
        return f'&{static_name}'

    runtime_prop_lines = []
    for prop in properties:
        inner_expr = 'nullptr'
        if prop['type_info'].get('inner'):
            inner_expr = make_inner_property(prop['type_info']['inner'], prop['name'])
        params = make_property_params_block(
            prop,
            cpp_string_literal(prop['name']),
            f'offsetof({qualified_name}, {prop["name"]})',
            f'sizeof((({qualified_name}*)nullptr)->{prop["name"]})',
            inner_expr)
        runtime_prop_lines.append(f'        Struct->AddProperty(FProperty({params}));')

    static_property_defs_str = '\n'.join(static_property_defs)
    if static_property_defs_str:
        static_property_defs_str += '\n\n'

    runtime_props_str = '\n'.join(runtime_prop_lines)
    include_path = make_include_path(header_path)
    struct_display_name = get_metadata_value(struct_info['metadata'], 'DisplayName', 'Display') or struct_name
    struct_category = get_metadata_value(struct_info['metadata'], 'Category')

    gen_code = f"""// AUTO-GENERATED FILE. DO NOT MODIFY.
#include \"{include_path}\"
#include \"Core/Reflection/ReflectionRegistry.h\"
#include \"Object/Class.h\"
#include \"Object/Property.h\"

{enum_metadata_str}{static_property_defs_str}\
struct Z_Construct_UScriptStruct_{struct_name} {{
    static void RegisterRuntimeEnums() {{
{enum_registration}    }}

    static void RegisterRuntimeProperties(UScriptStruct* Struct) {{
        if (!Struct) {{
            return;
        }}
{runtime_props_str}
    }}
}};

const UScriptStruct* {qualified_name}::StaticStruct()
{{
    static UScriptStruct Struct(
        \"{struct_name}\",
        sizeof({qualified_name}),
        alignof({qualified_name}),
        GetStructOps<{qualified_name}>(),
        {cpp_string_literal(struct_display_name)},
        {cpp_string_literal(struct_category)});

    static bool bRegistered = false;
    if (!bRegistered)
    {{
        bRegistered = true;
        Z_Construct_UScriptStruct_{struct_name}::RegisterRuntimeEnums();
        FReflectionRegistry::Get().RegisterStruct(&Struct);
        Z_Construct_UScriptStruct_{struct_name}::RegisterRuntimeProperties(&Struct);
    }}
    return &Struct;
}}

struct Z_AutoRegister_UScriptStruct_{struct_name} {{
    Z_AutoRegister_UScriptStruct_{struct_name}() {{
        {qualified_name}::StaticStruct();
    }}
}};

static Z_AutoRegister_UScriptStruct_{struct_name} Z_AutoRegister_UScriptStruct_{struct_name}_Var;
"""

    gen_filepath = make_generated_file_path(header_path, struct_name)
    gen_filepath.parent.mkdir(parents=True, exist_ok=True)
    with open(gen_filepath, 'w', encoding='utf-8', newline='\n') as f:
        f.write(gen_code)
    print(f'Generated: {gen_filepath.relative_to(ROOT)}')


# 헤더 파일 하나를 분석하여 UCLASS, UPROPERTY 등을 추출하고 .gen.cpp 코드를 생성합니다.
def parse_header_and_generate(header_path, enum_map, struct_map):
    try:
        raw_content = header_path.read_text(encoding='utf-8')
    except UnicodeDecodeError:
        return

    content = strip_comments(raw_content)
    if 'UCLASS' not in content and 'USTRUCT' not in content:
        return

    struct_infos = find_ustruct_declarations(content) if 'USTRUCT' in content else []
    for struct_info in struct_infos:
        properties = []
        used_enums = {}

        for prop in find_uproperties_in_struct(content, struct_info):
            cpp_type, var_name = parse_property_declaration(prop['declaration'])
            if not cpp_type or not var_name:
                print(
                    f"Reflection warning: failed to parse UPROPERTY declaration in {header_path.relative_to(ROOT)}; skipped.",
                    file=sys.stderr,
                )
                continue

            metadata = prop['metadata']
            type_info = resolve_property_type(cpp_type, enum_map, struct_map, metadata)
            if not type_info:
                warn_unknown_type(header_path, cpp_type, var_name)
                continue

            for nested_type_info in iter_type_infos(type_info):
                enum_info = nested_type_info.get('enum_info')
                if enum_info:
                    used_enums[enum_info['qualified_name']] = enum_info

            properties.append({
                'name': var_name,
                'display_name': get_metadata_value(metadata, 'DisplayName', 'Display'),
                'category': get_metadata_value(metadata, 'Category'),
                'property_flags': make_runtime_property_flags(metadata),
                'min': cpp_float_literal(get_metadata_value(metadata, 'Min', 'ClampMin', 'UIMin'), '0.0f'),
                'max': cpp_float_literal(get_metadata_value(metadata, 'Max', 'ClampMax', 'UIMax'), '0.0f'),
                'speed': cpp_float_literal(get_metadata_value(metadata, 'Speed', 'Step'), '0.1f'),
                'type_info': type_info,
            })

        generate_struct_file(header_path, struct_info, properties, used_enums)

    class_infos = find_uclass_declarations(content) if 'UCLASS' in content else []
    for class_info in class_infos:
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

            metadata = prop['metadata']
            type_info = resolve_property_type(cpp_type, enum_map, struct_map, metadata)
            if not type_info:
                warn_unknown_type(header_path, cpp_type, var_name)
                continue

            for nested_type_info in iter_type_infos(type_info):
                enum_info = nested_type_info.get('enum_info')
                if enum_info:
                    used_enums[enum_info['qualified_name']] = enum_info

            properties.append({
                'name': var_name,
                'display_name': get_metadata_value(metadata, 'DisplayName', 'Display'),
                'category': get_metadata_value(metadata, 'Category'),
                'property_flags': make_runtime_property_flags(metadata),
                'min': cpp_float_literal(get_metadata_value(metadata, 'Min', 'ClampMin', 'UIMin'), '0.0f'),
                'max': cpp_float_literal(get_metadata_value(metadata, 'Max', 'ClampMax', 'UIMax'), '0.0f'),
                'speed': cpp_float_literal(get_metadata_value(metadata, 'Speed', 'Step'), '0.1f'),
                'type_info': type_info,
            })

        generate_class_file(header_path, class_info, properties, used_enums)


if __name__ == '__main__':
    enums = collect_enums()
    structs = collect_structs()
    for header in iter_header_paths():
        parse_header_and_generate(header, enums, structs)
