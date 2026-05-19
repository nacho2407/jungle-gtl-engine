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

        if (!ClassInfo->ReflectedProperties.empty())
        {
            for (FProperty* Prop : ClassInfo->ReflectedProperties)
            {
                Prop->SerializeInContainer(Ar, Object);
            }
            return;
        }

        // fallback: old FPropertyInfo path
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

        for (FProperty* Prop : StructInfo->ReflectedProperties)
        {
            if (!Prop || !Prop->IsEditorVisible())
                continue;

            FString PropertyLabel = Prop->DisplayName.empty()
                                        ? Prop->Name.ToString()
                                        : Prop->DisplayName;

            FString DisplayName = Prefix + "." + PropertyLabel;

            FPropertyDescriptor Desc;
            Desc.Name = GetStablePropertyName(FName(DisplayName));
            Desc.ValuePtr = Prop->ContainerPtrToValuePtr(StructBasePtr);
            Desc.InternalName = GetStablePropertyName(Prop->Name);
            Desc.Category = Prop->Category.empty()
                                ? "Default"
                                : GetStablePropertyName(FName(Prop->Category));

            if (FNumericProperty* NumericProp = dynamic_cast<FNumericProperty*>(Prop))
            {
                Desc.Type = NumericProp->IsFloatingPoint()
                                ? EPropertyType::Float
                                : EPropertyType::Int;

                OutProps.push_back(Desc);
            }
            else if (dynamic_cast<FBoolProperty*>(Prop))
            {
                Desc.Type = EPropertyType::Bool;
                OutProps.push_back(Desc);
            }
            else if (dynamic_cast<FStrProperty*>(Prop))
            {
                Desc.Type = EPropertyType::String;
                OutProps.push_back(Desc);
            }
            else if (dynamic_cast<FNameProperty*>(Prop))
            {
                Desc.Type = EPropertyType::Name;
                OutProps.push_back(Desc);
            }
            else if (FEnumProperty* EnumProp = dynamic_cast<FEnumProperty*>(Prop))
            {
                Desc.Type = EPropertyType::Enum;

                if (EnumProp->EnumInfo && !EnumProp->EnumInfo->CachedNames.empty())
                {
                    Desc.EnumNames = EnumProp->EnumInfo->CachedNames.data();
                    Desc.EnumCount = static_cast<uint32>(EnumProp->EnumInfo->CachedNames.size());
                }

                OutProps.push_back(Desc);
            }
            else if (FStructProperty* NestedStructProp = dynamic_cast<FStructProperty*>(Prop))
            {
                FStructInfo* NestedStructInfo = NestedStructProp->StructInfo;

                if (!NestedStructInfo)
                {
                    NestedStructInfo = ReflectionDatabase::GetStruct(NestedStructProp->CPPType);
                }

                if (NestedStructInfo)
                {
                    uint8* NestedStructBase =
                        static_cast<uint8*>(NestedStructProp->ContainerPtrToValuePtr(StructBasePtr));

                    AppendStructProperties(
                        NestedStructBase,
                        NestedStructInfo,
                        OutProps,
                        DisplayName);
                }
            }
            else if (FArrayProperty* ArrayProp = dynamic_cast<FArrayProperty*>(Prop))
            {
                if (ArrayProp->Inner && ArrayProp->Inner->CPPType == "FVector")
                {
                    Desc.Type = EPropertyType::Vec3Array;
                    OutProps.push_back(Desc);
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

        for (auto& Prop : ClassInfo->ReflectedProperties)
        {
            if (!Prop->IsEditorVisible())
                continue;

            FPropertyDescriptor Desc;
            Desc.Name = Prop->GetLabel();
            Desc.ValuePtr = Prop->ContainerPtrToValuePtr(Object);
            Desc.InternalName = GetStablePropertyName(Prop->Name);
            Desc.Category = Prop->Category.c_str();
            if (FNumericProperty* NumericProp = dynamic_cast<FNumericProperty*>(Prop))
            {
                if (NumericProp->IsFloatingPoint())
                {
                    Desc.Type = EPropertyType::Float;
                }
                else
                {
                    Desc.Type = EPropertyType::Int;
                }

                OutProps.push_back(Desc);
            }
            else if (dynamic_cast<FBoolProperty*>(Prop))
            {
                Desc.Type = EPropertyType::Bool;
                OutProps.push_back(Desc);
            }
            else if (dynamic_cast<FStrProperty*>(Prop))
            {
                Desc.Type = EPropertyType::String;
                OutProps.push_back(Desc);
            }
            else if (dynamic_cast<FNameProperty*>(Prop))
            {
                Desc.Type = EPropertyType::Name;
                OutProps.push_back(Desc);
            }
            else if (FEnumProperty* EnumProp = dynamic_cast<FEnumProperty*>(Prop))
            {
                Desc.Type = EPropertyType::Enum;

                if (EnumProp->EnumInfo && !EnumProp->EnumInfo->CachedNames.empty())
                {
                    Desc.EnumNames = EnumProp->EnumInfo->CachedNames.data();
                    Desc.EnumCount = static_cast<uint32>(EnumProp->EnumInfo->CachedNames.size());
                }

                OutProps.push_back(Desc);
            }
            else if (FStructProperty* StructProp = dynamic_cast<FStructProperty*>(Prop))
            {
                FStructInfo* StructInfo = StructProp->StructInfo
                                              ? StructProp->StructInfo
                                              : ReflectionDatabase::GetStruct(StructProp->CPPType);

                if (!StructInfo)
                    continue;

                if (StructInfo->EditorWidget == EStructEditorWidget::Vector3)
                {
                    Desc.Type = EPropertyType::Vec3;
                    OutProps.push_back(Desc);
                }
                else if (StructInfo->EditorWidget == EStructEditorWidget::Vector4)
                {
                    Desc.Type = EPropertyType::Vec4;
                    OutProps.push_back(Desc);
                }
                else if (StructInfo->EditorWidget == EStructEditorWidget::Color)
                {
                    Desc.Type = EPropertyType::Color;
                    OutProps.push_back(Desc);
                }
                else
                {
                    uint8* StructBasePtr =
                        static_cast<uint8*>(Prop->ContainerPtrToValuePtr(Object));

                    AppendStructProperties(
                        StructBasePtr,
                        StructInfo,
                        OutProps,
                        Prop->GetLabel());
                }
            }
            else if (FArrayProperty* ArrayProp = dynamic_cast<FArrayProperty*>(Prop))
            {
                // 임시 bridge: 기존 UI가 Vec3Array만 지원하니까 그 경우만 연결
                if (ArrayProp->Inner && ArrayProp->Inner->CPPType == "FVector")
                {
                    Desc.Type = EPropertyType::Vec3Array;
                    OutProps.push_back(Desc);
                }
            }
            else if (FObjectProperty* ObjectProp = dynamic_cast<FObjectProperty*>(Prop))
            {
                // 지금 기존 UI에서 UObject* 일반 picker가 없다면 일단 skip.
                // 나중에 Asset/Object picker 생기면 여기서 EPropertyType::Object 같은 걸로 연결.
                (void)ObjectProp;
            }
            else if (FSoftObjectProperty* SoftObjectProp = dynamic_cast<FSoftObjectProperty*>(Prop))
            {
                // 에셋 드래그앤드롭 UI 붙일 자리.
                // 지금은 기존 EPropertyType::String 또는 Material로 임시 연결 가능.
                (void)SoftObjectProp;
            }
        }
    }
};
