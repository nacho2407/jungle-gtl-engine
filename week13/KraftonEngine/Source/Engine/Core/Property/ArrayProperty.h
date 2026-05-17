#pragma once

#include "Core/PropertyTypes.h"

struct FArrayProperty : FProperty
{
	EPropertyType Type = EPropertyType::SoftObjectRefArray;
	EPropertyType ElementType = EPropertyType::SoftObjectRef;

	struct FOps
	{
		size_t (*GetNum)(const void* ArrayPtr) = nullptr;
		void (*Resize)(void* ArrayPtr, size_t Num) = nullptr;
		void* (*GetElementPtr)(void* ArrayPtr, size_t Index) = nullptr;
		const void* (*GetConstElementPtr)(const void* ArrayPtr, size_t Index) = nullptr;
	};

	template<typename ElementT>
	static const FOps* GetOps()
	{
		static const FOps Ops = {
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
		const FOps* InOps,
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
		, Ops(InOps)
	{
	}

	EPropertyType GetType() const override { return Type; }
	EPropertyType GetElementType() const { return ElementType; }
	const FArrayProperty* AsArrayProperty() const override { return this; }

	json::JSON Serialize(void* Container) const override;
	void	   Deserialize(void* Container, json::JSON& Value) const override;
	void	   Serialize(void* Container, FArchive& Ar) const override;

private:
	const FOps* Ops = nullptr;
};
