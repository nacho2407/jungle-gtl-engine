#pragma once

#include "Core/PropertyTypes.h"

struct FNumericProperty : FProperty
{
	float Min = 0.0f;
	float Max = 0.0f;
	float Speed = 0.1f;

	FNumericProperty() = default;
	FNumericProperty(
		const char* InName,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		float InMin,
		float InMax,
		float InSpeed,
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName)
		: FProperty(InName, InCategory, InFlags, InOffset, InSize, InDisplayName, InMetadata, InOwnerClassName)
		, Min(InMin)
		, Max(InMax)
		, Speed(InSpeed)
	{
	}

	float GetMin() const override { return Min; }
	float GetMax() const override { return Max; }
	float GetSpeed() const override { return Speed; }
	const FNumericProperty* AsNumericProperty() const override { return this; }
};


struct FIntProperty : FNumericProperty
{
	using FNumericProperty::FNumericProperty;

	EPropertyType GetType() const override { return EPropertyType::Int; }

	json::JSON SerializeValue(void* ValuePtr) const override;
	void	   DeserializeValue(void* ValuePtr, json::JSON& Value) const override;
	void	   SerializeValue(void* ValuePtr, FArchive& Ar) const override;
};

struct FFloatProperty : FNumericProperty
{
	using FNumericProperty::FNumericProperty;

	EPropertyType GetType() const override { return EPropertyType::Float; }

	json::JSON SerializeValue(void* ValuePtr) const override;
	void	   DeserializeValue(void* ValuePtr, json::JSON& Value) const override;
	void	   SerializeValue(void* ValuePtr, FArchive& Ar) const override;
};

