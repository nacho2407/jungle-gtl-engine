#pragma once

// ============================================================
//  ReflectionPropertyFlags.h
//  프로퍼티 플래그(EPropertyFlags) 와 FPropertyInfo 정의.
//
//  의존성 : CoreTypes, String, FName
//  역방향 의존 없음 → 다른 리플렉션 헤더가 자유롭게 include 가능.
// ============================================================

#include "Core/CoreTypes.h"
#include "Core/Containers/String.h"
#include "Object/FName.h"

// ------------------------------------------------------------------
// 매크로 껍데기 (C# 파서 전용, C++ 컴파일러는 무시)
// ------------------------------------------------------------------
#define UCLASS()
#define UPROPERTY(...)
#define GENERATED_BODY()
#define USTRUCT(...)
#define UENUM(...)
#define UFUNCTION(...)

// ------------------------------------------------------------------
// 프로퍼티 플래그
// ------------------------------------------------------------------
enum EPropertyFlags : uint32
{
    PF_None          = 0,
    PF_EditAnywhere  = 1 << 0,
    PF_VisibleAnywhere = 1 << 1,
    PF_HideInEditor  = 1 << 2,
    PF_Transient     = 1 << 3,
    PF_SaveGame      = 1 << 4,
};

// ------------------------------------------------------------------
// FPropertyInfo  (레거시 / 파서 생성 코드용)
// ------------------------------------------------------------------
struct FPropertyInfo
{
    FName    Name;
    FString  Type;
    size_t   Offset      = 0;
    uint32   Flags       = PF_None;
    FString  Category    = "Default";
    FString  DisplayName;

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
