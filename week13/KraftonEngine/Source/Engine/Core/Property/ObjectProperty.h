#pragma once

#include "ObjectPropertyBase.h"
#include "Object/ObjectPtr.h"

struct FObjectProperty : FObjectPropertyBase
{
	struct FOps
	{
		UObject* (*GetObject)(const void* ValuePtr) = nullptr;
		void (*SetObject)(void* ValuePtr, UObject* Object) = nullptr;
	};

	template<typename ObjectT>
	static const FOps* GetRawPointerOps()
	{
		static const FOps Ops = {
			[](const void* ValuePtr) -> UObject*
			{
				return *static_cast<ObjectT* const*>(ValuePtr);
			},
			[](void* ValuePtr, UObject* Object)
			{
				*static_cast<ObjectT**>(ValuePtr) = static_cast<ObjectT*>(Object);
			},
		};
		return &Ops;
	}

	template<typename ObjectT>
	static const FOps* GetObjectPtrOps()
	{
		static const FOps Ops = {
			[](const void* ValuePtr) -> UObject*
			{
				return static_cast<const TObjectPtr<ObjectT>*>(ValuePtr)->Get();
			},
			[](void* ValuePtr, UObject* Object)
			{
				*static_cast<TObjectPtr<ObjectT>*>(ValuePtr) = static_cast<ObjectT*>(Object);
			},
		};
		return &Ops;
	}

	FObjectProperty() = default;
	FObjectProperty(
		const char* InName,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		const FOps* InOps,
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName,
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
	{
	}

	EPropertyType GetType() const override { return EPropertyType::ObjectRef; }
	const FObjectProperty* AsObjectProperty() const override { return this; }
	UObject* GetObjectValue(void* Container) const;
	void SetObjectValue(void* Container, UObject* Object) const;
	UObject* GetObjectValueFromValuePtr(void* ValuePtr) const;
	void SetObjectValueFromValuePtr(void* ValuePtr, UObject* Object) const;

	json::JSON SerializeValue(void* ValuePtr) const override;
	void	   DeserializeValue(void* ValuePtr, json::JSON& Value) const override;
	json::JSON Serialize(UObject* Object, const FJsonObjectReferenceContext* RefContext) const override;
	void	   Deserialize(UObject* Object, json::JSON& Value, const FJsonObjectReferenceContext* RefContext) const override;
	void	   Serialize(UObject* Object, FArchive& Ar) const override;
	void	   SerializeValue(void* ValuePtr, FArchive& Ar) const override;
	json::JSON SerializeValue(void* ValuePtr, const FPropertySerializeContext& Context) const override;
	void	   DeserializeValue(void* ValuePtr, json::JSON& Value, const FPropertySerializeContext& Context) const override;
	void	   SerializeValue(void* ValuePtr, FArchive& Ar, const FPropertySerializeContext& Context) const override;

private:
	const FOps* Ops = nullptr;
};
