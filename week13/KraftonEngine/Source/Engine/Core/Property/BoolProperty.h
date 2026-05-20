#pragma once

#include "Core/PropertyTypes.h"

struct FBoolProperty : FProperty
{
	FBoolProperty() = default;
	FBoolProperty(
		const char* InName,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName)
		: FProperty(InName, InCategory, InFlags, InOffset, InSize, InDisplayName, InMetadata, InOwnerClassName)
	{
	}

	EPropertyType GetType() const override { return EPropertyType::Bool; }
	const FBoolProperty* AsBoolProperty() const override { return this; }

	void	   SerializeValue(void* ValuePtr, FArchive& Ar) const override;
};
