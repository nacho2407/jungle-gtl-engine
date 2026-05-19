#pragma once

// ============================================================
//  ReflectionTypeInfo.h
//  클래스·구조체·열거형의 메타데이터 구조체를 정의합니다.
//
//  ★ FProperty 의 완전한 정의는 필요 없으므로
//     ReflectionFwd.h 의 전방선언만 사용합니다.
//     → ReflectedProperty.h 를 include 하지 않아도 됩니다.
// ============================================================

#include "Core/CoreTypes.h"
#include "Core/Containers/String.h"
#include "Core/Containers/Array.h"
#include "Object/FName.h"
#include "ReflectionFwd.h"

// ------------------------------------------------------------------
// 매크로 껍데기 (C++ 컴파일러는 무시, C# 파서만 읽음)
// ------------------------------------------------------------------
#define UCLASS()
#define UPROPERTY(...)
#define GENERATED_BODY()
#define USTRUCT(...)
#define UENUM(...)

// ------------------------------------------------------------------
// 프로퍼티 플래그
// ------------------------------------------------------------------
enum EPropertyFlags : uint32
{
    PF_None = 0,
    PF_EditAnywhere = 1 << 0,
    PF_VisibleAnywhere = 1 << 1,
    PF_HideInEditor = 1 << 2,
    PF_Transient = 1 << 3,
    PF_SaveGame = 1 << 4,
};

// ------------------------------------------------------------------
// FPropertyInfo  (레거시 / 파서 생성 코드용)
// ------------------------------------------------------------------
struct FPropertyInfo
{
    FName Name;
    FString Type;
    size_t Offset = 0;
    uint32 Flags = PF_None;
    FString Category = "Default";
    FString DisplayName;

    bool HasAnyFlags(uint32 InFlags) const { return (Flags & InFlags) != 0; }
    bool HasAllFlags(uint32 InFlags) const { return (Flags & InFlags) == InFlags; }

    bool IsEditorVisible() const
    {
        return !HasAnyFlags(PF_HideInEditor) &&
               HasAnyFlags(PF_EditAnywhere | PF_VisibleAnywhere);
    }

    bool IsEditorEditable() const
    {
        return !HasAnyFlags(PF_HideInEditor) &&
               HasAnyFlags(PF_EditAnywhere);
    }

    bool ShouldSerialize() const
    {
        return !HasAnyFlags(PF_Transient) &&
               HasAnyFlags(PF_SaveGame | PF_EditAnywhere);
    }
};

// ------------------------------------------------------------------
// FClassInfo
// ------------------------------------------------------------------
struct FClassInfo
{
    FName ClassName;
    FName ParentClassName;
    FClassInfo* ParentClass = nullptr;

    TArray<FPropertyInfo> Properties;
    TArray<FProperty*> ReflectedProperties; // FProperty* → 전방선언으로 충분
    TArray<size_t> GcPointerOffsets;
};

// ------------------------------------------------------------------
// FStructInfo
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
struct FStructInfo
{
    FName StructName;
    size_t Size = 0;

    FName ParentStructName;
    FStructInfo* ParentStruct = nullptr;

    TArray<FPropertyInfo> Properties;
    TArray<FProperty*> ReflectedProperties;
    TArray<size_t> GcPointerOffsets;

    EStructEditorWidget EditorWidget = EStructEditorWidget::Default;
};

// ------------------------------------------------------------------
// FEnumInfo
// ------------------------------------------------------------------
struct FEnumValueInfo
{
    FName Name;
    int64 Value = 0;
};

struct FEnumInfo
{
    FName EnumName;
    TArray<FEnumValueInfo> Values;
    TArray<const char*> CachedNames;
};
