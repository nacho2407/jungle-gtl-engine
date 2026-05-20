#pragma once

#include "Core/Types/PropertyTypes.h"

struct FEnumProperty : FProperty
{
	const FEnum* EnumType = nullptr;

	FEnumProperty() = default;
	FEnumProperty(
		const char* InName,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		const FEnum* InEnumType,
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName)
		: FProperty(InName, InCategory, InFlags, InOffset, InSize, InDisplayName, InMetadata, InOwnerClassName)
		, EnumType(InEnumType)
	{
	}

	EPropertyType GetType() const override { return EPropertyType::Enum; }
	const FEnum* GetEnumType() const override { return EnumType; }
	const FEnumProperty* AsEnumProperty() const override { return this; }

	json::JSON SerializeValue(void* ValuePtr) const override;
	void	   DeserializeValue(void* ValuePtr, json::JSON& Value) const override;
	void	   SerializeValue(void* ValuePtr, FArchive& Ar) const override;
};
