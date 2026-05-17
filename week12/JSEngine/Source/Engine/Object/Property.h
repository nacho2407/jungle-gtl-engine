#pragma once

#include "Core/PropertyTypes.h"
#include "Object/FName.h"

class UObject;
class UClass;
class UMaterialInterface;
struct FArchive;

// 런타임 리플렉션에서 프로퍼티 접근 권한과 용도를 표현합니다.
// 기존 EPropertyUsageFlags는 에디터 노출 힌트로 유지하고,
// EPropertyFlags는 런타임 read/write/serialize/bind 정책까지 포괄합니다.
enum class EPropertyFlags : uint32
{
	None       = 0,
	Read       = 1 << 0,
	Write      = 1 << 1,
	Edit       = 1 << 2,
	Transient  = 1 << 3,
	SaveGame   = 1 << 4,
	Animatable = 1 << 5,
	LuaRead    = 1 << 6,
	LuaWrite   = 1 << 7,
};


constexpr EPropertyFlags operator|(EPropertyFlags Lhs, EPropertyFlags Rhs)
{
	return static_cast<EPropertyFlags>(static_cast<uint32>(Lhs) | static_cast<uint32>(Rhs));
}

constexpr EPropertyFlags operator&(EPropertyFlags Lhs, EPropertyFlags Rhs)
{
	return static_cast<EPropertyFlags>(static_cast<uint32>(Lhs) & static_cast<uint32>(Rhs));
}

constexpr EPropertyFlags& operator|=(EPropertyFlags& Lhs, EPropertyFlags Rhs)
{
	Lhs = Lhs | Rhs;
	return Lhs;
}

constexpr bool HasPropertyFlag(EPropertyFlags Value, EPropertyFlags Flag)
{
	return (static_cast<uint32>(Value) & static_cast<uint32>(Flag)) != 0;
}

template<typename T>
struct TPropertyType;

template<> struct TPropertyType<bool>    { static constexpr EPropertyType Value = EPropertyType::Bool; };
template<> struct TPropertyType<int32>   { static constexpr EPropertyType Value = EPropertyType::Int; };
template<> struct TPropertyType<float>   { static constexpr EPropertyType Value = EPropertyType::Float; };
template<> struct TPropertyType<FString> { static constexpr EPropertyType Value = EPropertyType::String; };
template<> struct TPropertyType<FName>   { static constexpr EPropertyType Value = EPropertyType::Name; };
template<> struct TPropertyType<FVector> { static constexpr EPropertyType Value = EPropertyType::Vec3; };
template<> struct TPropertyType<FVector4>{ static constexpr EPropertyType Value = EPropertyType::Vec4; };
template<> struct TPropertyType<FColor>  { static constexpr EPropertyType Value = EPropertyType::Color; };
template<> struct TPropertyType<FGuid>   { static constexpr EPropertyType Value = EPropertyType::Guid; };
template<> struct TPropertyType<FQuat>   { static constexpr EPropertyType Value = EPropertyType::Quat; };

template<typename T>
struct TPropertyType<T*>
{
	static constexpr EPropertyType Value = EPropertyType::SceneComponentRef;
};

struct FProperty
{
	const char* Name = nullptr;
	const char* DisplayName = nullptr;
	const char* Category = nullptr;

	EPropertyType Type = EPropertyType::Unknown;
	EPropertyFlags Flags = EPropertyFlags::None;

	size_t Offset = 0;
	size_t Size = 0;

	float Min = 0.0f;
	float Max = 0.0f;
	float Speed = 0.1f;

	const FEnumMetaData* EnumMeta = nullptr;
	UClass* ObjectClass = nullptr;
	const FProperty* InnerProperty = nullptr;

	FProperty() = default;

	explicit FProperty(const struct FPropertyParams& Params);

	void* GetValuePtr(UObject* Container) const;
	const void* GetValuePtr(const UObject* Container) const;

	void SerializeItem(FArchive& Ar, UObject* Container) const;
	bool CopyValue(UObject* DstContainer, const UObject* SrcContainer) const;
	bool IsSequencerScalar() const;
	bool ReadScalarChannelValue(const UObject* Container, const FString& ChannelName, float& OutValue) const;
	bool WriteScalarChannelValue(UObject* Container, const FString& ChannelName, float NewValue) const;

