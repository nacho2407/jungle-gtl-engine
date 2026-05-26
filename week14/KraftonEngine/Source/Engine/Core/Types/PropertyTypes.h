#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include "Core/Types/CoreTypes.h"

class FArchive;
class UStruct;
class UClass;
struct FPropertyValue;
struct FProperty;
struct FNumericProperty;
struct FBoolProperty;
struct FStringProperty;
struct FNameProperty;
struct FEnumProperty;
struct FObjectPropertyBase;
struct FObjectProperty;
struct FClassProperty;
struct FSoftObjectProperty;
struct FStructProperty;
struct FArrayProperty;
class UObject;

struct FPropertySerializeContext
{
	UObject* Owner = nullptr;
};

// 에디터에서 자동 위젯 매핑에 사용되는 프로퍼티 타입
enum class EPropertyType : uint8_t
{
	Bool,
	ByteBool, // uint8을 bool처럼 사용 (std::vector<bool> 회피용)
	Int,
	Float,
	Vec3,
	Vec4,
	Rotator,	// FRotator (Pitch, Yaw, Roll)
	String,
	Name,		  // FName — 문자열 풀 기반 이름 (리소스 키 등)
	ObjectRef,
	Color4,	   // FVector4 RGBA — ImGui::ColorEdit4 위젯
	ClassRef,	  // TSubclassOf<T> 의 UClass* 슬롯. allowedclass metadata 의 자식 콤보로 노출.
	Enum,
	Struct,    // 자기기술 구조체 — StructType의 property metadata로 Children 생성
	SoftObjectRef,
	Array,
};

struct FEnum
{
	const char* Name = nullptr;
	const char** Names = nullptr;
	uint32 Count = 0;
	uint32 Size = sizeof(int32);

	const char* GetName() const { return Name ? Name : ""; }
	const char** GetNames() const { return Names; }
	uint32 GetCount() const { return Count; }
	uint32 GetSize() const { return Size; }

	static TArray<const FEnum*>& GetAllEnums()
	{
		static TArray<const FEnum*> Registry;
		return Registry;
	}

	static const FEnum* FindEnumByName(const char* InName)
	{
		if (!InName) return nullptr;
		for (const FEnum* Enum : GetAllEnums())
		{
			if (Enum && Enum->Name && std::strcmp(Enum->Name, InName) == 0)
			{
				return Enum;
			}
		}
		return nullptr;
	}
};

struct FEnumRegistrar
{
	FEnumRegistrar(const FEnum* InEnum)
	{
		FEnum::GetAllEnums().push_back(InEnum);
	}
};

// 객체 인스턴스에 바인딩된 프로퍼티 값 뷰
struct FPropertyValue
{
	UObject* Object = nullptr;
	const FProperty* Property = nullptr;
	void* ContainerPtr = nullptr;

	void*	   GetValuePtr() const;
	void	   GetStructChildren(TArray<FPropertyValue>& OutProps) const;
	bool	   PassesEditCondition() const;

	const char* GetName() const;
	const char* GetDisplayName() const;
	const char* GetCategory() const;
	EPropertyType GetType() const;
	float GetMin() const;
	float GetMax() const;
	float GetSpeed() const;
	const FEnum* GetEnumType() const;
	UStruct* GetStructType() const;
	const TMap<FString, FString>& GetMetadata() const;
};

enum EPropertyFlags : uint32
{
	PF_None = 0,
	PF_Edit = 1 << 0,
	PF_Save = 1 << 1,
	PF_ReadOnly = 1 << 2,
	PF_Transient = 1 << 3, //저장, 로드에서 제외
	PF_InstancedReference = 1 << 4,
};

enum class EPropertyChangeType : uint8
{
	ValueSet,
	Interactive,
	ArrayAdd,
	ArrayRemove,
	Duplicate,
	Load,
};

struct FPropertyChangedEvent
{
	UObject* Object = nullptr;
	const FProperty* Property = nullptr;
	const char* PropertyName = nullptr;
	const char* DisplayName = nullptr;
	FString PropertyPath;
	EPropertyType Type = EPropertyType::Bool;
	EPropertyChangeType ChangeType = EPropertyChangeType::ValueSet;
	int32 ArrayIndex = -1;
};

