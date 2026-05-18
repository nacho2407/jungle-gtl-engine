#pragma once

#include "Core/CoreMinimal.h"
#include "Object/Object.h"
#include "Object/Property.h"

#include <new>

class UObject;

struct IStructOps
{
	virtual ~IStructOps() = default;

	virtual void Construct(void* Ptr) const = 0;
	virtual void Destruct(void* Ptr) const = 0;
	virtual void Copy(void* Dst, const void* Src) const = 0;
};

template <typename T>
struct TStructOps final : IStructOps
{
	void Construct(void* Ptr) const override
	{
		new (Ptr) T();
	}

	void Destruct(void* Ptr) const override
	{
		static_cast<T*>(Ptr)->~T();
	}

	void Copy(void* Dst, const void* Src) const override
	{
		*static_cast<T*>(Dst) = *static_cast<const T*>(Src);
	}
};

template <typename T>
inline const IStructOps* GetStructOps()
{
	static TStructOps<T> Ops;
	return &Ops;
}

class UField : public UObject
{
public:
	UField(const char* InName, const char* InDisplayName = nullptr, const char* InCategory = nullptr);

	const char* GetName() const { return Name; }
	const char* GetDisplayName() const { return DisplayName ? DisplayName : Name; }
	const char* GetCategory() const { return Category; }

protected:
	const char* Name = nullptr;
	const char* DisplayName = nullptr;
	const char* Category = nullptr;
};

class UStruct : public UField
{
public:
	UStruct(const char* InName, UStruct* InSuperStruct, size_t InSize, size_t InAlignment, const char* InDisplayName = nullptr, const char* InCategory = nullptr);

	UStruct* GetSuperStruct() const { return SuperStruct; }
	size_t GetStructureSize() const { return StructureSize; }
	size_t GetMinAlignment() const { return MinAlignment; }

	void AddProperty(const FProperty& Property);
	const FProperty* FindProperty(const char* PropertyName) const;
	void GetAllProperties(TArray<const FProperty*>& OutProperties) const;

	const TArray<FProperty>& GetProperties() const { return Properties; }

protected:
	UStruct* SuperStruct = nullptr;
	size_t StructureSize = 0;
	size_t MinAlignment = 0;
	TArray<FProperty> Properties;
};

class UClass : public UStruct
{
public:
	using FCreateObjectFunc = UObject*(*)();

	UClass(const char* InName, UClass* InSuperClass, size_t InClassSize, uint32 InClassFlags, FCreateObjectFunc InCreateFunc = nullptr, const char* InDisplayName = nullptr, const char* InCategory = nullptr);

	UClass* GetSuperClass() const { return static_cast<UClass*>(GetSuperStruct()); }
	size_t GetClassSize() const { return GetStructureSize(); }
	uint32 GetClassFlags() const { return ClassFlags; }

	bool IsChildOf(const UClass* Other) const;
	bool HasAnyClassFlags(uint32 Flags) const { return (ClassFlags & Flags) != 0; }
	UObject* CreateObject() const;

private:
	uint32 ClassFlags = 0;
	FCreateObjectFunc CreateFunc = nullptr;
};

class UScriptStruct : public UStruct
{
public:
	UScriptStruct(const char* InName, size_t InSize, size_t InAlignment, const IStructOps* InStructOps, const char* InDisplayName = nullptr, const char* InCategory = nullptr);

	const IStructOps* GetStructOps() const { return StructOps; }

	void Construct(void* Ptr) const;
	void Destruct(void* Ptr) const;
	void Copy(void* Dst, const void* Src) const;

private:
	const IStructOps* StructOps = nullptr;
};

struct FEnumValue
{
	const char* Name = nullptr;
	const char* DisplayName = nullptr;
	int64 Value = 0;
};

class UEnum : public UField
{
public:
	UEnum(const char* InName, uint8 InSize, const FEnumValue* InValues, uint32 InCount, const char* InDisplayName = nullptr, const char* InCategory = nullptr);

	uint8 GetSize() const { return Size; }
	const FEnumValue* GetValues() const { return Values; }
	uint32 GetCount() const { return Count; }

	uint8 Size = 0;
	const FEnumValue* Values = nullptr;
	uint32 Count = 0;
};
