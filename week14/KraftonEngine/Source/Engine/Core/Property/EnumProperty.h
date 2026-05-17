#pragma once

#include "Core/Property/GenericProperty.h"

struct FEnumProperty : FGenericProperty
{
	const FEnum* EnumType = nullptr;

	FEnumProperty() = default;
	FEnumProperty(
		const char* InName,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		float InMin,
		float InMax,
		float InSpeed,
		const FEnum* InEnumType,
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName)
		: FGenericProperty(
			InName,
			EPropertyType::Enum,
			InCategory,
			InFlags,
			InOffset,
			InSize,
			InMin,
			InMax,
			InSpeed,
			InDisplayName,
			InMetadata,
			InOwnerClassName)
		, EnumType(InEnumType)
	{
	}

	const FEnum* GetEnumType() const override { return EnumType; }
	const FEnumProperty* AsEnumProperty() const override { return this; }

	json::JSON Serialize(void* Container) const override;
	void	   Deserialize(void* Container, json::JSON& Value) const override;
	void	   Serialize(void* Container, FArchive& Ar) const override;
};
