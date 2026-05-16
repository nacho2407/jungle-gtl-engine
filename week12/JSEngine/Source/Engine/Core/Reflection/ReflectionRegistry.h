#pragma once

#include "Core/PropertyTypes.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/Array.h"
#include "Core/Singleton.h"
#include "Core/Containers/String.h"
#include "Core/Reflection/ReflectionMacros.h"
#include "Object/Class.h"

struct FEnumValueMetaData
{
	const char* Name = nullptr;
	const char* DisplayName = nullptr;
	int64 Value = 0;
};

// 파싱된 Enum 타입에 대한 메타데이터
struct FEnumMetaData
{
	const char* Name = nullptr;
	uint8 Size = 0;
	const FEnumValueMetaData* Values = nullptr;
	uint32 Count = 0;
};

// 파싱된 프로퍼티의 영구 메타데이터
struct FPropertyMetaData
{
	const char* Name = nullptr;
	EPropertyType Type = EPropertyType::Unknown;
	size_t Offset = 0;
	EPropertyUsageFlags UsageFlags = EPropertyUsageFlags::Editable;
	
	float Min = 0.0f;
	float Max = 0.0f;
	float Speed = 0.1f;

	const char* DisplayName = nullptr;

	const FEnumMetaData* EnumMeta = nullptr;
};

// 파싱된 클래스의 영구 메타데이터
struct FClassMetaData
{
	const char* ClassName = nullptr;
	const struct FTypeInfo* TypeInfo = nullptr;
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

	void RegisterUClass(UClass* Class)
	{
		if (!Class || !Class->GetName())
		{
			return;
		}

		const FString ClassName = Class->GetName();
		auto It = RuntimeClasses.find(ClassName);
		if (It != RuntimeClasses.end())
		{
			return;
		}
		RuntimeClasses[ClassName] = Class;
	}

	UClass* FindUClass(const FString& ClassName) const
	{
		auto It = RuntimeClasses.find(ClassName);
		return It != RuntimeClasses.end() ? It->second : nullptr;
	}

	const TMap<FString, UClass*>& GetRuntimeClasses() const
	{
		return RuntimeClasses;
	}
	
private:
	TMap<FString, FClassMetaData> RegisteredClass;
	TMap<FString, UClass*> RuntimeClasses;
};

// 전역 변수 초기화를 이용한 자동 등록 헬퍼
struct FAutoClassRegister
{
	FAutoClassRegister(const FClassMetaData& MetaData)
	{
		FReflectionRegistry::Get().RegisterClass(MetaData);
	}
};
