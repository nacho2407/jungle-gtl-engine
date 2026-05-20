#pragma once

#include "Core/Types/PropertyTypes.h"

struct FStringProperty : FProperty
{
	FStringProperty() = default;
	FStringProperty(
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

	EPropertyType GetType() const override { return EPropertyType::String; }
	const FStringProperty* AsStringProperty() const override { return this; }

	void	   SerializeValue(void* ValuePtr, FArchive& Ar) const override;
};
