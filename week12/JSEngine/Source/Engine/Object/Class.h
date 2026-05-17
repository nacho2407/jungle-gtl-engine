#pragma once

#include "Core/CoreMinimal.h"
#include "Object/Property.h"

class UObject;

class UClass
{
public:
	using FCreateObjectFunc = UObject*(*)();

	UClass(const char* InName, UClass* InSuperClass, size_t InClassSize, uint32 InClassFlags, FCreateObjectFunc InCreateFunc = nullptr);

	const char* GetName() const { return Name; }
	UClass* GetSuperClass() const { return SuperClass; }
	size_t GetClassSize() const { return ClassSize; }
	uint32 GetClassFlags() const { return ClassFlags; }

	bool IsChildOf(const UClass* Other) const;
	bool HasAnyClassFlags(uint32 Flags) const { return (ClassFlags & Flags) != 0; }
	UObject* CreateObject() const;

	void AddProperty(const FProperty& Property);
	const FProperty* FindProperty(const char* PropertyName) const;
	void GetAllProperties(TArray<const FProperty*>& OutProperties) const;

	const TArray<FProperty>& GetProperties() const { return Properties; }

private:
	const char* Name = nullptr;
	UClass* SuperClass = nullptr;
	size_t ClassSize = 0;
	uint32 ClassFlags = 0;
	FCreateObjectFunc CreateFunc = nullptr;

	TArray<FProperty> Properties;
};
