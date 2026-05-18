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

    static const char* GetStablePropertyLabel(const FPropertyInfo& Prop)
    {
        return GetStablePropertyName(FName(Prop.DisplayName.empty() ? Prop.Name.ToString() : Prop.DisplayName));
    }

    static const char* GetStableInternalPropertyName(const FPropertyInfo& Prop)
    {
        return GetStablePropertyName(Prop.Name);
    }

    static const char* GetStablePropertyCategory(const FPropertyInfo& Prop)
    {
        return GetStablePropertyName(FName(Prop.Category.empty() ? "Default" : Prop.Category));
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

	static FString NormalizeTypeName(FString TypeName)
    {
        TypeName.erase(
            std::remove_if(TypeName.begin(), TypeName.end(), ::isspace),
            TypeName.end());
        return TypeName;
    }

	static bool TryConvertType(const FString& TypeName, EPropertyType& OutType)
    {
        FString NormalizedType = NormalizeTypeName(TypeName);

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
        if (NormalizedType == "TArray<FVector>")
        {
            OutType = EPropertyType::Vec3Array;
            return true;
        }

        return false;
    }

    static void SerializeGeneratedPropertiesLocal(
        UObject* Object,
        FClassInfo* ClassInfo,
        FArchive& Ar)
    {
        if (!Object || !ClassInfo)
            return;

        uint8* Base = reinterpret_cast<uint8*>(Object);

        for (const FPropertyInfo& Prop : ClassInfo->Properties)
        {
            if (!Prop.ShouldSerialize())
                continue;

            SerializePropertyValue(Ar, Base + Prop.Offset, Prop.Name.ToString(), Prop.Type);
        }
    }

    static void SerializeGeneratedPropertiesRecursive(
        UObject* Object,
        FClassInfo* ClassInfo,
        FArchive& Ar)
    {
        if (!Object || !ClassInfo)
            return;

        SerializeGeneratedPropertiesRecursive(Object, ClassInfo->ParentClass, Ar);
        SerializeGeneratedPropertiesLocal(Object, ClassInfo, Ar);
    }

    static void SerializePropertyValue(
        FArchive& Ar,
        void* ValuePtr,
        const FString& Key,
        const FString& TypeName)
    {
        if (!ValuePtr)
            return;

        if (Ar.IsLoading() && !Ar.HasKey(Key))
            return;

        FString NormalizedType = NormalizeTypeName(TypeName);

        if (TypeName == "bool")
        {
            Ar << Key.c_str() << *reinterpret_cast<bool*>(ValuePtr);
        }
        else if (TypeName == "int" || TypeName == "int32")
        {
            Ar << Key.c_str() << *reinterpret_cast<int32*>(ValuePtr);
        }
        else if (TypeName == "uint32")
        {
            Ar << Key.c_str() << *reinterpret_cast<uint32*>(ValuePtr);
        }
        else if (TypeName == "float")
        {
            Ar << Key.c_str() << *reinterpret_cast<float*>(ValuePtr);
        }
        else if (TypeName == "FVector")
        {
            Ar << Key.c_str() << *reinterpret_cast<FVector*>(ValuePtr);
        }
        else if (TypeName == "FVector4")
        {
            Ar << Key.c_str() << *reinterpret_cast<FVector4*>(ValuePtr);
        }
        else if (TypeName == "FColor")
        {
            Ar << Key.c_str() << *reinterpret_cast<FColor*>(ValuePtr);
        }
        else if (TypeName == "FString")
        {
            Ar << Key.c_str() << *reinterpret_cast<FString*>(ValuePtr);
        }
        else if (TypeName == "FName")
        {
            Ar << Key.c_str() << *reinterpret_cast<FName*>(ValuePtr);
        }
        else if (NormalizedType == "TArray<FVector>")
        {
            Ar << Key.c_str() << *reinterpret_cast<TArray<FVector>*>(ValuePtr);
        }
        else if (ReflectionDatabase::GetEnum(TypeName))
        {
            uint32 EnumValue = *reinterpret_cast<uint32*>(ValuePtr);
            Ar << Key.c_str() << EnumValue;
            if (Ar.IsLoading())
            {
                *reinterpret_cast<uint32*>(ValuePtr) = EnumValue;
            }
        }
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

		if (StructInfo->ParentStruct)
        {
            AppendStructProperties(StructBasePtr, StructInfo->ParentStruct, OutProps, Prefix);
        }

        for (auto& Prop : StructInfo->Properties)
        {
            if (!Prop.IsEditorVisible())
                continue;

            EPropertyType UiType;
            FString PropertyLabel = Prop.DisplayName.empty() ? Prop.Name.ToString() : Prop.DisplayName;
            FString DisplayName = Prefix + "." + PropertyLabel;

            // 1. 구조체 안의 변수가 int, float 같은 기본 타입이라면?
            if (TryConvertType(Prop.Type.c_str(), UiType))
            {
                FPropertyDescriptor Desc;
                Desc.Name = GetStablePropertyName(FName(DisplayName));
                Desc.Type = UiType;
                Desc.ValuePtr = StructBasePtr + Prop.Offset;
                Desc.InternalName = GetStablePropertyName(Prop.Name);
                Desc.Category = GetStablePropertyCategory(Prop);
                OutProps.push_back(Desc);
            }
            // ★ 2. 열거형(Enum)인지 검사
            else if (FEnumInfo* EnumInfo = ReflectionDatabase::GetEnum(Prop.Type))
            {
                FPropertyDescriptor Desc;
                Desc.Name = GetStablePropertyName(FName(DisplayName));
                Desc.Type = EPropertyType::Enum;
                Desc.ValuePtr = StructBasePtr + Prop.Offset;
                Desc.InternalName = GetStablePropertyName(Prop.Name);
                Desc.Category = GetStablePropertyCategory(Prop);

                if (!EnumInfo->CachedNames.empty())
                {
                    Desc.EnumNames = EnumInfo->CachedNames.data();
                    Desc.EnumCount = static_cast<uint32>(EnumInfo->CachedNames.size());
                }

                OutProps.push_back(Desc);
            }
            // 3. 구조체 안의 변수가 또 다른 구조체라면? (중첩 구조체)
            else
            {
                FStructInfo* NestedStruct = ReflectionDatabase::GetStruct(Prop.Type);
                if (NestedStruct)
                {
                    FString NextPrefix = Prefix + "." + PropertyLabel;
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
            if (!Prop.IsEditorVisible())
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
            if (!Prop.IsEditorVisible())
                continue;

            EPropertyType UiType;

            // 1. 기본 타입(int, float 등)인 경우 기존처럼 바로 추가
            if (TryConvertType(Prop.Type, UiType))
            {
                FPropertyDescriptor Desc;
                Desc.Name = GetStablePropertyLabel(Prop);
                Desc.Type = UiType;
                Desc.ValuePtr = Base + Prop.Offset;
                Desc.InternalName = GetStableInternalPropertyName(Prop);
                Desc.Category = GetStablePropertyCategory(Prop);
                OutProps.push_back(Desc);
            }
            // 2. 열거형(Enum)인지 검사
            else if (FEnumInfo* EnumInfo = ReflectionDatabase::GetEnum(Prop.Type))
            {
                // 구조체나 클래스 모두 동일하게 적용
                FPropertyDescriptor Desc;
                Desc.Name = GetStablePropertyLabel(Prop);
                Desc.Type = EPropertyType::Enum;
                Desc.ValuePtr = Base + Prop.Offset;
                Desc.InternalName = GetStableInternalPropertyName(Prop);
                Desc.Category = GetStablePropertyCategory(Prop);

                if (!EnumInfo->CachedNames.empty())
                {
                    Desc.EnumNames = EnumInfo->CachedNames.data();
                    Desc.EnumCount = static_cast<uint32>(EnumInfo->CachedNames.size());
                }

                OutProps.push_back(Desc);
            }
            // 3. 기본 타입이 아니라면 구조체(FStructInfo)인지 확인!
            else
            {
                FStructInfo* StructInfo = ReflectionDatabase::GetStruct(Prop.Type);
                if (StructInfo)
                {
                    // 구조체 메모리 시작 주소 = 객체 베이스 주소 + 구조체 변수의 오프셋
                    uint8* StructBasePtr = Base + Prop.Offset;

                    // 구조체 내부 변수들을 긁어와서 OutProps에 평탄화해서 담아줍니다.
                    AppendStructProperties(StructBasePtr, StructInfo, OutProps, GetStablePropertyLabel(Prop));
                }
            }
        }
    }
};
