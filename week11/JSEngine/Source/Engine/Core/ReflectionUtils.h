#pragma once
#include <unordered_map>

#include "Object/Object.h"

// 아무 객체나 오프셋만 알면 값을 읽고(Get), 쓸(Set) 수 있는 템플릿 함수

class ReflectionUtils
{
public:
    static const char* GetStablePropertyName(const FName& Name)
    {
        static std::unordered_map<FString, FString> NameCache;

        FString NameString = Name.ToString();
        auto It = NameCache.find(NameString);
        if (It == NameCache.end())
        {
            It = NameCache.emplace(NameString, NameString).first;
        }

        return It->second.c_str();
    }

    // 1. 값 쓰기 (Set)
    template <typename T>
    static void SetValue(UObject* TargetObject, size_t Offset, T NewValue)
    {
        if (!TargetObject)
            return;

        // 객체의 시작 주소 가져오기
        uint8* BaseAddress = reinterpret_cast<uint8*>(TargetObject);

        // 오프셋만큼 더해서 실제 변수 위치 찾기
        T* TargetPtr = reinterpret_cast<T*>(BaseAddress + Offset);

        // 값 덮어씌우기
        *TargetPtr = NewValue;
    }

    // 2. 값 읽기 (Get)
    template <typename T>
    static T GetValue(UObject* TargetObject, size_t Offset)
    {
        if (!TargetObject)
            return T(); // 기본값 반환

        uint8* BaseAddress = reinterpret_cast<uint8*>(TargetObject);
        T* TargetPtr = reinterpret_cast<T*>(BaseAddress + Offset);

        return *TargetPtr;
    }

	static bool TryConvertType(const FString& TypeName, EPropertyType& OutType)
    {
        if (TypeName == "bool")
        {
            OutType = EPropertyType::Bool;
            return true;
        }
        if (TypeName == "int" || TypeName == "int32")
        {
            OutType = EPropertyType::Int;
            return true;
        }
        if (TypeName == "float")
        {
            OutType = EPropertyType::Float;
            return true;
        }
        if (TypeName == "FVector")
        {
            OutType = EPropertyType::Vec3;
            return true;
        }
        if (TypeName == "FString")
        {
            OutType = EPropertyType::String;
            return true;
        }

        return false;
    }

    static void AppendGeneratedProperties(
        UObject* Object,
        FClassInfo& ClassInfo,
        TArray<FPropertyDescriptor>& OutProps)
    {
        if (!Object)
            return;

        uint8* Base = reinterpret_cast<uint8*>(Object);

        for (FPropertyInfo& Prop : ClassInfo.Properties)
        {
            if (!Prop.bIsEditAnywhere)
                continue;

            EPropertyType UiType;
            if (!TryConvertType(Prop.Type, UiType))
                continue;

            OutProps.push_back({ GetStablePropertyName(Prop.Name),
                                 UiType,
                                 Base + Prop.Offset });
        }
    }

	static void AppendGeneratedPropertiesRecursive(
        UObject* Object,
        FClassInfo* ClassInfo,
        TArray<FPropertyDescriptor>& OutProps)
    {
        if (!Object || !ClassInfo)
            return;

        AppendGeneratedPropertiesRecursive(Object, ClassInfo->ParentClass, OutProps);

        uint8* Base = reinterpret_cast<uint8*>(Object);

        for (FPropertyInfo& Prop : ClassInfo->Properties)
        {
            if (!Prop.bIsEditAnywhere)
                continue;

            EPropertyType UiType;
            if (!TryConvertType(Prop.Type, UiType))
                continue;

            OutProps.push_back({ GetStablePropertyName(Prop.Name),
                                 UiType,
                                 Base + Prop.Offset });
        }
    }
};