struct FProperty
{
	const char* Name = nullptr;
	const char* Category = nullptr;
	uint32 Flags = PF_None;

	size_t Offset = 0;
	size_t Size = 0;

	const char* DisplayName = nullptr;
	TMap<FString, FString> Metadata;
	const char* OwnerClassName = nullptr;

	FProperty() = default;
	FProperty(
		const char* InName,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName)
		: Name(InName)
		, Category(InCategory)
		, Flags(InFlags)
		, Offset(InOffset)
		, Size(InSize)
		, DisplayName(InDisplayName)
		, Metadata(InMetadata)
		, OwnerClassName(InOwnerClassName)
	{
	}

	virtual ~FProperty() = default;

	inline void* GetValuePtrFor(void* Container) const
	{
		return Container ? reinterpret_cast<uint8*>(Container) + Offset : nullptr;
	}

	inline FPropertyValue ToValue(void* Container, UObject* Object = nullptr) const
	{
		FPropertyValue Desc;
		Desc.Object = Object;
		Desc.Property = this;
		Desc.ContainerPtr = Container;
		return Desc;
	}

	virtual EPropertyType GetType() const = 0;
	virtual float GetMin() const { return 0.0f; }
	virtual float GetMax() const { return 0.0f; }
	virtual float GetSpeed() const { return 0.1f; }
	virtual const FEnum* GetEnumType() const { return nullptr; }
	virtual UStruct* GetStructType() const { return nullptr; }
	virtual const FNumericProperty* AsNumericProperty() const { return nullptr; }
	virtual const FBoolProperty* AsBoolProperty() const { return nullptr; }
	virtual const FStringProperty* AsStringProperty() const { return nullptr; }
	virtual const FNameProperty* AsNameProperty() const { return nullptr; }
	virtual const FEnumProperty* AsEnumProperty() const { return nullptr; }
	virtual const FObjectPropertyBase* AsObjectPropertyBase() const { return nullptr; }
	virtual const FObjectProperty* AsObjectProperty() const { return nullptr; }
	virtual const FClassProperty* AsClassProperty() const { return nullptr; }
	virtual const FSoftObjectProperty* AsSoftObjectProperty() const { return nullptr; }
	virtual const FStructProperty* AsStructProperty() const { return nullptr; }
	virtual const FArrayProperty* AsArrayProperty() const { return nullptr; }

	virtual void	   Serialize(void* Container, FArchive& Ar) const;
	virtual void	   SerializeValue(void* ValuePtr, FArchive& Ar) const = 0;
	virtual void	   SerializeValue(void* ValuePtr, FArchive& Ar, const FPropertySerializeContext& Context) const;

	virtual void	   Serialize(UObject* Object, FArchive& Ar) const;
};

struct FGenericProperty : FProperty
{
	EPropertyType Type = EPropertyType::Bool;
	float Min = 0.0f;
	float Max = 0.0f;
	float Speed = 0.1f;

	FGenericProperty() = default;
	FGenericProperty(
		const char* InName,
		EPropertyType InType,
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
		, Type(InType)
		, Min(InMin)
		, Max(InMax)
		, Speed(InSpeed)
	{
	}

	EPropertyType GetType() const override { return Type; }
	float GetMin() const override { return Min; }
	float GetMax() const override { return Max; }
	float GetSpeed() const override { return Speed; }

	void	   SerializeValue(void* ValuePtr, FArchive& Ar) const override;
};

struct FObjectPropertyBase : FProperty
{
	const char* AllowedClass = nullptr;

	FObjectPropertyBase() = default;
	FObjectPropertyBase(
		const char* InName,
		const char* InCategory,
		uint32 InFlags,
		size_t InOffset,
		size_t InSize,
		const char* InDisplayName,
		const TMap<FString, FString>& InMetadata,
		const char* InOwnerClassName,
		const char* InAllowedClass)
		: FProperty(InName, InCategory, InFlags, InOffset, InSize, InDisplayName, InMetadata, InOwnerClassName)
		, AllowedClass(InAllowedClass)
	{
	}

	const char* GetAllowedClass() const { return AllowedClass ? AllowedClass : ""; }
	UClass* GetAllowedClassType() const;
	const FObjectPropertyBase* AsObjectPropertyBase() const override { return this; }
};
