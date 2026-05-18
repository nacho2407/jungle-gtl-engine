#pragma once

#include "Core/PropertyTypes.h"

struct FArrayProperty : FProperty
{
	EPropertyType Type = EPropertyType::SoftObjectRefArray;
	EPropertyType ElementType = EPropertyType::SoftObjectRef;

	struct FArrayOps
	{
		size_t (*GetNum)(const void* ArrayPtr) = nullptr;
		void (*Resize)(void* ArrayPtr, size_t Num) = nullptr;
		void* (*GetElementPtr)(void* ArrayPtr, size_t Index) = nullptr;
		const void* (*GetConstElementPtr)(const void* ArrayPtr, size_t Index) = nullptr;
	};

	template<typename ElementT>
	static const FArrayOps* GetArrayOps()
	{
		static const FArrayOps Ops = {
			[](const void* ArrayPtr) -> size_t
			{
				return static_cast<const TArray<ElementT>*>(ArrayPtr)->size();
			},
			[](void* ArrayPtr, size_t Num)
			{
				static_cast<TArray<ElementT>*>(ArrayPtr)->resize(Num);
			},
			[](void* ArrayPtr, size_t Index) -> void*
			{
				return &(*static_cast<TArray<ElementT>*>(ArrayPtr))[Index];
			},
			[](const void* ArrayPtr, size_t Index) -> const void*
			{
				return &(*static_cast<const TArray<ElementT>*>(ArrayPtr))[Index];
			},
		};
		return &Ops;
	}

	FArrayProperty() = default;
	FArrayProperty(
		const char* InName,
		EPropertyType InType,
		EPropertyType InElementType,
		const FArrayOps* InArrayOps,
		const FProperty* InInnerProperty,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName)
		: FProperty(InName, InCategory, InFlags, InOffset, InSize, InDisplayName, InMetadata, InOwnerClassName)
		, Type(InType)
		, ElementType(InElementType)
		, ArrayOps(InArrayOps)
		, InnerProperty(InInnerProperty)
	{
	}

	EPropertyType GetType() const override { return Type; }
	EPropertyType GetElementType() const { return ElementType; }
	const FProperty* GetInnerProperty() const { return InnerProperty; }
	const FArrayProperty* AsArrayProperty() const override { return this; }

	json::JSON SerializeValue(void* ValuePtr) const override;
	void	   DeserializeValue(void* ValuePtr, json::JSON& Value) const override;
	void	   SerializeValue(void* ValuePtr, FArchive& Ar) const override;

private:
	const FArrayOps* ArrayOps = nullptr;
	const FProperty* InnerProperty = nullptr;
};
