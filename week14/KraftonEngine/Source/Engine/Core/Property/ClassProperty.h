#pragma once

#include "ObjectPropertyBase.h"
#include "Object/SubclassOf.h"

struct FClassProperty : FObjectPropertyBase
{
	struct FOps
	{
		UClass* (*GetClass)(const void* ValuePtr) = nullptr;
		void (*SetClass)(void* ValuePtr, UClass* Class) = nullptr;
	};

	static const FOps* GetRawClassOps()
	{
		static const FOps Ops = {
			[](const void* ValuePtr) -> UClass*
			{
				return *static_cast<UClass* const*>(ValuePtr);
			},
			[](void* ValuePtr, UClass* Class)
			{
				*static_cast<UClass**>(ValuePtr) = Class;
			},
		};
		return &Ops;
	}

	template<typename BaseT>
	static const FOps* GetSubclassOfOps()
	{
		static const FOps Ops = {
			[](const void* ValuePtr) -> UClass*
			{
				return static_cast<const TSubclassOf<BaseT>*>(ValuePtr)->Get();
			},
			[](void* ValuePtr, UClass* Class)
			{
				static_cast<TSubclassOf<BaseT>*>(ValuePtr)->AssignUnchecked(Class);
			},
		};
		return &Ops;
	}

	FClassProperty() = default;
	FClassProperty(
		const char* InName,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		const FOps* InOps,
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName,
		const char* InAllowedClass)
		: FObjectPropertyBase(
			InName,
			InCategory,
			InFlags,
			InOffset,
			InSize,
			InDisplayName,
			InMetadata,
			InOwnerClassName,
			InAllowedClass)
		, Ops(InOps)
	{
	}

	EPropertyType GetType() const override { return EPropertyType::ClassRef; }
	const FClassProperty* AsClassProperty() const override { return this; }

	UClass* GetClassValue(void* Container) const;
	void SetClassValue(void* Container, UClass* Class) const;
	UClass* GetClassValueFromValuePtr(void* ValuePtr) const;
	void SetClassValueFromValuePtr(void* ValuePtr, UClass* Class) const;

	json::JSON SerializeValue(void* ValuePtr) const override;
	void	   DeserializeValue(void* ValuePtr, json::JSON& Value) const override;
	void	   SerializeValue(void* ValuePtr, FArchive& Ar) const override;

private:
	const FOps* Ops = nullptr;
};
