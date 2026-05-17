#pragma once
#include <unordered_map>

#include "Object/Object.h"
#include "Core/ReflectionDatabase.h"

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
        if (TypeName == "int" || TypeName == "int32" || TypeName == "uint32")
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
        if (TypeName == "FVector4")
        {
            OutType = EPropertyType::Vec4;
            return true;
        }
        if (TypeName == "FColor")
        {
            OutType = EPropertyType::Color;
            return true;
        }
        if (TypeName == "FString")
        {
            OutType = EPropertyType::String;
            return true;
        }
        if (TypeName == "FName")
        {
            OutType = EPropertyType::Name;
            return true;
        }

        return false;
    }

	// 구조체 내부를 까서 프로퍼티 배열에 평탄화(Flatten)하여 집어넣는 도우미 함수
    static void AppendStructProperties(
        uint8* StructBasePtr,
        FStructInfo* StructInfo,
        TArray<FPropertyDescriptor>& OutProps,
        const FString& Prefix)
    {
        if (!StructBasePtr || !StructInfo)
            return;

        for (auto& Prop : StructInfo->Properties)
        {
            if (!Prop.bIsEditAnywhere)
                continue;

            EPropertyType UiType;
            // 1. 구조체 안의 변수가 int, float 같은 기본 타입이라면?
            if (TryConvertType(Prop.Type.c_str(), UiType))
            {
                // UI에서 보기 좋게 "구조체변수명.내부변수명" (예: MyData.Health) 형태로 이름을 지어줍니다.
                FString DisplayName = Prefix + "." + Prop.Name.ToString();
                OutProps.push_back({ GetStablePropertyName(FName(DisplayName)),
                                     UiType,
                                     StructBasePtr + Prop.Offset });
            }
            // 2. 구조체 안의 변수가 또 다른 구조체라면? (중첩 구조체)
            else
            {
                FStructInfo* NestedStruct = ReflectionDatabase::GetStruct(Prop.Type);
                if (NestedStruct)
                {
                    FString NextPrefix = Prefix + "." + Prop.Name.ToString();
                    // 재귀적으로 한 번 더 파고듭니다!
                    AppendStructProperties(StructBasePtr + Prop.Offset, NestedStruct, OutProps, NextPrefix);
                }
            }
        }
    }

    static void AppendGeneratedProperties(
        UObject* Object,
        FClassInfo* ClassInfo,
        TArray<FPropertyDescriptor>& OutProps)
    {
        AppendGeneratedPropertiesRecursive(Object, ClassInfo, OutProps);
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

        /*for (FPropertyInfo& Prop : ClassInfo->Properties)
        {
            if (!Prop.bIsEditAnywhere)
                continue;

            EPropertyType UiType;
            if (!TryConvertType(Prop.Type, UiType))
                continue;

            OutProps.push_back({ GetStablePropertyName(Prop.Name),
                                 UiType,
                                 Base + Prop.Offset });
        }*/

		for (auto& Prop : ClassInfo->Properties)
        {
            if (!Prop.bIsEditAnywhere)
                continue;

            EPropertyType UiType;

            // 1. 기본 타입(int, float 등)인 경우 기존처럼 바로 추가
            if (TryConvertType(Prop.Type, UiType))
            {
                OutProps.push_back({ GetStablePropertyName(Prop.Name),
                                     UiType,
                                     Base + Prop.Offset });
            }
            // 2. 기본 타입이 아니라면 구조체(FStructInfo)인지 확인!
            else
            {
                FStructInfo* StructInfo = ReflectionDatabase::GetStruct(Prop.Type);
                if (StructInfo)
                {
                    // 구조체 메모리 시작 주소 = 객체 베이스 주소 + 구조체 변수의 오프셋
                    uint8* StructBasePtr = Base + Prop.Offset;

                    // 구조체 내부 변수들을 긁어와서 OutProps에 평탄화해서 담아줍니다.
                    AppendStructProperties(StructBasePtr, StructInfo, OutProps, GetStablePropertyName(Prop.Name));
                }
            }
        }
    }
};
