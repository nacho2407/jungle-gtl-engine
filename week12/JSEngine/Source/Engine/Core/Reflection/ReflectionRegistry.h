#pragma once

#include "Core/PropertyTypes.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/Array.h"
#include "Core/Singleton.h"
#include "Core/Containers/String.h"
#include "Core/Reflection/ReflectionMacros.h"
#include "Object/Class.h"

struct FEnumValue
{
	const char* Name = nullptr;
	const char* DisplayName = nullptr;
	int64 Value = 0;
};

// 파싱된 Enum 타입에 대한 런타임 메타데이터입니다.
// 추후 UObject - UField - UEnum 구조로 확장될 수 있습니다.
struct UEnum
{
	const char* Name = nullptr;
	uint8 Size = 0;
	const FEnumValue* Values = nullptr;
	uint32 Count = 0;
};

class FReflectionRegistry : public TSingleton<FReflectionRegistry>
{
public:
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

	UClass* FindClass(const FString& ClassName) const
	{
		return FindUClass(ClassName);
	}

	void GetClassesDerivedFrom(const UClass* BaseClass, TArray<UClass*>& OutClasses) const
	{
		if (!BaseClass)
		{
			return;
		}

		for (const auto& Pair : RuntimeClasses)
		{
			UClass* Class = Pair.second;
			if (Class && Class->IsChildOf(BaseClass))
			{
				OutClasses.push_back(Class);
			}
		}
	}

	const TMap<FString, UClass*>& GetRuntimeClasses() const
	{
		return RuntimeClasses;
	}

	void RegisterEnum(const UEnum* Enum)
	{
		if (!Enum || !Enum->Name)
		{
			return;
		}

		RuntimeEnums[Enum->Name] = Enum;
	}

	const UEnum* FindEnum(const FString& EnumName) const
	{
		auto It = RuntimeEnums.find(EnumName);
		return It != RuntimeEnums.end() ? It->second : nullptr;
	}
	
private:
	TMap<FString, UClass*> RuntimeClasses;
	TMap<FString, const UEnum*> RuntimeEnums;
};
