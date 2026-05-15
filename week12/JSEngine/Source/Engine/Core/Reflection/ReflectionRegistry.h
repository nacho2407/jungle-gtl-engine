#pragma once

#include "Core/PropertyTypes.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/Array.h"
#include "Core/Singleton.h"
#include "Core/Containers/String.h"
#include "Core/Reflection/ReflectionMacros.h"

struct FEnumValueMetaData
{
    const char* Name;
    const char* DisplayName;
    int64 Value;
};

// 파싱된 Enum 타입에 대한 메타데이터
struct FEnumMetaData
{
    const char* Name;
    uint8 Size;
    const FEnumValueMetaData* Values;
    uint32 ValueCount;
};

// 파싱된 프로퍼티의 영구 메타데이터
struct FPropertyMetaData
{
	const char* Name;
	EPropertyType Type;
	size_t Offset;
	EPropertyUsageFlags UsageFlags = EPropertyUsageFlags::Editable;
	
	float Min = 0.0f;
	float Max = 0.0f;
	float Speed = 0.1f;

	const char* DisplayName = nullptr;

	uint8 ElementSize = 0;
    const FEnumMetaData* EnumMetaData = nullptr;
};

// 파싱된 클래스의 영구 메타데이터
struct FClassMetaData
{
	const char* ClassName;
	const struct FTypeInfo* TypeInfo;
	TArray<FPropertyMetaData> Properties;
};

class FReflectionRegistry : public TSingleton<FReflectionRegistry>
{
public:
	void RegisterClass(const FClassMetaData& MetaData)
	{
		RegisteredClass[MetaData.ClassName] = MetaData;
	}
	
	const FClassMetaData* GetRegisteredClass(const char* ClassName)
	{
		auto It = RegisteredClass.find(ClassName);
		if (It != RegisteredClass.end())
			return &It->second;
		return nullptr;
	}
	
private:
	TMap<FString, FClassMetaData> RegisteredClass;
};

// 전역 변수 초기화를 이용한 자동 등록 헬퍼
struct FAutoClassRegister
{
	FAutoClassRegister(const FClassMetaData& MetaData)
	{
		FReflectionRegistry::Get().RegisterClass(MetaData);
	}
};
