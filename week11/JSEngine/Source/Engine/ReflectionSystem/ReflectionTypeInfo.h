#pragma once

// ============================================================
//  ReflectionTypeInfo.h  (통합 umbrella 헤더)
//  기존 코드와의 호환성 유지를 위해 분리된 헤더를 한 번에 include.
//
//  새 코드는 필요한 헤더만 개별 include 하는 것을 권장합니다.
//
//  include 순서 (의존성 낮은 → 높은 순):
//    1. ReflectionFwd.h          — 전방 선언
//    2. ReflectionPropertyFlags.h — EPropertyFlags, FPropertyInfo
//    3. ReflectionEnumInfo.h      — FEnumValueInfo, FEnumInfo
//    4. ReflectionStructInfo.h    — EStructEditorWidget, FStructInfo
//    5. ReflectionFunctionInfo.h  — FParameterInfo, FFunctionInfo
//    6. ReflectionClassInfo.h     — FClassInfo
// ============================================================

#include "ReflectionFwd.h"
#include "ReflectionPropertyFlags.h"
#include "ReflectionEnumInfo.h"
#include "ReflectionStructInfo.h"
#include "ReflectionFunctionInfo.h"
#include "ReflectionClassInfo.h"
