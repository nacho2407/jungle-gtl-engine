#pragma once
#include "Core/PropertyTypes.h"

struct FStructProperty : FProperty
{
	UStruct* StructType = nullptr;

	FStructProperty() = default;
	FStructProperty(
		const char* InName,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		UStruct* InStructType,
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName)
		: FProperty(InName, InCategory, InFlags, InOffset, InSize, InDisplayName, InMetadata, InOwnerClassName)
		, StructType(InStructType)
	{
	}

	EPropertyType GetType() const override { return EPropertyType::Struct; }
	UStruct* GetStructType() const override { return StructType; }
	const FStructProperty* AsStructProperty() const override { return this; }

	json::JSON SerializeValue(void* ValuePtr) const override;
	void	   DeserializeValue(void* ValuePtr, json::JSON& Value) const override;
	void	   SerializeValue(void* ValuePtr, FArchive& Ar) const override;
	json::JSON SerializeValue(void* ValuePtr, const FPropertySerializeContext& Context) const override;
	void	   DeserializeValue(void* ValuePtr, json::JSON& Value, const FPropertySerializeContext& Context) const override;
	void	   SerializeValue(void* ValuePtr, FArchive& Ar, const FPropertySerializeContext& Context) const override;
};
