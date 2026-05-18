#pragma once
#include "CoreMinimal.h"
#include "Object/FName.h"

// 1. 매크로 정의 (C++ 컴파일러는 무시하고, C# 파서만 읽을 껍데기)
#define UCLASS()
#define UPROPERTY(...)
#define GENERATED_BODY()
#define USTRUCT(...)
#define UENUM(...)

// 2. 리플렉션 & 에디터용 메타데이터
enum EPropertyFlags : uint32
{
    PF_None            = 0,
    PF_EditAnywhere    = 1 << 0,
    PF_VisibleAnywhere = 1 << 1,
    PF_HideInEditor    = 1 << 2,
    PF_Transient       = 1 << 3,
    PF_SaveGame        = 1 << 4,
};

struct FPropertyInfo
{
    FName Name;
    FString Type;
    size_t Offset;
    uint32 Flags = PF_None;
    FString Category = "Default";
    FString DisplayName;

    bool HasAnyFlags(uint32 InFlags) const
    {
        return (Flags & InFlags) != 0;
    }

    bool HasAllFlags(uint32 InFlags) const
    {
        return (Flags & InFlags) == InFlags;
    }

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

// 3. 클래스 전체 정보 (+ GC 연동)
struct FClassInfo
{
    FName ClassName;
    FName ParentClassName;
    FClassInfo* ParentClass = nullptr;

    TArray<FPropertyInfo> Properties;
    TArray<size_t> GcPointerOffsets;
};

struct FStructInfo
{
    FName StructName;
    size_t Size; // 구조체의 크기 (UI에서 복사/할당할 때 유용함)

    // 부모 구조체 정보 기억용
    FName ParentStructName;
    FStructInfo* ParentStruct = nullptr;

    TArray<FPropertyInfo> Properties;
    TArray<size_t> GcPointerOffsets;
};

// enum
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
