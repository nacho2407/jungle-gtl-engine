#pragma once

#include "Core/Property/GenericProperty.h"

struct FSoftObjectProperty : FGenericProperty
{
	const char* AssetType = nullptr;
	const char* AllowedClass = nullptr;

	FSoftObjectProperty() = default;
	FSoftObjectProperty(
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
		const char* InOwnerClassName,
		const char* InAssetType,
		const char* InAllowedClass)
		: FGenericProperty(
			InName,
			EPropertyType::SoftObjectRef,
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
		, AssetType(InAssetType)
		, AllowedClass(InAllowedClass)
	{
	}

	const char* GetAssetType() const { return AssetType ? AssetType : ""; }
	const char* GetAllowedClass() const { return AllowedClass ? AllowedClass : ""; }
	const FSoftObjectProperty* AsSoftObjectProperty() const override { return this; }
};
