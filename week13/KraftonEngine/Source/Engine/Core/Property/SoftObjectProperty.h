#pragma once

#include "ObjectPropertyBase.h"
#include "Object/Ptr/SoftObjectPtr.h"
#include "Serialization/Archive.h"

struct FSoftObjectProperty : FObjectPropertyBase
{
	struct FOps
	{
		const FString& (*GetPath)(const void* ValuePtr) = nullptr;
		void (*SetPath)(void* ValuePtr, const FString& Path) = nullptr;
		void (*SerializeArchive)(void* ValuePtr, FArchive& Ar) = nullptr;
	};

	static const FOps* GetStringOps()
	{
		static const FOps Ops = {
			[](const void* ValuePtr) -> const FString&
			{
				return *static_cast<const FString*>(ValuePtr);
			},
			[](void* ValuePtr, const FString& Path)
			{
				*static_cast<FString*>(ValuePtr) = Path;
			},
			[](void* ValuePtr, FArchive& Ar)
			{
				Ar << *static_cast<FString*>(ValuePtr);
			},
		};
		return &Ops;
	}

	static const FOps* GetSoftObjectPtrOps()
	{
		static const FOps Ops = {
			[](const void* ValuePtr) -> const FString&
			{
				return static_cast<const FSoftObjectPtr*>(ValuePtr)->ToString();
			},
			[](void* ValuePtr, const FString& Path)
			{
				static_cast<FSoftObjectPtr*>(ValuePtr)->SetPath(Path);
			},
			[](void* ValuePtr, FArchive& Ar)
			{
				FSoftObjectPtr* Ptr = static_cast<FSoftObjectPtr*>(ValuePtr);
				FString Path = Ar.IsSaving() ? Ptr->ToString() : FString();
				Ar << Path;
				if (Ar.IsLoading())
				{
					Ptr->SetPath(Path);
				}
			},
		};
		return &Ops;
	}

	const char* AssetType = nullptr;

	FSoftObjectProperty() = default;
	FSoftObjectProperty(
		const char* InName,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		const FOps* InOps,
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
		, Ops(InOps)
		, AssetType(InAssetType)
	{
	}

	EPropertyType GetType() const override { return EPropertyType::SoftObjectRef; }
	const char* GetAssetType() const { return AssetType ? AssetType : ""; }
	const FString& GetPathFromValuePtr(void* ValuePtr) const
	{
		static const FString EmptyPath = "None";
		return ValuePtr && Ops && Ops->GetPath ? Ops->GetPath(ValuePtr) : EmptyPath;
	}
	void SetPathFromValuePtr(void* ValuePtr, const FString& Path) const
	{
		if (ValuePtr && Ops && Ops->SetPath)
		{
			Ops->SetPath(ValuePtr, Path);
		}
	}
	const FString& GetPath(void* Container) const
	{
		return GetPathFromValuePtr(GetValuePtrFor(Container));
	}
	void SetPath(void* Container, const FString& Path) const
	{
		SetPathFromValuePtr(GetValuePtrFor(Container), Path);
	}
	const FSoftObjectProperty* AsSoftObjectProperty() const override { return this; }

	void	   SerializeValue(void* ValuePtr, FArchive& Ar) const override;

private:
	const FOps* Ops = nullptr;
};
