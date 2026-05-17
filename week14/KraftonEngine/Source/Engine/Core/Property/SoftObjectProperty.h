#pragma once

#include "ObjectPropertyBase.h"

struct FSoftObjectProperty : FObjectPropertyBase
{
	const char* AssetType = nullptr;

	FSoftObjectProperty() = default;
	FSoftObjectProperty(
		const char* InName,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName,
		const char* InAssetType,
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
		, AssetType(InAssetType)
	{
	}

	EPropertyType GetType() const override { return EPropertyType::SoftObjectRef; }
	const char* GetAssetType() const { return AssetType ? AssetType : ""; }
	const FSoftObjectProperty* AsSoftObjectProperty() const override { return this; }

	json::JSON Serialize(void* Container) const override;
	void	   Deserialize(void* Container, json::JSON& Value) const override;
	void	   Serialize(void* Container, FArchive& Ar) const override;
};
