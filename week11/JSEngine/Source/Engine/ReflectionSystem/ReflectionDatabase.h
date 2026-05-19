#pragma once

// ============================================================
//  ReflectionDatabase.h
//
//  include 순서 (단방향, 순환 없음):
//    ReflectionFwd.h       (전방선언)
//    ReflectionTypeInfo.h  (FClassInfo / FStructInfo / FEnumInfo)
//    ReflectedProperty.h   (FProperty 계층 — 위 두 헤더를 이미 포함)
//    ReflectionDatabase.h  ← 여기서 DB 클래스 정의
// ============================================================

#include "Core/CoreMinimal.h"
#include "Object/Object.h"
#include "ReflectedProperty.h"   // FProperty + FClassInfo + FStructInfo + FEnumInfo 전부 확보

class ReflectionDatabase
{
private:
    inline static TMap<FString, FClassInfo*>  ClassMap;
    inline static TMap<FString, FStructInfo*> StructMap;
    inline static TMap<FString, FEnumInfo*>   EnumMap;

public:
    static void ResolveStructEditorWidget(FStructInfo* StructInfo)
    {
        if (!StructInfo)
            return;

        FString Name = StructInfo->StructName.ToString();

        if (Name == "FVector2")
        {
            StructInfo->EditorWidget = EStructEditorWidget::Vector2;
        }
        else if (Name == "FVector")
        {
            StructInfo->EditorWidget = EStructEditorWidget::Vector3;
        }
        else if (Name == "FVector4")
        {
            StructInfo->EditorWidget = EStructEditorWidget::Vector4;
        }
        else if (Name == "FRotator")
        {
            StructInfo->EditorWidget = EStructEditorWidget::Rotator;
        }
        else if (Name == "FColor")
        {
            StructInfo->EditorWidget = EStructEditorWidget::Color;
        }
        else if (Name == "FTransform")
        {
            StructInfo->EditorWidget = EStructEditorWidget::Transform;
        }
        else
        {
            StructInfo->EditorWidget = EStructEditorWidget::Default;
        }
    }
    // ---------------------------------------------------------------
    // Class
    // ---------------------------------------------------------------
    static void AddClass(const FString& ClassName, FClassInfo* ClassInfo)
    {
        ClassMap[ClassName] = ClassInfo;

        // 이미 등록된 클래스라면 바로 부모 연결 시도
        if (ClassInfo && ClassInfo->ParentClassName.IsValid())
            ClassInfo->ParentClass = GetClass(ClassInfo->ParentClassName.ToString());

        // 나중에 등록된 클래스들 중 부모가 아직 안 붙은 것도 연결
        for (auto& Pair : ClassMap)
        {
            FClassInfo* Other = Pair.second;
            if (!Other || Other->ParentClass)
                continue;
            if (Other->ParentClassName == FName(ClassName))
                Other->ParentClass = ClassInfo;
        }
    }

    static FClassInfo* GetClass(const FString& ClassName)
    {
        auto It = ClassMap.find(ClassName);
        return It != ClassMap.end() ? It->second : nullptr;
    }

    static const TMap<FString, FClassInfo*>& GetAllClasses()
    {
        return ClassMap;
    }

    // ---------------------------------------------------------------
    // Struct
    // ---------------------------------------------------------------
    static void AddStruct(const FString& StructName, FStructInfo* StructInfo)
    {
        StructMap[StructName] = StructInfo;
    }

    static FStructInfo* GetStruct(const FString& StructName)
    {
        auto It = StructMap.find(StructName);
        return It != StructMap.end() ? It->second : nullptr;
    }

    static const TMap<FString, FStructInfo*>& GetAllStructs()
    {
        return StructMap;
    }

    // ---------------------------------------------------------------
    // Enum
    // ---------------------------------------------------------------
    static void AddEnum(const FString& EnumName, FEnumInfo* EnumInfo)
    {
        EnumMap[EnumName] = EnumInfo;
    }

    static FEnumInfo* GetEnum(const FString& EnumName)
    {
        auto It = EnumMap.find(EnumName);
        return It != EnumMap.end() ? It->second : nullptr;
    }

    // ---------------------------------------------------------------
    // ResolveDependencies
    // main() 시작 직후 단 한 번 호출해서 내부 포인터를 전부 연결합니다.
    // ---------------------------------------------------------------
    static void ResolveDependencies()
    {
        // 1. 클래스 족보 연결
        for (auto& Pair : ClassMap)
        {
            FClassInfo* Info = Pair.second;
            if (Info && Info->ParentClassName.IsValid())
                Info->ParentClass = GetClass(Info->ParentClassName.ToString());

            for (FProperty* Prop : Info->ReflectedProperties)
            {
                ResolvePropertyPointers(Prop);
            }
        }

        // 2. 구조체 족보 연결
        for (auto& Pair : StructMap)
        {
            FStructInfo* Info = Pair.second;
            if (Info && Info->ParentStructName.IsValid())
                Info->ParentStruct = GetStruct(Info->ParentStructName.ToString());
            
			ResolveStructEditorWidget(Info);

            for (FProperty* Prop : Info->ReflectedProperties)
            {
                if (Prop)
                {
                    ResolvePropertyPointers(Prop);
                }
            }
        }

        // 3. 클래스 소속 프로퍼티들의 내부 포인터 연결
        for (auto& Pair : ClassMap)
        {
            if (!Pair.second) continue;
            for (FProperty* Prop : Pair.second->ReflectedProperties)
            {
                if (!Prop) continue;
                ResolvePropertyPointers(Prop);
            }
        }

        // 4. 구조체 소속 프로퍼티들의 내부 포인터 연결
        for (auto& Pair : StructMap)
        {
            if (!Pair.second) continue;
            for (FProperty* Prop : Pair.second->ReflectedProperties)
            {
                if (!Prop) continue;
                ResolvePropertyPointers(Prop);
            }
        }
    }

private:
    // Property 하나의 내부 포인터를 확정합니다.
    static void ResolvePropertyPointers(FProperty* Prop)
    {
        switch (Prop->GetKind())
        {
        case EReflectedPropertyKind::Object:
        {
            // "AActor*" → '*' 제거 후 클래스 검색
            FString BaseType = Prop->CPPType;
            BaseType.erase(std::remove(BaseType.begin(), BaseType.end(), '*'), BaseType.end());
            static_cast<FObjectProperty*>(Prop)->PropertyClass = GetClass(BaseType);
            break;
        }
        case EReflectedPropertyKind::Struct:
            static_cast<FStructProperty*>(Prop)->StructInfo = GetStruct(Prop->CPPType);
            break;

        case EReflectedPropertyKind::Enum:
            static_cast<FEnumProperty*>(Prop)->EnumInfo = GetEnum(Prop->CPPType);
            break;
        case EReflectedPropertyKind::Array:
        {
            FArrayProperty* ArrayProp = static_cast<FArrayProperty*>(Prop);
            if (ArrayProp->Inner)
                ResolvePropertyPointers(ArrayProp->Inner);
            break;
        }
        default:
            break;
        }
    }
};
