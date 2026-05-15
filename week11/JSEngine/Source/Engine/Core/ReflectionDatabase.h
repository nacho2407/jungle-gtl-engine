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

public:
    // 1. 파서가 생성한 코드가 이 함수를 통해 정보를 제출합니다.
    static void AddClass(const FString& ClassName, FClassInfo* ClassInfo)
    {
        ClassMap[ClassName] = ClassInfo;
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
};