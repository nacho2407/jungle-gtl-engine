// 오브젝트 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Core/CoreTypes.h"

class UObject;

// EClassFlags는 오브젝트 처리에서 사용할 선택지를 정의합니다.
enum EClassFlags : uint32
{
    CF_None = 0,
    CF_Actor = 1 << 0,
    CF_Component = 1 << 1,
    CF_Camera = 1 << 2,
    CF_Abstract = 1 << 3,
};

enum class EEditorComponentCategory : uint8
{
    Basic,
    Lights,
    Shapes,
    Movement,
    Visual,
    Scripts,
    Hidden,
};

// UClass는 오브젝트 영역의 핵심 동작을 담당합니다.
class UClass
{
public:
    UClass(const char* InName, UClass* InSuperClass, size_t InSize, uint32 InFlags = CF_None, EEditorComponentCategory InEditorComponentCategory = EEditorComponentCategory::Basic)
        : Name(InName), SuperClass(InSuperClass), Size(InSize), ClassFlags(InFlags), EditorComponentCategory(InEditorComponentCategory)
    {
    }

    const char* GetName() const { return Name; }
    UClass* GetSuperClass() const { return SuperClass; }
    size_t GetSize() const { return Size; }
    uint32 GetClassFlags() const { return ClassFlags; }
    EEditorComponentCategory GetEditorComponentCategory() const { return EditorComponentCategory; }

    bool IsA(const UClass* Other) const
    {
        for (const UClass* C = this; C; C = C->SuperClass)
        {
            if (C == Other)
                return true;
        }
        return false;
    }

    bool HasAnyClassFlags(uint32 Flags) const
    {
        return (ClassFlags & Flags) != 0;
    }

    // --- Global class registry ---
    static TArray<UClass*>& GetAllClasses()
    {
        static TArray<UClass*> Registry;
        return Registry;
    }

private:
    const char* Name = nullptr;
    UClass* SuperClass = nullptr;
    size_t Size = 0;
    uint32 ClassFlags = CF_None;
    EEditorComponentCategory EditorComponentCategory = EEditorComponentCategory::Basic;
};

// static initializer 에서 UClass를 전역 레지스트리에 등록
struct FClassRegistrar
{
    FClassRegistrar(UClass* InClass)
    {
        UClass::GetAllClasses().push_back(InClass);
    }
};
