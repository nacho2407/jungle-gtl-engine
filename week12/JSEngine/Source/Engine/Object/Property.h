#pragma once

#include "Core/PropertyTypes.h"
#include "Object/FName.h"

class UObject;
class UClass;

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
struct TReflectedPropertyType;

template<> struct TReflectedPropertyType<bool>    { static constexpr EPropertyType Value = EPropertyType::Bool; };
template<> struct TReflectedPropertyType<int32>   { static constexpr EPropertyType Value = EPropertyType::Int; };
template<> struct TReflectedPropertyType<float>   { static constexpr EPropertyType Value = EPropertyType::Float; };
template<> struct TReflectedPropertyType<FString> { static constexpr EPropertyType Value = EPropertyType::String; };
template<> struct TReflectedPropertyType<FName>   { static constexpr EPropertyType Value = EPropertyType::Name; };
template<> struct TReflectedPropertyType<FVector> { static constexpr EPropertyType Value = EPropertyType::Vec3; };
template<> struct TReflectedPropertyType<FVector4>{ static constexpr EPropertyType Value = EPropertyType::Vec4; };
template<> struct TReflectedPropertyType<FColor>  { static constexpr EPropertyType Value = EPropertyType::Color; };
template<> struct TReflectedPropertyType<FGuid>   { static constexpr EPropertyType Value = EPropertyType::Guid; };
template<> struct TReflectedPropertyType<FQuat>   { static constexpr EPropertyType Value = EPropertyType::Quat; };

template<typename T>
struct TReflectedPropertyType<T*>
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

	FPropertyDescriptor ToDescriptor(UObject* Container) const
	{
		FPropertyDescriptor Desc;
		Desc.Name = Name;
		Desc.Type = Type;
		Desc.ValuePtr = Container ? reinterpret_cast<uint8*>(Container) + Offset : nullptr;
		Desc.UsageFlags = ToUsageFlags();
		Desc.Min = Min;
		Desc.Max = Max;
		Desc.Speed = Speed;
		Desc.DisplayName = DisplayName;
		Desc.EnumMeta = EnumMeta;
		return Desc;
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
		if (!Container || Type != TReflectedPropertyType<ValueType>::Value)
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
		if (!Container || Type != TReflectedPropertyType<ValueType>::Value)
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
