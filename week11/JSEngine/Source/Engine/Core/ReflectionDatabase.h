#pragma once
#include "CoreMinimal.h"
#include "Object/Object.h"

// FClassInfo가 정의된 헤더를 인클루드하세요 (예: Object.h 또는 ClassInfo.h)
struct FClassInfo;

class ReflectionDatabase
{
private:
    // "클래스 이름" -> "클래스 정보 포인터"를 매핑해두는 거대한 사전
    inline static TMap<FString, FClassInfo*> ClassMap;

    inline static TMap<FString, FStructInfo*> StructMap;

	inline static TMap<FString, FEnumInfo*> EnumMap;


public:
    // 1. 파서가 생성한 코드가 이 함수를 통해 정보를 제출합니다.
    static void AddClass(const FString& ClassName, FClassInfo* ClassInfo)
    {
        ClassMap[ClassName] = ClassInfo;

        if (ClassInfo && ClassInfo->ParentClassName.IsValid())
        {
            ClassInfo->ParentClass = GetClass(ClassInfo->ParentClassName.ToString());
        }

        for (auto& Pair : ClassMap)
        {
            FClassInfo* OtherClass = Pair.second;
            if (!OtherClass || OtherClass->ParentClass)
            {
                continue;
            }

            if (OtherClass->ParentClassName == FName(ClassName))
            {
                OtherClass->ParentClass = ClassInfo;
            }
        }
    }

    // 2. 나중에 에디터나 GC가 클래스 이름으로 정보를 찾을 때 사용합니다.
    static FClassInfo* GetClass(const FString& ClassName)
    {
        auto it = ClassMap.find(ClassName);
        if (it != ClassMap.end())
        {
            return it->second;
        }
        return nullptr; // 못 찾음
    }

    // 3. 에디터 UI에서 "모든 컴포넌트 목록"을 보여줄 때 사용합니다.
    static const TMap<FString, FClassInfo*>& GetAllClasses()
    {
        return ClassMap;
    }

    static void AddStruct(const FString& StructName, FStructInfo* StructInfo)
    {
        StructMap[StructName] = StructInfo;
    }

    static FStructInfo* GetStruct(const FString& StructName)
    {
        auto it = StructMap.find(StructName);
        if (it != StructMap.end())
        {
            return it->second;
        }
        return nullptr; // 못 찾음
    }

    static const TMap<FString, FStructInfo*>& GetAllStructs()
    {
        return StructMap;
    }

	//main()이 시작된 직후에 단 한 번 호출해서 부모자식 포인터를 싹 연결해줍니다.
	static void ResolveDependencies()
    {
        // 1. 클래스 족보 연결
        for (auto& Pair : ClassMap)
        {
            FClassInfo* Info = Pair.second;
            if (Info->ParentClassName.IsValid())
            {
                Info->ParentClass = GetClass(Info->ParentClassName.ToString());
            }
        }

        // 2. 구조체 족보 연결
        for (auto& Pair : StructMap)
        {
            FStructInfo* Info = Pair.second;
            if (Info->ParentStructName.IsValid())
            {
                Info->ParentStruct = GetStruct(Info->ParentStructName.ToString());
            }
        }
    }

	static void AddEnum(const FString& EnumName, FEnumInfo* EnumInfo)
    {
        EnumMap[EnumName] = EnumInfo;
    }

    static FEnumInfo* GetEnum(const FString& EnumName)
    {
        auto It = EnumMap.find(EnumName);
        return It != EnumMap.end() ? It->second : nullptr;
    }
};