	bool IsEditable() const { return HasPropertyFlag(Flags, EPropertyFlags::Edit); }
	bool IsTransient() const { return HasPropertyFlag(Flags, EPropertyFlags::Transient); }

	EPropertyUsageFlags ToUsageFlags() const
	{
		EPropertyUsageFlags Usage = EPropertyUsageFlags::None;
		if (HasPropertyFlag(Flags, EPropertyFlags::Edit))
		{
			Usage = Usage | EPropertyUsageFlags::Editable;
		}
		if (HasPropertyFlag(Flags, EPropertyFlags::Animatable))
		{
			Usage = Usage | EPropertyUsageFlags::Animatable;
		}
		return Usage;
	}

	template <typename ValueType>
	ValueType* ContainerPtrToValuePtr(UObject* Container) const
	{
		if (!Container)
		{
			return nullptr;
		}
		return reinterpret_cast<ValueType*>(reinterpret_cast<uint8*>(Container) + Offset);
	}

	template <typename ValueType>
	const ValueType* ContainerPtrToValuePtr(const UObject* Container) const
	{
		if (!Container)
		{
			return nullptr;
		}
		return reinterpret_cast<const ValueType*>(reinterpret_cast<const uint8*>(Container) + Offset);
	}

	template <typename ValueType>
	bool GetPropertyValueInContainer(const UObject* Container, ValueType& OutValue) const
	{
		if (!Container || Type != TPropertyType<ValueType>::Value)
		{
			return false;
		}
		if (!HasPropertyFlag(Flags, EPropertyFlags::Read))
		{
			return false;
		}
		const ValueType* ValuePtr = ContainerPtrToValuePtr<ValueType>(Container);
		if (!ValuePtr)
		{
			return false;
		}
		OutValue = *ValuePtr;
		return true;
	}

	template <typename ValueType>
	bool SetPropertyValueInContainer(UObject* Container, const ValueType& InValue) const
	{
		if (!Container || Type != TPropertyType<ValueType>::Value)
		{
			return false;
		}
		if (!HasPropertyFlag(Flags, EPropertyFlags::Write))
		{
			return false;
		}
		ValueType* ValuePtr = ContainerPtrToValuePtr<ValueType>(Container);
		if (!ValuePtr)
		{
			return false;
		}
		*ValuePtr = InValue;
		return true;
	}
};

struct FPropertyParams
{
	const char* Name = nullptr;
	const char* DisplayName = nullptr;
	const char* Category = nullptr;

	EPropertyType Type = EPropertyType::Unknown;
	EPropertyFlags Flags = EPropertyFlags::None;

	size_t Offset = 0;
	size_t Size = 0;

	float Min = 0.0f;
	float Max = 0.0f;
	float Speed = 0.1f;

	const FEnumMetaData* EnumMeta = nullptr;
	UClass* ObjectClass = nullptr;
	const FProperty* InnerProperty = nullptr;
};

inline FProperty::FProperty(const FPropertyParams& Params)
	: Name(Params.Name)
	, DisplayName(Params.DisplayName)
	, Category(Params.Category)
	, Type(Params.Type)
	, Flags(Params.Flags)
	, Offset(Params.Offset)
	, Size(Params.Size)
	, Min(Params.Min)
	, Max(Params.Max)
	, Speed(Params.Speed)
	, EnumMeta(Params.EnumMeta)
	, ObjectClass(Params.ObjectClass)
	, InnerProperty(Params.InnerProperty)
{
}

struct FPropertyHandle
{
	UObject* Owner = nullptr;
	const FProperty* Property = nullptr;

	bool IsValid() const { return Owner && Property; }
	const char* GetName() const { return Property ? Property->Name : nullptr; }

	void* GetValuePtr() const
	{
		return IsValid() ? Property->GetValuePtr(Owner) : nullptr;
	}

	template <typename ValueType>
	ValueType* GetValuePtr() const
	{
		return IsValid() ? Property->ContainerPtrToValuePtr<ValueType>(Owner) : nullptr;
	}
};

inline void SerializeProperty(FArchive& Ar, UObject* Object, const FProperty& Property)
{
	Property.SerializeItem(Ar, Object);
}

inline bool CopyPropertyValue(UObject* Dst, const UObject* Src, const FProperty& Property)
{
	return Property.CopyValue(Dst, Src);
}
