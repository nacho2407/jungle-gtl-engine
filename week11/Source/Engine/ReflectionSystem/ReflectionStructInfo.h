#pragma once

// ============================================================
//  ReflectionStructInfo.h
//  구조체 메타데이터 (EStructEditorWidget, FStructInfo) 정의.
//
//  의존성 (직접)  : ReflectionPropertyFlags.h, ReflectionFwd.h
//  전방선언 사용  : FProperty* (정의 불필요)
//  역방향 의존 없음 → FClassInfo/FFunctionInfo 가 자유롭게 include 가능.
// ============================================================

#include "Core/CoreTypes.h"
#include "Core/Containers/Array.h"
#include "Object/FName.h"
#include "ReflectionFwd.h"
#include "ReflectionPropertyFlags.h"

// ------------------------------------------------------------------
// 구조체 전용 에디터 위젯 힌트
// ------------------------------------------------------------------
enum class EStructEditorWidget : uint8
{
    Default,
    Vector2,
    Vector3,
    Vector4,
    Rotator,
    Color,
    Transform
};

// ------------------------------------------------------------------
// FStructInfo
// ------------------------------------------------------------------
struct FStructInfo
{
    FName    StructName;
    size_t   Size = 0;

    FName        ParentStructName;
    FStructInfo* ParentStruct = nullptr;   // 자기 참조 → 포인터로 OK

    TArray<FPropertyInfo>  Properties;
    TArray<FProperty*>     ReflectedProperties; // 전방선언으로 충분
    TArray<size_t>         GcPointerOffsets;

    EStructEditorWidget EditorWidget = EStructEditorWidget::Default;
};
