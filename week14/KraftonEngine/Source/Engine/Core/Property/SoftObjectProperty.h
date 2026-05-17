#pragma once

#include "Core/PropertyTypes.h"

struct FSoftObjectProperty : FProperty
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
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName,
		const char* InAssetType,
		const char* InAllowedClass)
		: FProperty(InName, InCategory, InFlags, InOffset, InSize, InDisplayName, InMetadata, InOwnerClassName)
		, AssetType(InAssetType)
		, AllowedClass(InAllowedClass)
	{
	}

	EPropertyType GetType() const override { return EPropertyType::SoftObjectRef; }
	const char* GetAssetType() const { return AssetType ? AssetType : ""; }
	const char* GetAllowedClass() const { return AllowedClass ? AllowedClass : ""; }
	const FSoftObjectProperty* AsSoftObjectProperty() const override { return this; }

	json::JSON Serialize(void* Container) const override;
	void	   Deserialize(void* Container, json::JSON& Value) const override;
	void	   Serialize(void* Container, FArchive& Ar) const override;
};
