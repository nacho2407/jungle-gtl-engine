#pragma once

#include "Core/PropertyTypes.h"
#include "Core/Containers/Map.h"
#include "Core/Containers/Array.h"
#include "Core/Singleton.h"
#include "Core/Containers/String.h"
#include "Core/Reflection/ReflectionMacros.h"
#include "Object/Class.h"

// UObject, UClass, UScriptStruct, UEnum 등의 메타데이터를 관리하는 등록소입니다.
// RegisterUClass()로 UClass*를 클래스 이름으로 저장하면, FindClass()로 다시 꺼냅니다.
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

	void RegisterStruct(const UScriptStruct* Struct)
	{
		if (!Struct || !Struct->GetName())
		{
			return;
		}

		RuntimeStructs[Struct->GetName()] = Struct;
	}

	const UScriptStruct* FindStruct(const FString& StructName) const
	{
		auto It = RuntimeStructs.find(StructName);
		return It != RuntimeStructs.end() ? It->second : nullptr;
	}

	void RegisterEnum(const UEnum* Enum)
	{
		if (!Enum || !Enum->GetName())
		{
			return;
		}

		RuntimeEnums[Enum->GetName()] = Enum;
	}

	const UEnum* FindEnum(const FString& EnumName) const
	{
		auto It = RuntimeEnums.find(EnumName);
		return It != RuntimeEnums.end() ? It->second : nullptr;
	}

private:
	TMap<FString, UClass*> RuntimeClasses;
	TMap<FString, const UScriptStruct*> RuntimeStructs;
	TMap<FString, const UEnum*> RuntimeEnums;
};
