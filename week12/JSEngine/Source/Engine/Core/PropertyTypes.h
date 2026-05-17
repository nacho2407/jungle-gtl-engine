#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "CoreTypes.h"      // int32, uint8, …
#include "Math/Vector.h"    // FVector  (for sizeof in GetPropertySize)
#include "Math/Vector4.h"   // FVector4 (for sizeof in GetPropertySize)
#include "Math/Quat.h"
#include "Core/Guid.h"

// 에디터에서 자동 위젯 매핑에 사용되는 프로퍼티 타입
enum class EPropertyType : uint8_t
{
	Unknown,
	Bool,
	Int,
	Float,
	Color,
	Vec3,
	Vec4,
	Guid,
	Quat,

	// String, Name
	String,
	Name,

	// Enum MetaData
	Enum,

	ObjectPtr,
	SoftObjectPtr,
	Array,
	Struct,
};

enum class EObjectReferenceKind : uint8_t
{
	None,
	RuntimeObject,
	ActorComponent,
	Asset,
};

enum class EPropertyUsageFlags : uint8_t
{
	None = 0,
	Editable = 1 << 0,
	Animatable = 1 << 1,
};

constexpr EPropertyUsageFlags operator|(EPropertyUsageFlags Lhs, EPropertyUsageFlags Rhs)
{
	return static_cast<EPropertyUsageFlags>(
		static_cast<uint8_t>(Lhs) | static_cast<uint8_t>(Rhs));
}

constexpr bool HasPropertyUsage(EPropertyUsageFlags Value, EPropertyUsageFlags Flag)
{
	return (static_cast<uint8_t>(Value) & static_cast<uint8_t>(Flag)) != 0;
}

enum class EPropertyChangeType : uint8_t
{
	ValueSet,
	Interactive,
	Preview,
	Runtime,
};

struct FPropertyChangedEvent
{
	const char* PropertyName = nullptr;
	EPropertyChangeType ChangeType = EPropertyChangeType::ValueSet;
};

struct UEnum;
using FEnumMetaData = UEnum;

/** 각 프로퍼티의 Size 값을 반환합니다. 0을 반환하는 경우 특수 케이스입니다.
 * 이런 경우에는 CopyPropertiesFrom 함수 내에서 알아서 잘 처리해줄 수 있어야 합니다. 
 **/
inline size_t GetPropertySize(EPropertyType Type)
{
	switch (Type)
	{
	case EPropertyType::Bool:   return sizeof(bool);
	case EPropertyType::Int:    return sizeof(int32);
	case EPropertyType::Float:  return sizeof(float);
	case EPropertyType::Vec3:   return sizeof(FVector);
	case EPropertyType::Color:	return sizeof(FColor);
	case EPropertyType::Vec4:   return sizeof(FVector4);
	case EPropertyType::Guid:   return sizeof(FGuid);
	case EPropertyType::Quat:   return sizeof(FQuat);
	// String, Name 은 ValuePtr 기반 특수 처리
	case EPropertyType::String: return 0;
	case EPropertyType::Name:   return 0;
	case EPropertyType::ObjectPtr: return 0;
	case EPropertyType::SoftObjectPtr: return 0;
	case EPropertyType::Array: return 0;
	default: return 0;
	}
}
